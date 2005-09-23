# 
#  $Id$
# 
#  Copyright © 2003 Keith Packard
# 
#  Permission to use, copy, modify, distribute, and sell this software and its
#  documentation for any purpose is hereby granted without fee, provided that
#  the above copyright notice appear in all copies and that both that
#  copyright notice and this permission notice appear in supporting
#  documentation, and that the name of Keith Packard not be used in
#  advertising or publicity pertaining to distribution of the software without
#  specific, written prior permission.  Keith Packard makes no
#  representations about the suitability of this software for any purpose.  It
#  is provided "as is" without express or implied warranty.
# 
#  KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
#  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
#  EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
#  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
#  DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
#  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
#  PERFORMANCE OF THIS SOFTWARE.

DOCSRC=@DOCSRC@
SUBDIRS=fontconfig fc-case fc-lang fc-glyphname src \
	fc-cache fc-cat fc-list fc-match conf.d $(DOCSRC) test

DEBIAN = \
	debian/changelog \
	debian/compat \
	debian/control \
	debian/copyright \
	debian/fontconfig.config \
	debian/fontconfig.defoma \
	debian/fontconfig.dirs \
	debian/fontconfig.install \
	debian/fontconfig.postinst \
	debian/fontconfig.postrm \
	debian/fontconfig.templates \
	debian/fontconfig-udeb.install \
	debian/libfontconfig1-dev.install \
	debian/libfontconfig1.install \
	debian/local.conf.md5sum \
	debian/README.Debian \
	debian/rules \
	debian/po/POTFILES.in \
	debian/po/templates.pot \
	debian/po/cs.po \
	debian/po/da.po \
	debian/po/de.po \
	debian/po/es.po \
	debian/po/fr.po \
	debian/po/ja.po \
	debian/po/nl.po \
	debian/po/pt_BR.po \
	debian/po/pt.po \
	debian/po/tr.po \
	debian/po/zh_CN.po
	 
EXTRA_DIST = \
        fontconfig.pc.in \
        fonts.conf.in \
	fonts.dtd \
        fontconfig.spec.in \
        fontconfig.spec \
	fontconfig-zip.in \
	$(DEBIAN)

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = fontconfig.pc

configdir=$(CONFDIR)
config_DATA=fonts.dtd

if CROSS_COMPILING
  RUN_FC_CACHE_TEST=false
else
  RUN_FC_CACHE_TEST=test -z "$(DESTDIR)"
endif

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(configdir)
	if [ -f $(DESTDIR)$(configdir)/fonts.conf ]; then \
	  echo "backing up existing $(DESTDIR)$(configdir)/fonts.conf"; \
	  mv $(DESTDIR)$(configdir)/fonts.conf $(DESTDIR)$(configdir)/fonts.conf.bak; \
	fi
	if [ -f $(srcdir)/fonts.conf ]; then \
	  echo " $(INSTALL_DATA) $(srcdir)/fonts.conf $(DESTDIR)$(configdir)/fonts.conf"; \
	  $(INSTALL_DATA) $(srcdir)/fonts.conf $(DESTDIR)$(configdir)/fonts.conf; \
	else if [ -f fonts.conf ]; then \
	  echo " $(INSTALL_DATA) fonts.conf $(DESTDIR)$(configdir)/fonts.conf"; \
	  $(INSTALL_DATA) fonts.conf $(DESTDIR)$(configdir)/fonts.conf; \
	fi; fi
	if $(RUN_FC_CACHE_TEST); then \
	    echo " fc-cache/fc-cache -f -v"; \
	    fc-cache/fc-cache -f -v; \
	else \
	    echo "***"; \
	    echo "*** Warning: fonts.cache not built"; \
	    echo "***"; \
	    echo "*** Generate this file manually on host system using fc-cache"; \
	    echo "***"; \
	fi

uninstall-local:
	if [ -f $(srcdir)/fonts.conf ]; then \
	  if cmp -s $(srcdir)/fonts.conf $(DESTDIR)$(configdir)/fonts.conf; then \
	     echo " uninstall standard $(DESTDIR)$(configdir)/fonts.conf"; \
	     rm -f $(DESTDIR)$(configdir)/fonts.conf; \
	  fi; \
	else if [ -f fonts.conf ]; then \
	  if cmp -s fonts.conf $(DESTDIR)$(configdir)/fonts.conf; then \
	     echo " uninstall standard $(DESTDIR)$(configdir)/fonts.conf"; \
	     rm -f $(DESTDIR)$(configdir)/fonts.conf; \
	  fi; \
	fi; fi

debuild debuild-signed: debuild-dirs
	(cd $(distdir)/debian && debuild)

debuild-unsigned: debuild-dirs
	(cd $(distdir)/debian && debuild -us -uc)

debuild-dirs: distdir
	rm -f $(PACKAGE)_$(VERSION).orig.tar.gz
	rm -rf $(distdir).orig
	cp -a $(distdir) $(distdir).orig
	rm -rf $(distdir).orig/debian
