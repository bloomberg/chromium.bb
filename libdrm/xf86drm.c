/* xf86drm.c -- User-level interface to DRM device
 * Created: Tue Jan  5 08:16:21 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * Authors: Rickard E. (Rik) Faith <faith@valinux.com>
 *	    Kevin E. Martin <martin@valinux.com>
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/xf86drm.c,v 1.17 2000/09/24 13:51:32 alanh Exp $
 * 
 */

#ifdef XFree86Server
# include "xf86.h"
# include "xf86_OSproc.h"
# include "xf86_ansic.h"
# include "xf86Priv.h"
# define _DRM_MALLOC xalloc
# define _DRM_FREE   xfree
# ifndef XFree86LOADER
#  include <sys/stat.h>
#  include <sys/mman.h>
# endif
#else
# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
# include <ctype.h>
# include <fcntl.h>
# include <errno.h>
# include <signal.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/ioctl.h>
# include <sys/mman.h>
# include <sys/time.h>
# ifdef DRM_USE_MALLOC
#  define _DRM_MALLOC malloc
#  define _DRM_FREE   free
extern int xf86InstallSIGIOHandler(int fd, void (*f)(int, void *), void *);
extern int xf86RemoveSIGIOHandler(int fd);
# else
#  include <Xlibint.h>
#  define _DRM_MALLOC Xmalloc
#  define _DRM_FREE   Xfree
# endif
#endif

#ifdef __alpha__
extern unsigned long _bus_base(void);
#define BUS_BASE _bus_base()
#endif

/* Not all systems have MAP_FAILED defined */
#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

#include "xf86drm.h"
#include "drm.h"

#define DRM_FIXED_DEVICE_MAJOR 145

#ifdef __linux__
#include <sys/sysmacros.h>	/* for makedev() */
#endif

#ifndef makedev
				/* This definition needs to be changed on
                                   some systems if dev_t is a structure.
                                   If there is a header file we can get it
                                   from, there would be best. */
#define makedev(x,y)    ((dev_t)(((x) << 8) | (y)))
#endif

static void *drmHashTable = NULL; /* Context switch callbacks */

typedef struct drmHashEntry {
    int      fd;
    void     (*f)(int, void *, void *);
    void     *tagTable;
} drmHashEntry;

void *drmMalloc(int size)
{
    void *pt;
    if ((pt = _DRM_MALLOC(size))) memset(pt, 0, size);
    return pt;
}

void drmFree(void *pt)
{
    if (pt) _DRM_FREE(pt);
}

/* drmStrdup can't use strdup(3), since it doesn't call _DRM_MALLOC... */
static char *drmStrdup(const char *s)
{
    char *retval = NULL;
    
    if (s) {
	retval = _DRM_MALLOC(strlen(s)+1);
	strcpy(retval, s);
    }
    return retval;
}


static unsigned long drmGetKeyFromFd(int fd)
{
#ifdef XFree86LOADER
    struct xf86stat st;
#else
    struct stat     st;
#endif

    st.st_rdev = 0;
    fstat(fd, &st);
    return st.st_rdev;
}

static drmHashEntry *drmGetEntry(int fd)
{
    unsigned long key = drmGetKeyFromFd(fd);
    void          *value;
    drmHashEntry  *entry;

    if (!drmHashTable) drmHashTable = drmHashCreate();

    if (drmHashLookup(drmHashTable, key, &value)) {
	entry           = drmMalloc(sizeof(*entry));
	entry->fd       = fd;
	entry->f        = NULL;
	entry->tagTable = drmHashCreate();
	drmHashInsert(drmHashTable, key, entry);
    } else {
	entry = value;
    }
    return entry;
}

/* drm_open is used to open the /dev/dri device */

static int drm_open(const char *file)
{
    int fd = open(file, O_RDWR, 0);

    if (fd >= 0) return fd;
    return -errno;
}

static int drmOpenDevice(const char *path, long dev,
			 mode_t mode, uid_t user, gid_t group)
{
#ifdef XFree86LOADER
    struct xf86stat st;
#else
    struct stat     st;
#endif

				/* Fiddle mode to remove execute bits */
    mode &= ~(S_IXUSR|S_IXGRP|S_IXOTH);

    if (!stat(path, &st) && st.st_rdev == dev) {
	if (!geteuid()) {
	    chown(path, user, group);
	    chmod(path, mode);
	}
	return drm_open(path);
    }

    if (geteuid()) return DRM_ERR_NOT_ROOT;
    remove(path);
    if (mknod(path, S_IFCHR, dev)) {
	remove(path);
	return DRM_ERR_NOT_ROOT;
    }
    chown(path, user, group);
    chmod(path, mode);
    return drm_open(path);
}

