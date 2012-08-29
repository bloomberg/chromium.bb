if BUILD_WCAP_TOOLS
wcap_subdir = wcap
endif

SUBDIRS = shared src clients data protocol tests $(wcap_subdir) man

DIST_SUBDIRS = shared src clients data protocol tests wcap man


DISTCHECK_CONFIGURE_FLAGS = --disable-setuid-install
