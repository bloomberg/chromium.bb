CFLAGS +=  -Wall -g $(shell pkg-config --cflags libffi libdrm)
LDLIBS += $(shell pkg-config --libs libffi libdrm)

EAGLE_CFLAGS = -I../eagle
EAGLE_LDLIBS = -L../eagle -leagle

clients = flower pointer background window

all : wayland $(clients)

wayland_objs = wayland.o event-loop.o connection.o hash.o input.o egl-compositor.o
wayland : CFLAGS += $(EAGLE_CFLAGS)
wayland : LDLIBS += $(EAGLE_LDLIBS)

wayland : $(wayland_objs)
	gcc -o $@ $(LDLIBS) $(wayland_objs)

libwayland_objs = wayland-client.o connection.o

libwayland.so : $(libwayland_objs)
	gcc -o $@ $(libwayland_objs) -shared

flower_objs = flower.o
pointer_objs = pointer.o
background_objs = background.o
window_objs = window.o gears.o

$(clients) : CFLAGS += $(shell pkg-config --cflags cairo)
$(clients) : LDLIBS += $(shell pkg-config --libs cairo gdk-pixbuf-2.0) -lrt

background : CFLAGS += $(shell pkg-config --cflags gdk-pixbuf-2.0)
background : LDLIBS += $(shell pkg-config --libs gdk-pixbuf-2.0)

window : CFLAGS += $(EAGLE_CFLAGS)
window : LDLIBS += $(EAGLE_LDLIBS)

define client_template
$(1): $$($(1)_objs) libwayland.so
endef

$(foreach c,$(clients),$(eval $(call client_template,$(c))))

$(clients) :
	gcc -o $@ -L. -lwayland $(LDLIBS) $^

clean :
	rm -f $(clients) wayland *.o libwayland.so