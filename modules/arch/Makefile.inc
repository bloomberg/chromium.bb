# $Id: Makefile.inc 1914 2007-08-20 05:13:35Z peter $

EXTRA_DIST += modules/arch/x86/Makefile.inc
EXTRA_DIST += modules/arch/lc3b/Makefile.inc

include modules/arch/x86/Makefile.inc
include modules/arch/lc3b/Makefile.inc

dist_man_MANS += yasm_arch.7

if BUILD_MAN
yasm_arch.7: modules/arch/yasm_arch.xml
	$(XMLTO) -o $(top_builddir) man $(srcdir)/modules/arch/yasm_arch.xml
endif

EXTRA_DIST += modules/arch/yasm_arch.xml
