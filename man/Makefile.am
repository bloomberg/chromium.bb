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
	$(patsubst %.1,%.xml,$(patsubst %.3,%.xml,$(patsubst %.5,%.xml,$(patsubst %.7,%.xml,$(MANPAGES)))))

EXTRA_DIST = $(XML_FILES)
CLEANFILES = $(MANPAGES) $(MANPAGES_ALIASES) .man_fixup
man_MANS =

if BUILD_MANPAGES
if HAVE_MANPAGES_STYLESHEET

man_MANS += $(MANPAGES) $(MANPAGES_ALIASES)

XSLTPROC_FLAGS = \
	--stringparam man.authors.section.enabled 0 \
	--stringparam man.copyright.section.enabled 0 \
	--stringparam funcsynopsis.style ansi \
	--stringparam man.output.quietly 1 \
	--nonet

XSLTPROC_PROCESS_MAN = \
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
	$(XSLTPROC) -o "$@" $(XSLTPROC_FLAGS) $(MANPAGES_STYLESHEET) "$<" && \
	touch .man_fixup

# Force .man_fixup if $(MANPAGES) are not built
.man_fixup: | $(MANPAGES)
	@touch .man_fixup

$(MANPAGES_ALIASES): $(MANPAGES) .man_fixup
	$(AM_V_GEN)if test -n "$@" ; then $(SED) -i -e 's/^\.so \([a-z_]\+\)\.\([0-9]\)$$/\.so man\2\/\1\.\2/' "$@" ; fi

%.1: $(top_srcdir)/man/%.xml
	$(XSLTPROC_PROCESS_MAN)

%.3: $(top_srcdir)/man/%.xml
	$(XSLTPROC_PROCESS_MAN)

%.5: $(top_srcdir)/man/%.xml
	$(XSLTPROC_PROCESS_MAN)

%.7: $(top_srcdir)/man/%.xml
	$(XSLTPROC_PROCESS_MAN)

endif # HAVE_MANPAGES_STYLESHEET
endif # BUILD_MANPAGES
