/*
 * $RCSId: xc/lib/fontconfig/fc-cache/fc-cache.c,v 1.8tsi Exp $
 *
 * Copyright © 2002 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#else
#ifdef linux
#define HAVE_GETOPT_LONG 1
#endif
#define HAVE_GETOPT 1
#endif

#ifndef HAVE_GETOPT
#define HAVE_GETOPT 0
#endif
#ifndef HAVE_GETOPT_LONG
#define HAVE_GETOPT_LONG 0
#endif

#if HAVE_GETOPT_LONG
#undef  _GNU_SOURCE
#define _GNU_SOURCE
#include <getopt.h>
const struct option longopts[] = {
    {"force", 0, 0, 'f'},
    {"system-only", 0, 0, 's'},
    {"version", 0, 0, 'V'},
    {"verbose", 0, 0, 'v'},
    {"help", 0, 0, '?'},
    {NULL,0,0,0},
};
#else
#if HAVE_GETOPT
extern char *optarg;
extern int optind, opterr, optopt;
#endif
#endif

static void
usage (char *program)
{
#if HAVE_GETOPT_LONG
    fprintf (stderr, "usage: %s [-fsvV?] [--force] [--system-only] [--verbose] [--version] [--help] [dirs]\n",
	     program);
#else
    fprintf (stderr, "usage: %s [-fsvV?] [dirs]\n",
	     program);
#endif
    fprintf (stderr, "Build font information caches in [dirs]\n"
	     "(all directories in font configuration by default).\n");
    fprintf (stderr, "\n");
#if HAVE_GETOPT_LONG
    fprintf (stderr, "  -f, --force          scan directories with apparently valid caches\n");
    fprintf (stderr, "  -s, --system-only    scan system-wide directories only\n");
    fprintf (stderr, "  -v, --verbose        display status information while busy\n");
    fprintf (stderr, "  -V, --version        display font config version and exit\n");
    fprintf (stderr, "  -?, --help           display this help and exit\n");
#else
    fprintf (stderr, "  -f         (force)   scan directories with apparently valid caches\n");
    fprintf (stderr, "  -s         (system)  scan system-wide directories only\n");
    fprintf (stderr, "  -v         (verbose) display status information while busy\n");
    fprintf (stderr, "  -V         (version) display font config version and exit\n");
    fprintf (stderr, "  -?         (help)    display this help and exit\n");
#endif
    exit (1);
}

static int
nsubdirs (FcStrSet *set)
{
    FcStrList	*list;
    int		n = 0;

    list = FcStrListCreate (set);
    if (!list)
	return 0;
    while (FcStrListNext (list))
	n++;
    FcStrListDone (list);
    return n;
}

static int
scanDirs (FcStrList *list, FcConfig *config, char *program, FcBool force, FcBool verbose)
{
    int		ret = 0;
    FcChar8	*dir;
    FcFontSet	*set;
    FcStrSet	*subdirs;
    FcStrList	*sublist;
    struct stat	statb;
    
    /*
     * Now scan all of the directories into separate databases
     * and write out the results
     */
    while ((dir = FcStrListNext (list)))
    {
	if (verbose)
	{
	    printf ("%s: \"%s\": ", program, dir);
	    fflush (stdout);
	}
	set = FcFontSetCreate ();
	if (!set)
	{
	    fprintf (stderr, "Can't create font set\n");
	    ret++;
	    continue;
	}
	subdirs = FcStrSetCreate ();
	if (!subdirs)
	{
	    fprintf (stderr, "Can't create directory set\n");
	    ret++;
	    FcFontSetDestroy (set);
	    continue;
	}
	
	if (access ((char *) dir, W_OK) < 0)
	{
	    switch (errno) {
	    case ENOENT:
	    case ENOTDIR:
		if (verbose)
		    printf ("skipping, no such directory\n");
		break;
	    case EACCES:
	    case EROFS:
		if (verbose)
		    printf ("skipping, no write access\n");
		break;
	    default:
		fprintf (stderr, "\"%s\": ", dir);
		perror ("");
		ret++;
	    }
	    FcFontSetDestroy (set);
	    FcStrSetDestroy (subdirs);
	    continue;
	}
	if (stat ((char *) dir, &statb) == -1)
	{
	    fprintf (stderr, "\"%s\": ", dir);
	    perror ("");
	    FcFontSetDestroy (set);
	    FcStrSetDestroy (subdirs);
	    ret++;
	    continue;
	}
	if (!S_ISDIR (statb.st_mode))
	{
	    fprintf (stderr, "\"%s\": not a directory, skipping\n", dir);
	    FcFontSetDestroy (set);
	    FcStrSetDestroy (subdirs);
	    continue;
	}
	if (!FcDirScan (set, subdirs, 0, FcConfigGetBlanks (config), dir, force))
	{
	    fprintf (stderr, "\"%s\": error scanning\n", dir);
	    FcFontSetDestroy (set);
	    FcStrSetDestroy (subdirs);
	    ret++;
	    continue;
	}
	if (!force && FcDirCacheValid (dir))
	{
	    if (verbose)
		printf ("skipping, %d fonts, %d dirs\n",
			set->nfont, nsubdirs(subdirs));
	}
	else
	{
	    if (verbose)
		printf ("caching, %d fonts, %d dirs\n", 
			set->nfont, nsubdirs (subdirs));

	    if (!FcDirSave (set, dir))
	    {
		fprintf (stderr, "Can't save cache in \"%s\"\n", dir);
		ret++;
	    }
	}
	FcFontSetDestroy (set);
	sublist = FcStrListCreate (subdirs);
	FcStrSetDestroy (subdirs);
	if (!sublist)
	{
	    fprintf (stderr, "Can't create subdir list in \"%s\"\n", dir);
	    ret++;
	    continue;
	}
	ret += scanDirs (sublist, config, program, force, verbose);
    }
    FcStrListDone (list);
    return ret;
}

