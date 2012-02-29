if HAVE_PUBLICAN
noinst_DATA = Wayland

publican_sources = \
	$(srcdir)/en-US/Wayland.ent \
	$(srcdir)/en-US/Architecture.xml \
	$(srcdir)/en-US/Author_Group.xml \
	$(srcdir)/en-US/Book_Info.xml \
	$(srcdir)/en-US/Chapter.xml \
	$(srcdir)/en-US/Preface.xml \
	$(srcdir)/en-US/Wayland.xml \
	$(srcdir)/en-US/Revision_History.xml \
	$(srcdir)/en-US/images/icon.svg  \
	$(srcdir)/en-US/images/wayland-architecture.png \
	$(srcdir)/en-US/images/wayland.png  \
	$(srcdir)/en-US/images/x-architecture.png

# publican does not support out-of-tree builds and during make distcheck the
# source tree is read-only so we can't chdir and/or dump the protocol there
# copy the tree into _build, leaving a marker file to clean up after
# distclean
# See https://bugzilla.redhat.com/show_bug.cgi?id=798484 for the chmod
copy-sources:
	if ! test -e "en-US/"; then \
		mkdir en-US/; \
		touch en-US/need-distclean; \
		cp -r $(srcdir)/en-US/* en-US; \
		chmod u+w en-US/images; \
	fi

Wayland: docbook-xsl publican.cfg $(publican_sources)
	publican build --config=$(srcdir)/publican.cfg --lang en-US --format html,pdf

# This must be run befor the publican run
docbook-xsl: copy-sources $(top_srcdir)/protocol/wayland.xml $(srcdir)/protocol-to-docbook.xsl
	$(AM_V_GEN)$(XSLTPROC) $(srcdir)/protocol-to-docbook.xsl $(top_srcdir)/protocol/wayland.xml > $(builddir)/en-US/Protocol.xml

clean-local:
	-rm -rf Wayland
	if test -e "en-US/need-distclean"; then \
		rm -rf en-US; \
	fi

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(docdir)
	cp -R $(builddir)/Wayland $(DESTDIR)$(docdir)

uninstall-local:
	-rm -rf $(DESTDIR)$(docdir)/Wayland/*
	-rmdir $(DESTDIR)$(docdir)/Wayland

EXTRA_DIST = $(publican_sources) publican.cfg protocol-to-docbook.xsl

endif
