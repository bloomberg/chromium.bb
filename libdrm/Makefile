SOURCES = xf86drm.c xf86drmHash.c xf86drmRandom.c xf86drmSL.c
OBJECTS = xf86drm.o xf86drmHash.o xf86drmRandom.o xf86drmSL.o

CFLAGS ?= -O2 -fPIC -g
DEFINES = -D_IN_LIBDRM

# set this if you're installing into a fake root
DESTDIR ?=

DRIVER_HEADERS = ../shared-core/drm.h \
		 ../shared-core/drm_sarea.h \
		 ../shared-core/i915_drm.h \
		 ../shared-core/i915_drv.h \
		 ../shared-core/mach64_drm.h \
		 ../shared-core/mach64_drv.h \
		 ../shared-core/mga_drm.h \
		 ../shared-core/mga_drv.h \
		 ../shared-core/mga_ucode.h \
		 ../shared-core/r128_drm.h \
		 ../shared-core/r128_drv.h \
		 ../shared-core/radeon_drm.h \
		 ../shared-core/radeon_drv.h \
		 ../shared-core/savage_drm.h \
		 ../shared-core/savage_drv.h \
		 ../shared-core/sis_drm.h \
		 ../shared-core/sis_drv.h \
		 ../shared-core/sis_ds.h \
		 ../shared-core/tdfx_drv.h \
		 ../shared-core/via_3d_reg.h \
		 ../shared-core/via_drm.h \
		 ../shared-core/via_drv.h \
		 ../shared-core/via_ds.h \
		 ../shared-core/via_mm.h \
		 ../shared-core/via_verifier.h

all: libdrm.so

libdrm.so: $(OBJECTS)
	$(CC) -shared -Wl,-hlibdrm.so.1 -o $@ $^

.c.o:
	$(CC) $(DEFINES) $(CFLAGS) -c -I../shared-core $<

clean:
	rm -f *.a *.o *.so

# XXX remove the xf86drm.h compat once this is widely installed
install: libdrm.so xf86drm.h $(DRIVER_HEADERS)
	install -m 755 libdrm.so $(DESTDIR)/lib
	install -m 644 xf86drm.h $(DESTDIR)/usr/include/libdrm.h
	install -m 644 xf86drm.h $(DESTDIR)/usr/include/xf86drm.h
	echo '#warning Including xf86drm.h is deprecated, use libdrm.h' >> \
	    $(DESTDIR)/usr/include/xf86drm.h
	mkdir -p -m 755 $(DESTDIR)/usr/include/drm
	install -m 644 $(DRIVER_HEADERS) $(DESTDIR)/usr/include/drm
