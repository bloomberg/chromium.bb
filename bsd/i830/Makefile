# $FreeBSD$

KMOD=	i830
NOMAN=	YES
SRCS=	i830_drv.c i830_dma.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_drm_linux.h
CFLAGS+=	${DEBUG_FLAGS} -I. -I..

@:
	ln -sf /sys @

machine:
	ln -sf /sys/i386/include machine

.if ${MACHINE_ARCH} == "i386"
# This line enables linux ioctl handling
# If you want support for this uncomment this line
#I830_OPTS=     "\#define DRM_LINUX" 1
.endif

opt_drm_linux.h:
	touch opt_drm_linux.h
	echo $(I830_OPTS) >> opt_drm_linux.h

.include <bsd.kmod.mk>
