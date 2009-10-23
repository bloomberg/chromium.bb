# $Id: Makefile.inc 2130 2008-10-07 05:38:11Z peter $

EXTRA_DIST += frontends/yasm/Makefile.inc
EXTRA_DIST += frontends/tasm/Makefile.inc

include frontends/yasm/Makefile.inc
include frontends/tasm/Makefile.inc
