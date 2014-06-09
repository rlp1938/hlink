
/*      hlink.c
 *
 *	Copyright 2011 Bob Parker <rlp1938@gmail.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *	MA 02110-1301, USA.
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>
#include "fileutil.h"

static void help_print(int forced);
static char *dostrdup(const char *s);
static void recursedir(char *headdir, FILE *fpo, char **vlist);
static char **mem2pathlist(char *from, char *to);

static const char *helpmsg =
	"\n\tUsage: hlink [option] from_dir to_dir\n"
	"\tCompares files in from_dir with files in to_dir\n"
	"\tTakes no action if they have the same inode number,\n"
	"\tbut if they differ it unlinks one and hard links them.\n"
	"\n\tOptions:\n"
	"\t-h outputs this help message.\n"
  ;

int main(int argc, char **argv)
{
	int opt, result, index;
	struct stat sb;
	char dirfrom[PATH_MAX], dirto[PATH_MAX];
	char *workfilefrom1, *workfileto1, *workfilefrom2, *workfileto2;
	char command[FILENAME_MAX];
	FILE *fpo;
	char **plfrom, **plto;
	struct fdata fdt;
	char *fromfrom, *fromto, *tofrom, *toto;

	while((opt = getopt(argc, argv, ":h")) != -1) {
		switch(opt){
		case 'h':
			help_print(0);
		break;
		case ':':
			fprintf(stderr, "Option %c requires an argument\n",optopt);
			help_print(1);
		break;
		case '?':
			fprintf(stderr, "Illegal option: %c\n",optopt);
			help_print(1);
		break;
		} //switch()
	}//while()
	// now process the non-option arguments

	// 1.Check that argv[???] exists.
	if (!(argv[optind])) {
		fprintf(stderr, "No dir from provided.\n");
		help_print(1);
	}
	strcpy(dirfrom, argv[optind]);
	// 2. Check that it's meaningful, ie dir exists.
	if (stat(dirfrom, &sb) == -1) {
		perror(dirfrom);
		exit(EXIT_FAILURE);
	}
	// Check that this is a dir
	if (!(S_ISDIR(sb.st_mode))) {
		fprintf(stderr, "Not a directory: %s\n", dirfrom);
		help_print(EXIT_FAILURE);
	}

	// Next non-option arguments if any
	optind++;

	// 3.Check that argv[???] exists.
	if (!(argv[optind])) {
		fprintf(stderr, "No dir to provided.\n");
		help_print(1);
	}
	strcpy(dirto, argv[optind]);
	// 4. Check that it's meaningful, ie dir exists.
	if (stat(dirto, &sb) == -1) {
		perror(dirto);
		exit(EXIT_FAILURE);
	}
	// Check that this is a dir
	if (!(S_ISDIR(sb.st_mode))) {
		fprintf(stderr, "Not a directory: %s\n", dirto);
		help_print(EXIT_FAILURE);
	}
	// strip trailing '/' off the dir names if they have them.
	if (dirfrom[strlen(dirfrom)-1] == '/') dirfrom[strlen(dirfrom)-1]
				= '\0';
	if (dirto[strlen(dirto)-1] == '/') dirto[strlen(dirto)-1] = '\0';

	// generate my workfile names
	sprintf(command, "/tmp/%shlinkfrom1", getenv("USER"));
	workfilefrom1 = dostrdup(command);
	sprintf(command, "/tmp/%shlinkfrom2", getenv("USER"));
	workfilefrom2 = dostrdup(command);
	sprintf(command, "/tmp/%shlinkto1", getenv("USER"));
	workfileto1 = dostrdup(command);
	sprintf(command, "/tmp/%shlinkto2", getenv("USER"));
	workfileto2 = dostrdup(command);

	fpo = dofopen(workfilefrom1, "w");
	recursedir(dirfrom, fpo, (char **)NULL);
	fclose(fpo);
	sprintf(command, "sort %s > %s",
					workfilefrom1, workfilefrom2);
	result = system(command);
	if(result == -1){
		perror(command);
		exit(EXIT_FAILURE);
	}

	fpo = dofopen(workfileto1, "w");
	recursedir(dirto, fpo, (char **)NULL);
	fclose(fpo);
	sprintf(command, "sort %s > %s",
					workfileto1, workfileto2);
	result = system(command);
	if(result == -1){
		perror(command);
		exit(EXIT_FAILURE);
	}

	// read in the sorted list of source files i.e., from.
	fdt = readfile(workfilefrom2, 1, 1);
	fromfrom = fdt.from;
	fromto = fdt.to;
	// turn the read in data into arrays of C strings
	plfrom = mem2pathlist(fromfrom, fromto);
	// free the memory block
	free(fromfrom);

	// read in the sorted list of target files i.e., to.
	fdt = readfile(workfileto2, 1, 1);
	tofrom = fdt.from;
	toto = fdt.to;
	// turn the read in data into arrays of C strings
	plto = mem2pathlist(tofrom, toto);
	// free the memory block
	// free(tofrom);	// malloc()/free() seems bugged, can't handle
						// identical sized allocations ????

	// traverse the two string arrays
	/* The big assumption here is that the lists have the same
	 * tree structure. This program is only really usable for
	 * two trees that have been rsync'd together.
	*/

	index = 0;
	while (plfrom[index]) {
		char pathfrom[PATH_MAX], pathto[PATH_MAX];
		char fnfrom[FILENAME_MAX], fnto[FILENAME_MAX];
		ino_t inofrom, inoto;

		strcpy(pathfrom, plfrom[index]);
		if (!(plto[index])) {
			fprintf(stderr, "To list is shorter than from list.\n"
			"Need to synchronise them again. %s\n", plfrom[index]);
			exit(EXIT_FAILURE);
		}

		strcpy(pathto, plto[index]);
		strcpy(fnfrom, basename(pathfrom));
		strcpy(fnto, basename(pathto));
		if (strcmp(fnfrom, fnto) != 0) {
			fprintf(stderr, "Filename mismatch: %s, %s\n", fnfrom,fnto);
			fputs("You need to synchronise the dirs again\n", stderr);
		}
		// Now the action
		if (stat(plfrom[index], &sb) == -1) {
			perror(plfrom[index]);
			inofrom = 0;
		}
		inofrom = sb.st_ino;
		if (stat(plto[index], &sb) == -1) {
			perror(plto[index]);
			inoto = 0;
		}
		inoto = sb.st_ino;
		fprintf(stdout, "%s %lu %lu\n", fnfrom, inofrom, inoto);
		if (inofrom != inoto) {
			if (unlink(fnto) == -1) {
				perror(fnto);
			} else { // unlink succeeded
				sync();	// maybe required
				if (link(fnfrom, fnto) == -1 ) {
					perror(fnto);
				}
			}
		} // if (unequal inodes)
		index++;
	}

	return 0;
}//main()

