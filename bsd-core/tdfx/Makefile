# $FreeBSD$

KMOD	= tdfx
SRCS    = tdfx_drv.c tdfx_context.c
SRCS   += device_if.h bus_if.h pci_if.h
CFLAGS += ${DEBUG_FLAGS} -I..
KERN    = /usr/src/sys

@:
	ln -sf /sys @

machine:
	ln -sf /sys/i386/include machine

.include "/usr/src/sys/conf/kmod.mk"
