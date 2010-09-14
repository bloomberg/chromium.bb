include ../config.mk

CFLAGS += $(COMPOSITOR_CFLAGS)
LDLIBS += -L../wayland -lwayland-server $(COMPOSITOR_LIBS) -rdynamic -lrt -lm

all : compositor

compositor :					\
	compositor.o				\
	compositor-drm.o			\
	compositor-x11.o			\
	screenshooter.o				\
	drm.o

clean :
	rm -f compositor *.o .*.deps

install :
	install 70-wayland.rules ${udev_rules_dir}
