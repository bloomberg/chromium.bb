# $Id: Makefile.inc 2130 2008-10-07 05:38:11Z peter $

EXTRA_DIST += modules/parsers/gas/Makefile.inc
EXTRA_DIST += modules/parsers/nasm/Makefile.inc

include modules/parsers/gas/Makefile.inc
include modules/parsers/nasm/Makefile.inc
include modules/parsers/tasm/Makefile.inc

dist_man_MANS += yasm_parsers.7

if BUILD_MAN
yasm_parsers.7: modules/parsers/yasm_parsers.xml
	$(XMLTO) -o $(top_builddir) man $(srcdir)/modules/parsers/yasm_parsers.xml
endif

EXTRA_DIST += modules/parsers/yasm_parsers.xml