/* drmAvailable looks for /proc/dri, and returns 1 if it is present.  On
   OSs that do not have a Linux-like /proc, this information will not be
   available, and we'll have to create a device and check if the driver is
   loaded that way. */

int drmAvailable(void)
{
    char          dev_name[64];
    drmVersionPtr version;
    int           retval = 0;
    int           fd;
    
    if (!access("/proc/dri/0", R_OK)) return 1;

    sprintf(dev_name, "/dev/dri-temp-%d", getpid());

    remove(dev_name);
    if ((fd = drmOpenDevice(dev_name, makedev(DRM_FIXED_DEVICE_MAJOR, 0),
			    S_IRUSR, geteuid(), getegid())) >= 0) {
				/* Read version to make sure this is
                                   actually a DRI device. */
	if ((version = drmGetVersion(fd))) {
	    retval = 1;
	    drmFreeVersion(version);
	}
	close(fd);
    }
    remove(dev_name);

    return retval;
}

static int drmOpenByBusid(const char *busid)
{
    int    i;
    char   dev_name[64];
    char   *buf;
    int    fd;

    for (i = 0; i < 8; i++) {
	sprintf(dev_name, "/dev/dri/card%d", i);
	if ((fd = drm_open(dev_name)) >= 0) {
	    buf = drmGetBusid(fd);
	    if (buf && !strcmp(buf, busid)) {
	      drmFreeBusid(buf);
	      return fd;
	    }
	    if (buf) drmFreeBusid(buf);
	    close(fd);
	}
    }
    return -1;
}

static int drmOpenByName(const char *name)
{
    int    i;
    char   proc_name[64];
    char   dev_name[64];
    char   buf[512];
    mode_t mode   = DRM_DEV_MODE;
    mode_t dirmode;
    gid_t  group  = DRM_DEV_GID;
    uid_t  user   = DRM_DEV_UID;
    int    fd;
    char   *pt;
    char   *driver = NULL;
    char   *devstring;
    long   dev     = 0;
    int    retcode;

#if defined(XFree86Server)
    mode  = xf86ConfigDRI.mode ? xf86ConfigDRI.mode : DRM_DEV_MODE;
    group = (xf86ConfigDRI.group >= 0) ? xf86ConfigDRI.group : DRM_DEV_GID;
#endif

#if defined(XFree86Server)
    if (!drmAvailable()) {
        /* try to load the kernel module now */
        if (!xf86LoadKernelModule(name)) {
            ErrorF("[drm] failed to load kernel module \"%s\"\n",
		   name);
            return -1;
        }
    }
#else
    if (!drmAvailable())
       return -1;
#endif

    if (!geteuid()) {
	dirmode = mode;
	if (dirmode & S_IRUSR) dirmode |= S_IXUSR;
	if (dirmode & S_IRGRP) dirmode |= S_IXGRP;
	if (dirmode & S_IROTH) dirmode |= S_IXOTH;
	dirmode &= ~(S_IWGRP | S_IWOTH);
	mkdir("/dev/dri", 0);
	chown("/dev/dri", user, group);
	chmod("/dev/dri", dirmode);
    }

    for (i = 0; i < 8; i++) {
	sprintf(proc_name, "/proc/dri/%d/name", i);
	sprintf(dev_name, "/dev/dri/card%d", i);
	if ((fd = open(proc_name, 0, 0)) >= 0) {
	    retcode = read(fd, buf, sizeof(buf)-1);
	    close(fd);
	    if (retcode) {
		buf[retcode-1] = '\0';
		for (driver = pt = buf; *pt && *pt != ' '; ++pt)
		    ;
		if (*pt) {	/* Device is next */
		    *pt = '\0';
		    if (!strcmp(driver, name)) { /* Match */
			for (devstring = ++pt; *pt && *pt != ' '; ++pt)
			    ;
			if (*pt) { /* Found busid */
			  return drmOpenByBusid(++pt);
			} else {	/* No busid */
			  dev = strtol(devstring, NULL, 0);
			  return drmOpenDevice(dev_name, dev,
					       mode, user, group);
			}
		    }
		}
	    }
	} else {
	    drmVersionPtr version;
				/* /proc/dri not available, possibly
                                   because we aren't on a Linux system.
                                   So, try to create the next device and
                                   see if it's active. */
	    dev = makedev(DRM_FIXED_DEVICE_MAJOR, i);
	    if ((fd = drmOpenDevice(dev_name, dev, mode, user, group))) {
		if ((version = drmGetVersion(fd))) {
		    if (!strcmp(version->name, name)) {
			drmFreeVersion(version);
			return fd;
		    }
		    drmFreeVersion(version);
		}
	    }
	    remove(dev_name);
	}
    }
    return -1;
}

