# $Id: Makefile.inc 1966 2007-09-20 03:54:36Z peter $

EXTRA_DIST += modules/preprocs/nasm/Makefile.inc
EXTRA_DIST += modules/preprocs/raw/Makefile.inc
EXTRA_DIST += modules/preprocs/cpp/Makefile.inc

include modules/preprocs/nasm/Makefile.inc
include modules/preprocs/raw/Makefile.inc
include modules/preprocs/cpp/Makefile.inc
