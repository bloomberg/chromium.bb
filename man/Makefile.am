man_MANS = weston.1

MAN_SUBSTS = \
	-e 's|__weston_native_backend__|$(WESTON_NATIVE_BACKEND)|g' \
	-e 's|__weston_modules_dir__|$(pkglibdir)|g' \
	-e 's|__version__|$(PACKAGE_VERSION)|g'

SUFFIXES = .1 .man

.man.1:
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@

EXTRA_DIST = weston.man

CLEANFILES = $(man_MANS)
