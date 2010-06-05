include config.mk

subdirs = clients
libs = libwayland-server.so libwayland.so
compositors = wayland-system-compositor

all : $(libs) $(compositors) subdirs

libwayland-server.so :				\
	wayland.o				\
	event-loop.o				\
	connection.o				\
	wayland-util.o				\
	wayland-hash.o				\
	wayland-protocol.o

libwayland.so :					\
	wayland-client.o			\
	connection.o				\
	wayland-util.o				\
	wayland-hash.o				\
	wayland-protocol.o

$(libs) : CFLAGS += -fPIC $(FFI_CFLAGS)
$(libs) : LDLIBS += $(FFI_LIBS)
$(libs) :
	gcc -shared $^ $(LDLIBS)  -o $@

wayland-system-compositor :			\
	wayland-system-compositor.o		\
	evdev.o					\
	cairo-util.o				\
	wayland-util.o

wayland-system-compositor : CFLAGS += $(EGL_COMPOSITOR_CFLAGS)
wayland-system-compositor : LDLIBS += ./libwayland-server.so $(EGL_COMPOSITOR_LIBS) -rdynamic -lrt -lEGL

subdirs-all subdirs-clean :
	for f in $(subdirs); do $(MAKE) -C $$f $(@:subdirs-%=%); done

install : $(libs) $(compositors)
	install -d $(libdir) $(libdir)/pkgconfig ${udev_rules_dir}
	install $(libs) $(libdir)
	install wayland-server.pc wayland.pc $(libdir)/pkgconfig
	install wayland-util.h wayland-client.h $(includedir)
	install 70-wayland.rules ${udev_rules_dir}

clean : subdirs-clean
	rm -f $(compositors) *.o *.so

config.mk : config.mk.in
	./config.status