/* drmOpen looks up the specified name and busid, and opens the device
   found.  The entry in /dev/dri is created if necessary (and if root).
   A file descriptor is returned.  On error, the return value is
   negative. */

int drmOpen(const char *name, const char *busid)
{

    if (busid) return drmOpenByBusid(busid);
    return drmOpenByName(name);
}

void drmFreeVersion(drmVersionPtr v)
{
    if (!v) return;
    if (v->name) drmFree(v->name);
    if (v->date) drmFree(v->date);
    if (v->desc) drmFree(v->desc);
    drmFree(v);
}

static void drmFreeKernelVersion(drm_version_t *v)
{
    if (!v) return;
    if (v->name) drmFree(v->name);
    if (v->date) drmFree(v->date);
    if (v->desc) drmFree(v->desc);
    drmFree(v);
}

static void drmCopyVersion(drmVersionPtr d, const drm_version_t *s)
{
    d->version_major      = s->version_major;
    d->version_minor      = s->version_minor;
    d->version_patchlevel = s->version_patchlevel;
    d->name_len           = s->name_len;
    d->name               = drmStrdup(s->name);
    d->date_len           = s->date_len;
    d->date               = drmStrdup(s->date);
    d->desc_len           = s->desc_len;
    d->desc               = drmStrdup(s->desc);
}

/* drmVersion obtains the version information via an ioctl.  Similar
 * information is available via /proc/dri. */

drmVersionPtr drmGetVersion(int fd)
{
    drmVersionPtr retval;
    drm_version_t *version = drmMalloc(sizeof(*version));

				/* First, get the lengths */
    version->name_len    = 0;
    version->name        = NULL;
    version->date_len    = 0;
    version->date        = NULL;
    version->desc_len    = 0;
    version->desc        = NULL;
    
    if (ioctl(fd, DRM_IOCTL_VERSION, version)) {
	drmFreeKernelVersion(version);
	return NULL;
    }

				/* Now, allocate space and get the data */
    if (version->name_len)
	version->name    = drmMalloc(version->name_len + 1);
    if (version->date_len)
	version->date    = drmMalloc(version->date_len + 1);
    if (version->desc_len)
	version->desc    = drmMalloc(version->desc_len + 1);
    
    if (ioctl(fd, DRM_IOCTL_VERSION, version)) {
	drmFreeKernelVersion(version);
	return NULL;
    }

				/* The results might not be null-terminated
                                   strings, so terminate them. */

    if (version->name_len) version->name[version->name_len] = '\0';
    if (version->date_len) version->date[version->date_len] = '\0';
    if (version->desc_len) version->desc[version->desc_len] = '\0';

				/* Now, copy it all back into the
                                   client-visible data structure... */
    retval = drmMalloc(sizeof(*retval));
    drmCopyVersion(retval, version);
    drmFreeKernelVersion(version);
    return retval;
}

void drmFreeBusid(const char *busid)
{
    drmFree((void *)busid);
}

char *drmGetBusid(int fd)
{
    drm_unique_t u;

    u.unique_len = 0;
    u.unique     = NULL;

    if (ioctl(fd, DRM_IOCTL_GET_UNIQUE, &u)) return NULL;
    u.unique = drmMalloc(u.unique_len + 1);
    if (ioctl(fd, DRM_IOCTL_GET_UNIQUE, &u)) return NULL;
    u.unique[u.unique_len] = '\0';
    return u.unique;
}

