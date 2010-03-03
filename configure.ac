AC_INIT(wayland, 0.1)
AC_PROG_CC

PKG_PROG_PKG_CONFIG()
PKG_CHECK_MODULES(FFI, [libffi])

PKG_CHECK_MODULES(EGL_COMPOSITOR,
		  [gl libpng cairo gdk-pixbuf-2.0 libudev >= 136 libdrm >= 2.4.17])
PKG_CHECK_MODULES(EGL_CLIENT, [gl cairo glib-2.0 libudev])
PKG_CHECK_MODULES(CAIRO_CLIENT, [cairo-drm glib-2.0])
PKG_CHECK_MODULES(GDK_PIXBUF, [gdk-pixbuf-2.0])
PKG_CHECK_MODULES(POPPLER, [poppler-glib gdk-2.0])

if test $CC = gcc; then
	GCC_CFLAGS="-Wall -g -Wstrict-prototypes -Wmissing-prototypes -fvisibility=hidden"
fi
AC_SUBST(GCC_CFLAGS)

AC_CONFIG_FILES([Makefile wayland-server.pc wayland.pc])
AC_OUTPUT
