# Documentation is built with xmlto, but some of the recipes in here are
# leftovers from building with Publican (https://fedorahosted.org/publican/)
#
# How this build works:
# * the main target is Wayland, documentation ends up in $(builddir)/Wayland/
# * hand-written chapters and CSS files are located in sources. These are
#   copied into $(builddir)/en-US/
# * ProtocolSpec.xml is generated from $(top_srcdir)/protocol/wayland.xml,
#   changed into docbook via XSLT and saved in $(builddir)/en-US/
# * ProtocolInterfaces.xml, same as above, uses a different XSLT
# * WaylandClientAPI.xml is generated from the doxygen output and saved in
#   $(builddir)/en-US
# * WaylandServerAPI.xml is generated from the doxygen output and saved in
#   $(builddir)/en-US
# * run xmlto on $(builddir)/en-US, output to $(builddir)/Wayland/en-US
publican_sources = \
	$(srcdir)/sources/Wayland.ent \
	$(srcdir)/sources/Wayland.xml \
	$(srcdir)/sources/Book_Info.xml \
	$(srcdir)/sources/Author_Group.xml \
	$(srcdir)/sources/Foreword.xml \
	$(srcdir)/sources/Preface.xml \
	$(srcdir)/sources/Revision_History.xml \
	$(srcdir)/sources/Introduction.xml \
	$(srcdir)/sources/Architecture.xml \
	$(srcdir)/sources/Protocol.xml \
	$(srcdir)/sources/Library.xml \
	$(srcdir)/sources/Compositors.xml \
	$(srcdir)/sources/images/icon.svg  \
	$(srcdir)/sources/images/wayland-architecture.png \
	$(srcdir)/sources/images/wayland.png  \
	$(srcdir)/sources/images/x-architecture.png

css_sources = \
	$(srcdir)/sources/css/brand.css \
	$(srcdir)/sources/css/common.css \
	$(srcdir)/sources/css/default.css \
	$(srcdir)/sources/css/epub.css \
	$(srcdir)/sources/css/print.css
img_sources = \
	$(srcdir)/sources/images/icon.svg \
	$(srcdir)/sources/images/wayland-architecture.png \
	$(srcdir)/sources/images/wayland.png \
	$(srcdir)/sources/images/x-architecture.png

if HAVE_XMLTO
if HAVE_XSLTPROC
noinst_DATA = Wayland $(publican_targets)
XMLTO_PARAM = \
	--skip-validation \
	--stringparam chunk.section.depth=0 \
	--stringparam toc.section.depth=1 \
	--stringparam html.stylesheet=css/default.css

html_destdir = $(builddir)/Wayland/en-US/html

Wayland: $(publican_targets)
	$(AM_V_GEN)$(MKDIR_P) -p $(html_destdir)/css
	$(AM_V_GEN)cp -f $(css_sources) $(html_destdir)/css/
	$(AM_V_GEN)$(MKDIR_P) -p $(html_destdir)/images
	$(AM_V_GEN)cp -f $(img_sources) $(html_destdir)/images/
	$(AM_V_GEN)$(XMLTO) $(XMLTO_PARAM) html en-US/Wayland.xml -o $(html_destdir)
	@touch Wayland

pubdir = $(docdir)/Wayland/en-US

publican_targets = $(publican_sources:$(srcdir)/sources%=$(builddir)/en-US%) \
	en-US/ProtocolSpec.xml en-US/ProtocolInterfaces.xml \
	en-US/WaylandClientAPI.xml en-US/WaylandServerAPI.xml

# The Protocol.xml is purely generated and required before running publican
en-US/ProtocolSpec.xml: $(top_srcdir)/protocol/wayland.xml $(srcdir)/protocol-to-docbook.xsl
	$(AM_V_GEN)$(MKDIR_P) en-US/images
	$(AM_V_GEN)$(XSLTPROC) $(srcdir)/protocol-to-docbook.xsl \
		$(top_srcdir)/protocol/wayland.xml > en-US/ProtocolSpec.xml

en-US/ProtocolInterfaces.xml: $(top_srcdir)/protocol/wayland.xml $(srcdir)/protocol-interfaces-to-docbook.xsl
	$(AM_V_GEN)$(MKDIR_P) en-US/images
	$(AM_V_GEN)$(XSLTPROC) $(srcdir)/protocol-interfaces-to-docbook.xsl \
		$(top_srcdir)/protocol/wayland.xml > en-US/ProtocolInterfaces.xml

# * we don't want wayland-{server|client}_8h.xml to avoid duplicating output methods,
#   move it out of the way first.
# * use doxygen's combine.xslt to merge the xml files into one single file
# * move wayland-<foo>_8h.xml back to its original location
en-US/%API.xml.tmp: $(top_builddir)/doc/doxygen/xml/%/index.xml
	$(AM_V_GEN)mv $(top_builddir)/doc/doxygen/xml/$*/wayland-$*_8h.xml \
		$(top_builddir)/doc/doxygen/xml/
	$(AM_V_GEN)$(XSLTPROC) $(top_builddir)/doc/doxygen/xml/$*/combine.xslt \
		$(top_builddir)/doc/doxygen/xml/$*/index.xml > $@
	$(AM_V_GEN)mv $(top_builddir)/doc/doxygen/xml/wayland-$*_8h.xml \
		$(top_builddir)/doc/doxygen/xml/$*

# WaylandClientAPI.xml:
# merge doxygen xml files into one single file, then transform the combined XML file into docbook format
en-US/WaylandClientAPI.xml: en-US/clientAPI.xml.tmp $(srcdir)/doxygen-to-publican.xsl
	$(AM_V_GEN)$(XSLTPROC)  --stringparam which Client $(srcdir)/doxygen-to-publican.xsl \
		$(builddir)/en-US/clientAPI.xml.tmp > en-US/WaylandClientAPI.xml

# WaylandServerAPI.xml: see WaylandClientAPI.xml
en-US/WaylandServerAPI.xml: en-US/serverAPI.xml.tmp $(srcdir)/doxygen-to-publican.xsl
	$(AM_V_GEN)$(XSLTPROC) --stringparam which Server $(srcdir)/doxygen-to-publican.xsl \
		$(builddir)/en-US/serverAPI.xml.tmp > en-US/WaylandServerAPI.xml

# Copy the sources source files into en-US destination
# This is required for out-of-source-tree build as publican does not allow us
# to specify the location of the source code.
$(builddir)/en-US/%: $(srcdir)/sources/% en-US/ProtocolSpec.xml en-US/ProtocolInterfaces.xml en-US/WaylandClientAPI.xml $(publican_sources)
	$(AM_V_GEN)cp -f $< $@
	$(AM_V_GEN)chmod a+w $@

CLEANFILES = en-US/ProtocolSpec.xml en-US/ProtocolInterfaces.xml en-US/WaylandClientAPI.xml $(publican_targets)

clean-local:
	$(AM_V_at)rm -fr $(builddir)/en-US
	$(AM_V_at)rm -fr $(builddir)/Wayland

install-data-local:
	test -z "$(pubdir)/html/css" || $(mkdir_p) "$(DESTDIR)$(pubdir)/html/css"
	test -z "$(pubdir)/html/images" || $(mkdir_p) "$(DESTDIR)$(pubdir)/html/images"
	list=`find $(builddir)/Wayland/en-US -type f`; \
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

EXTRA_DIST = $(publican_sources) $(css_sources) $(img_sources) protocol-to-docbook.xsl protocol-interfaces-to-docbook.xsl doxygen-to-publican.xsl