int drmSetBusid(int fd, const char *busid)
{
    drm_unique_t u;

    u.unique     = (char *)busid;
    u.unique_len = strlen(busid);

    if (ioctl(fd, DRM_IOCTL_SET_UNIQUE, &u)) return -errno;
    return 0;
}

int drmGetMagic(int fd, drmMagicPtr magic)
{
    drm_auth_t auth;

    *magic = 0;
    if (ioctl(fd, DRM_IOCTL_GET_MAGIC, &auth)) return -errno;
    *magic = auth.magic;
    return 0;
}

int drmAuthMagic(int fd, drmMagic magic)
{
    drm_auth_t auth;

    auth.magic = magic;
    if (ioctl(fd, DRM_IOCTL_AUTH_MAGIC, &auth)) return -errno;
    return 0;
}

int drmAddMap(int fd,
	      drmHandle offset,
	      drmSize size,
	      drmMapType type,
	      drmMapFlags flags,
	      drmHandlePtr handle)
{
    drm_map_t map;

    map.offset  = offset;
#ifdef __alpha__
    /* Make sure we add the bus_base to all but shm */
    if (type != DRM_SHM) 
	map.offset += BUS_BASE;
#endif
    map.size    = size;
    map.handle  = 0;
    map.type    = type;
    map.flags   = flags;
    if (ioctl(fd, DRM_IOCTL_ADD_MAP, &map)) return -errno;
    if (handle) *handle = (drmHandle)map.handle;
    return 0;
}

int drmAddBufs(int fd, int count, int size, drmBufDescFlags flags,
	       int agp_offset)
{
    drm_buf_desc_t request;
    
    request.count     = count;
    request.size      = size;
    request.low_mark  = 0;
    request.high_mark = 0;
    request.flags     = flags;
    request.agp_start = agp_offset;
   
    if (ioctl(fd, DRM_IOCTL_ADD_BUFS, &request)) return -errno;
    return request.count;
}

int drmMarkBufs(int fd, double low, double high)
{
    drm_buf_info_t info;
    int            i;

    info.count = 0;
    info.list  = NULL;

    if (ioctl(fd, DRM_IOCTL_INFO_BUFS, &info)) return -EINVAL;

    if (!info.count) return -EINVAL;
    
    if (!(info.list = drmMalloc(info.count * sizeof(*info.list))))
	return -ENOMEM;
	
    if (ioctl(fd, DRM_IOCTL_INFO_BUFS, &info)) {
	int retval = -errno;
	drmFree(info.list);
	return retval;
    }
    
    for (i = 0; i < info.count; i++) {
	info.list[i].low_mark  = low  * info.list[i].count;
	info.list[i].high_mark = high * info.list[i].count;
	if (ioctl(fd, DRM_IOCTL_MARK_BUFS, &info.list[i])) {
	    int retval = -errno;
	    drmFree(info.list);
	    return retval;
	}
    }
    drmFree(info.list);
    
    return 0;
}

int drmFreeBufs(int fd, int count, int *list)
{
    drm_buf_free_t request;

    request.count = count;
    request.list  = list;
    if (ioctl(fd, DRM_IOCTL_FREE_BUFS, &request)) return -errno;
    return 0;
}

int drmClose(int fd)
{
    unsigned long key    = drmGetKeyFromFd(fd);
    drmHashEntry  *entry = drmGetEntry(fd);

    drmHashDestroy(entry->tagTable);
    entry->fd       = 0;
    entry->f        = NULL;
    entry->tagTable = NULL;

    drmHashDelete(drmHashTable, key);
    drmFree(entry);

    return close(fd);
}

int drmMap(int fd,
	   drmHandle handle,
	   drmSize size,
	   drmAddressPtr address)
{
    static unsigned long pagesize_mask = 0;

    if (fd < 0) return -EINVAL;

    if (!pagesize_mask)
	pagesize_mask = getpagesize() - 1;

    size = (size + pagesize_mask) & ~pagesize_mask;

    *address = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, handle);
    if (*address == MAP_FAILED) return -errno;
    return 0;
}

int drmUnmap(drmAddress address, drmSize size)
{
    return munmap(address, size);
}

