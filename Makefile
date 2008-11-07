CFLAGS = -Wall -g

EAGLE_CFLAGS = -I../eagle
EAGLE_LDLIBS = -L../eagle -leagle

clients = flower pointer background window
compositors = egl-compositor.so glx-compositor.so

all : wayland libwayland.so $(compositors) $(clients)

wayland_objs =					\
	wayland.o				\
	event-loop.o				\
	connection.o				\
	hash.o					\
	input.o

wayland : CFLAGS += $(shell pkg-config --cflags libffi)
wayland : LDLIBS += $(shell pkg-config --libs libffi) -ldl -rdynamic

wayland : $(wayland_objs)
	gcc -o $@ $(LDLIBS) $(wayland_objs)

libwayland_objs = wayland-client.o connection.o

libwayland.so : $(libwayland_objs)

$(compositors) $(clients) : CFLAGS += $(shell pkg-config --cflags libdrm)

egl_compositor_objs = egl-compositor.o
egl-compositor.so : CFLAGS += $(EAGLE_CFLAGS)
egl-compositor.so : LDLIBS += $(EAGLE_LDLIBS) -rdynamic

egl-compositor.so : $(egl_compositor_objs)

glx_compositor_objs = glx-compositor.o
glx-compositor.so : LDLIBS += -lGL

glx-compositor.so : $(glx_compositor_objs)


libwayland.so $(compositors) :
	gcc -o $@ $^ $(LDLIBS) -shared 

flower_objs = flower.o wayland-glib.o
pointer_objs = pointer.o
background_objs = background.o
window_objs = window.o gears.o

$(clients) : CFLAGS += $(shell pkg-config --cflags cairo)
$(clients) : LDLIBS += $(shell pkg-config --libs cairo) -lrt

background : CFLAGS += $(shell pkg-config --cflags gdk-pixbuf-2.0)
background : LDLIBS += $(shell pkg-config --libs gdk-pixbuf-2.0)

flower : CFLAGS += $(shell pkg-config --cflags glib-2.0)
flower : LDLIBS += $(shell pkg-config --libs glib-2.0)

window : CFLAGS += $(EAGLE_CFLAGS)
window : LDLIBS += $(EAGLE_LDLIBS)

define client_template
$(1): $$($(1)_objs) libwayland.so
endef

$(foreach c,$(clients),$(eval $(call client_template,$(c))))

$(clients) :
	gcc -o $@ -L. -lwayland $(LDLIBS) $^

clean :
	rm -f $(clients) wayland *.o *.so
