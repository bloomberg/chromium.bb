m4_define([weston_major_version], [1])
m4_define([weston_minor_version], [4])
m4_define([weston_micro_version], [0])
m4_define([weston_version],
          [weston_major_version.weston_minor_version.weston_micro_version])

AC_PREREQ([2.64])
AC_INIT([weston],
        [weston_version],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=Wayland&component=weston&version=weston_version],
        [weston],
        [http://wayland.freedesktop.org/])

AC_SUBST([WESTON_VERSION_MAJOR], [weston_major_version])
AC_SUBST([WESTON_VERSION_MINOR], [weston_minor_version])
AC_SUBST([WESTON_VERSION_MICRO], [weston_micro_version])
AC_SUBST([WESTON_VERSION], [weston_version])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

AM_INIT_AUTOMAKE([1.11 parallel-tests foreign no-dist-gzip dist-xz color-tests subdir-objects])

AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CC
AC_PROG_SED

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

AC_ARG_VAR([WESTON_NATIVE_BACKEND],
           [Set the native backend to use, if Weston is not running under Wayland nor X11. @<:@default=drm-backend.so@:>@])
AC_ARG_VAR([WESTON_SHELL_CLIENT],
           [Set the default desktop shell client to load if none is specified in weston.ini. @<:@default=weston-desktop-shell@:>@])

PKG_PROG_PKG_CONFIG()

AC_CHECK_FUNC([dlopen], [],
              AC_CHECK_LIB([dl], [dlopen], DLOPEN_LIBS="-ldl"))
AC_SUBST(DLOPEN_LIBS)

AC_CHECK_DECL(SFD_CLOEXEC,[],
	      [AC_MSG_ERROR("SFD_CLOEXEC is needed to compile weston")],
	      [[#include <sys/signalfd.h>]])
AC_CHECK_DECL(TFD_CLOEXEC,[],
	      [AC_MSG_ERROR("TFD_CLOEXEC is needed to compile weston")],
	      [[#include <sys/timerfd.h>]])
AC_CHECK_DECL(CLOCK_MONOTONIC,[],
	      [AC_MSG_ERROR("CLOCK_MONOTONIC is needed to compile weston")],
	      [[#include <time.h>]])
AC_CHECK_HEADERS([execinfo.h])

AC_CHECK_FUNCS([mkostemp strchrnul initgroups posix_fallocate])

COMPOSITOR_MODULES="wayland-server >= 1.3.90 pixman-1"

AC_ARG_ENABLE(egl, [  --disable-egl],,
              enable_egl=yes)
AM_CONDITIONAL(ENABLE_EGL, test x$enable_egl = xyes)
if test x$enable_egl = xyes; then
	AC_DEFINE([ENABLE_EGL], [1], [Build Weston with EGL support])
	PKG_CHECK_MODULES(EGL, [egl >= 7.10 glesv2])
        PKG_CHECK_MODULES([EGL_TESTS], [egl >= 7.10 glesv2 wayland-client wayland-egl])
fi

AC_ARG_ENABLE(xkbcommon,
	      AS_HELP_STRING([--disable-xkbcommon], [Disable libxkbcommon
			      support: This is only useful in environments
			      where you do not have a hardware keyboard. If
			      libxkbcommon support is disabled clients will not
			      be sent a keymap and and must know how to
			      interpret the keycode sent for any key event.]),,
	      enable_xkbcommon=yes)
if test x$enable_xkbcommon = xyes; then
	AC_DEFINE(ENABLE_XKBCOMMON, [1], [Build Weston with libxkbcommon support])
	COMPOSITOR_MODULES="$COMPOSITOR_MODULES xkbcommon >= 0.3.0"
fi

AC_ARG_ENABLE(setuid-install, [  --enable-setuid-install],,
	      enable_setuid_install=yes)
AM_CONDITIONAL(ENABLE_SETUID_INSTALL, test x$enable_setuid_install = xyes)


AC_ARG_ENABLE(xwayland, [  --enable-xwayland],,
	      enable_xwayland=yes)
AC_ARG_ENABLE(xwayland-test, [  --enable-xwayland-test],,
              enable_xwayland_test=yes)
AM_CONDITIONAL(ENABLE_XWAYLAND, test x$enable_xwayland = xyes)
AM_CONDITIONAL(ENABLE_XWAYLAND_TEST, test x$enable_xwayland = xyes -a x$enable_xwayland_test = xyes)
if test x$enable_xwayland = xyes; then
  PKG_CHECK_MODULES([XWAYLAND], xcb xcb-xfixes xcb-composite xcursor cairo-xcb)
  AC_DEFINE([BUILD_XWAYLAND], [1], [Build the X server launcher])

  AC_ARG_WITH(xserver-path, AS_HELP_STRING([--with-xserver-path=PATH],
              [Path to X server]), [XSERVER_PATH="$withval"],
              [XSERVER_PATH="$bindir/Xwayland"])
  AC_SUBST([XSERVER_PATH])
  if test x$enable_xwayland_test = xyes; then
    PKG_CHECK_MODULES([XWAYLAND_TEST], xcb xcb-dri2 libdrm)
  fi
fi

PKG_CHECK_MODULES(LIBDRM, [libdrm],
                  [AC_DEFINE(HAVE_LIBDRM, 1, [Define if libdrm is available]) have_libdrm=yes], have_libdrm=no)

AC_ARG_ENABLE(x11-compositor, [  --enable-x11-compositor],,
	      enable_x11_compositor=yes)
AM_CONDITIONAL(ENABLE_X11_COMPOSITOR, test x$enable_x11_compositor = xyes)
if test x$enable_x11_compositor = xyes; then
  PKG_CHECK_MODULES([XCB], xcb)
  xcb_save_LIBS=$LIBS
  xcb_save_CFLAGS=$CFLAGS
  CFLAGS=$XCB_CFLAGS
  LIBS=$XCB_LIBS
  AC_CHECK_FUNCS([xcb_poll_for_queued_event])
  LIBS=$xcb_save_LIBS
  CFLAGS=$xcb_save_CFLAGS

  X11_COMPOSITOR_MODULES="x11 x11-xcb xcb-shm"

  PKG_CHECK_MODULES(X11_COMPOSITOR_XKB, [xcb-xkb],
		    [have_xcb_xkb="yes"], [have_xcb_xkb="no"])
  if test "x$have_xcb_xkb" = xyes; then
	# Most versions of XCB have totally broken XKB bindings, where the
	# events don't work.  Make sure we can actually use them.
	xcb_xkb_save_CFLAGS=$CFLAGS
	CFLAGS=$X11_COMPOSITOR_XKB_CFLAGS
	AC_CHECK_MEMBER([struct xcb_xkb_state_notify_event_t.xkbType],
			[], [have_xcb_xkb=no], [[#include <xcb/xkb.h>]])
	CFLAGS=$xcb_xkb_save_CFLAGS
  fi
  if test "x$have_xcb_xkb" = xyes; then
	X11_COMPOSITOR_MODULES="$X11_COMPOSITOR_MODULES xcb-xkb"
	AC_DEFINE([HAVE_XCB_XKB], [1], [libxcb supports XKB protocol])
  fi

  PKG_CHECK_MODULES(X11_COMPOSITOR, [$X11_COMPOSITOR_MODULES])
  AC_DEFINE([BUILD_X11_COMPOSITOR], [1], [Build the X11 compositor])
fi


AC_ARG_ENABLE(drm-compositor, [  --enable-drm-compositor],,
	      enable_drm_compositor=yes)
AM_CONDITIONAL(ENABLE_DRM_COMPOSITOR, test x$enable_drm_compositor = xyes)
if test x$enable_drm_compositor = xyes; then
  AC_DEFINE([BUILD_DRM_COMPOSITOR], [1], [Build the DRM compositor])
  PKG_CHECK_MODULES(DRM_COMPOSITOR, [libudev >= 136 libdrm >= 2.4.30 gbm mtdev >= 1.1.0])
fi


AC_ARG_ENABLE(libinput-backend, [  --enable-libinput-backend],,
	      enable_libinput_backend=no)
AM_CONDITIONAL([ENABLE_LIBINPUT_BACKEND], [test x$enable_libinput_backend = xyes])
if test x$enable_libinput_backend = xyes; then
  AC_DEFINE([BUILD_LIBINPUT_BACKEND], [1], [Build the libinput input device backend])
  PKG_CHECK_MODULES(LIBINPUT_BACKEND, [libinput >= 0.1.0])
fi


PKG_CHECK_MODULES(COMPOSITOR, [$COMPOSITOR_MODULES])

AC_ARG_ENABLE(wayland-compositor, [  --enable-wayland-compositor],,
	      enable_wayland_compositor=yes)
AM_CONDITIONAL(ENABLE_WAYLAND_COMPOSITOR,
	       test x$enable_wayland_compositor = xyes -a x$enable_egl = xyes)
if test x$enable_wayland_compositor = xyes -a x$enable_egl = xyes; then
  AC_DEFINE([BUILD_WAYLAND_COMPOSITOR], [1],
	    [Build the Wayland (nested) compositor])
  PKG_CHECK_MODULES(WAYLAND_COMPOSITOR, [wayland-client wayland-egl wayland-cursor])
fi


AC_ARG_ENABLE(headless-compositor, [  --enable-headless-compositor],,
	      enable_headless_compositor=yes)
AM_CONDITIONAL(ENABLE_HEADLESS_COMPOSITOR,
	       test x$enable_headless_compositor = xyes)


AC_ARG_ENABLE(rpi-compositor,
	      AS_HELP_STRING([--disable-rpi-compositor],
	                     [do not build the Raspberry Pi backend]),,
	      enable_rpi_compositor=yes)
AM_CONDITIONAL(ENABLE_RPI_COMPOSITOR, test "x$enable_rpi_compositor" = "xyes")
have_bcm_host="no"
if test "x$enable_rpi_compositor" = "xyes"; then
  AC_DEFINE([BUILD_RPI_COMPOSITOR], [1], [Build the compositor for Raspberry Pi])
  PKG_CHECK_MODULES(RPI_COMPOSITOR, [libudev >= 136 mtdev >= 1.1.0])
  PKG_CHECK_MODULES(RPI_BCM_HOST, [bcm_host],
                    [have_bcm_host="yes"
                     AC_DEFINE([HAVE_BCM_HOST], [1], [have Raspberry Pi BCM headers])],
                    [AC_MSG_WARN([Raspberry Pi BCM host libraries not found, will use stubs instead.])])
fi
AM_CONDITIONAL(INSTALL_RPI_COMPOSITOR, test "x$have_bcm_host" = "xyes")


AC_ARG_ENABLE([fbdev-compositor], [  --enable-fbdev-compositor],,
              enable_fbdev_compositor=yes)
AM_CONDITIONAL([ENABLE_FBDEV_COMPOSITOR],
               [test x$enable_fbdev_compositor = xyes])
AS_IF([test x$enable_fbdev_compositor = xyes], [
  AC_DEFINE([BUILD_FBDEV_COMPOSITOR], [1], [Build the fbdev compositor])
  PKG_CHECK_MODULES([FBDEV_COMPOSITOR], [libudev >= 136 mtdev >= 1.1.0])
])

AC_ARG_ENABLE([rdp-compositor], [  --enable-rdp-compositor],,
              enable_rdp_compositor=no)
AM_CONDITIONAL([ENABLE_RDP_COMPOSITOR],
               [test x$enable_rdp_compositor = xyes])
if test x$enable_rdp_compositor = xyes; then
  AC_DEFINE([BUILD_RDP_COMPOSITOR], [1], [Build the RDP compositor])
  PKG_CHECK_MODULES(RDP_COMPOSITOR, [freerdp >= 1.1.0])

  SAVED_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $RDP_COMPOSITOR_CFLAGS"
  AC_CHECK_HEADERS([freerdp/version.h])
  CPPFLAGS="$SAVED_CPPFLAGS"
fi

AC_ARG_ENABLE([screen-sharing], [--enable-screen-sharing],,
              enable_screen_sharing=no)
AM_CONDITIONAL([ENABLE_SCREEN_SHARING],
               [test x$enable_screen_sharing = xyes])
if test x$enable_screen_sharing = xyes; then
  PKG_CHECK_MODULES(SCREEN_SHARE, [wayland-client])

  if test x$enable_rdp_compositor != xyes; then
    AC_MSG_WARN([The screen-share.so module requires the RDP backend.])
  fi
fi

AC_ARG_WITH(cairo,
	    AS_HELP_STRING([--with-cairo=@<:@image|gl|glesv2@:>@]
			   [Which Cairo renderer to use for the clients]),
			   [],[with_cairo="image"])

if test "x$with_cairo" = "ximage"; then
	cairo_modules="cairo"
else
if test "x$with_cairo" = "xgl"; then
	cairo_modules="cairo-gl"
else
if test "x$with_cairo" = "xglesv2"; then
	cairo_modules="cairo-glesv2"
else
	AC_ERROR([Unknown cairo renderer requested])
fi
fi
fi

# Included for legacy compat
AC_ARG_WITH(cairo-glesv2,
            AS_HELP_STRING([--with-cairo-glesv2],
                           [Use GLESv2 cairo]))
if test "x$with_cairo_glesv2" = "xyes"; then
  cairo_modules="cairo-glesv2"
  with_cairo="glesv2"
fi

if test "x$cairo_modules" = "xcairo-glesv2"; then
AC_DEFINE([USE_CAIRO_GLESV2], [1], [Use the GLESv2 GL cairo backend])
fi

PKG_CHECK_MODULES(PIXMAN, [pixman-1])
PKG_CHECK_MODULES(PNG, [libpng])
PKG_CHECK_MODULES(WEBP, [libwebp], [have_webp=yes], [have_webp=no])
AS_IF([test "x$have_webp" = "xyes"],
      [AC_DEFINE([HAVE_WEBP], [1], [Have webp])])

AC_ARG_ENABLE(vaapi-recorder, [  --enable-vaapi-recorder],,
	      enable_vaapi_recorder=auto)
if test x$enable_vaapi_recorder != xno; then
  PKG_CHECK_MODULES(LIBVA, [libva >= 0.34.0 libva-drm >= 0.34.0],
                    [have_libva=yes], [have_libva=no])
  if test "x$have_libva" = "xno" -a "x$enable_vaapi_recorder" = "xyes"; then
    AC_MSG_ERROR([vaapi-recorder explicitly enabled, but libva couldn't be found])
  fi
  AS_IF([test "x$have_libva" = "xyes"],
        [AC_DEFINE([BUILD_VAAPI_RECORDER], [1], [Build the vaapi recorder])])
fi
AM_CONDITIONAL(ENABLE_VAAPI_RECORDER, test "x$have_libva" = xyes)


AC_CHECK_LIB([jpeg], [jpeg_CreateDecompress], have_jpeglib=yes)
if test x$have_jpeglib = xyes; then
  JPEG_LIBS="-ljpeg"
else
  AC_ERROR([libjpeg not found])
fi
AC_SUBST(JPEG_LIBS)

PKG_CHECK_MODULES(CAIRO, [cairo])

PKG_CHECK_MODULES(TEST_CLIENT, [wayland-client])

AC_ARG_ENABLE(simple-clients,
              AS_HELP_STRING([--disable-simple-clients],
                             [do not build the simple wl_shm clients]),,
              enable_simple_clients=yes)
AM_CONDITIONAL(BUILD_SIMPLE_CLIENTS, test "x$enable_simple_clients" = "xyes")
if test x$enable_simple_clients = xyes; then
  PKG_CHECK_MODULES(SIMPLE_CLIENT, [wayland-client])
fi

AC_ARG_ENABLE(simple-egl-clients,
              AS_HELP_STRING([--disable-simple-egl-clients],
                             [do not build the simple EGL clients]),,
              enable_simple_egl_clients="$enable_egl")
AM_CONDITIONAL(BUILD_SIMPLE_EGL_CLIENTS, test "x$enable_simple_egl_clients" = "xyes")
if test x$enable_simple_egl_clients = xyes; then
  PKG_CHECK_MODULES(SIMPLE_EGL_CLIENT,
                    [egl >= 7.10 glesv2 wayland-client wayland-egl wayland-cursor])
fi

AC_ARG_ENABLE(clients, [  --enable-clients],, enable_clients=yes)
AM_CONDITIONAL(BUILD_CLIENTS, test x$enable_clients = xyes)
if test x$enable_clients = xyes; then
  AC_DEFINE([BUILD_CLIENTS], [1], [Build the Wayland clients])

  PKG_CHECK_MODULES(CLIENT, [wayland-client cairo >= 1.10.0 xkbcommon wayland-cursor])
  PKG_CHECK_MODULES(SERVER, [wayland-server])
  PKG_CHECK_MODULES(WESTON_INFO, [wayland-client])

  # Only check for cairo-egl if a GL or GLES renderer requested
  AS_IF([test "x$cairo_modules" = "xcairo-gl" -o "x$cairo_modules" = "xcairo-glesv2"], [
    PKG_CHECK_MODULES(CAIRO_EGL, [wayland-egl egl >= 7.10 cairo-egl >= 1.11.3 $cairo_modules],
                      [have_cairo_egl=yes], [have_cairo_egl=no])
    AS_IF([test "x$have_cairo_egl" = "xyes"],
	  [AC_DEFINE([HAVE_CAIRO_EGL], [1], [Have cairo-egl])],
	  [AC_ERROR([cairo-egl not used because $CAIRO_EGL_PKG_ERRORS])])],
  [have_cairo_egl=no])

  PKG_CHECK_MODULES(PANGO, [pangocairo], [have_pango=yes], [have_pango=no])
fi

AC_ARG_ENABLE(resize-optimization,
              AS_HELP_STRING([--disable-resize-optimization],
                             [disable resize optimization allocating a big buffer in toytoolkit]),,
              enable_resize_optimization=yes)
AS_IF([test "x$enable_resize_optimization" = "xyes"],
      [AC_DEFINE([USE_RESIZE_POOL], [1], [Use resize memory pool as a performance optimization])])

PKG_CHECK_MODULES(SYSTEMD_LOGIN, [libsystemd-login >= 198],
                  [have_systemd_login=yes], [have_systemd_login=no])
AS_IF([test "x$have_systemd_login" = "xyes"],
      [AC_DEFINE([HAVE_SYSTEMD_LOGIN], [1], [Have systemd-login])])
AM_CONDITIONAL(HAVE_SYSTEMD_LOGIN, test "x$have_systemd_login" = "xyes")

PKG_CHECK_MODULES(SYSTEMD_LOGIN_209, [libsystemd-login >= 209],
                  [have_systemd_login_209=yes], [have_systemd_login_209=no])
AS_IF([test "x$have_systemd_login_209" = "xyes"],
      [AC_DEFINE([HAVE_SYSTEMD_LOGIN_209], [1], [Have systemd-login >= 209])])

AC_ARG_ENABLE(weston-launch, [  --enable-weston-launch],, enable_weston_launch=yes)
AM_CONDITIONAL(BUILD_WESTON_LAUNCH, test x$enable_weston_launch == xyes)
if test x$enable_weston_launch == xyes; then
  AC_CHECK_LIB([pam], [pam_open_session], [have_pam=yes], [have_pam=no])
  if test x$have_pam == xno; then
    AC_ERROR([weston-launch requires pam])
  fi
  PAM_LIBS=-lpam
  AC_SUBST(PAM_LIBS)
fi

if test x$enable_egl = xyes; then
  PKG_CHECK_MODULES(GLU, [glu], [have_glu=yes], [have_glu=no])
fi
AM_CONDITIONAL(HAVE_GLU, test "x$have_glu" = "xyes")

AM_CONDITIONAL(HAVE_PANGO, test "x$have_pango" = "xyes")

AM_CONDITIONAL(HAVE_CAIRO_GLESV2,
	       [test "x$have_cairo_egl" = "xyes" -a "x$cairo_modules" = "xcairo-glesv2" -a "x$enable_egl" = "xyes"])

AM_CONDITIONAL(BUILD_FULL_GL_CLIENTS,
	       test x$cairo_modules = "xcairo-gl" -a "x$have_cairo_egl" = "xyes" -a "x$enable_egl" = "xyes")

AM_CONDITIONAL(BUILD_SUBSURFACES_CLIENT,
	       [test '(' "x$have_cairo_egl" != "xyes" -o "x$cairo_modules" = "xcairo-glesv2" ')' -a "x$enable_simple_egl_clients" = "xyes"])

AM_CONDITIONAL(ENABLE_DESKTOP_SHELL, true)

AC_ARG_ENABLE(fullscreen-shell,
              AS_HELP_STRING([--disable-fullscreen-shell],
                             [do not build fullscreen-shell server plugin]),,
	      enable_fullscreen_shell=yes)
AM_CONDITIONAL(ENABLE_FULLSCREEN_SHELL,
	       test "x$enable_fullscreen_shell" = "xyes")

# CMS modules
AC_ARG_ENABLE(colord,
              AS_HELP_STRING([--disable-colord],
                             [do not build colord CMS support]),,
	      enable_colord=auto)
if test "x$enable_colord" != "xno"; then
	PKG_CHECK_MODULES(COLORD,
			  colord >= 0.1.27,
			  have_colord=yes,
			  have_colord=no)
	if test "x$have_colord" = "xno" -a "x$enable_colord" = "xyes"; then
	  AC_MSG_ERROR([colord support explicitly requested, but colord couldn't be found])
	fi
	if test "x$have_colord" = "xyes"; then
	     enable_colord=yes
	fi
fi
AM_CONDITIONAL(ENABLE_COLORD, test "x$enable_colord" = "xyes")

# dbus support
AC_ARG_ENABLE(dbus,
              AS_HELP_STRING([--disable-dbus],
                             [do not build with dbus support]),,
              enable_dbus=auto)
if test "x$enable_dbus" != "xno"; then
        PKG_CHECK_MODULES(DBUS,
                          dbus-1 >= 1.6,
                          have_dbus=yes,
                          have_dbus=no)
        if test "x$have_dbus" = "xno" -a "x$enable_dbus" = "xyes"; then
                AC_MSG_ERROR([dbus support explicitly requested, but libdbus couldn't be found])
        fi
        if test "x$have_dbus" = "xyes"; then
                enable_dbus=yes
                AC_DEFINE([HAVE_DBUS], [1], [Build with dbus support])
        else
                enable_dbus=no
        fi
fi
AM_CONDITIONAL(ENABLE_DBUS, test "x$enable_dbus" = "xyes")

AC_ARG_ENABLE(wcap-tools, [  --disable-wcap-tools],, enable_wcap_tools=yes)
AM_CONDITIONAL(BUILD_WCAP_TOOLS, test x$enable_wcap_tools = xyes)
if test x$enable_wcap_tools = xyes; then
  AC_DEFINE([BUILD_WCAP_TOOLS], [1], [Build the wcap tools])
  PKG_CHECK_MODULES(WCAP, [cairo])
  WCAP_LIBS="$WCAP_LIBS -lm"
fi

PKG_CHECK_MODULES(SETBACKLIGHT, [libudev libdrm], enable_setbacklight=yes, enable_setbacklight=no)
AM_CONDITIONAL(BUILD_SETBACKLIGHT, test "x$enable_setbacklight" = "xyes")

if test "x$GCC" = "xyes"; then
	GCC_CFLAGS="-Wall -Wextra -Wno-unused-parameter \
		-Wno-missing-field-initializers -g -fvisibility=hidden \
		-Wstrict-prototypes -Wmissing-prototypes -Wsign-compare"
fi
AC_SUBST(GCC_CFLAGS)

AC_ARG_ENABLE(libunwind,
              AS_HELP_STRING([--disable-libunwind],
                             [Disable libunwind usage for backtraces]),,
              enable_libunwind=auto)
AM_CONDITIONAL(HAVE_LIBUNWIND, [test "x$enable_libunwind" = xyes])
if test "x$enable_libunwind" != "xno"; then
        PKG_CHECK_MODULES(LIBUNWIND,
                          libunwind,
                          have_libunwind=yes,
                          have_libunwind=no)
        if test "x$have_libunwind" = "xno" -a "x$enable_libunwind" = "xyes"; then
          AC_MSG_ERROR([libunwind support explicitly requested, but libunwind couldn't be found])
        fi
        if test "x$have_libunwind" = "xyes"; then
             enable_libunwind=yes
             AC_DEFINE(HAVE_LIBUNWIND, 1, [Have libunwind support])
        fi
fi


if test "x$WESTON_NATIVE_BACKEND" = "x"; then
	WESTON_NATIVE_BACKEND="drm-backend.so"
fi
AC_MSG_NOTICE([Weston's native backend: $WESTON_NATIVE_BACKEND])
AC_DEFINE_UNQUOTED([WESTON_NATIVE_BACKEND], ["$WESTON_NATIVE_BACKEND"],
		   [The default backend to load, if not wayland nor x11.])

if test "x$WESTON_SHELL_CLIENT" = "x"; then
	WESTON_SHELL_CLIENT="weston-desktop-shell"
fi
AC_MSG_NOTICE([Weston's default desktop shell client: $WESTON_SHELL_CLIENT])
AC_DEFINE_UNQUOTED([WESTON_SHELL_CLIENT], ["$WESTON_SHELL_CLIENT"],
		   [The default desktop shell client to load.])

AC_ARG_ENABLE(demo-clients-install,
              AS_HELP_STRING([--enable-demo-clients-install],
                             [Install demo clients built with weston]),,
			     enable_demo_clients_install=no)
AM_CONDITIONAL(INSTALL_DEMO_CLIENTS, [test "x$enable_demo_clients_install" = "xyes"])

PKG_CHECK_MODULES(LCMS, lcms2,
                  [have_lcms=yes], [have_lcms=no])
if test "x$have_lcms" = xyes; then
       AC_DEFINE(HAVE_LCMS, 1, [Have lcms support])
fi
AM_CONDITIONAL(HAVE_LCMS, [test "x$have_lcms" = xyes])

AC_PATH_PROG([wayland_scanner], [wayland-scanner])
if test x$wayland_scanner = x; then
	AC_MSG_ERROR([wayland-scanner is needed to compile weston])
fi

PKG_CHECK_MODULES(WAYLAND_SCANNER, wayland-scanner)

AC_CONFIG_FILES([Makefile src/version.h src/weston.pc])

AM_CONDITIONAL([HAVE_GIT_REPO], [test -f $srcdir/.git/logs/HEAD])

AC_OUTPUT

AC_MSG_RESULT([
	Native Backend			${WESTON_NATIVE_BACKEND}
	setuid Install			${enable_setuid_install}

	Cairo Renderer			${with_cairo}
	EGL				${enable_egl}
	libxkbcommon			${enable_xkbcommon}
	xcb_xkb				${have_xcb_xkb}
	XWayland			${enable_xwayland}
	dbus				${enable_dbus}

	Build wcap utility		${enable_wcap_tools}
	Build Fullscreen Shell		${enable_fullscreen_shell}

	weston-launch utility		${enable_weston_launch}
	systemd-login support		${have_systemd_login}

	DRM Compositor			${enable_drm_compositor}
	X11 Compositor			${enable_x11_compositor}
	Wayland Compositor		${enable_wayland_compositor}
	Headless Compositor		${enable_headless_compositor}
	RPI Compositor			${enable_rpi_compositor}
	FBDEV Compositor		${enable_fbdev_compositor}
	RDP Compositor			${enable_rdp_compositor}
	Screen Sharing			${enable_screen_sharing}

	libinput Backend		${enable_libinput_backend}

	Raspberry Pi BCM headers	${have_bcm_host}

	Build Clients			${enable_clients}
	Build EGL Clients		${have_cairo_egl}
	Build Simple Clients		${enable_simple_clients}
	Build Simple EGL Clients	${enable_simple_egl_clients}

	Install Demo Clients		${enable_demo_clients_install}

	Colord Support			${have_colord}
	GLU Support			${have_glu}
	LCMS2 Support			${have_lcms}
	libwebp Support			${have_webp}
	libunwind Support		${have_libunwind}
	VA H.264 encoding Support	${have_libva}
])
