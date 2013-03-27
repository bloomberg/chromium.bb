AM_CFLAGS = \
	$(WARN_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/freedreno \
	$(PTHREADSTUBS_CFLAGS) \
	-I$(top_srcdir)/include/drm

libdrm_freedreno_la_LTLIBRARIES = libdrm_freedreno.la
libdrm_freedreno_ladir = $(libdir)
libdrm_freedreno_la_LDFLAGS = -version-number 1:0:0 -no-undefined
libdrm_freedreno_la_LIBADD = ../libdrm.la @PTHREADSTUBS_LIBS@

libdrm_freedreno_la_SOURCES = \
	freedreno_device.c \
	freedreno_pipe.c \
	freedreno_priv.h \
	freedreno_ringbuffer.c \
	freedreno_bo.c \
	kgsl_drm.h \
	list.h \
	msm_kgsl.h

libdrm_freedrenocommonincludedir = ${includedir}/freedreno
libdrm_freedrenocommoninclude_HEADERS = freedreno_drmif.h freedreno_ringbuffer.h

pkgconfigdir = @pkgconfigdir@
pkgconfig_DATA = libdrm_freedreno.pc
