/* xf86drm.c -- User-level interface to DRM device
 * Created: Tue Jan  5 08:16:21 1999 by faith@precisioninsight.com
 * Revised: Fri Jun 18 09:52:23 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
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
 * $PI: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/xf86drm.c,v 1.41 1999/06/21 14:31:20 faith Exp $
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/xf86drm.c,v 1.3 1999/06/27 14:08:19 dawes Exp $
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
extern int xf86InstallSIGIOHandler(int fd, void (*f)(int));
extern int xf86RemoveSIGIOHandler(int fd);
# else
#  include <Xlibint.h>
#  define _DRM_MALLOC Xmalloc
#  define _DRM_FREE   Xfree
# endif
#endif

/* Not all systems have MAP_FAILED defined */
#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

#include <sys/sysmacros.h>	/* for makedev() */
#include "xf86drm.h"
#include "drm.h"

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

static char *drmStrdup(const char *s)
{
    return s ? strdup(s) : NULL;
}


/* drm_lookup searches file for a line tagged with name, and returns the
   value for that tag.  If busid is NULL, the file format is that used for
   /proc/devices and /proc/misc, or the first part of /proc/drm/devices:
 
       <value> <whitespace> <tag>

   If the busid is non-NULL, the file format is the extended format used
   for /proc/drm/devices:

       <value> <whitespace> <name> <whitespace> <busid>

   If both name and busid are non-NULL, both must match.  If either is
   NULL, then the other is matched.
*/

