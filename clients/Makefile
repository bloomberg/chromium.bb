include ../config.mk

egl_clients = gears
cairo_clients = flower screenshot terminal image view

all : $(egl_clients) $(cairo_clients)

clean :
	rm -f $(egl_clients) $(cairo_clients) *.o

flower : flower.o wayland-glib.o
gears : gears.o window.o wayland-glib.o cairo-util.o
screenshot : screenshot.o wayland-glib.o
terminal : terminal.o window.o wayland-glib.o cairo-util.o
image : image.o window.o wayland-glib.o cairo-util.o
view : view.o window.o wayland-glib.o cairo-util.o

terminal : LDLIBS += -lutil
image : CFLAGS += $(GDK_PIXBUF_CFLAGS)
image : LDLIBS += $(GDK_PIXBUF_LIBS)
view : CFLAGS += $(POPPLER_CFLAGS)
view : LDLIBS += $(POPPLER_LIBS)

$(egl_clients) : CFLAGS += $(EGL_CLIENT_CFLAGS)
$(egl_clients) : LDLIBS += -L.. -lwayland $(EGL_CLIENT_LIBS) -lrt
$(cairo_clients) : CFLAGS += $(CAIRO_CLIENT_CFLAGS)
$(cairo_clients) : LDLIBS += -L..  -lwayland $(CAIRO_CLIENT_LIBS) -lrt
