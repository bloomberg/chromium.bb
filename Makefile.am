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
SUBDIRS=fontconfig fc-lang fc-glyphname src fc-cache fc-list fc-match $(DOCSRC) test

EXTRA_DIST = \
        fontconfig.pc.in \
        fonts.conf.in \
	fonts.dtd \
	local.conf \
        fontconfig.spec.in \
        fontconfig.spec \
	fontconfig-zip.in

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = fontconfig.pc

configdir=$(CONFDIR)
config_DATA=fonts.dtd

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
	if [ -f $(DESTDIR)$(configdir)/local.conf ]; then \
	  echo "not overwriting existing $(DESTDIR)$(configdir)/local.conf"; \
	else if [ -f $(srcdir)/local.conf ]; then \
	  echo " $(INSTALL_DATA) $(srcdir)/local.conf $(DESTDIR)$(configdir)/local.conf"; \
	  $(INSTALL_DATA) $(srcdir)/local.conf $(DESTDIR)$(configdir)/local.conf; \
	else if [ -f local.conf ]; then \
	  echo " $(INSTALL_DATA) local.conf $(DESTDIR)$(configdir)/local.conf"; \
	  $(INSTALL_DATA) local.conf $(DESTDIR)$(configdir)/local.conf; \
	fi; fi; fi
	if [ x$(DESTDIR) = x ]; then fc-cache/fc-cache -f -v; fi
