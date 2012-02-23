if HAVE_PUBLICAN
EXTRA_DIST = Wayland
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


Wayland: publican.cfg $(publican_sources)
	publican build --lang en-US --format html,pdf

clean-local:
	-rm -rf Wayland

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(docdir)
	cp -R $(builddir)/Wayland $(DESTDIR)$(docdir)

uninstall-local:
	-rm -rf $(DESTDIR)$(docdir)/Wayland/*
	-rmdir $(DESTDIR)$(docdir)/Wayland

EXTRA_DIST = $(publican_sources) publican.cfg
endif