drmBufInfoPtr drmGetBufInfo(int fd)
{
    drm_buf_info_t info;
    drmBufInfoPtr  retval;
    int            i;

    info.count = 0;
    info.list  = NULL;

    if (ioctl(fd, DRM_IOCTL_INFO_BUFS, &info)) return NULL;

    if (info.count) {
	if (!(info.list = drmMalloc(info.count * sizeof(*info.list))))
	    return NULL;
	
	if (ioctl(fd, DRM_IOCTL_INFO_BUFS, &info)) {
	    drmFree(info.list);
	    return NULL;
	}
				/* Now, copy it all back into the
                                   client-visible data structure... */
	retval = drmMalloc(sizeof(*retval));
	retval->count = info.count;
	retval->list  = drmMalloc(info.count * sizeof(*retval->list));
	for (i = 0; i < info.count; i++) {
	    retval->list[i].count     = info.list[i].count;
	    retval->list[i].size      = info.list[i].size;
	    retval->list[i].low_mark  = info.list[i].low_mark;
	    retval->list[i].high_mark = info.list[i].high_mark;
	}
	drmFree(info.list);
	return retval;
    }
    return NULL;
}

drmBufMapPtr drmMapBufs(int fd)
{
    drm_buf_map_t bufs;
    drmBufMapPtr  retval;
    int           i;
    
    bufs.count = 0;
    bufs.list  = NULL;
    if (ioctl(fd, DRM_IOCTL_MAP_BUFS, &bufs)) return NULL;

    if (bufs.count) {
	if (!(bufs.list = drmMalloc(bufs.count * sizeof(*bufs.list))))
	    return NULL;

	if (ioctl(fd, DRM_IOCTL_MAP_BUFS, &bufs)) {
	    drmFree(bufs.list);
	    return NULL;
	}
				/* Now, copy it all back into the
                                   client-visible data structure... */
	retval = drmMalloc(sizeof(*retval));
	retval->count = bufs.count;
	retval->list  = drmMalloc(bufs.count * sizeof(*retval->list));
	for (i = 0; i < bufs.count; i++) {
	    retval->list[i].idx     = bufs.list[i].idx;
	    retval->list[i].total   = bufs.list[i].total;
	    retval->list[i].used    = 0;
	    retval->list[i].address = bufs.list[i].address;
	}
	return retval;
    }
    return NULL;
}

int drmUnmapBufs(drmBufMapPtr bufs)
{
    int i;
    
    for (i = 0; i < bufs->count; i++) {
	munmap(bufs->list[i].address, bufs->list[i].total);
    }
    return 0;
}

int drmDMA(int fd, drmDMAReqPtr request)
{
    drm_dma_t dma;

				/* Copy to hidden structure */
    dma.context         = request->context;
    dma.send_count      = request->send_count;
    dma.send_indices    = request->send_list;
    dma.send_sizes      = request->send_sizes;
    dma.flags           = request->flags;
    dma.request_count   = request->request_count;
    dma.request_size    = request->request_size;
    dma.request_indices = request->request_list;
    dma.request_sizes   = request->request_sizes;
    if (ioctl(fd, DRM_IOCTL_DMA, &dma)) return -errno;
    request->granted_count = dma.granted_count;
    
    return 0;
}

int drmGetLock(int fd, drmContext context, drmLockFlags flags)
{
    drm_lock_t lock;

    lock.context = context;
    lock.flags   = 0;
    if (flags & DRM_LOCK_READY)      lock.flags |= _DRM_LOCK_READY;
    if (flags & DRM_LOCK_QUIESCENT)  lock.flags |= _DRM_LOCK_QUIESCENT;
    if (flags & DRM_LOCK_FLUSH)      lock.flags |= _DRM_LOCK_FLUSH;
    if (flags & DRM_LOCK_FLUSH_ALL)  lock.flags |= _DRM_LOCK_FLUSH_ALL;
    if (flags & DRM_HALT_ALL_QUEUES) lock.flags |= _DRM_HALT_ALL_QUEUES;
    if (flags & DRM_HALT_CUR_QUEUES) lock.flags |= _DRM_HALT_CUR_QUEUES;
    
    while (ioctl(fd, DRM_IOCTL_LOCK, &lock))
	;
    return 0;
}

int drmUnlock(int fd, drmContext context)
{
    drm_lock_t lock;

    lock.context = context;
    lock.flags   = 0;
    return ioctl(fd, DRM_IOCTL_UNLOCK, &lock);
}

