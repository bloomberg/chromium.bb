/*-
 * Copyright 2003-2005 Colin Percival
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions 
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if 0
__FBSDID("$FreeBSD: src/usr.bin/bsdiff/bspatch/bspatch.c,v 1.1 2005/08/06 01:59:06 cperciva Exp $");
#endif

#include <sys/types.h>

#include <bzlib.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <unistd.h>
#include <zlib.h>

#if defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define le64toh(x) OSSwapLittleToHostInt64(x)
#elif defined(__linux__)
#include <endian.h>
#elif defined(_WIN32) && (defined(_M_IX86) || defined(_M_X64))
#define le64toh(x) (x)
#else
#error Provide le64toh for this platform
#endif

static inline off_t offtin(u_char *buf)
{
	return le64toh(*((off_t*)buf));
}

static void sha1tostr(const u_char *sha1, char *sha1str)
{
	int i;
	for (i = 0; i < SHA_DIGEST_LENGTH; ++i)
		sprintf(&sha1str[i * 2], "%02x", sha1[i]);
}

/* cfile is a uniform interface to read from maybe-compressed files. */

typedef struct {
	FILE *f;              /* method = 1, 2 */
	union {
		BZFILE *bz2;  /* method = 2 */
		gzFile gz;    /* method = 3 */
	} u;
	const char *tag;
	unsigned char method;
} cfile;

/* Opens a file at path, seeks to offset off, and prepares for reading using
 * the specified method. Supported methods are plain uncompressed (1), bzip2
 * (2), and gzip (3). tag is used as an identifier for error reporting. */
static void cfopen(cfile *cf, const char *path, off_t off,
                   const char *tag, unsigned char method)
{
	int fd;
	int zerr;

	if (method == 1 || method == 2) {
		/* Use stdio for uncompressed files. The bzip interface also
		 * sits on top of a stdio FILE* but does not take "ownership"
		 * of the FILE*. */
		if ((cf->f = fopen(path, "rb")) == NULL)
			err(1, "fdopen(%s)", tag);
		if ((fseeko(cf->f, off, SEEK_SET)) != 0)
			err(1, "fseeko(%s, %lld)", tag, off);
		if (method == 2) {
			if ((cf->u.bz2 = BZ2_bzReadOpen(&zerr, cf->f, 0, 0,
			                                NULL, 0)) == NULL)
				errx(1, "BZ2_bzReadOpen(%s): %d", tag, zerr);
		}
	} else if (method == 3) {
		if ((fd = open(path, O_RDONLY)) < 0)
			err(1, "open(%s)", tag);
		if (lseek(fd, off, SEEK_SET) != off)
			err(1, "lseek(%s, %lld)", tag, off);
		if ((cf->u.gz = gzdopen(fd, "rb")) == NULL)
			errx(1, "gzdopen(%s)", tag);
	} else {
		errx(1, "cfopen(%s): unknown method %d", tag, method);
	}

	cf->tag = tag;
	cf->method = method;
}

static void cfclose(cfile *cf)
{
	int zerr;

	if (cf->method == 1 || cf->method == 2) {
		if (cf->method == 2) {
			zerr = BZ_OK;
			BZ2_bzReadClose(&zerr, cf->u.bz2);
			if (zerr != BZ_OK)
				errx(1, "BZ2_bzReadClose(%s): %d\n",
				     cf->tag, zerr);
		}
		if (fclose(cf->f) != 0)
			err(1, "fclose(%s)", cf->tag);
	} else if (cf->method == 3) {
		if ((zerr = gzclose(cf->u.gz)) != Z_OK)
			errx(1, "gzclose(%s): %d", cf->tag, zerr);
	} else {
		errx(1, "cfclose(%s): unknown method %d", cf->tag, cf->method);
	}
}

static void cfread(cfile *cf, u_char *buf, size_t len)
{
	size_t nread;
	int zerr;

	if (cf->method == 1) {
		if ((nread = fread(buf, 1, len, cf->f)) != len) {
			if (!ferror(cf->f))
				errx(1, "fread(%s, %zd): short read %zd",
				     cf->tag, len, nread);
			err(1, "fread(%s, %zd)", cf->tag, len);
		}
	} else if (cf->method == 2) {
		zerr = BZ_OK;
		if ((nread = BZ2_bzRead(&zerr, cf->u.bz2, buf, len)) != len) {
			if (zerr == BZ_OK)
				errx(1, "BZ2_bzRead(%s, %zd): short read %zd",
				     cf->tag, len, nread);
			errx(1, "BZ2_bzRead(%s, %zd): %d", cf->tag, len, zerr);
		}
	} else if (cf->method == 3) {
		if ((nread = gzread(cf->u.gz, buf, len)) != len) {
			zerr = Z_OK;
			gzerror(cf->u.gz, &zerr);
			if (zerr == Z_OK)
				errx(1, "gzread(%s, %zd): short read %zd",
				     cf->tag, len, nread);
			errx(1, "gzread(%s, %zd): %d", cf->tag, len, zerr);
		}
	} else {
		errx(1, "cfread(%s, %zd): unknown method %d",
		     cf->tag, len, cf->method);
	}
}

