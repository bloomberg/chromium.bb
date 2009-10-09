# Only build man pages if configure found the HELP2MAN tool
if HAVE_HELP2MAN
#man_MANS = lou_allround.1 lou_checkhyphens.1 lou_checktable.1 lou_debug.1 lou_translate.1
man_MANS = lou_checkhyphens.1 lou_checktable.1 lou_translate.1
endif

EXTRA_DIST = $(man_MANS)
MAINTAINERCLEANFILES = $(man_MANS)

# The man pages depend on the --help strings and the version number.
common_mandeps = $(top_srcdir)/configure.ac

# Depend on the source, not the binary; we don't need to regenerate the
# binary when any source file changes, only the main one.
lou_allround.1: $(top_srcdir)/tools/lou_allround.c $(common_mandeps)
	$(HELP2MAN) ../tools/lou_allround --info-page=$(PACKAGE) --output=$@

lou_checkhyphens.1: $(top_srcdir)/tools/lou_checkhyphens.c $(common_mandeps)
	$(HELP2MAN) ../tools/lou_checkhyphens --info-page=$(PACKAGE) --output=$@

lou_checktable.1: $(top_srcdir)/tools/lou_checktable.c $(common_mandeps)
	$(HELP2MAN) ../tools/lou_checktable --info-page=$(PACKAGE) --output=$@

lou_debug.1: $(top_srcdir)/tools/lou_debug.c $(common_mandeps)
	$(HELP2MAN) ../tools/lou_debug --info-page=$(PACKAGE) --output=$@

lou_translate.1: $(top_srcdir)/tools/lou_translate.c $(common_mandeps)
	$(HELP2MAN) ../tools/lou_translate --info-page=$(PACKAGE) --output=$@