static int drm_lookup(const char *file, const char *name, const char *busid)
{
    FILE *str;
    char buf[128];
    char *pt;
    int  name_match;
    int  busid_match;
    char *namept     = NULL;
    char *busidpt    = NULL;

    if (!(str = fopen(file, "r"))) return DRM_ERR_NO_DEVICE;
    while (fgets(buf, sizeof(buf)-1, str)) {
	buf[sizeof(buf)-1] = '\0';
	for (pt = buf; *pt && isspace(*pt); ++pt);  /* skip whitespace */
	for (; *pt && !isspace(*pt); ++pt);         /* next space or null */
	if (isspace(pt[0]) && pt[1]) {
	    pt++;
	    for (; *pt && isspace(*pt); ++pt);      /* skip whitespace */
	    namept = pt;
	    for (; *pt && !isspace(*pt); ++pt);	    /* next space or null */
	    if (isspace(pt[0]) && pt[1]) {          /* busid present */
		*pt = '\0';		            /* proper termination */
		pt++;
		for (; *pt && isspace(*pt); ++pt);  /* skip whitespace */
		busidpt = pt;
		for (; *pt && !isspace(*pt); ++pt); /* next space or null */
	    }
	    *pt = '\0';		                    /* proper termination */
	    name_match  = name ? 0 : 1;  /* match if we don't care */
	    busid_match = busid ? 0 : 1; /* match if we don't care */
	    if (name && namept && !strcmp(name, namept)) ++name_match;
	    if (busid && busidpt && !strcmp(busid, busidpt)) ++busid_match;
	    if (name_match && busid_match) {
		fclose(str);
		return atoi(buf); /* stops at whitespace */
	    }
	}
    }
    fclose(str);
    return DRM_ERR_NO_DEVICE;
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

/* drm_open is used to open the /dev/drm device */

static int drm_open(const char *file)
{
    int fd = open(file, O_RDWR, 0);

    if (fd >= 0) return fd;
    return -errno;
}

/* drmAvailable looks for /proc/drm, and returns 1 if it is present. */

int drmAvailable(void)
{
    if (!access(DRM_PROC_DRM, R_OK)) return 1;
    return 0;
}

/* drmGetMajor tries to find the major device number for /dev/drm by
   searching /proc/devices.  A negative value is returned on error. */

static int drmGetMajor(void)
{
    int major;

    if (!drmAvailable()) return DRM_ERR_NO_DEVICE;
    
    if ((major = drm_lookup(DRM_PROC_DEVICES, DRM_NAME, NULL)) >= 0)
	return major;
    
    return DRM_ERR_NO_DEVICE;
}

/* drmGetMinor tries to find the minor device number for name by looking in
   /proc/drm/devices.  A negative value is retruned on error. */

static int drmGetMinor(const char *name, const char *busid)
{
    int  minor;
    char buf[128];
     
    if (!drmAvailable()) return DRM_ERR_NO_DEVICE;

    sprintf(buf, "/proc/%s/%s", DRM_NAME, DRM_DEVICES);
    
    if ((minor = drm_lookup(buf, name, busid))) return minor;
    return 0;
}


/* drmOpen looks up the specified name and/or busid in /proc/drm/devices,
   and opens the device found.  The entry in /dev is created if necessary
   (and if root).  A file descriptor is returned.  On error, the return
   value is negative. */

static int drmOpen(const char *name, const char *busid)
{
#ifdef XFree86LOADER
    struct xf86stat st;
#else
    struct stat     st;
#endif
    char            path[128];
    int             major;
    int             minor;
    dev_t           dev;
    
    if (!drmAvailable())                       return DRM_ERR_NO_DEVICE;
    if ((major = drmGetMajor()) < 0)           return major;
    if ((minor = drmGetMinor(name,busid)) < 0) return minor;
    dev = makedev(major, minor);

    if (!minor) {
	sprintf(path, "/dev/%s", DRM_NAME );
    } else {
	sprintf(path, "/dev/%s%d", DRM_NAME, minor-1 );
    }

				/* Check device major/minor for match */
    if (!access(path, F_OK)) {
	if (stat(path, &st))   return DRM_ERR_NO_ACCESS;
	if (st.st_rdev == dev) {
#if defined(XFree86Server)
	    chmod(path,
		  xf86ConfigDRI.mode  ? xf86ConfigDRI.mode : DRM_DEV_MODE);
	    chown(path, DRM_DEV_UID,
		  xf86ConfigDRI.group ? xf86ConfigDRI.group : DRM_DEV_GID);
#endif
#if defined(DRM_USE_MALLOC)
	    chmod(path, DRM_DEV_MODE);
	    chown(path, DRM_DEV_UID, DRM_DEV_GID);
#endif
	    return drm_open(path);
	}
    }

				/* Doesn't exist or match failed, so we
                                   have to be root to create it. */
    if (geteuid()) return DRM_ERR_NOT_ROOT;
    remove(path);
    if (mknod(path, S_IFCHR | DRM_DEV_MODE, dev)) {
	remove(path);
	return DRM_ERR_NOT_ROOT;
    }
#if defined(XFree86Server)
    chmod(path, xf86ConfigDRI.mode ? xf86ConfigDRI.mode : DRM_DEV_MODE);
    chown(path, DRM_DEV_UID,
	  xf86ConfigDRI.group ? xf86ConfigDRI.group : DRM_DEV_GID);
#endif
#if defined(DRM_USE_MALLOC)
    chmod(path, DRM_DEV_MODE);
    chown(path, DRM_DEV_UID, DRM_DEV_GID);
#endif
    return drm_open(path);
}

/* drmOpenDRM returns a file descriptor for the main /dev/drm control
   device.  The entry in /dev is created if necessary (and if root).  A
   file descriptor is returned.  On error, the return value is negative. */

int drmOpenDRM(void)
{
    return drmOpen(DRM_NAME, NULL);
}


/* drmClose closes the file descriptor returned from drmOpen. */
   
int drmCloseDRM(int fd)
{
    return close(fd);
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

static void drmCopyVersion(drmVersionPtr d, drm_version_t *s)
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
 * information is available via /proc/drm. */

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

void drmFreeVersionList(drmListPtr list)
{
    int i;
    
    if (!list) return;
    if (list->version) {
	for (i = 0; i < list->count; i++) {
	    if (list->version[i].name) drmFree(list->version[i].name);
	    if (list->version[i].date) drmFree(list->version[i].date);
	    if (list->version[i].desc) drmFree(list->version[i].desc);
	    
	}
	drmFree(list->version);
    }
    if (list->capability) drmFree(list->capability);
    drmFree(list);
}

static void drmFreeKernelVersionList(drm_list_t *list)
{
    int i;
    
    if (!list) return;
    if (list->version) {
	for (i = 0; i < list->count; i++) {
	    if (list->version[i].name) drmFree(list->version[i].name);
	    if (list->version[i].date) drmFree(list->version[i].date);
	    if (list->version[i].desc) drmFree(list->version[i].desc);
	    
	}
	drmFree(list->version);
    }
    drmFree(list);
}

/* drmList obtains a list of all drivers and capabilities via an ioctl. */

drmListPtr drmGetVersionList(int fd)
{
    drmListPtr retval;
    drm_list_t *list = drmMalloc(sizeof(*list));
    int        i;

    list->count = 0;

				/* First, get the count */
    
    if (ioctl(fd, DRM_IOCTL_LIST, list)) {
	drmFreeKernelVersionList(list);
	return NULL;
    }

				/* Next, get the version sizes */
    for (i = 0; i < list->count; i++) {
	list->version
	    = drmMalloc(list->count * sizeof(*list->version));
	list->version[i].name_len = 0;
	list->version[i].name     = NULL;
	list->version[i].date_len = 0;
	list->version[i].date     = NULL;
	list->version[i].desc_len = 0;
	list->version[i].desc     = NULL;
    }

    if (ioctl(fd, DRM_IOCTL_LIST, list)) {
	drmFreeKernelVersionList(list);
	return NULL;
    }
    
				/* Now, allocate space and get the data */
    for (i = 0; i < list->count; i++) {
	if (list->version[i].name_len)
	    list->version[i].name = drmMalloc(list->version[i].name_len + 1);
	if (list->version[i].date_len)
	    list->version[i].date = drmMalloc(list->version[i].date_len + 1);
	if (list->version[i].desc_len)
	    list->version[i].desc = drmMalloc(list->version[i].desc_len + 1);
    }
    
    if (ioctl(fd, DRM_IOCTL_LIST, list)) {
	drmFreeKernelVersionList(list);
	return NULL;
    }

				/* The results might not be null-terminated
                                   strings, so terminate them. */

    for (i = 0; i < list->count; i++) {
	if (list->version[i].name_len)
	    list->version[i].name[list->version[i].name_len] = '\0';
	if (list->version[i].date_len)
	    list->version[i].date[list->version[i].date_len] = '\0';
	if (list->version[i].desc_len)
	    list->version[i].desc[list->version[i].desc_len] = '\0';
    }
    
				/* Now, copy it all back into the
                                   client-visible data structure... */
    retval = drmMalloc(sizeof(*retval));
    retval->count      = list->count;
    retval->version    = drmMalloc(list->count * sizeof(*retval->version));
    retval->capability = drmMalloc(list->count * sizeof(*retval->capability));
    for (i = 0; i < list->count; i++) {
	drmCopyVersion(&retval->version[i], &list->version[i]);
    }
    drmFreeKernelVersionList(list);
    return retval;
}

int drmCreateSub(int fd, const char *name, const char *busid)
{
    drm_request_t request;

    request.device_name  = name;
    request.device_busid = busid;
    if (ioctl(fd, DRM_IOCTL_CREATE, &request)) {
	return -errno;
    }
    return 0;
}

int drmDestroySub(int fd, const char *busid)
{
    drm_request_t request;

    request.device_busid = busid;
    request.device_major = drmGetMajor();
    request.device_minor = drmGetMinor(NULL, busid);
    if (ioctl(fd, DRM_IOCTL_DESTROY, &request)) {
	return -errno;
    }
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
    map.size    = size;
    map.handle  = 0;
    map.type    = type;
    map.flags   = flags;
    if (ioctl(fd, DRM_IOCTL_ADD_MAP, &map)) return -errno;
    if (handle) *handle = (drmHandle)map.handle;
    return 0;
}

int drmAddBufs(int fd, int count, int size, int flags)
{
    drm_buf_desc_t request;
    
    request.count     = count;
    request.size      = size;
    request.low_mark  = 0;
    request.high_mark = 0;
    request.flags     = flags;
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

int drmOpenSub(const char *busid)
{
    return drmOpen(NULL, busid);
}

int drmCloseSub(int fd)
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
    if (fd < 0) return -EINVAL;
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

int drmCtlAddCommand(int fd, drmCtlDesc desc, int count, int *inst)
{
    drm_control_t ctl;

    ctl.func  = DRM_ADD_COMMAND;
    ctl.irq   = 0;
    ctl.count = count;
    ctl.inst  = inst;

    switch (desc) {
    case DRM_IH_PRE_INST:    ctl.desc = _DRM_IH_PRE_INST;    break;
    case DRM_IH_POST_INST:   ctl.desc = _DRM_IH_POST_INST;   break;
    case DRM_IH_SERVICE:     ctl.desc = _DRM_IH_SERVICE;     break;
    case DRM_IH_PRE_UNINST:  ctl.desc = _DRM_IH_PRE_UNINST;  break;
    case DRM_IH_POST_UNINST: ctl.desc = _DRM_IH_POST_UNINST; break;
    case DRM_DMA_DISPATCH:   ctl.desc = _DRM_DMA_DISPATCH;   break;
    case DRM_DMA_READY:      ctl.desc = _DRM_DMA_READY;      break;
    case DRM_DMA_IS_READY:   ctl.desc = _DRM_DMA_IS_READY;   break;
    case DRM_DMA_QUIESCENT:  ctl.desc = _DRM_DMA_QUIESCENT;  break;
    default:
	return -EINVAL;
    }

    if (ioctl(fd, DRM_IOCTL_CONTROL, &ctl)) return -errno;
    return 0;
}

int drmCtlRemoveCommands(int fd)
{
    drm_control_t ctl;
    drm_desc_t    i;

    for (i = 0; i < DRM_DESC_MAX; i++) {
	ctl.func  = DRM_RM_COMMAND;
	ctl.desc  = i;
	ctl.irq   = 0;
	ctl.count = 0;
	ctl.inst  = NULL;
	ioctl(fd, DRM_IOCTL_CONTROL, &ctl);
    }
    return 0;
}

int drmCtlInstHandler(int fd, int irq)
{
    drm_control_t ctl;

    ctl.func  = DRM_INST_HANDLER;
    ctl.desc  = 0;		/* unused */
    ctl.irq   = irq;
    ctl.count = 0;
    ctl.inst  = NULL;
    if (ioctl(fd, DRM_IOCTL_CONTROL, &ctl)) return -errno;
    return 0;
}

int drmCtlUninstHandler(int fd)
{
    drm_control_t ctl;

    ctl.func  = DRM_UNINST_HANDLER;
    ctl.desc  = 0;		/* unused */
    ctl.irq   = 0;
    ctl.count = 0;
    ctl.inst  = NULL;
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
static void drmSIGIOHandler(int interrupt)
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

    return xf86InstallSIGIOHandler(fd, drmSIGIOHandler);
}

int drmRemoveSIGIOHandler(int fd)
{
    drmHashEntry     *entry = drmGetEntry(fd);

    entry->f = NULL;
    
    return xf86RemoveSIGIOHandler(fd);
}
#endif
