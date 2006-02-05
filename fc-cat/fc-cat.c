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
#include <../src/fccache.c>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
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
    {"version", 0, 0, 'V'},
    {"help", 0, 0, '?'},
    {NULL,0,0,0},
};
#else
#if HAVE_GETOPT
extern char *optarg;
extern int optind, opterr, optopt;
#endif
#endif

/*
 * POSIX has broken stdio so that getc must do thread-safe locking,
 * this is a serious performance problem for applications doing large
 * amounts of IO with getc (as is done here).  If available, use
 * the getc_unlocked varient instead.
 */
 
#if defined(getc_unlocked) || defined(_IO_getc_unlocked)
#define GETC(f) getc_unlocked(f)
#define PUTC(c,f) putc_unlocked(c,f)
#else
#define GETC(f) getc(f)
#define PUTC(c,f) putc(c,f)
#endif

FcBool
FcCachePrintSet (FcFontSet *set, FcStrSet *dirs, char *base_name);

static FcBool
FcCacheWriteChars (FILE *f, const FcChar8 *chars)
{
    FcChar8    c;
    while ((c = *chars++))
    {
	switch (c) {
	case '"':
	case '\\':
	    if (PUTC ('\\', f) == EOF)
		return FcFalse;
	    /* fall through */
	default:
	    if (PUTC (c, f) == EOF)
		return FcFalse;
	}
    }
    return FcTrue;
}

static FcBool
FcCacheWriteUlong (FILE *f, unsigned long t)
{
    int	    pow;
    unsigned long   temp, digit;

    temp = t;
    pow = 1;
    while (temp >= 10)
    {
	temp /= 10;
	pow *= 10;
    }
    temp = t;
    while (pow)
    {
	digit = temp / pow;
	if (PUTC ((char) digit + '0', f) == EOF)
	    return FcFalse;
	temp = temp - pow * digit;
	pow = pow / 10;
    }
    return FcTrue;
}

static FcBool
FcCacheWriteInt (FILE *f, int i)
{
    return FcCacheWriteUlong (f, (unsigned long) i);
}

static FcBool
FcCacheWriteStringOld (FILE *f, const FcChar8 *string)
{

    if (PUTC ('"', f) == EOF)
	return FcFalse;
    if (!FcCacheWriteChars (f, string))
	return FcFalse;
    if (PUTC ('"', f) == EOF)
	return FcFalse;
    return FcTrue;
}

static void
usage (char *program)
{
#if HAVE_GETOPT_LONG
    fprintf (stderr, "usage: %s [-V?] [--version] [--help] <fonts.cache-2>\n",
	     program);
#else
    fprintf (stderr, "usage: %s [-fsvV?] <fonts.cache-2>\n",
	     program);
#endif
    fprintf (stderr, "Reads font information caches in <fonts.cache-2>\n");
    fprintf (stderr, "\n");
#if HAVE_GETOPT_LONG
    fprintf (stderr, "  -V, --version        display font config version and exit\n");
    fprintf (stderr, "  -?, --help           display this help and exit\n");
#else
    fprintf (stderr, "  -V         (version) display font config version and exit\n");
    fprintf (stderr, "  -?         (help)    display this help and exit\n");
#endif
    exit (1);
}

static FcBool 
FcCacheGlobalFileReadAndPrint (FcFontSet * set, FcStrSet *dirs, char *cache_file)
{
    char		name_buf[8192];
    int fd;
    char * current_arch_machine_name;
    char candidate_arch_machine_name[9+MACHINE_SIGNATURE_SIZE];
    char 		subdirName[FC_MAX_FILE_LEN + 1 + 12 + 1];
    off_t current_arch_start = 0;

    if (!cache_file)
	goto bail;

    current_arch_machine_name = FcCacheMachineSignature();
    fd = open(cache_file, O_RDONLY);
    if (fd == -1)
	goto bail;

    current_arch_start = FcCacheSkipToArch(fd, current_arch_machine_name);
    if (current_arch_start < 0)
	goto bail1;

    lseek (fd, current_arch_start, SEEK_SET);
    if (FcCacheReadString (fd, candidate_arch_machine_name, 
			   sizeof (candidate_arch_machine_name)) == 0)
	goto bail1;

    while (1) 
    {
	char * dir;
	FcCacheReadString (fd, name_buf, sizeof (name_buf));
	if (!strlen(name_buf))
	    break;
	printf ("fc-cat: printing global cache contents for dir %s\n", 
		name_buf);

	do
	{
	    if (!FcCacheReadString (fd, subdirName, 
				    sizeof (subdirName)) ||
		!strlen (subdirName))
		break;
	    /* then don't do anything with subdirName. */
	} while (1);

	if (!FcDirCacheConsume (fd, name_buf, set, 0))
	    goto bail1;

	dir = malloc (strlen (name_buf) + 2);
	if (!dir)
	    goto bail1;

	strcpy (dir, name_buf);
	strcat (dir, "/");

	FcCachePrintSet (set, dirs, dir);
	free (dir);

	FcFontSetDestroy (set);
	set = FcFontSetCreate();
    }

 bail1:
    close (fd);
 bail:
    return FcFalse;
}

