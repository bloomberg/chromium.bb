CFLAGS +=  -Wall -g $(shell pkg-config --cflags libffi libdrm)
LDLIBS += $(shell pkg-config --libs libffi libdrm)

all : wayland client

wayland_objs = wayland.o event-loop.o connection.o hash.o compositor.o

wayland : $(wayland_objs)
	gcc -o $@ $(wayland_objs) $(LDLIBS)

libwayland_objs = wayland-client.o

libwayland.so : $(libwayland_objs)
	gcc -o $@ $(libwayland_objs) -shared

client_objs = client.o
client : CFLAGS += $(shell pkg-config --cflags cairo)
client : LDLIBS += $(shell pkg-config --libs cairo)

client : $(client_objs) libwayland.so
	gcc -o $@ -L. -lwayland $(LDLIBS) $(client_objs)

clean :
	rm -f client wayland *.o libwayland.so