AC_PREREQ([2.64])

m4_define([wayland_major_version], [1])
m4_define([wayland_minor_version], [0])
m4_define([wayland_micro_version], [90])
m4_define([wayland_version],
          [wayland_major_version.wayland_minor_version.wayland_micro_version])

AC_INIT([wayland],
        [wayland_version],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=Wayland&component=wayland&version=wayland_version],
        [wayland],
        [http://wayland.freedesktop.org/])

AC_SUBST([WAYLAND_VERSION_MAJOR], [wayland_major_version])
AC_SUBST([WAYLAND_VERSION_MINOR], [wayland_minor_version])
AC_SUBST([WAYLAND_VERSION_MICRO], [wayland_micro_version])
AC_SUBST([WAYLAND_VERSION], [wayland_version])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz])

AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CC

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT

PKG_PROG_PKG_CONFIG()
PKG_CHECK_MODULES(FFI, [libffi])

if test "x$GCC" = "xyes"; then
	GCC_CFLAGS="-Wall -Wextra -Wno-unused-parameter -g -Wstrict-prototypes -Wmissing-prototypes -fvisibility=hidden"
fi
AC_SUBST(GCC_CFLAGS)

AC_CHECK_FUNCS([accept4 mkostemp])

AC_ARG_ENABLE([scanner],
              [AC_HELP_STRING([--disable-scanner],
                              [Disable compilation of wayland-scanner])],
              [],
              [enable_scanner=yes])

AC_ARG_ENABLE([documentation],
	      [AC_HELP_STRING([--disable-documentation],
		              [Disable building the documentation])],
	      [],
	      [enable_documentation=yes])

AM_CONDITIONAL(ENABLE_SCANNER, test "x$enable_scanner" = xyes)

AC_ARG_WITH(icondir, [  --with-icondir=<dir>    Look for cursor icons here],
		     [  ICONDIR=$withval],
		     [  ICONDIR=${datadir}/icons])
AC_SUBST([ICONDIR])

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

AC_PATH_PROG(XSLTPROC, xsltproc)
AM_CONDITIONAL([HAVE_XSLTPROC], [test "x$XSLTPROC" != "x"])

AC_MSG_CHECKING([for docbook manpages stylesheet])
MANPAGES_STYLESHEET=http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl
AC_PATH_PROGS_FEATURE_CHECK([XSLTPROC_TMP], [xsltproc],
			    AS_IF([`"$ac_path_XSLTPROC_TMP" --nonet "$MANPAGES_STYLESHEET" > /dev/null 2>&1`],
				  [HAVE_MANPAGES_STYLESHEET=yes]))
if test "x$HAVE_MANPAGES_STYLESHEET" = "xyes"; then
	AM_CONDITIONAL([HAVE_MANPAGES_STYLESHEET], true)
	AC_SUBST(MANPAGES_STYLESHEET)
	AC_MSG_RESULT([yes])
else
	AM_CONDITIONAL([HAVE_MANPAGES_STYLESHEET], false)
	AC_MSG_RESULT([no])
fi

AM_CONDITIONAL(BUILD_DOCS, [test x$enable_documentation = xyes])
if test "x$enable_documentation" = "xyes"; then
	AC_PATH_PROG(DOXYGEN, doxygen)

	if test "x$DOXYGEN" = "x"; then
		AC_MSG_ERROR([Documentation build requested but doxygen not found. Install doxygen or disable the documentation using --disable-documentation])
	fi

	AC_PATH_PROG(PUBLICAN, publican)

	if test "x$PUBLICAN" != "x"; then
		PUBLICAN_VERSION=`$PUBLICAN -v | cut -d'=' -f2`

		if test [ 1 -eq `echo "${PUBLICAN_VERSION} < 2.8" | bc` ]; then
			AC_MSG_ERROR([Publican version is not supported. Install publican >= 2.8 or disable the documentation using --disable-documentation])
		fi
	fi

	AC_CONFIG_FILES([
	doc/doxygen/wayland.doxygen
	])

fi
AM_CONDITIONAL([HAVE_PUBLICAN], [test "x$PUBLICAN" != "x"])

AC_CONFIG_FILES([Makefile
		 wayland-scanner.m4
		 cursor/Makefile
		 cursor/wayland-cursor.pc
		 cursor/wayland-cursor-uninstalled.pc
		 doc/Makefile
		 doc/publican/Makefile
		 doc/doxygen/Makefile
		 doc/man/Makefile
		 src/Makefile
		 src/wayland-server-uninstalled.pc
		 src/wayland-client-uninstalled.pc
		 src/wayland-server.pc
		 src/wayland-client.pc
		 src/wayland-version.h
		 protocol/Makefile
		 tests/Makefile])
AC_OUTPUT
