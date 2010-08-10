include config.mk

subdirs = clients spec
libs = libwayland-server.so libwayland-client.so

all : $(libs) compositor subdirs-all scanner

headers =					\
	wayland-util.h				\
	wayland-server-protocol.h		\
	wayland-server.h			\
	wayland-client-protocol.h		\
	wayland-client.h \

libwayland-server.so :				\
	wayland-protocol.o			\
	wayland-server.o			\
	event-loop.o				\
	connection.o				\
	wayland-util.o				\
	wayland-hash.o

libwayland-client.so :				\
	wayland-protocol.o			\
	wayland-client.o			\
	connection.o				\
	wayland-util.o				\
	wayland-hash.o

wayland-server.o : wayland-server-protocol.h
wayland-client.o : wayland-client-protocol.h

wayland-protocol.c : protocol.xml scanner
	./scanner code < $< > $@

wayland-server-protocol.h : protocol.xml scanner
	./scanner server-header < $< > $@

wayland-client-protocol.h : protocol.xml scanner
	./scanner client-header < $< > $@

$(libs) : CFLAGS += -fPIC $(FFI_CFLAGS)
$(libs) : LDLIBS += $(FFI_LIBS)
$(libs) :
	gcc -shared $^ $(LDLIBS)  -o $@

compositor :					\
	compositor.o				\
	compositor-drm.o			\
	compositor-x11.o			\
	screenshooter.o				\
	cairo-util.o				\
	drm.o

compositor : CFLAGS += $(COMPOSITOR_CFLAGS)
compositor : LDLIBS += ./libwayland-server.so $(COMPOSITOR_LIBS) -rdynamic -lrt -lEGL -lm

scanner :					\
	scanner.o				\
	wayland-util.o

scanner : LDLIBS += $(EXPAT_LIBS)

subdirs-all subdirs-clean :
	for f in $(subdirs); do $(MAKE) -C $$f $(@:subdirs-%=%); done

install : $(libs) compositor
	install -d $(libdir) $(libdir)/pkgconfig ${udev_rules_dir}
	install $(libs) $(libdir)
	install wayland-server.pc wayland-client.pc $(libdir)/pkgconfig
	install $(headers) $(includedir)
	install 70-wayland.rules ${udev_rules_dir}

clean : subdirs-clean
	rm -f compositor scanner *.o *.so
	rm -f wayland-protocol.c \
		wayland-server-protocol.h wayland-client-protocol.h

config.mk : config.mk.in
	./config.status
