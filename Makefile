include config.mk

subdirs = clients spec
libs = libwayland-server.so libwayland.so

all : $(libs) compositor subdirs-all

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

compositor :					\
	compositor.o				\
	evdev.o					\
	cairo-util.o				\
	wayland-util.o

compositor : CFLAGS += $(COMPOSITOR_CFLAGS)
compositor : LDLIBS += ./libwayland-server.so $(COMPOSITOR_LIBS) -rdynamic -lrt -lEGL -lm

subdirs-all subdirs-clean :
	for f in $(subdirs); do $(MAKE) -C $$f $(@:subdirs-%=%); done

install : $(libs) compositor
	install -d $(libdir) $(libdir)/pkgconfig ${udev_rules_dir}
	install $(libs) $(libdir)
	install wayland-server.pc wayland.pc $(libdir)/pkgconfig
	install wayland-util.h wayland-client.h $(includedir)
	install 70-wayland.rules ${udev_rules_dir}

clean : subdirs-clean
	rm -f compositor *.o *.so

config.mk : config.mk.in
	./config.status
