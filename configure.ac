AC_PREREQ([2.64])

m4_define([wayland_protocols_major_version], [1])
m4_define([wayland_protocols_minor_version], [0])
m4_define([wayland_protocols_version],
          [wayland_protocols_major_version.wayland_protocols_minor_version])

AC_INIT([wayland-protocols],
        [wayland_protocols_version],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=Wayland&component=wayland&version=unspecified],
        [wayland-protocols],
        [http://wayland.freedesktop.org/])

AC_SUBST([WAYLAND_PROTOCOLS_VERSION], [wayland_protocols_version])

AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz])

AM_SILENT_RULES([yes])

PKG_NOARCH_INSTALLDIR

AC_CONFIG_FILES([
	Makefile
	wayland-protocols.pc
	])
AC_OUTPUT

AC_MSG_RESULT([
	Version			${WAYLAND_PROTOCOLS_VERSION}
	Prefix			${prefix}
	])
