libmandir = $(LIB_MAN_DIR)
libman_PRE = drmAvailable.man \
	drmHandleEvent.man \
	drmModeGetResources.man
libman_DATA = $(libman_PRE:man=@LIB_MAN_SUFFIX@)
EXTRA_DIST = $(libman_PRE)
CLEANFILE = $(libman_DATA)
SUFFIXES = .$(LIB_MAN_SUFFIX) .man

.man.$(LIB_MAN_SUFFIX):
	$(AM_V_GEN)$(SED) $(MAN_SUBSTS) < $< > $@
