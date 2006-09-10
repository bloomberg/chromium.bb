# 
#  $Id$
# 
#  Copyright © 2005 Keith Packard
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

CONF_FILES = \
	10-urw-aliases.conf \
	15-amt-aliases.conf \
	20-fix-globaladvance.conf \
	30-unhint-small-vera.conf \
	50-user.conf \
	51-local.conf \
	60-LohitGujarati.conf \
	60-delicious.conf \
	60-fonts-persian.conf \
	70-no-sub-pixel.conf \
	70-sub-pixel-bgr.conf \
	70-sub-pixel-rgb.conf \
	70-sub-pixel-vbgr.conf \
	70-sub-pixel-vrgb.conf \
	73-autohint.conf \
	73-unhinted.conf \
	76-no-bitmaps.conf \
	76-yes-bitmaps.conf

CONF_LINKS = \
	10-urw-aliases.conf \
	15-amt-aliases.conf \
	20-fix-globaladvance.conf \
	30-unhint-small-vera.conf \
	50-user.conf \
	51-local.conf \
	60-LohitGujarati.conf \
	60-delicious.conf \
	60-fonts-persian.conf

EXTRA_DIST = $(CONF_FILES)

configdir=$(CONFDIR)
confavaildir=$(configdir)/conf.avail
confddir=$(configdir)/conf.d

confavail_DATA=$(CONF_FILES)

install-data-local:
	mkdir -p ${DESTDIR}${confddir}
	@(echo cd ${DESTDIR}${confddir}; \
	 cd ${DESTDIR}${confddir}; \
	 for i in ${CONF_LINKS}; do \
	 	rm -f $$i; \
		echo rm -f $$i";" ln -s ../conf.avail/$$i .; \
		ln -s ../conf.avail/$$i .; \
	 done)
