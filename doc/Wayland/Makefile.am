if HAVE_PUBLICAN
noinst_DATA = Wayland $(publican_targets)
pubdir = $(docdir)/Wayland/en-US

publican_sources = \
	$(srcdir)/en_US/Wayland.ent \
	$(srcdir)/en_US/Architecture.xml \
	$(srcdir)/en_US/Author_Group.xml \
	$(srcdir)/en_US/Book_Info.xml \
	$(srcdir)/en_US/Chapter.xml \
	$(srcdir)/en_US/Preface.xml \
	$(srcdir)/en_US/Wayland.xml \
	$(srcdir)/en_US/Revision_History.xml \
	$(srcdir)/en_US/images/icon.svg  \
	$(srcdir)/en_US/images/wayland-architecture.png \
	$(srcdir)/en_US/images/wayland.png  \
	$(srcdir)/en_US/images/x-architecture.png

publican_targets = $(publican_sources:$(srcdir)/en_US%=$(builddir)/en-US%) \
	en-US/Protocol.xml

# The Protocol.xml is purely generated and required before running publican
en-US/Protocol.xml: $(top_srcdir)/protocol/wayland.xml $(srcdir)/protocol-to-docbook.xsl
	$(AM_V_GEN)$(MKDIR_P) en-US/images
	$(AM_V_GEN)$(XSLTPROC) $(srcdir)/protocol-to-docbook.xsl \
		$(top_srcdir)/protocol/wayland.xml > en-US/Protocol.xml

# Copy the en_US source files into en-US destination
# This is required for out-of-source-tree build as publican does not allow us
# to specify the location of the source code.
$(builddir)/en-US/%: $(srcdir)/en_US/% en-US/Protocol.xml $(publican_sources)
	$(AM_V_GEN)cp -f $< $@

# Run publican for the builddir on the generated (or copied) source
# The output formats are generated in the Wayland sub directory
Wayland: $(publican_targets)
	$(AM_V_GEN)$(PUBLICAN) build --lang en-US --format html,pdf \
		--config $(srcdir)/publican.cfg

CLEANFILES = en-US/Protocol.xml $(publican_targets)

clean-local:
	$(AM_V_at)rm -fr en-US
	$(AM_V_at)rm -fr Wayland

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

EXTRA_DIST = $(publican_sources) publican.cfg protocol-to-docbook.xsl

endif
