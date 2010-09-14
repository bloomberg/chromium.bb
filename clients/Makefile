include ../config.mk

CFLAGS += -I.. $(CLIENT_CFLAGS)
LDLIBS += -L../wayland -lwayland-client $(CLIENT_LIBS) -lrt -lm

egl_clients = gears
cairo_clients = flower screenshot terminal image view dnd

all : $(egl_clients) $(cairo_clients)

clean :
	rm -f $(egl_clients) $(cairo_clients) *.o .*.deps

flower : flower.o window.o wayland-glib.o cairo-util.o
gears : gears.o window.o wayland-glib.o cairo-util.o
screenshot : screenshot.o wayland-glib.o cairo-util.o
terminal : terminal.o window.o wayland-glib.o cairo-util.o
image : image.o window.o wayland-glib.o cairo-util.o
view : view.o window.o wayland-glib.o cairo-util.o
dnd : dnd.o window.o wayland-glib.o cairo-util.o

terminal : LDLIBS += -lutil
view : CFLAGS += $(POPPLER_CFLAGS)
view : LDLIBS += $(POPPLER_LIBS)

install :