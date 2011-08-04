AC_PREREQ([2.64])
AC_INIT([wayland],
        [0.1],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=wayland],
        [wayland],
        [http://wayland.freedesktop.org/])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 foreign dist-bzip2])

AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CC

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT

PKG_PROG_PKG_CONFIG()
PKG_CHECK_MODULES(FFI, [libffi])

if test "x$GCC" = "xyes"; then
	GCC_CFLAGS="-Wall -g -Wstrict-prototypes -Wmissing-prototypes -fvisibility=hidden"
fi
AC_SUBST(GCC_CFLAGS)

AC_ARG_ENABLE([scanner],
              [AC_HELP_STRING([--disable-scanner],
                              [Disable compilation of wayland-scannner])],
              [],
              [enable_scanner=yes])

AM_CONDITIONAL(ENABLE_SCANNER, test "x$enable_scanner" = xyes)

EXPAT_LIB=""
AC_ARG_WITH(expat, [  --with-expat=<dir>      Use expat from here],
		   [ expat=$withval
		     CPPFLAGS="$CPPFLAGS -I$withval/include"
		     LDFLAGS="$LDFLAGS -L$withval/lib" ] )
if test "x$enable_scanner" = "xyes"; then
	AC_CHECK_HEADERS(expat.h, [AC_DEFINE(HAVE_EXPAT_H)],
			 [AC_MSG_ERROR([Can't find expat.h. Please install expat.])])
	AC_CHECK_LIB(expat, XML_ParserCreate, [EXPAT_LIBS="-lexpat"],
		     [AC_MSG_ERROR([Can't find expat library. Please install expat.])])
	AC_SUBST(EXPAT_LIBS)
fi

AC_CONFIG_FILES([Makefile
		 wayland-scanner.m4
		 src/Makefile
		 src/wayland-server.pc
		 src/wayland-client.pc])
AC_OUTPUT