int
main (int argc, char **argv)
{
    FcStrSet	*dirs;
    FcStrList	*list;
    FcBool    	verbose = FcFalse;
    FcBool	force = FcFalse;
    FcBool	systemOnly = FcFalse;
    FcConfig	*config;
    int		i;
    int		ret;
#if HAVE_GETOPT_LONG || HAVE_GETOPT
    int		c;

#if HAVE_GETOPT_LONG
    while ((c = getopt_long (argc, argv, "fsVv?", longopts, NULL)) != -1)
#else
    while ((c = getopt (argc, argv, "fsVv?")) != -1)
#endif
    {
	switch (c) {
	case 'f':
	    force = FcTrue;
	    break;
	case 's':
	    systemOnly = FcTrue;
	    break;
	case 'V':
	    fprintf (stderr, "fontconfig version %d.%d.%d\n", 
		     FC_MAJOR, FC_MINOR, FC_REVISION);
	    exit (0);
	case 'v':
	    verbose = FcTrue;
	    break;
	default:
	    usage (argv[0]);
	}
    }
    i = optind;
#else
    i = 1;
#endif

    if (systemOnly)
	FcConfigEnableHome (FcFalse);
    /* need to use FcInitLoadConfig when we use dirs */
    config = FcInitLoadConfig ();
    if (!config)
    {
	fprintf (stderr, "%s: Can't init font config library\n", argv[0]);
	return 1;
    }

    if (argv[i])
    {
	dirs = FcStrSetCreate ();
	if (!dirs)
	{
	    fprintf (stderr, "%s: Can't create list of directories\n",
		     argv[0]);
	    return 1;
	}
	while (argv[i])
	{
	    if (!FcStrSetAdd (dirs, (FcChar8 *) argv[i]))
	    {
		fprintf (stderr, "%s: Can't add directory\n", argv[0]);
		return 1;
	    }
	    i++;
	}
	list = FcStrListCreate (dirs);
	FcStrSetDestroy (dirs);
    }
    else
	list = FcConfigGetConfigDirs (config);
    ret = scanDirs (list, config, argv[0], force, verbose);
    /* 
     * Now we need to sleep a second  (or two, to be extra sure), to make
     * sure that timestamps for changes after this run of fc-cache are later
     * then any timestamps we wrote.  We don't use gettimeofday() because
     * sleep(3) can't be interrupted by a signal here -- this isn't in the
     * library, and there aren't any signals flying around here.
     */
    FcConfigDestroy (config);
    sleep (2);
    if (verbose)
	printf ("%s: %s\n", argv[0], ret ? "failed" : "succeeded");
    return ret;
}
