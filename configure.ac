#  Copyright 2005 Adam Jackson.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

AC_PREREQ([2.63])
AC_INIT([libdrm],
        [2.4.34],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=DRI],
        [libdrm])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.10 foreign dist-bzip2])
AM_MAINTAINER_MODE([enable])

# Enable quiet compiles on automake 1.11.
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Check for programs
AC_PROG_CC

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_FUNC_ALLOCA

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])


PKG_CHECK_MODULES(PTHREADSTUBS, pthread-stubs)
AC_SUBST(PTHREADSTUBS_CFLAGS)
AC_SUBST(PTHREADSTUBS_LIBS)

pkgconfigdir=${libdir}/pkgconfig
AC_SUBST(pkgconfigdir)
AC_ARG_ENABLE([udev],
              [AS_HELP_STRING([--enable-udev],
                              [Enable support for using udev instead of mknod (default: disabled)])],
              [UDEV=$enableval], [UDEV=no])

AC_ARG_ENABLE(libkms,
	      AS_HELP_STRING([--disable-libkms],
	      [Disable KMS mm abstraction library (default: auto)]),
	      [LIBKMS=$enableval], [LIBKMS=auto])

AC_ARG_ENABLE(intel,
	      AS_HELP_STRING([--disable-intel],
	      [Enable support for intel's KMS API (default: auto)]),
	      [INTEL=$enableval], [INTEL=auto])

AC_ARG_ENABLE(radeon,
	      AS_HELP_STRING([--disable-radeon],
	      [Enable support for radeon's KMS API (default: auto)]),
	      [RADEON=$enableval], [RADEON=auto])

AC_ARG_ENABLE(nouveau,
	      AS_HELP_STRING([--disable-nouveau],
	      [Enable support for nouveau's KMS API (default: auto)]),
	      [NOUVEAU=$enableval], [NOUVEAU=auto])

AC_ARG_ENABLE(vmwgfx-experimental-api,
	      AS_HELP_STRING([--enable-vmwgfx-experimental-api],
	      [Install vmwgfx's experimental kernel API header (default: disabled)]),
	      [VMWGFX=$enableval], [VMWGFX=no])

AC_ARG_ENABLE(omap-experimental-api,
	      AS_HELP_STRING([--enable-omap-experimental-api],
	      [Enable support for OMAP's experimental API (default: disabled)]),
	      [OMAP=$enableval], [OMAP=no])

AC_ARG_ENABLE(exynos-experimental-api,
	      AS_HELP_STRING([--enable-exynos-experimental-api],
	      [Enable support for EXYNOS's experimental API (default: disabled)]),
	      [EXYNOS=$enableval], [EXYNOS=no])

dnl ===========================================================================
dnl check compiler flags
AC_DEFUN([LIBDRM_CC_TRY_FLAG], [
  AC_MSG_CHECKING([whether $CC supports $1])

  libdrm_save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $1"

  AC_COMPILE_IFELSE([ ], [libdrm_cc_flag=yes], [libdrm_cc_flag=no])
  CFLAGS="$libdrm_save_CFLAGS"

  if test "x$libdrm_cc_flag" = "xyes"; then
    ifelse([$2], , :, [$2])
  else
    ifelse([$3], , :, [$3])
  fi
  AC_MSG_RESULT([$libdrm_cc_flag])
])

dnl We use clock_gettime to check for timeouts in drmWaitVBlank

AC_CHECK_FUNCS([clock_gettime], [CLOCK_LIB=],
               [AC_CHECK_LIB([rt], [clock_gettime], [CLOCK_LIB=-lrt],
                             [AC_MSG_ERROR([Couldn't find clock_gettime])])])
AC_SUBST([CLOCK_LIB])

AC_CHECK_FUNCS([open_memstream], [HAVE_OPEN_MEMSTREAM=yes])

dnl Use lots of warning flags with with gcc and compatible compilers

dnl Note: if you change the following variable, the cache is automatically
dnl skipped and all flags rechecked.  So there's no need to do anything
dnl else.  If for any reason you need to force a recheck, just change
dnl MAYBE_WARN in an ignorable way (like adding whitespace)

MAYBE_WARN="-Wall -Wextra \
-Wsign-compare -Werror-implicit-function-declaration \
-Wpointer-arith -Wwrite-strings -Wstrict-prototypes \
-Wmissing-prototypes -Wmissing-declarations -Wnested-externs \
-Wpacked -Wswitch-enum -Wmissing-format-attribute \
-Wstrict-aliasing=2 -Winit-self -Wunsafe-loop-optimizations \
-Wdeclaration-after-statement -Wold-style-definition \
-Wno-missing-field-initializers -Wno-unused-parameter \
-Wno-attributes -Wno-long-long -Winline"

# invalidate cached value if MAYBE_WARN has changed
if test "x$libdrm_cv_warn_maybe" != "x$MAYBE_WARN"; then
	unset libdrm_cv_warn_cflags
fi
AC_CACHE_CHECK([for supported warning flags], libdrm_cv_warn_cflags, [
	echo
	WARN_CFLAGS=""

	# Some warning options are not supported by all versions of
	# gcc, so test all desired options against the current
	# compiler.
	#
	# Note that there are some order dependencies
	# here. Specifically, an option that disables a warning will
	# have no net effect if a later option then enables that
	# warnings, (perhaps implicitly). So we put some grouped
	# options (-Wall and -Wextra) up front and the -Wno options
	# last.

	for W in $MAYBE_WARN; do
		LIBDRM_CC_TRY_FLAG([$W], [WARN_CFLAGS="$WARN_CFLAGS $W"])
	done

	libdrm_cv_warn_cflags=$WARN_CFLAGS
	libdrm_cv_warn_maybe=$MAYBE_WARN

	AC_MSG_CHECKING([which warning flags were supported])])
WARN_CFLAGS="$libdrm_cv_warn_cflags"

if test "x$UDEV" = xyes; then
	AC_DEFINE(UDEV, 1, [Have UDEV support])
fi

AC_CANONICAL_HOST
if test "x$LIBKMS" = xauto ; then
	case $host_os in
		linux*)		LIBKMS="yes" ;;
		*)		LIBKMS="no" ;;
	esac
fi

AM_CONDITIONAL(HAVE_LIBKMS, [test "x$LIBKMS" = xyes])

AM_CONDITIONAL(HAVE_VMWGFX, [test "x$VMWGFX" = xyes])
if test "x$VMWGFX" = xyes; then
	AC_DEFINE(HAVE_VMWGFX, 1, [Have vmwgfx kernel headers])
fi

AM_CONDITIONAL(HAVE_NOUVEAU, [test "x$NOUVEAU" = xyes])
if test "x$NOUVEAU" = xyes; then
	AC_DEFINE(HAVE_NOUVEAU, 1, [Have nouveau (nvidia) support])
fi

AM_CONDITIONAL(HAVE_OMAP, [test "x$OMAP" = xyes])
if test "x$OMAP" = xyes; then
	AC_DEFINE(HAVE_OMAP, 1, [Have OMAP support])
fi

AM_CONDITIONAL(HAVE_EXYNOS, [test "x$EXYNOS" = xyes])
if test "x$EXYNOS" = xyes; then
	AC_DEFINE(HAVE_EXYNOS, 1, [Have EXYNOS support])
fi

PKG_CHECK_MODULES(CAIRO, cairo, [HAVE_CAIRO=yes], [HAVE_CAIRO=no])
if test "x$HAVE_CAIRO" = xyes; then
	AC_DEFINE(HAVE_CAIRO, 1, [Have cairo support])
fi
AM_CONDITIONAL(HAVE_CAIRO, [test "x$HAVE_CAIRO" = xyes])

# For enumerating devices in test case
PKG_CHECK_MODULES(LIBUDEV, libudev, [HAVE_LIBUDEV=yes], [HAVE_LIBUDEV=no])
if test "x$HAVE_LIBUDEV" = xyes; then
   	AC_DEFINE(HAVE_LIBUDEV, 1, [Have libudev support])
fi
AM_CONDITIONAL(HAVE_LIBUDEV, [test "x$HAVE_LIBUDEV" = xyes])

if test "x$INTEL" != "xno" -o "x$RADEON" != "xno" -o "x$NOUVEAU" != "xno"; then
    # Check for atomic intrinsics
    AC_CACHE_CHECK([for native atomic primitives], drm_cv_atomic_primitives,
    [
	    drm_cv_atomic_primitives="none"

	    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
    int atomic_add(int i) { return __sync_fetch_and_add (&i, 1); }
    int atomic_cmpxchg(int i, int j, int k) { return __sync_val_compare_and_swap (&i, j, k); }
                                            ]],[[]])],
                           [drm_cv_atomic_primitives="Intel"],[])

	    if test "x$drm_cv_atomic_primitives" = "xnone"; then
		    AC_CHECK_HEADER([atomic_ops.h], drm_cv_atomic_primitives="libatomic-ops")
	    fi

	    # atomic functions defined in <atomic.h> & libc on Solaris
	    if test "x$drm_cv_atomic_primitives" = "xnone"; then
		    AC_CHECK_FUNC([atomic_cas_uint],
				  drm_cv_atomic_primitives="Solaris")
	    fi

    ])
    if test "x$drm_cv_atomic_primitives" = xIntel; then
	    AC_DEFINE(HAVE_LIBDRM_ATOMIC_PRIMITIVES, 1,
		      [Enable if your compiler supports the Intel __sync_* atomic primitives])
    fi
    if test "x$drm_cv_atomic_primitives" = "xlibatomic-ops"; then
	    AC_DEFINE(HAVE_LIB_ATOMIC_OPS, 1, [Enable if you have libatomic-ops-dev installed])
    fi

    if test "x$drm_cv_atomic_primitives" = "xnone"; then
           if test "x$INTEL" != "xauto"; then
                   if test "x$INTEL" != "xno"; then
                        AC_MSG_ERROR([libdrm_intel depends upon atomic operations, which were not found for your compiler/cpu. Try compiling with -march=native, or install the libatomics-op-dev package, or, failing both of those, disable support for Intel GPUs by passing --disable-intel to ./configure])
                   fi
           else
                   AC_MSG_WARN([Disabling libdrm_intel. It depends on atomic operations, which were not found for your compiler/cpu. Try compiling with -march=native, or install the libatomics-op-dev package.])
                   INTEL=no
           fi
           if test "x$RADEON" != "xauto"; then
                   if test "x$RADEON" != "xno"; then
                        AC_MSG_ERROR([libdrm_radeon depends upon atomic operations, which were not found for your compiler/cpu. Try compiling with -march=native, or install the libatomics-op-dev package, or, failing both of those, disable support for Radeon support by passing --disable-radeon to ./configure])
                   fi
           else
                   AC_MSG_WARN([Disabling libdrm_radeon. It depends on atomic operations, which were not found for your compiler/cpu. Try compiling with -march=native, or install the libatomics-op-dev package.])
                   RADEON=no
           fi
           if test "x$NOUVEAU" != "xauto"; then
                   if test "x$NOUVEAU" != "xno"; then
                        AC_MSG_ERROR([libdrm_nouveau depends upon atomic operations, which were not found for your compiler/cpu. Try compiling with -march=native, or install the libatomics-op-dev package, or, failing both of those, disable support for NVIDIA GPUs by passing --disable-nouveau to ./configure])
                   fi
           else
                   AC_MSG_WARN([Disabling libdrm_nouveau. It depends on atomic operations, which were not found for your compiler/cpu. Try compiling with -march=native, or install the libatomics-op-dev package.])
                   NOUVEAU=no
           fi
    else
           if test "x$INTEL" != "xno"; then
                   case $host_cpu in
                           i?86|x86_64) INTEL=yes ;;
                           *) INTEL=no ;;
                   esac
           fi
           if test "x$RADEON" != "xno"; then
                   RADEON=yes
           fi
           if test "x$NOUVEAU" != "xno"; then
                   NOUVEAU=yes
           fi
    fi
fi

if test "x$INTEL" != "xno"; then
	PKG_CHECK_MODULES(PCIACCESS, [pciaccess >= 0.10])
fi
AC_SUBST(PCIACCESS_CFLAGS)
AC_SUBST(PCIACCESS_LIBS)

PKG_CHECK_MODULES(VALGRIND, [valgrind], [have_valgrind=yes], [have_valgrind=no])
if test "x$have_valgrind" = "xyes"; then
	AC_DEFINE([HAVE_VALGRIND], 1, [Use valgrind intrinsics to suppress false warnings])
fi

AM_CONDITIONAL(HAVE_INTEL, [test "x$INTEL" != "xno"])
AM_CONDITIONAL(HAVE_RADEON, [test "x$RADEON" != "xno"])
AM_CONDITIONAL(HAVE_NOUVEAU, [test "x$NOUVEAU" != "xno"])
if test "x$RADEON" = xyes; then
	AC_DEFINE(HAVE_RADEON, 1, [Have radeon support])
fi

AC_ARG_WITH([kernel-source],
            [AS_HELP_STRING([--with-kernel-source],
              [specify path to linux kernel source])],
	    [kernel_source="$with_kernel_source"])
AC_SUBST(kernel_source)

AC_SUBST(WARN_CFLAGS)
AC_CONFIG_FILES([
	Makefile
	libkms/Makefile
	libkms/libkms.pc
	intel/Makefile
	intel/libdrm_intel.pc
	radeon/Makefile
	radeon/libdrm_radeon.pc
	nouveau/Makefile
	nouveau/libdrm_nouveau.pc
	omap/Makefile
	omap/libdrm_omap.pc
	exynos/Makefile
	exynos/libdrm_exynos.pc
	tests/Makefile
	tests/modeprint/Makefile
	tests/modetest/Makefile
	tests/kmstest/Makefile
	tests/radeon/Makefile
	tests/vbltest/Makefile
	include/Makefile
	include/drm/Makefile
	libdrm.pc])
AC_OUTPUT

echo ""
echo "$PACKAGE_STRING will be compiled with:"
echo ""
echo "  libkms         $LIBKMS"
echo "  Intel API      $INTEL"
echo "  vmwgfx API     $VMWGFX"
echo "  Radeon API     $RADEON"
echo "  Nouveau API    $NOUVEAU"
echo "  OMAP API       $OMAP"
echo "  EXYNOS API     $EXYNOS"
echo ""
