if BUILD_WCAP_TOOLS
wcap_subdir = wcap
endif

SUBDIRS = shared src clients data protocol tests $(wcap_subdir)

DIST_SUBDIRS = shared src clients data protocol tests wcap


DISTCHECK_CONFIGURE_FLAGS = --disable-setuid-install
