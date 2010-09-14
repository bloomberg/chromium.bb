include ../config.mk

CFLAGS += $(COMPOSITOR_CFLAGS)
LDLIBS += -L.. -lwayland-server $(COMPOSITOR_LIBS) -rdynamic -lrt -lEGL -lm

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