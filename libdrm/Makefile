SOURCES = xf86drm.c xf86drmHash.c xf86drmRandom.c xf86drmSL.c
OBJECTS = xf86drm.o xf86drmHash.o xf86drmRandom.o xf86drmSL.o

CFLAGS ?= -O2 -fPIC -g

all: libdrm.so

libxf86drm.a:	$(OBJECTS)	
	ar rc $@ $+
	ranlib $@

libdrm.so: $(OBJECTS)
	$(CC) -shared -Wl,-hlibdrm.so.1 -o $@ $^

.c.o:
	$(CC) $(CFLAGS) -c -I../shared -I../linux $<

clean:
	rm -f *.a *.o *.so

install: libdrm.so xf86drm.h ../shared-core/drm.h
	install -m 755 libdrm.so /lib
	install -m 644 xf86drm.h /usr/include
	install -m 644 ../shared-core/drm.h /usr/include