/* read serialized state from the cache file */
static char *
FcCacheFileRead (FcFontSet * set, FcStrSet *dirs, char *cache_file)
{
    int fd;
    char * current_arch_machine_name;
    off_t current_arch_start = 0;
    char subdirName[FC_MAX_FILE_LEN + 1 + 12 + 1];
    static char name_buf[8192], *dir;
    FcChar8 * ls;

    if (!cache_file)
        goto bail;

    current_arch_machine_name = FcCacheMachineSignature();
    fd = open(cache_file, O_RDONLY);
    if (fd == -1)
        goto bail;

    FcCacheReadString (fd, name_buf, sizeof (name_buf));
    if (!strlen (name_buf))
	goto bail;
    if (strcmp (name_buf, FC_GLOBAL_MAGIC_COOKIE) == 0)
	goto bail;
    printf ("fc-cat: printing directory cache for cache which would be named %s\n", 
	    name_buf);

    current_arch_start = FcCacheSkipToArch(fd, current_arch_machine_name);
    if (current_arch_start < 0)
        goto bail1;

    while (strlen(FcCacheReadString (fd, subdirName, sizeof (subdirName))) > 0)
        FcStrSetAdd (dirs, (FcChar8 *)subdirName);

    dir = strdup(name_buf);
    ls = FcStrLastSlash ((FcChar8 *)dir);
    if (ls)
	*ls = 0;

    if (!FcDirCacheConsume (fd, dir, set, 0))
	goto bail2;
    free (dir);

    close(fd);
    return name_buf;

 bail2:
    free (dir);

 bail1:
    close (fd);
 bail:
    return 0;
}

/*
 * return the path from the directory containing 'cache' to 'file'
 */

static const FcChar8 *
FcFileBaseName (const char *cache, const FcChar8 *file)
{
    const FcChar8   *cache_slash;

    cache_slash = FcStrLastSlash ((const FcChar8 *)cache);
    if (cache_slash && !strncmp ((const char *) cache, (const char *) file,
				 (cache_slash + 1) - (const FcChar8 *)cache))
	return file + ((cache_slash + 1) - (const FcChar8 *)cache);
    return file;
}

FcBool
FcCachePrintSet (FcFontSet *set, FcStrSet *dirs, char *base_name)
{
    FcPattern	    *font;
    FcChar8	    *name, *dir;
    const FcChar8   *file, *base;
    int		    ret;
    int		    n;
    int		    id;
    FcStrList	    *list;

    list = FcStrListCreate (dirs);
    if (!list)
	goto bail2;
    
    while ((dir = FcStrListNext (list)))
    {
	base = FcFileBaseName (base_name, dir);
	if (!FcCacheWriteStringOld (stdout, base))
	    goto bail3;
	if (PUTC (' ', stdout) == EOF)
	    goto bail3;
	if (!FcCacheWriteInt (stdout, 0))
	    goto bail3;
        if (PUTC (' ', stdout) == EOF)
	    goto bail3;
	if (!FcCacheWriteStringOld (stdout, FC_FONT_FILE_DIR))
	    goto bail3;
	if (PUTC ('\n', stdout) == EOF)
	    goto bail3;
    }
    
    for (n = 0; n < set->nfont; n++)
    {
	font = set->fonts[n];
	if (FcPatternGetString (font, FC_FILE, 0, (FcChar8 **) &file) != FcResultMatch)
	    goto bail3;
	base = FcFileBaseName (base_name, file);
	if (FcPatternGetInteger (font, FC_INDEX, 0, &id) != FcResultMatch)
	    goto bail3;
	if (FcDebug () & FC_DBG_CACHEV)
	    printf (" write file \"%s\"\n", base);
	if (!FcCacheWriteStringOld (stdout, base))
	    goto bail3;
	if (PUTC (' ', stdout) == EOF)
	    goto bail3;
	if (!FcCacheWriteInt (stdout, id))
	    goto bail3;
        if (PUTC (' ', stdout) == EOF)
	    goto bail3;
	name = FcNameUnparse (font);
	if (!name)
	    goto bail3;
	ret = FcCacheWriteStringOld (stdout, name);
	FcStrFree (name);
	if (!ret)
	    goto bail3;
	if (PUTC ('\n', stdout) == EOF)
	    goto bail3;
    }
    
    FcStrListDone (list);

    return FcTrue;
    
bail3:
    FcStrListDone (list);
bail2:
    return FcFalse;
}

int
main (int argc, char **argv)
{
    int		i;
#if HAVE_GETOPT_LONG || HAVE_GETOPT
    int		c;
    FcFontSet	*fs = FcFontSetCreate();
    FcStrSet    *dirs = FcStrSetCreate();
    char	*name_buf;

#if HAVE_GETOPT_LONG
    while ((c = getopt_long (argc, argv, "fsVv?", longopts, NULL)) != -1)
#else
    while ((c = getopt (argc, argv, "fsVv?")) != -1)
#endif
    {
	switch (c) {
	case 'V':
	    fprintf (stderr, "fontconfig version %d.%d.%d\n", 
		     FC_MAJOR, FC_MINOR, FC_REVISION);
	    exit (0);
	default:
	    usage (argv[0]);
	}
    }
    i = optind;
#else
    i = 1;
#endif

    if (i >= argc)
        usage (argv[0]);

    if ((name_buf = FcCacheFileRead (fs, dirs, argv[i])) != 0)
	FcCachePrintSet (fs, dirs, name_buf);
    else
    {
        FcStrSetDestroy (dirs);
        dirs = FcStrSetCreate ();
        if (FcCacheGlobalFileReadAndPrint (fs, dirs, argv[i]))
            ;
    }

    FcStrSetDestroy (dirs);
    FcFontSetDestroy (fs);

    return 0;
}
