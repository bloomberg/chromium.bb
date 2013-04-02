publican_sources = \
	$(srcdir)/en_US/Wayland.ent \
	$(srcdir)/en_US/Wayland.xml \
	$(srcdir)/en_US/Book_Info.xml \
	$(srcdir)/en_US/Author_Group.xml \
	$(srcdir)/en_US/Foreword.xml \
	$(srcdir)/en_US/Preface.xml \
	$(srcdir)/en_US/Revision_History.xml \
	$(srcdir)/en_US/Introduction.xml \
	$(srcdir)/en_US/Architecture.xml \
	$(srcdir)/en_US/Protocol.xml \
	$(srcdir)/en_US/Library.xml \
	$(srcdir)/en_US/Compositors.xml \
	$(srcdir)/en_US/images/icon.svg  \
	$(srcdir)/en_US/images/wayland-architecture.png \
	$(srcdir)/en_US/images/wayland.png  \
	$(srcdir)/en_US/images/x-architecture.png

if HAVE_PUBLICAN
if HAVE_XSLTPROC
noinst_DATA = Wayland $(publican_targets)
pubdir = $(docdir)/Wayland/en-US

publican_targets = $(publican_sources:$(srcdir)/en_US%=$(builddir)/en-US%) \
	en-US/ProtocolSpec.xml en-US/ProtocolInterfaces.xml \
	en-US/WaylandClientAPI.xml

# The Protocol.xml is purely generated and required before running publican
en-US/ProtocolSpec.xml: $(top_srcdir)/protocol/wayland.xml $(srcdir)/protocol-to-docbook.xsl
	$(AM_V_GEN)$(MKDIR_P) en-US/images
	$(AM_V_GEN)$(XSLTPROC) $(srcdir)/protocol-to-docbook.xsl \
		$(top_srcdir)/protocol/wayland.xml > en-US/ProtocolSpec.xml

en-US/ProtocolInterfaces.xml: $(top_srcdir)/protocol/wayland.xml $(srcdir)/protocol-interfaces-to-docbook.xsl
	$(AM_V_GEN)$(MKDIR_P) en-US/images
	$(AM_V_GEN)$(XSLTPROC) $(srcdir)/protocol-interfaces-to-docbook.xsl \
		$(top_srcdir)/protocol/wayland.xml > en-US/ProtocolInterfaces.xml

# WaylandClientAPI.xml is generated after combining all xml in one single file
# with the exception of wayland-client_8h.xml that is omitted to not duplicate
# the output methods.
en-US/WaylandClientAPI.xml: $(top_builddir)/doc/doxygen/xml/index.xml $(srcdir)/doxygen-to-publican.xsl
	$(AM_V_GEN)$(MKDIR_P) en-US/images
	$(AM_V_GEN)mv $(top_builddir)/doc/doxygen/xml/wayland-client_8h.xml \
		$(top_builddir)/doc/doxygen/
	$(AM_V_GEN)$(XSLTPROC) $(top_builddir)/doc/doxygen/xml/combine.xslt \
		$(top_builddir)/doc/doxygen/xml/index.xml > \
		$(top_builddir)/doc/doxygen/xml/clientAPI.xml
	$(AM_V_GEN)mv $(top_builddir)/doc/doxygen/wayland-client_8h.xml \
		$(top_builddir)/doc/doxygen/xml
	$(AM_V_GEN)$(XSLTPROC) $(srcdir)/doxygen-to-publican.xsl \
		$(top_builddir)/doc/doxygen/xml/clientAPI.xml > en-US/WaylandClientAPI.xml

# Copy the en_US source files into en-US destination
# This is required for out-of-source-tree build as publican does not allow us
# to specify the location of the source code.
$(builddir)/en-US/%: $(srcdir)/en_US/% en-US/ProtocolSpec.xml en-US/ProtocolInterfaces.xml en-US/WaylandClientAPI.xml $(publican_sources)
	$(AM_V_GEN)cp -f $< $@
	$(AM_V_GEN)chmod a+w $@

# Run publican for the builddir on the generated (or copied) source
# The output formats are generated in the Wayland sub directory. Also, we need
# to use a tmp publican.cfg cause 'publican rename' modifies the original.
Wayland: $(publican_targets)
	$(AM_V_GEN)cp -f $(srcdir)/publican.cfg $(builddir)/publican-copy.cfg
	$(AM_V_GEN)$(PUBLICAN) rename --name Wayland \
		--version "$(WAYLAND_VERSION_MAJOR).$(WAYLAND_VERSION_MINOR)" \
		--config $(builddir)/publican-copy.cfg
	$(AM_V_GEN)$(PUBLICAN) build --quiet --langs en-US --formats html,pdf \
		--config $(builddir)/publican-copy.cfg
	@touch Wayland

CLEANFILES = en-US/ProtocolSpec.xml en-US/ProtocolInterfaces.xml en-US/WaylandClientAPI.xml $(publican_targets)

clean-local:
	$(AM_V_at)rm -fr $(builddir)/en-US
	$(AM_V_at)rm -fr $(builddir)/Wayland
	$(AM_V_at)rm -fr $(builddir)/publican-copy.cfg

install-data-local:
	test -z "$(pubdir)/html/Common_Content/css" || $(mkdir_p) "$(DESTDIR)$(pubdir)/html/Common_Content/css"
	test -z "$(pubdir)/html/Common_Content/images" || $(mkdir_p) "$(DESTDIR)$(pubdir)/html/Common_Content/images"
	test -z "$(pubdir)/html/images" || $(mkdir_p) "$(DESTDIR)$(pubdir)/html/images"
	test -z "$(pubdir)/pdf" || $(mkdir_p) "$(DESTDIR)$(pubdir)/pdf"
	test -z "$(pubdir)/xml/Common_Content/css" || $(mkdir_p) "$(DESTDIR)$(pubdir)/xml/Common_Content/css"
	test -z "$(pubdir)/xml/Common_Content/images" || $(mkdir_p) "$(DESTDIR)$(pubdir)/xml/Common_Content/images"
	test -z "$(pubdir)/xml/images" || $(mkdir_p) "$(DESTDIR)$(pubdir)/xml/images"
	list=`find $(builddir)/Wayland/en-US -type f -not -path '$(builddir)/Wayland/en-US/xml_tmp*'`; \
	for p in $$list; do \
	  echo " $(INSTALL_DATA) '$$p' '$(DESTDIR)$(docdir)/$$p'"; \
	  $(INSTALL_DATA) "$$p" "$(DESTDIR)$(docdir)/$$p"; \
	done;

uninstall-local:
	@if test -n $(DESTDIR)$(docdir); then \
	  if test -d $(DESTDIR)$(docdir); then \
	    echo " rm -fr $(DESTDIR)$(docdir)/Wayland;"; \
	    rm -fr $(DESTDIR)$(docdir)/Wayland; \
	  fi; \
	fi;

endif
endif

EXTRA_DIST = $(publican_sources) publican.cfg protocol-to-docbook.xsl protocol-interfaces-to-docbook.xsl doxygen-to-publican.xsl
