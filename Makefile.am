if BUILD_WCAP_TOOLS
wcap_subdir = wcap
endif

if ENABLE_XWAYLAND
xwayland_subdir = xwayland
endif

SUBDIRS =					\
	shared					\
	src					\
	$(xwayland_subdir)			\
	desktop-shell				\
	clients					\
	protocol				\
	tests					\
	$(wcap_subdir)				\
	man

DISTCHECK_CONFIGURE_FLAGS = --disable-setuid-install

EXTRA_DIST = weston.ini.in wayland-scanner.mk

weston.ini : $(srcdir)/weston.ini.in
	$(AM_V_GEN)$(SED) \
		-e 's|@bindir[@]|$(bindir)|g' \
		-e 's|@abs_top_builddir[@]|$(abs_top_builddir)|g' \
		-e 's|@libexecdir[@]|$(libexecdir)|g' \
		$< > $@

all-local : weston.ini

CLEANFILES = weston.ini


westondatadir = $(datadir)/weston
dist_westondata_DATA =				\
	data/wayland.svg			\
	data/wayland.png			\
	data/pattern.png			\
	data/terminal.png			\
	data/border.png				\
	data/icon_window.png			\
	data/sign_close.png			\
	data/sign_maximize.png			\
	data/sign_minimize.png