int main(int argc,char * argv[])
{
	FILE * f;
	cfile cf, df, ef;
	int fd;
	off_t expect_oldsize, oldsize, newsize, patchsize;
	off_t zctrllen, zdatalen, zextralen;
	u_char header[96], buf[8];
	u_char *old, *new;
	off_t oldpos,newpos;
	off_t ctrl[3];
	off_t i;
	u_char sha1[SHA_DIGEST_LENGTH];
	char sha1str[SHA_DIGEST_LENGTH * 2 + 1];
	char expected_sha1str[SHA_DIGEST_LENGTH * 2 + 1];

	if(argc!=4) errx(1,"usage: %s oldfile newfile patchfile",argv[0]);

	/* Open patch file */
	if ((f = fopen(argv[3], "rb")) == NULL)
		err(1, "fopen(%s)", argv[3]);

	/*
	File format:
		0	8	"BSDIFF4G"
		8	8	length of compressed control block (x)
		16	8	length of compressed diff block (y)
		24	8	length of compressed extra block (z)
		32	8	length of old file
		40	8	length of new file
		48	20	SHA1 of old file
		68	20	SHA1 of new file
		88	1	encoding of control block
		89	1	encoding of diff block
		90	1	encoding of extra block
		91	5	unused
		96	x	compressed control block
		96+x	y	compressed diff block
		96+x+y	z	compressed extra block
	Encodings are 1 (uncompressed), 2 (bzip2), and 3 (gzip).
	The control block is a set of triples (x,y,z) meaning "add x bytes
	from oldfile to x bytes from the diff block; copy y bytes from the
	extra block; seek forwards in oldfile by z bytes".
	*/

	/* Read header */
	if (fread(header, 1, sizeof(header), f) < sizeof(header)) {
		if (feof(f))
			errx(1, "corrupt patch (header size)");
		err(1, "fread(%s)", argv[3]);
	}

	/* Check for appropriate magic */
	if (memcmp(header, "BSDIFF4G", 8) != 0)
		errx(1, "corrupt patch (magic)");

	/* Read lengths from header */
	zctrllen = offtin(header + 8);
	zdatalen = offtin(header + 16);
	zextralen = offtin(header + 24);
	expect_oldsize = offtin(header + 32);
	newsize = offtin(header + 40);
	if (zctrllen < 0 || zdatalen < 0 || zextralen < 0)
		errx(1, "corrupt patch (stream sizes)");
	if (expect_oldsize < 0 || newsize < 0)
		errx(1, "corrupt patch (file sizes)");

	if (fseeko(f, 0, SEEK_END) != 0 || (patchsize = ftello(f)) < 0)
		err(1, "fseeko/ftello(%s)", argv[3]);
	if (patchsize != sizeof(header) + zctrllen + zdatalen + zextralen)
		errx(1, "corrupt patch (patch size)");

	cfopen(&cf, argv[3], sizeof(header), "control", header[88]);
	cfopen(&df, argv[3], sizeof(header) + zctrllen, "diff", header[89]);
	cfopen(&ef, argv[3], sizeof(header) + zctrllen + zdatalen, "extra",
	       header[90]);

	if (fclose(f))
		err(1, "fclose(%s)", argv[3]);

	if(((fd=open(argv[1],O_RDONLY,0))<0) ||
		((oldsize=lseek(fd,0,SEEK_END))==-1) ||
		((old=malloc(oldsize+1))==NULL) ||
		(lseek(fd,0,SEEK_SET)!=0) ||
		(read(fd,old,oldsize)!=oldsize) ||
		(close(fd)==-1)) err(1,"%s",argv[1]);
	if (expect_oldsize != oldsize)
		errx(1, "old size mismatch: %lld != %lld",
		     oldsize, expect_oldsize);
	SHA1(old, oldsize, sha1);
	if (memcmp(sha1, header + 48, sizeof(sha1)) != 0) {
		sha1tostr(sha1, sha1str);
		sha1tostr(header + 48, expected_sha1str);
		errx(1, "old hash mismatch: %s != %s",
	             sha1str, expected_sha1str);
	}
	if((new=malloc(newsize+1))==NULL) err(1,NULL);

	oldpos=0;newpos=0;
	while(newpos<newsize) {
		/* Read control data */
		for(i=0;i<=2;i++) {
			cfread(&cf, buf, 8);
			ctrl[i]=offtin(buf);
		};

		/* Sanity-check */
		if(newpos+ctrl[0]>newsize)
			errx(1,"corrupt patch (diff): overrun");

		/* Read diff string */
		cfread(&df, new + newpos, ctrl[0]);

		/* Add old data to diff string */
		for(i=0;i<ctrl[0];i++)
			if((oldpos+i>=0) && (oldpos+i<oldsize))
				new[newpos+i]+=old[oldpos+i];

		/* Adjust pointers */
		newpos+=ctrl[0];
		oldpos+=ctrl[0];

		/* Sanity-check */
		if(newpos+ctrl[1]>newsize)
			errx(1,"corrupt patch (extra): overrun");

		/* Read extra string */
		cfread(&ef, new + newpos, ctrl[1]);

		/* Adjust pointers */
		newpos+=ctrl[1];
		oldpos+=ctrl[2];
	};

	/* Clean up the readers */
	cfclose(&cf);
	cfclose(&df);
	cfclose(&ef);

	SHA1(new, newsize, sha1);
	if (memcmp(sha1, header + 68, sizeof(sha1)) != 0) {
		sha1tostr(sha1, sha1str);
		sha1tostr(header + 68, expected_sha1str);
		errx(1, "new hash mismatch: %s != %s",
		     sha1str, expected_sha1str);
	}

	/* Write the new file */
	if(((fd=open(argv[2],O_CREAT|O_TRUNC|O_WRONLY,0644))<0) ||
		(write(fd,new,newsize)!=newsize) || (close(fd)==-1))
		err(1,"open/write/close(%s)",argv[2]);

	free(new);
	free(old);

	return 0;
}
