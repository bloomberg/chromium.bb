// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>

#include "base/bind.h"
#include "base/logging.h"
#include "content/common/gpu/media/vaapi_wrapper.h"

#define LOG_VA_ERROR_AND_REPORT(va_error, err_msg)         \
  do {                                                     \
    DVLOG(1) << err_msg                                    \
             << " VA error: " << VAAPI_ErrorStr(va_error); \
    report_error_to_uma_cb_.Run();                         \
  } while (0)

#define VA_LOG_ON_ERROR(va_error, err_msg)                 \
  do {                                                     \
    if ((va_error) != VA_STATUS_SUCCESS)                   \
      LOG_VA_ERROR_AND_REPORT(va_error, err_msg);          \
  } while (0)

#define VA_SUCCESS_OR_RETURN(va_error, err_msg, ret)       \
  do {                                                     \
    if ((va_error) != VA_STATUS_SUCCESS) {                 \
      LOG_VA_ERROR_AND_REPORT(va_error, err_msg);          \
      return (ret);                                        \
    }                                                      \
  } while (0)

namespace content {

static void *vaapi_handle = NULL;
static void *vaapi_x11_handle = NULL;

typedef VAStatus (*VaapiBeginPicture)(VADisplay dpy,
                                      VAContextID context,
                                      VASurfaceID render_target);
typedef VAStatus (*VaapiCreateBuffer)(VADisplay dpy,
                                      VAContextID context,
                                      VABufferType type,
                                      unsigned int size,
                                      unsigned int num_elements,
                                      void *data,
                                      VABufferID *buf_id);
typedef VAStatus (*VaapiCreateConfig)(VADisplay dpy,
                                      VAProfile profile,
                                      VAEntrypoint entrypoint,
                                      VAConfigAttrib *attrib_list,
                                      int num_attribs,
                                      VAConfigID *config_id);
typedef VAStatus (*VaapiCreateContext)(VADisplay dpy,
                                       VAConfigID config_id,
                                       int picture_width,
                                       int picture_height,
                                       int flag,
                                       VASurfaceID *render_targets,
                                       int num_render_targets,
                                       VAContextID *context);
// In VAAPI version < 0.34, vaCreateSurface has 6 parameters, but in VAAPI
// version >= 0.34, vaCreateSurface has 8 parameters.
// TODO(chihchung): Remove the old path once ChromeOS updates to 1.2.1.
typedef void *VaapiCreateSurfaces;
typedef VAStatus (*VaapiCreateSurfaces6)(VADisplay dpy,
                                         int width,
                                         int height,
                                         int format,
                                         int num_surfaces,
                                         VASurfaceID *surfaces);
typedef VAStatus (*VaapiCreateSurfaces8)(VADisplay dpy,
                                         unsigned int format,
                                         unsigned int width,
                                         unsigned int height,
                                         VASurfaceID *surfaces,
                                         unsigned int num_surfaces,
                                         VASurfaceAttrib *attrib_list,
                                         unsigned int num_attribs);
typedef VAStatus (*VaapiDeriveImage)(VADisplay dpy,
                                     VASurfaceID surface,
                                     VAImage* image);
typedef VAStatus (*VaapiDestroyBuffer)(VADisplay dpy, VABufferID buffer_id);
typedef VAStatus (*VaapiDestroyConfig)(VADisplay dpy, VAConfigID config_id);
typedef VAStatus (*VaapiDestroyContext)(VADisplay dpy, VAContextID context);
typedef VAStatus (*VaapiDestroyImage)(VADisplay dpy, VAImageID image);
typedef VAStatus (*VaapiDestroySurfaces)(VADisplay dpy,
                                         VASurfaceID *surfaces,
                                         int num_surfaces);
typedef int (*VaapiDisplayIsValid)(VADisplay dpy);
typedef VAStatus (*VaapiEndPicture)(VADisplay dpy, VAContextID context);
typedef const char* (*VaapiErrorStr)(VAStatus error_status);
typedef VAStatus (*VaapiGetConfigAttributes)(VADisplay dpy,
                                             VAProfile profile,
                                             VAEntrypoint entrypoint,
                                             VAConfigAttrib *attrib_list,
                                             int num_attribs);
typedef VADisplay (*VaapiGetDisplay)(Display *dpy);
typedef VAStatus (*VaapiInitialize)(VADisplay dpy,
                                    int *major_version,
                                    int *minor_version);
typedef VAStatus (*VaapiMapBuffer)(VADisplay dpy,
                                   VABufferID buf_id,
                                   void** pbuf);
typedef VAStatus (*VaapiPutSurface)(VADisplay dpy,
                                    VASurfaceID surface,
                                    Drawable draw,
                                    short srcx,
                                    short srcy,
                                    unsigned short srcw,
                                    unsigned short srch,
                                    short destx,
                                    short desty,
                                    unsigned short destw,
                                    unsigned short desth,
                                    VARectangle *cliprects,
                                    unsigned int number_cliprects,
                                    unsigned int flags);
typedef VAStatus (*VaapiRenderPicture)(VADisplay dpy,
                                       VAContextID context,
                                       VABufferID *buffers,
                                       int num_buffers);
typedef VAStatus (*VaapiSetDisplayAttributes)(VADisplay dpy,
                                              VADisplayAttribute *type,
                                              int num_attributes);
typedef VAStatus (*VaapiSyncSurface)(VADisplay dpy, VASurfaceID render_target);
typedef VAStatus (*VaapiTerminate)(VADisplay dpy);
typedef VAStatus (*VaapiUnmapBuffer)(VADisplay dpy, VABufferID buf_id);

#define VAAPI_SYM(name, handle) Vaapi##name VAAPI_##name = NULL

VAAPI_SYM(BeginPicture, vaapi_handle);
VAAPI_SYM(CreateBuffer, vaapi_handle);
VAAPI_SYM(CreateConfig, vaapi_handle);
VAAPI_SYM(CreateContext, vaapi_handle);
VAAPI_SYM(CreateSurfaces, vaapi_handle);
VAAPI_SYM(DeriveImage, vaapi_handle);
VAAPI_SYM(DestroyBuffer, vaapi_handle);
VAAPI_SYM(DestroyConfig, vaapi_handle);
VAAPI_SYM(DestroyContext, vaapi_handle);
VAAPI_SYM(DestroyImage, vaapi_handle);
VAAPI_SYM(DestroySurfaces, vaapi_handle);
VAAPI_SYM(DisplayIsValid, vaapi_handle);
VAAPI_SYM(EndPicture, vaapi_handle);
VAAPI_SYM(ErrorStr, vaapi_handle);
VAAPI_SYM(GetConfigAttributes, vaapi_handle);
VAAPI_SYM(GetDisplay, vaapi_x11_handle);
VAAPI_SYM(Initialize, vaapi_handle);
VAAPI_SYM(MapBuffer, vaapi_handle);
VAAPI_SYM(PutSurface, vaapi_x11_handle);
VAAPI_SYM(RenderPicture, vaapi_handle);
VAAPI_SYM(SetDisplayAttributes, vaapi_handle);
VAAPI_SYM(SyncSurface, vaapi_x11_handle);
VAAPI_SYM(Terminate, vaapi_handle);
VAAPI_SYM(UnmapBuffer, vaapi_handle);

#undef VAAPI_SYM

// Maps Profile enum values to VaProfile values.
static bool ProfileToVAProfile(media::VideoCodecProfile profile,
                               VAProfile* va_profile) {
  switch (profile) {
    case media::H264PROFILE_BASELINE:
      *va_profile = VAProfileH264Baseline;
      break;
    case media::H264PROFILE_MAIN:
      *va_profile = VAProfileH264Main;
      break;
    // TODO(posciak): See if we can/want support other variants
    // of media::H264PROFILE_HIGH*.
    case media::H264PROFILE_HIGH:
      *va_profile = VAProfileH264High;
      break;
    default:
      return false;
  }
  return true;
}

VASurface::VASurface(VASurfaceID va_surface_id, const ReleaseCB& release_cb)
    : va_surface_id_(va_surface_id),
      release_cb_(release_cb) {
  DCHECK(!release_cb_.is_null());
}

VASurface::~VASurface() {
  release_cb_.Run(va_surface_id_);
}

VaapiWrapper::VaapiWrapper()
    : va_display_(NULL),
      va_config_id_(VA_INVALID_ID),
      va_context_id_(VA_INVALID_ID) {
}

VaapiWrapper::~VaapiWrapper() {
  DestroyPendingBuffers();
  DestroySurfaces();
  Deinitialize();
}

scoped_ptr<VaapiWrapper> VaapiWrapper::Create(
    media::VideoCodecProfile profile,
    Display* x_display,
    const base::Closure& report_error_to_uma_cb) {
  scoped_ptr<VaapiWrapper> vaapi_wrapper(new VaapiWrapper());

  if (!vaapi_wrapper->Initialize(profile, x_display, report_error_to_uma_cb))
    vaapi_wrapper.reset();

  return vaapi_wrapper.Pass();
}

void VaapiWrapper::TryToSetVADisplayAttributeToLocalGPU() {
  VADisplayAttribute item = {VADisplayAttribRenderMode,
                             1,  // At least support '_LOCAL_OVERLAY'.
                             -1,  // The maximum possible support 'ALL'.
                             VA_RENDER_MODE_LOCAL_GPU,
                             VA_DISPLAY_ATTRIB_SETTABLE};

  VAStatus va_res = VAAPI_SetDisplayAttributes(va_display_, &item, 1);
  if (va_res != VA_STATUS_SUCCESS)
    DVLOG(2) << "VAAPI_SetDisplayAttributes unsupported, ignoring by default.";
}

bool VaapiWrapper::Initialize(media::VideoCodecProfile profile,
                              Display* x_display,
                              const base::Closure& report_error_to_uma_cb) {
  static bool vaapi_functions_initialized = PostSandboxInitialization();
  if (!vaapi_functions_initialized) {
    DVLOG(1) << "Failed to initialize VAAPI libs";
    return false;
  }

  report_error_to_uma_cb_ = report_error_to_uma_cb;

  base::AutoLock auto_lock(va_lock_);

  VAProfile va_profile;
  if (!ProfileToVAProfile(profile, &va_profile)) {
    DVLOG(1) << "Unsupported profile";
    return false;
  }

  va_display_ = VAAPI_GetDisplay(x_display);
  if (!VAAPI_DisplayIsValid(va_display_)) {
    DVLOG(1) << "Could not get a valid VA display";
    return false;
  }

  VAStatus va_res;
  va_res = VAAPI_Initialize(va_display_, &major_version_, &minor_version_);
  VA_SUCCESS_OR_RETURN(va_res, "vaInitialize failed", false);
  DVLOG(1) << "VAAPI version: " << major_version_ << "." << minor_version_;

  VAConfigAttrib attrib = {VAConfigAttribRTFormat, 0};

  const VAEntrypoint kEntrypoint = VAEntrypointVLD;
  va_res = VAAPI_GetConfigAttributes(va_display_, va_profile, kEntrypoint,
                                     &attrib, 1);
  VA_SUCCESS_OR_RETURN(va_res, "vaGetConfigAttributes failed", false);

  if (!(attrib.value & VA_RT_FORMAT_YUV420)) {
    DVLOG(1) << "YUV420 not supported by this VAAPI implementation";
    return false;
  }

  TryToSetVADisplayAttributeToLocalGPU();

  va_res = VAAPI_CreateConfig(va_display_, va_profile, kEntrypoint,
                              &attrib, 1, &va_config_id_);
  VA_SUCCESS_OR_RETURN(va_res, "vaCreateConfig failed", false);

  return true;
}

void VaapiWrapper::Deinitialize() {
  base::AutoLock auto_lock(va_lock_);

  if (va_config_id_ != VA_INVALID_ID) {
    VAStatus va_res = VAAPI_DestroyConfig(va_display_, va_config_id_);
    VA_LOG_ON_ERROR(va_res, "vaDestroyConfig failed");
  }

  if (va_display_) {
    VAStatus va_res = VAAPI_Terminate(va_display_);
    VA_LOG_ON_ERROR(va_res, "vaTerminate failed");
  }

  va_config_id_ = VA_INVALID_ID;
  va_display_ = NULL;
}

bool VaapiWrapper::VAAPIVersionLessThan(int major, int minor) {
  return (major_version_ < major) ||
      (major_version_ == major && minor_version_ < minor);
}

bool VaapiWrapper::CreateSurfaces(gfx::Size size,
                                  size_t num_surfaces,
                                   std::vector<VASurfaceID>* va_surfaces) {
  base::AutoLock auto_lock(va_lock_);
  DVLOG(2) << "Creating " << num_surfaces << " surfaces";

  DCHECK(va_surfaces->empty());
  DCHECK(va_surface_ids_.empty());
  va_surface_ids_.resize(num_surfaces);

  // Allocate surfaces in driver.
  VAStatus va_res;
  if (VAAPIVersionLessThan(0, 34)) {
    va_res = reinterpret_cast<VaapiCreateSurfaces6>(VAAPI_CreateSurfaces)(
        va_display_,
        size.width(), size.height(),
        VA_RT_FORMAT_YUV420,
        va_surface_ids_.size(),
        &va_surface_ids_[0]);
  } else {
    va_res = reinterpret_cast<VaapiCreateSurfaces8>(VAAPI_CreateSurfaces)(
        va_display_,
        VA_RT_FORMAT_YUV420,
        size.width(), size.height(),
        &va_surface_ids_[0],
        va_surface_ids_.size(),
        NULL, 0);
  }

  VA_LOG_ON_ERROR(va_res, "vaCreateSurfaces failed");
  if (va_res != VA_STATUS_SUCCESS) {
    va_surface_ids_.clear();
    return false;
  }

  // And create a context associated with them.
  va_res = VAAPI_CreateContext(va_display_, va_config_id_,
                               size.width(), size.height(), VA_PROGRESSIVE,
                               &va_surface_ids_[0], va_surface_ids_.size(),
                               &va_context_id_);

  VA_LOG_ON_ERROR(va_res, "vaCreateContext failed");
  if (va_res != VA_STATUS_SUCCESS) {
    DestroySurfaces();
    return false;
  }

  *va_surfaces = va_surface_ids_;
  return true;
}

void VaapiWrapper::DestroySurfaces() {
  base::AutoLock auto_lock(va_lock_);
  DVLOG(2) << "Destroying " << va_surface_ids_.size()  << " surfaces";

  if (va_context_id_ != VA_INVALID_ID) {
    VAStatus va_res = VAAPI_DestroyContext(va_display_, va_context_id_);
    VA_LOG_ON_ERROR(va_res, "vaDestroyContext failed");
  }

  if (!va_surface_ids_.empty()) {
    VAStatus va_res = VAAPI_DestroySurfaces(va_display_, &va_surface_ids_[0],
                                            va_surface_ids_.size());
    VA_LOG_ON_ERROR(va_res, "vaDestroySurfaces failed");
  }

  va_surface_ids_.clear();
  va_context_id_ = VA_INVALID_ID;
}

bool VaapiWrapper::SubmitBuffer(VABufferType va_buffer_type,
                                size_t size,
                                void* buffer) {
  base::AutoLock auto_lock(va_lock_);

  VABufferID buffer_id;
  VAStatus va_res = VAAPI_CreateBuffer(va_display_, va_context_id_,
                                       va_buffer_type, size,
                                       1, buffer, &buffer_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed to create a VA buffer", false);

  switch (va_buffer_type) {
    case VASliceParameterBufferType:
    case VASliceDataBufferType:
      pending_slice_bufs_.push_back(buffer_id);
      break;

    default:
      pending_va_bufs_.push_back(buffer_id);
      break;
  }

  return true;
}

void VaapiWrapper::DestroyPendingBuffers() {
  base::AutoLock auto_lock(va_lock_);

  for (size_t i = 0; i < pending_va_bufs_.size(); ++i) {
    VAStatus va_res = VAAPI_DestroyBuffer(va_display_, pending_va_bufs_[i]);
    VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");
  }

  for (size_t i = 0; i < pending_slice_bufs_.size(); ++i) {
    VAStatus va_res = VAAPI_DestroyBuffer(va_display_, pending_slice_bufs_[i]);
    VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");
  }

  pending_va_bufs_.clear();
  pending_slice_bufs_.clear();
}

bool VaapiWrapper::SubmitDecode(VASurfaceID va_surface_id) {
  base::AutoLock auto_lock(va_lock_);

  DVLOG(4) << "Pending VA bufs to commit: " << pending_va_bufs_.size();
  DVLOG(4) << "Pending slice bufs to commit: " << pending_slice_bufs_.size();
  DVLOG(4) << "Decoding into VA surface " << va_surface_id;

  // Get ready to decode into surface.
  VAStatus va_res = VAAPI_BeginPicture(va_display_, va_context_id_,
                                       va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "vaBeginPicture failed", false);

  // Commit parameter and slice buffers.
  va_res = VAAPI_RenderPicture(va_display_, va_context_id_,
                               &pending_va_bufs_[0], pending_va_bufs_.size());
  VA_SUCCESS_OR_RETURN(va_res, "vaRenderPicture for va_bufs failed", false);

  va_res = VAAPI_RenderPicture(va_display_, va_context_id_,
                               &pending_slice_bufs_[0],
                               pending_slice_bufs_.size());
  VA_SUCCESS_OR_RETURN(va_res, "vaRenderPicture for slices failed", false);

  // Instruct HW decoder to start processing committed buffers (decode this
  // picture). This does not block until the end of decode.
  va_res = VAAPI_EndPicture(va_display_, va_context_id_);
  VA_SUCCESS_OR_RETURN(va_res, "vaEndPicture failed", false);

  return true;
}

bool VaapiWrapper::DecodeAndDestroyPendingBuffers(VASurfaceID va_surface_id) {
  bool result = SubmitDecode(va_surface_id);
  DestroyPendingBuffers();
  return result;
}

bool VaapiWrapper::PutSurfaceIntoPixmap(VASurfaceID va_surface_id,
                                        Pixmap x_pixmap,
                                        gfx::Size dest_size) {
  base::AutoLock auto_lock(va_lock_);

  VAStatus va_res = VAAPI_SyncSurface(va_display_, va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed syncing surface", false);

  // Put the data into an X Pixmap.
  va_res = VAAPI_PutSurface(va_display_,
                            va_surface_id,
                            x_pixmap,
                            0, 0, dest_size.width(), dest_size.height(),
                            0, 0, dest_size.width(), dest_size.height(),
                            NULL, 0, 0);
  VA_SUCCESS_OR_RETURN(va_res, "Failed putting decode surface to pixmap",
                       false);
  return true;
}

bool VaapiWrapper::GetVaImageForTesting(VASurfaceID va_surface_id,
                                        VAImage* image,
                                        void** mem) {
  base::AutoLock auto_lock(va_lock_);

  VAStatus va_res = VAAPI_SyncSurface(va_display_, va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed syncing surface", false);

  // Derive a VAImage from the VASurface
  va_res = VAAPI_DeriveImage(va_display_, va_surface_id, image);
  VA_LOG_ON_ERROR(va_res, "vaDeriveImage failed");
  if (va_res != VA_STATUS_SUCCESS)
    return false;

  // Map the VAImage into memory
  va_res = VAAPI_MapBuffer(va_display_, image->buf, mem);
  VA_LOG_ON_ERROR(va_res, "vaMapBuffer failed");
  if (va_res == VA_STATUS_SUCCESS)
    return true;

  VAAPI_DestroyImage(va_display_, image->image_id);
  return false;
}

void VaapiWrapper::ReturnVaImageForTesting(VAImage* image) {
  base::AutoLock auto_lock(va_lock_);

  VAAPI_UnmapBuffer(va_display_, image->buf);
  VAAPI_DestroyImage(va_display_, image->image_id);
}

// static
bool VaapiWrapper::PostSandboxInitialization() {
  vaapi_handle = dlopen("libva.so.1", RTLD_NOW);
  vaapi_x11_handle = dlopen("libva-x11.so.1", RTLD_NOW);

  if (!vaapi_handle || !vaapi_x11_handle)
    return false;
#define VAAPI_DLSYM_OR_RETURN_ON_ERROR(name, handle)                          \
  do {                                                                        \
    VAAPI_##name = reinterpret_cast<Vaapi##name>(dlsym((handle), "va"#name)); \
    if (VAAPI_##name == NULL) {                                               \
      DVLOG(1) << "Failed to dlsym va"#name;                                  \
      return false;                                                           \
    }                                                                         \
  } while (0)

  VAAPI_DLSYM_OR_RETURN_ON_ERROR(BeginPicture, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(CreateBuffer, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(CreateConfig, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(CreateContext, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(CreateSurfaces, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(DeriveImage, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(DestroyBuffer, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(DestroyConfig, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(DestroyContext, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(DestroyImage, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(DestroySurfaces, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(DisplayIsValid, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(EndPicture, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(ErrorStr, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(GetConfigAttributes, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(GetDisplay, vaapi_x11_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(Initialize, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(MapBuffer, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(PutSurface, vaapi_x11_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(RenderPicture, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(SetDisplayAttributes, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(SyncSurface, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(Terminate, vaapi_handle);
  VAAPI_DLSYM_OR_RETURN_ON_ERROR(UnmapBuffer, vaapi_handle);
#undef VAAPI_DLSYM

  return true;
}

}  // namespace content
