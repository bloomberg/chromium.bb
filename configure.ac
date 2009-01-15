AC_INIT(wayland, 0.1)
AC_PROG_CC

PKG_PROG_PKG_CONFIG()
PKG_CHECK_MODULES(FFI, [libffi])
PKG_CHECK_MODULES(LIBDRM, [libdrm])

PKG_CHECK_MODULES(EGL_COMPOSITOR, [eagle libpng cairo gdk-pixbuf-2.0 libudev])
PKG_CHECK_MODULES(GL_COMPOSITOR, [gl x11])
PKG_CHECK_MODULES(CLIENT, [eagle cairo glib-2.0 libudev])

if test $CC = gcc; then
	GCC_CFLAGS="-Wall -g -Wstrict-prototypes -Wmissing-prototypes -fvisibility=hidden"
fi
AC_SUBST(GCC_CFLAGS)

AC_CONFIG_FILES([Makefile wayland-server.pc wayland.pc])
AC_OUTPUT