drmContextPtr drmGetReservedContextList(int fd, int *count)
{
    drm_ctx_res_t res;
    drm_ctx_t     *list;
    drmContextPtr retval;
    int           i;

    res.count    = 0;
    res.contexts = NULL;
    if (ioctl(fd, DRM_IOCTL_RES_CTX, &res)) return NULL;

    if (!res.count) return NULL;

    if (!(list   = drmMalloc(res.count * sizeof(*list)))) return NULL;
    if (!(retval = drmMalloc(res.count * sizeof(*retval)))) {
	drmFree(list);
	return NULL;
    }

    res.contexts = list;
    if (ioctl(fd, DRM_IOCTL_RES_CTX, &res)) return NULL;

    for (i = 0; i < res.count; i++) retval[i] = list[i].handle;
    drmFree(list);

    *count = res.count;
    return retval;
}

void drmFreeReservedContextList(drmContextPtr pt)
{
    drmFree(pt);
}

int drmCreateContext(int fd, drmContextPtr handle)
{
    drm_ctx_t ctx;

    ctx.flags = 0;	/* Modified with functions below */
    if (ioctl(fd, DRM_IOCTL_ADD_CTX, &ctx)) return -errno;
    *handle = ctx.handle;
    return 0;
}

int drmSwitchToContext(int fd, drmContext context)
{
    drm_ctx_t ctx;

    ctx.handle = context;
    if (ioctl(fd, DRM_IOCTL_SWITCH_CTX, &ctx)) return -errno;
    return 0;
}

int drmSetContextFlags(int fd, drmContext context, drmContextFlags flags)
{
    drm_ctx_t ctx;

				/* Context preserving means that no context
                                   switched are done between DMA buffers
                                   from one context and the next.  This is
                                   suitable for use in the X server (which
                                   promises to maintain hardware context,
                                   or in the client-side library when
                                   buffers are swapped on behalf of two
                                   threads. */
    ctx.handle = context;
    ctx.flags  = 0;
    if (flags & DRM_CONTEXT_PRESERVED) ctx.flags |= _DRM_CONTEXT_PRESERVED;
    if (flags & DRM_CONTEXT_2DONLY)    ctx.flags |= _DRM_CONTEXT_2DONLY;
    if (ioctl(fd, DRM_IOCTL_MOD_CTX, &ctx)) return -errno;
    return 0;
}

int drmGetContextFlags(int fd, drmContext context, drmContextFlagsPtr flags)
{
    drm_ctx_t ctx;

    ctx.handle = context;
    if (ioctl(fd, DRM_IOCTL_GET_CTX, &ctx)) return -errno;
    *flags = 0;
    if (ctx.flags & _DRM_CONTEXT_PRESERVED) *flags |= DRM_CONTEXT_PRESERVED;
    if (ctx.flags & _DRM_CONTEXT_2DONLY)    *flags |= DRM_CONTEXT_2DONLY;
    return 0;
}
    
int drmDestroyContext(int fd, drmContext handle)
{
    drm_ctx_t ctx;
    ctx.handle = handle;
    if (ioctl(fd, DRM_IOCTL_RM_CTX, &ctx)) return -errno;
    return 0;
}

int drmCreateDrawable(int fd, drmDrawablePtr handle)
{
    drm_draw_t draw;
    if (ioctl(fd, DRM_IOCTL_ADD_DRAW, &draw)) return -errno;
    *handle = draw.handle;
    return 0;
}

int drmDestroyDrawable(int fd, drmDrawable handle)
{
    drm_draw_t draw;
    draw.handle = handle;
    if (ioctl(fd, DRM_IOCTL_RM_DRAW, &draw)) return -errno;
    return 0;
}

int drmAgpAcquire(int fd)
{
    if (ioctl(fd, DRM_IOCTL_AGP_ACQUIRE, NULL)) return -errno;
    return 0;
}

int drmAgpRelease(int fd)
{
    if (ioctl(fd, DRM_IOCTL_AGP_RELEASE, NULL)) return -errno;
    return 0;
}

int drmAgpEnable(int fd, unsigned long mode)
{
    drm_agp_mode_t m;

    m.mode = mode;
    if (ioctl(fd, DRM_IOCTL_AGP_ENABLE, &m)) return -errno;
    return 0;
}

