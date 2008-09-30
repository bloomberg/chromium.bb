CFLAGS = -Wall -g $(shell pkg-config --cflags libffi)
LDLIBS = $(shell pkg-config --libs libffi)

all : wayland client

wayland_objs = wayland.o event-loop.o hash.o

wayland : $(wayland_objs)
	gcc -o $@ $(wayland_objs) $(LDLIBS)

client_objs = client.o

client : $(client_objs)
	gcc -o $@ $(client_objs)

clean :
	rm client wayland *.o