void help_print(int forced){
    fputs(helpmsg, stderr);
    exit(forced);
} // help_print()

char *dostrdup(const char *s)
{
	/*
	 * strdup() with in built error handling
	*/
	char *cp = strdup(s);
	if(!(cp)) {
		perror(s);
		exit(EXIT_FAILURE);
	}
	return cp;
} // dostrdup()

void recursedir(char *headdir, FILE *fpo, char **vlist)
{
	/* open the dir at headdir and process according to file type.
	*/
	DIR *dirp;
	struct dirent *de;

	dirp = opendir(headdir);
	if (!(dirp)) {
		perror(headdir);
		exit(EXIT_FAILURE);
	}
	while((de = readdir(dirp))) {
		int index, want;
		if (strcmp(de->d_name, "..") == 0) continue;
		if (strcmp(de->d_name, ".") == 0) continue;

		switch(de->d_type) {
			char newpath[FILENAME_MAX];
			struct stat sb;
			// Nothing to do for these.
			case DT_BLK:
			case DT_CHR:
			case DT_FIFO:
			case DT_SOCK:
			continue;
			break;
			case DT_LNK:
			strcpy(newpath, headdir);
			strcat(newpath, "/");
			strcat(newpath, de->d_name);
			if (stat(newpath, &sb) == -1) {
				perror(newpath);
				break;
			}
			//if (sb.st_size == 0) break;	// no interest in 0 length files
			//ftyp = "s";
			goto REG_LNK_common;
			case DT_REG:
			strcpy(newpath, headdir);
			strcat(newpath, "/");
			strcat(newpath, de->d_name);
			if (stat(newpath, &sb) == -1) {
				perror(newpath);
				break;
			}
			//if (sb.st_size == 0) break;	// no interest in 0 length files
			//ftyp = "f";
REG_LNK_common:
			// check our excludes
			want = 1;
			index = 0;
			if (vlist) {
				while (vlist[index]) {
					if (strstr(newpath, vlist[index])) {
						want = 0;
						break;
					}
					index++;
				}
			}
			if(want){
				// I don't need to mark the path ends with 'pathend'
				// because I'll never to need to extract it from
				// many fields.
				fprintf(fpo, "%s\n", newpath);
			}
			break;
			case DT_DIR:
			// recurse using this pathname.
			strcpy(newpath, headdir);
			strcat(newpath, "/");
			strcat(newpath, de->d_name);
			recursedir(newpath, fpo, vlist);
			break;
			// Just report the error but nothing else.
			case DT_UNKNOWN:
			fprintf(stderr, "Unknown type:\n%s/%s\n\n", headdir,
					de->d_name);
			break;
		} // switch()
	} // while
	closedir(dirp);
} // recursedir()

char **mem2pathlist(char *from, char *to)
{	/* Turn the block of memory into an array of C strings.
		This is way simpler than vlist() which must deal with comments
		in the source.
	*/
	char **plist;
	char *cp;
	int count = 0;
	int i;

	cp = from;
	while(cp < to) {
		if (*cp == '\n') {
			count++;
			*cp = '\0';
		}
		cp++;
	}
	count++;
	plist = malloc(count * sizeof(char *));
	memset(plist, 0, count * sizeof(char *));

	cp = from;
	i = 0;
	while (cp < to) {
		if (strlen(cp)) plist[i] = dostrdup(cp);
		i++;
		cp += strlen(cp) + 1;
	}
	plist[i] = (char *)NULL;
	return plist;
} // mem2pathlist()