int drmAgpAlloc(int fd, unsigned long size, unsigned long type,
		unsigned long *address, unsigned long *handle)
{
    drm_agp_buffer_t b;
    *handle = 0;
    b.size   = size;
    b.handle = 0;
    b.type   = type;
    if (ioctl(fd, DRM_IOCTL_AGP_ALLOC, &b)) return -errno;
    if (address != 0UL) *address = b.physical;
    *handle = b.handle;
    return 0;
}

int drmAgpFree(int fd, unsigned long handle)
{
    drm_agp_buffer_t b;

    b.size   = 0;
    b.handle = handle;
    if (ioctl(fd, DRM_IOCTL_AGP_FREE, &b)) return -errno;
    return 0;
}

int drmAgpBind(int fd, unsigned long handle, unsigned long offset)
{
    drm_agp_binding_t b;

    b.handle = handle;
    b.offset = offset;
    if (ioctl(fd, DRM_IOCTL_AGP_BIND, &b)) return -errno;
    return 0;
}

int drmAgpUnbind(int fd, unsigned long handle)
{
    drm_agp_binding_t b;

    b.handle = handle;
    b.offset = 0;
    if (ioctl(fd, DRM_IOCTL_AGP_UNBIND, &b)) return -errno;
    return 0;
}

int drmAgpVersionMajor(int fd)
{
    drm_agp_info_t i;

    if (ioctl(fd, DRM_IOCTL_AGP_INFO, &i)) return -errno;
    return i.agp_version_major;
}

int drmAgpVersionMinor(int fd)
{
    drm_agp_info_t i;

    if (ioctl(fd, DRM_IOCTL_AGP_INFO, &i)) return -errno;
    return i.agp_version_minor;
}

unsigned long drmAgpGetMode(int fd)
{
    drm_agp_info_t i;

    if (ioctl(fd, DRM_IOCTL_AGP_INFO, &i)) return 0;
    return i.mode;
}

unsigned long drmAgpBase(int fd)
{
    drm_agp_info_t i;

    if (ioctl(fd, DRM_IOCTL_AGP_INFO, &i)) return 0;
    return i.aperture_base;
}

unsigned long drmAgpSize(int fd)
{
    drm_agp_info_t i;

    if (ioctl(fd, DRM_IOCTL_AGP_INFO, &i)) return 0;
    return i.aperture_size;
}

unsigned long drmAgpMemoryUsed(int fd)
{
    drm_agp_info_t i;

    if (ioctl(fd, DRM_IOCTL_AGP_INFO, &i)) return 0;
    return i.memory_used;
}

unsigned long drmAgpMemoryAvail(int fd)
{
    drm_agp_info_t i;

    if (ioctl(fd, DRM_IOCTL_AGP_INFO, &i)) return 0;
    return i.memory_allowed;
}

unsigned int drmAgpVendorId(int fd)
{
    drm_agp_info_t i;

    if (ioctl(fd, DRM_IOCTL_AGP_INFO, &i)) return 0;
    return i.id_vendor;
}

unsigned int drmAgpDeviceId(int fd)
{
    drm_agp_info_t i;

    if (ioctl(fd, DRM_IOCTL_AGP_INFO, &i)) return 0;
    return i.id_device;
}

int drmError(int err, const char *label)
{
    switch (err) {
    case DRM_ERR_NO_DEVICE: fprintf(stderr, "%s: no device\n", label);   break;
    case DRM_ERR_NO_ACCESS: fprintf(stderr, "%s: no access\n", label);   break;
    case DRM_ERR_NOT_ROOT:  fprintf(stderr, "%s: not root\n", label);    break;
    case DRM_ERR_INVALID:   fprintf(stderr, "%s: invalid args\n", label);break;
    default:
	if (err < 0) err = -err;
	fprintf( stderr, "%s: error %d (%s)\n", label, err, strerror(err) );
	break;
    }

    return 1;
}

int drmCtlInstHandler(int fd, int irq)
{
    drm_control_t ctl;

    ctl.func  = DRM_INST_HANDLER;
    ctl.irq   = irq;
    if (ioctl(fd, DRM_IOCTL_CONTROL, &ctl)) return -errno;
    return 0;
}

int drmCtlUninstHandler(int fd)
{
    drm_control_t ctl;

    ctl.func  = DRM_UNINST_HANDLER;
    ctl.irq   = 0;
    if (ioctl(fd, DRM_IOCTL_CONTROL, &ctl)) return -errno;
    return 0;
}

