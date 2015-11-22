unstable_protocols =								\
	unstable/pointer-gestures/pointer-gestures-unstable-v1.xml		\
	unstable/fullscreen-shell/fullscreen-shell-unstable-v1.xml		\
	unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml			\
	unstable/text-input/text-input-unstable-v1.xml				\
	unstable/input-method/input-method-unstable-v1.xml			\
	unstable/xdg-shell/xdg-shell-unstable-v5.xml				\
	$(NULL)

nobase_dist_pkgdata_DATA =							\
	$(unstable_protocols)							\
	$(NULL)

dist_noinst_DATA =								\
	$(sort $(foreach p,$(unstable_protocols),$(dir $p)README))		\
	$(NULL)

noarch_pkgconfig_DATA = wayland-protocols.pc
