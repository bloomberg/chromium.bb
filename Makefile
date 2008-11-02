CFLAGS +=  -Wall -g $(shell pkg-config --cflags libffi libdrm)
LDLIBS += $(shell pkg-config --libs libffi libdrm)

clients = flower pointer

all : wayland $(clients)

wayland_objs = wayland.o event-loop.o connection.o hash.o input.o egl-compositor.o
wayland : CFLAGS += -I../eagle
wayland : LDLIBS += -L../eagle -leagle -ldl

wayland : $(wayland_objs)
	gcc -o $@ $(LDLIBS) $(wayland_objs)

libwayland_objs = wayland-client.o connection.o

libwayland.so : $(libwayland_objs)
	gcc -o $@ $(libwayland_objs) -shared

flower_objs = flower.o
pointer_objs = pointer.o

$(clients) : CFLAGS += $(shell pkg-config --cflags cairo)
$(clients) : LDLIBS += $(shell pkg-config --libs cairo) -lrt

define client_template
$(1): $$($(1)_objs) libwayland.so
endef

$(foreach c,$(clients),$(eval $(call client_template,$(c))))

$(clients) :
	gcc -o $@ -L. -lwayland $(LDLIBS) $^

clean :
	rm -f $(clients) wayland *.o libwayland.so