int drmFinish(int fd, int context, drmLockFlags flags)
{
    drm_lock_t lock;

    lock.context = context;
    lock.flags   = 0;
    if (flags & DRM_LOCK_READY)      lock.flags |= _DRM_LOCK_READY;
    if (flags & DRM_LOCK_QUIESCENT)  lock.flags |= _DRM_LOCK_QUIESCENT;
    if (flags & DRM_LOCK_FLUSH)      lock.flags |= _DRM_LOCK_FLUSH;
    if (flags & DRM_LOCK_FLUSH_ALL)  lock.flags |= _DRM_LOCK_FLUSH_ALL;
    if (flags & DRM_HALT_ALL_QUEUES) lock.flags |= _DRM_HALT_ALL_QUEUES;
    if (flags & DRM_HALT_CUR_QUEUES) lock.flags |= _DRM_HALT_CUR_QUEUES;
    if (ioctl(fd, DRM_IOCTL_FINISH, &lock)) return -errno;
    return 0;
}

int drmGetInterruptFromBusID(int fd, int busnum, int devnum, int funcnum)
{
    drm_irq_busid_t p;

    p.busnum  = busnum;
    p.devnum  = devnum;
    p.funcnum = funcnum;
    if (ioctl(fd, DRM_IOCTL_IRQ_BUSID, &p)) return -errno;
    return p.irq;
}

int drmAddContextTag(int fd, drmContext context, void *tag)
{
    drmHashEntry  *entry = drmGetEntry(fd);

    if (drmHashInsert(entry->tagTable, context, tag)) {
	drmHashDelete(entry->tagTable, context);
	drmHashInsert(entry->tagTable, context, tag);
    }
    return 0;
}

int drmDelContextTag(int fd, drmContext context)
{
    drmHashEntry  *entry = drmGetEntry(fd);

    return drmHashDelete(entry->tagTable, context);
}

void *drmGetContextTag(int fd, drmContext context)
{
    drmHashEntry  *entry = drmGetEntry(fd);
    void          *value;
    
    if (drmHashLookup(entry->tagTable, context, &value)) return NULL;

    return value;
}

#if defined(XFree86Server) || defined(DRM_USE_MALLOC)
static void drmSIGIOHandler(int interrupt, void *closure)
{
    unsigned long key;
    void          *value;
    ssize_t       count;
    drm_ctx_t     ctx;
    typedef void  (*_drmCallback)(int, void *, void *);
    char          buf[256];
    drmContext    old;
    drmContext    new;
    void          *oldctx;
    void          *newctx;
    char          *pt;
    drmHashEntry  *entry;

    if (!drmHashTable) return;
    if (drmHashFirst(drmHashTable, &key, &value)) {
	entry = value;
	do {
#if 0
	    fprintf(stderr, "Trying %d\n", entry->fd);
#endif
	    if ((count = read(entry->fd, buf, sizeof(buf)))) {
		buf[count] = '\0';
#if 0
		fprintf(stderr, "Got %s\n", buf);
#endif
		
		for (pt = buf; *pt != ' '; ++pt); /* Find first space */
		++pt;
		old    = strtol(pt, &pt, 0);
		new    = strtol(pt, NULL, 0);
		oldctx = drmGetContextTag(entry->fd, old);
		newctx = drmGetContextTag(entry->fd, new);
#if 0
		fprintf(stderr, "%d %d %p %p\n", old, new, oldctx, newctx);
#endif
		((_drmCallback)entry->f)(entry->fd, oldctx, newctx);
		ctx.handle = new;
		ioctl(entry->fd, DRM_IOCTL_NEW_CTX, &ctx);
	    }
	} while (drmHashNext(drmHashTable, &key, &value));
    }
}

int drmInstallSIGIOHandler(int fd, void (*f)(int, void *, void *))
{
    drmHashEntry     *entry;

    entry     = drmGetEntry(fd);
    entry->f  = f;

    return xf86InstallSIGIOHandler(fd, drmSIGIOHandler, 0);
}

int drmRemoveSIGIOHandler(int fd)
{
    drmHashEntry     *entry = drmGetEntry(fd);

    entry->f = NULL;
    
    return xf86RemoveSIGIOHandler(fd);
}
#endif
