#define __NO_VERSION__
#include "gamma.h"
#include "drmP.h"
#include "gamma_drv.h"

#define DRIVER_DEV_PRIV_T	drm_gamma_private_t
#define DRIVER_AGP_BUFFER_MAP	dev_priv->buffers

#include "drm_auth.h"

#include "drm_bufs.h"

#include "drm_dma.h"

#include "drm_drawable.h"

#include "drm_fops.h"

#include "drm_init.h"

#include "drm_ioctl.h"

#include "drm_lists.h"

#include "drm_lock.h"

#include "drm_memory.h"

#include "drm_proc.h"

#include "drm_vm.h"

#include "drm_stub.h"
