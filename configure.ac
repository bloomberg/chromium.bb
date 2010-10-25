AC_INIT(wayland, 0.1)
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AC_PROG_CC
AC_PROG_LIBTOOL
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_SILENT_RULES([yes])

PKG_PROG_PKG_CONFIG()
PKG_CHECK_MODULES(FFI, [libffi])

PKG_CHECK_MODULES(COMPOSITOR,
		  [egl glesv2 gdk-pixbuf-2.0 libudev >= 136 libdrm >= 2.4.17] xcb-dri2 xcb-xfixes)
PKG_CHECK_MODULES(CLIENT, [egl gl cairo gdk-pixbuf-2.0 glib-2.0 gobject-2.0 xkbcommon libdrm])
PKG_CHECK_MODULES(POPPLER, [poppler-glib gdk-2.0])
PKG_CHECK_MODULES(CAIRO_GL, [cairo-gl],
		  [have_cairo_gl=yes], [have_cairo_gl=no])
AS_IF([test "x$have_cairo_gl" = "xyes"],
      [AC_DEFINE([HAVE_CAIRO_GL], [1], [Have cairo-gl])])

if test $CC = gcc; then
	GCC_CFLAGS="-Wall -g -Wstrict-prototypes -Wmissing-prototypes -fvisibility=hidden"
fi
AC_SUBST(GCC_CFLAGS)

EXPAT_LIB=""
AC_ARG_WITH(expat, [  --with-expat=<dir>      Use expat from here],
		   [ expat=$withval
		     CPPFLAGS="$CPPFLAGS -I$withval/include"
		     LDFLAGS="$LDFLAGS -L$withval/lib" ] )
AC_CHECK_HEADERS(expat.h, [AC_DEFINE(HAVE_EXPAT_H)], 
		 [AC_MSG_ERROR([Can't find expat.h. Please install expat.])])
AC_CHECK_LIB(expat, XML_ParserCreate, [EXPAT_LIBS="-lexpat"],
	     [AC_MSG_ERROR([Can't find expat library. Please install expat.])])
AC_SUBST(EXPAT_LIBS)

AC_CONFIG_FILES([wayland/wayland-server.pc
		 wayland/wayland-client.pc
		 Makefile
		 wayland/Makefile
		 compositor/Makefile
		 clients/Makefile
		 data/Makefile])
AC_OUTPUT
