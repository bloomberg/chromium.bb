SOURCES = xf86drm.c  xf86drmCompat.c xf86drmHash.c xf86drmRandom.c xf86drmSL.c
OBJECTS = xf86drm.o  xf86drmCompat.o xf86drmHash.o xf86drmRandom.o xf86drmSL.o

libxf86drm.a:	$(OBJECTS)	
	ar rc $@ $+
	ranlib $@

$(OBJECTS):	$(SOURCES)
	$(CC) $(CFLAGS) -c -I../shared -I../linux $+

clean:
	rm *.a *.o
