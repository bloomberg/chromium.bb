man_MANS = weston.1 weston.ini.5

if ENABLE_DRM_COMPOSITOR
man_MANS += weston-drm.7
endif

MAN_SUBSTS = \
	-e 's|__weston_native_backend__|$(WESTON_NATIVE_BACKEND)|g' \
	-e 's|__weston_modules_dir__|$(pkglibdir)|g' \
	-e 's|__weston_shell_client__|$(WESTON_SHELL_CLIENT)|g' \
	-e 's|__version__|$(PACKAGE_VERSION)|g'

SUFFIXES = .1 .5 .7 .man

.man.1:
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@

.man.5:
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@

.man.7:
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@

EXTRA_DIST = weston.man weston-drm.man weston.ini.man

CLEANFILES = $(man_MANS)
