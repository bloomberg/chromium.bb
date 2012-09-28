#
# This generates man-pages out of the Docbook XML files. Simply add your files
# to the $MANPAGES array. If aliases are created, please add them to the
# MANPAGES_ALIASES array so they get installed correctly.
#

MANPAGES = \
	drm.7 \
	drm-kms.7 \
	drm-memory.7 \
	drmAvailable.3 \
	drmHandleEvent.3 \
	drmModeGetResources.3
MANPAGES_ALIASES = \
	drm-mm.7 \
	drm-gem.7 \
	drm-ttm.7

XML_FILES = \
	${patsubst %.1,%.xml,${patsubst %.3,%.xml,${patsubst %.5,%.xml,${patsubs %.7,%.xml,$(MANPAGES)}}}}
CLEANFILES =
EXTRA_DIST =
man_MANS =

if HAVE_XSLTPROC

CLEANFILES += $(MANPAGES) $(MANPAGES_ALIASES)
EXTRA_DIST += $(MANPAGES) $(MANPAGES_ALIASES) $(XML_FILES)
man_MANS += $(MANPAGES) $(MANPAGES_ALIASES)

XSLTPROC_FLAGS = \
	--stringparam man.authors.section.enabled 0 \
	--stringparam man.copyright.section.enabled 0 \
	--stringparam funcsynopsis.style ansi \
	--stringparam man.output.quietly 1

XSLTPROC_PROCESS_MAN = \
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
	$(XSLTPROC) -o $@ $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $< && \
	$(SED) -i -e 's/^\.so \(.*\)\.\(.\)$$/\.so man\2\/\1\.\2/' $(MANPAGES_ALIASES)

%.1: %.xml
	$(XSLTPROC_PROCESS_MAN)

%.3: %.xml
	$(XSLTPROC_PROCESS_MAN)

%.5: %.xml
	$(XSLTPROC_PROCESS_MAN)

%.7: %.xml
	$(XSLTPROC_PROCESS_MAN)

endif # HAVE_XSLTPROC
