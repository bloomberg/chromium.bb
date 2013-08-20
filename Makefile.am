if BUILD_WCAP_TOOLS
wcap_subdir = wcap
endif

SUBDIRS = shared src clients data protocol tests $(wcap_subdir) man

DISTCHECK_CONFIGURE_FLAGS = --disable-setuid-install

EXTRA_DIST = weston.ini wayland-scanner.mk
