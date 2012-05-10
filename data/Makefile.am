westondatadir = $(datadir)/weston

dist_westondata_DATA =				\
	wayland.svg				\
	$(wayland_icon_png)			\
	pattern.png				\
	terminal.png				\
	border.png				\
	icon_window.png				\
	sign_close.png				\
	sign_maximize.png			\
	sign_minimize.png

if HAVE_RSVG_CONVERT
wayland_icon_png = wayland.png

wayland.png : $(top_srcdir)/data/wayland.svg
	$(RSVG_CONVERT) -w 128 -h 128 $< -o $@
endif
