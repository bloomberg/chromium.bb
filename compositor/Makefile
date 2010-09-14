include ../config.mk

CFLAGS += $(COMPOSITOR_CFLAGS)
LDLIBS += -L../wayland -lwayland-server $(COMPOSITOR_LIBS) -rdynamic -lrt -lm

all : compositor

compositor :					\
	compositor.o				\
	compositor-drm.o			\
	compositor-x11.o			\
	screenshooter.o				\
	screenshooter-protocol.o		\
	drm.o

screenshooter.c : screenshooter-server-protocol.h

screenshooter-protocol.c : screenshooter.xml
	../wayland/scanner code < $< > $@

screenshooter-server-protocol.h : screenshooter.xml
	../wayland/scanner server-header < $< > $@

clean :
	rm -f compositor *.o .*.deps

install :
	install 70-wayland.rules ${udev_rules_dir}
