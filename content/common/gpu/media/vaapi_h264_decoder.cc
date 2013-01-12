// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "content/common/gpu/gl_scoped_binders.h"
#include "content/common/gpu/media/vaapi_h264_decoder.h"
#include "third_party/libva/va/va.h"
#include "third_party/libva/va/va_x11.h"
#include "ui/gl/gl_bindings.h"

#define VA_LOG_ON_ERROR(va_res, err_msg)                   \
  do {                                                     \
    if ((va_res) != VA_STATUS_SUCCESS) {                   \
      DVLOG(1) << err_msg                                  \
               << " VA error: " << VAAPI_ErrorStr(va_res); \
    }                                                      \
  } while (0)

#define VA_SUCCESS_OR_RETURN(va_res, err_msg, ret)         \
  do {                                                     \
    if ((va_res) != VA_STATUS_SUCCESS) {                   \
      DVLOG(1) << err_msg                                  \
               << " VA error: " << VAAPI_ErrorStr(va_res); \
      return (ret);                                        \
    }                                                      \
  } while (0)

namespace content {

void *vaapi_handle = NULL;
void *vaapi_x11_handle = NULL;

typedef VADisplay (*VaapiGetDisplay)(Display *dpy);
typedef int (*VaapiDisplayIsValid)(VADisplay dpy);
typedef VAStatus (*VaapiInitialize)(VADisplay dpy,
                                    int *major_version,
                                    int *minor_version);
typedef VAStatus (*VaapiTerminate)(VADisplay dpy);
typedef VAStatus (*VaapiGetConfigAttributes)(VADisplay dpy,
                                             VAProfile profile,
                                             VAEntrypoint entrypoint,
                                             VAConfigAttrib *attrib_list,
                                             int num_attribs);
typedef VAStatus (*VaapiCreateConfig)(VADisplay dpy,
                                      VAProfile profile,
                                      VAEntrypoint entrypoint,
                                      VAConfigAttrib *attrib_list,
                                      int num_attribs,
                                      VAConfigID *config_id);
typedef VAStatus (*VaapiDestroyConfig)(VADisplay dpy, VAConfigID config_id);
typedef VAStatus (*VaapiCreateSurfaces)(VADisplay dpy,
                                        int width,
                                        int height,
                                        int format,
                                        int num_surfaces,
                                        VASurfaceID *surfaces);
typedef VAStatus (*VaapiDestroySurfaces)(VADisplay dpy,
                                         VASurfaceID *surfaces,
                                         int num_surfaces);
typedef VAStatus (*VaapiCreateContext)(VADisplay dpy,
                                       VAConfigID config_id,
                                       int picture_width,
                                       int picture_height,
                                       int flag,
                                       VASurfaceID *render_targets,
                                       int num_render_targets,
                                       VAContextID *context);
typedef VAStatus (*VaapiDestroyContext)(VADisplay dpy, VAContextID context);
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
typedef VAStatus (*VaapiSyncSurface)(VADisplay dpy, VASurfaceID render_target);
typedef VAStatus (*VaapiBeginPicture)(VADisplay dpy,
                                      VAContextID context,
                                      VASurfaceID render_target);
typedef VAStatus (*VaapiRenderPicture)(VADisplay dpy,
                                       VAContextID context,
                                       VABufferID *buffers,
                                       int num_buffers);
typedef VAStatus (*VaapiEndPicture)(VADisplay dpy, VAContextID context);
typedef VAStatus (*VaapiCreateBuffer)(VADisplay dpy,
                                      VAContextID context,
                                      VABufferType type,
                                      unsigned int size,
                                      unsigned int num_elements,
                                      void *data,
                                      VABufferID *buf_id);
typedef VAStatus (*VaapiDestroyBuffer)(VADisplay dpy, VABufferID buffer_id);
typedef const char* (*VaapiErrorStr)(VAStatus error_status);

#define VAAPI_SYM(name, handle) Vaapi##name VAAPI_##name = NULL

VAAPI_SYM(GetDisplay, vaapi_x11_handle);
VAAPI_SYM(DisplayIsValid, vaapi_handle);
VAAPI_SYM(Initialize, vaapi_handle);
VAAPI_SYM(Terminate, vaapi_handle);
VAAPI_SYM(GetConfigAttributes, vaapi_handle);
VAAPI_SYM(CreateConfig, vaapi_handle);
VAAPI_SYM(DestroyConfig, vaapi_handle);
VAAPI_SYM(CreateSurfaces, vaapi_handle);
VAAPI_SYM(DestroySurfaces, vaapi_handle);
VAAPI_SYM(CreateContext, vaapi_handle);
VAAPI_SYM(DestroyContext, vaapi_handle);
VAAPI_SYM(PutSurface, vaapi_x11_handle);
VAAPI_SYM(SyncSurface, vaapi_x11_handle);
VAAPI_SYM(BeginPicture, vaapi_handle);
VAAPI_SYM(RenderPicture, vaapi_handle);
VAAPI_SYM(EndPicture, vaapi_handle);
VAAPI_SYM(CreateBuffer, vaapi_handle);
VAAPI_SYM(DestroyBuffer, vaapi_handle);
VAAPI_SYM(ErrorStr, vaapi_handle);

// static
bool VaapiH264Decoder::pre_sandbox_init_done_ = false;

class VaapiH264Decoder::DecodeSurface {
 public:
  DecodeSurface(const GLXFBConfig& fb_config,
                Display* x_display,
                VADisplay va_display,
                const base::Callback<bool(void)>& make_context_current,
                VASurfaceID va_surface_id,
                int32 picture_buffer_id,
                uint32 texture_id,
                int width, int height);
  ~DecodeSurface();

  VASurfaceID va_surface_id() {
    return va_surface_id_;
  }

  int32 picture_buffer_id() {
    return picture_buffer_id_;
  }

  uint32 texture_id() {
    return texture_id_;
  }

  bool available() {
    return available_;
  }

  bool used() {
    return used_;
  }

  void set_used(bool used) {
    DCHECK(!available_);
    used_ = used;
  }

  bool at_client() {
    return at_client_;
  }

  void set_at_client(bool at_client) {
    DCHECK(!available_);
    at_client_ = at_client;
  }

  int32 input_id() {
    return input_id_;
  }

  int poc() {
    return poc_;
  }

  // Associate the surface with |input_id| and |poc|, and make it unavailable
  // (in use).
  void Acquire(int32 input_id, int poc);

  // Make this surface available, ready to be reused.
  void Release();

  // Has to be called before output to sync texture contents.
  // Returns true if successful.
  bool Sync();

 private:
  Display* x_display_;
  VADisplay va_display_;
  base::Callback<bool(void)> make_context_current_;
  VASurfaceID va_surface_id_;

  // Client-provided ids.
  int32 input_id_;
  int32 picture_buffer_id_;
  uint32 texture_id_;

  int width_;
  int height_;

  // Available for decoding (data no longer used for reference or displaying).
  // TODO(posciak): this is almost surely not needed anymore. Rethink and
  // remove if possible.
  bool available_;
  // Used for decoding.
  bool used_;
  // Whether the surface has been sent to client for display.
  bool at_client_;

  // PicOrderCount
  int poc_;

  // Pixmaps bound to this texture.
  Pixmap x_pixmap_;
  GLXPixmap glx_pixmap_;

  DISALLOW_COPY_AND_ASSIGN(DecodeSurface);
};

VaapiH264Decoder::DecodeSurface::DecodeSurface(
    const GLXFBConfig& fb_config,
    Display* x_display,
    VADisplay va_display,
    const base::Callback<bool(void)>& make_context_current,
    VASurfaceID va_surface_id,
    int32 picture_buffer_id,
    uint32 texture_id,
    int width, int height)
    : x_display_(x_display),
      va_display_(va_display),
      make_context_current_(make_context_current),
      va_surface_id_(va_surface_id),
      input_id_(0),
      picture_buffer_id_(picture_buffer_id),
      texture_id_(texture_id),
      width_(width),
      height_(height),
      available_(false),
      used_(false),
      at_client_(false),
      poc_(0),
      x_pixmap_(0),
      glx_pixmap_(0) {
  // Bind the surface to a texture of the given width and height,
  // allocating pixmaps as needed.
  if (!make_context_current_.Run())
    return;

  content::ScopedTextureBinder texture_binder(texture_id_);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  XWindowAttributes win_attr;
  int screen = DefaultScreen(x_display_);
  XGetWindowAttributes(x_display_, RootWindow(x_display_, screen), &win_attr);
  //TODO(posciak): pass the depth required by libva, not the RootWindow's depth
  x_pixmap_ = XCreatePixmap(x_display_, RootWindow(x_display_, screen),
                            width_, height_, win_attr.depth);
  if (!x_pixmap_) {
    DVLOG(1) << "Failed creating an X Pixmap for TFP";
    return;
  }

  static const int pixmap_attr[] = {
    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
    GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGB_EXT,
    GL_NONE,
  };

  glx_pixmap_ = glXCreatePixmap(x_display_, fb_config, x_pixmap_, pixmap_attr);
  if (!glx_pixmap_) {
    // x_pixmap_ will be freed in the destructor.
    DVLOG(1) << "Failed creating a GLX Pixmap for TFP";
    return;
  }

  glXBindTexImageEXT(x_display_, glx_pixmap_, GLX_FRONT_LEFT_EXT, NULL);

  available_ = true;
}

VaapiH264Decoder::DecodeSurface::~DecodeSurface() {
  // Unbind surface from texture and deallocate resources.
  if (glx_pixmap_ && make_context_current_.Run()) {
    glXReleaseTexImageEXT(x_display_, glx_pixmap_, GLX_FRONT_LEFT_EXT);
    glXDestroyPixmap(x_display_, glx_pixmap_);
  }

  if (x_pixmap_)
    XFreePixmap(x_display_, x_pixmap_);
  XSync(x_display_, False);  // Needed to work around buggy vdpau-driver.
}

void VaapiH264Decoder::DecodeSurface::Acquire(int32 input_id, int poc) {
  DCHECK_EQ(available_, true);
  available_ = false;
  at_client_ = false;
  used_ = true;
  input_id_ = input_id;
  poc_ = poc;
}

void VaapiH264Decoder::DecodeSurface::Release() {
  DCHECK_EQ(available_, false);
  available_ = true;
  used_ = false;
  at_client_ = false;
}

bool VaapiH264Decoder::DecodeSurface::Sync() {
  if (!make_context_current_.Run())
    return false;

  // Wait for the data to be put into the buffer so it'd ready for output.
  VAStatus va_res = VAAPI_SyncSurface(va_display_, va_surface_id_);
  VA_SUCCESS_OR_RETURN(va_res, "Failed syncing decoded picture", false);

  // Put the decoded data into XPixmap bound to the texture.
  va_res = VAAPI_PutSurface(va_display_,
                            va_surface_id_, x_pixmap_,
                            0, 0, width_, height_,
                            0, 0, width_, height_,
                            NULL, 0, 0);
  VA_SUCCESS_OR_RETURN(va_res, "Failed putting decoded picture to texture",
                       false);

  return true;
}

VaapiH264Decoder::VaapiH264Decoder() {
  Reset();
  curr_input_id_ = -1;
  x_display_ = NULL;
  fb_config_ = NULL;
  va_display_ = NULL;
  curr_sps_id_ = -1;
  curr_pps_id_ = -1;
  pic_width_ = -1;
  pic_height_ = -1;
  max_frame_num_ = 0;
  max_pic_num_ = 0;
  max_long_term_frame_idx_ = 0;
  max_pic_order_cnt_lsb_ = 0;
  state_ = kUninitialized;
  num_available_decode_surfaces_ = 0;
  va_context_created_ = false;
  last_output_poc_ = 0;
}

VaapiH264Decoder::~VaapiH264Decoder() {
  Destroy();
}

// This puts the decoder in state where it keeps stream data and is ready
// to resume playback from a random location in the stream, but drops all
// inputs and outputs and makes all surfaces available for use.
void VaapiH264Decoder::Reset() {
  frame_ready_at_hw_ = false;

  curr_pic_.reset();

  frame_num_ = 0;
  prev_frame_num_ = -1;
  prev_frame_num_offset_ = -1;

  prev_ref_has_memmgmnt5_ = false;
  prev_ref_top_field_order_cnt_ = -1;
  prev_ref_pic_order_cnt_msb_ = -1;
  prev_ref_pic_order_cnt_lsb_ = -1;
  prev_ref_field_ = H264Picture::FIELD_NONE;

  // When called from the constructor, although va_display_ is invalid,
  // |pending_slice_bufs_| and |pending_va_bufs_| are empty.
  DestroyPendingBuffers();

  pending_slice_bufs_ = std::queue<VABufferID>();
  pending_va_bufs_ = std::queue<VABufferID>();

  ref_pic_list0_.clear();
  ref_pic_list1_.clear();

  for (POCToDecodeSurfaces::iterator it = poc_to_decode_surfaces_.begin();
       it != poc_to_decode_surfaces_.end(); ) {
    int poc = it->second->poc();
    // Must be incremented before UnassignSurfaceFromPoC as this call
    // invalidates |it|.
    ++it;
    UnassignSurfaceFromPoC(poc);
  }
  DCHECK(poc_to_decode_surfaces_.empty());

  dpb_.Clear();
  parser_.Reset();
  last_output_poc_ = 0;

  // Still initialized and ready to decode, unless called from constructor,
  // which will change it back.
  state_ = kAfterReset;
}

void VaapiH264Decoder::Destroy() {
  if (state_ == kUninitialized)
    return;

  VAStatus va_res;
  bool destroy_surfaces = false;
  switch (state_) {
    case kDecoding:
    case kAfterReset:
    case kError:
      destroy_surfaces = true;
      // fallthrough
    case kInitialized:
      if (!make_context_current_.Run())
        break;
      if (destroy_surfaces)
        DestroyVASurfaces();
      DestroyPendingBuffers();
      va_res = VAAPI_DestroyConfig(va_display_, va_config_id_);
      VA_LOG_ON_ERROR(va_res, "vaDestroyConfig failed");
      va_res = VAAPI_Terminate(va_display_);
      VA_LOG_ON_ERROR(va_res, "vaTerminate failed");
      // fallthrough
    case kUninitialized:
      break;
  }

  state_ = kUninitialized;
}

// Maps Profile enum values to VaProfile values.
bool VaapiH264Decoder::SetProfile(media::VideoCodecProfile profile) {
  switch (profile) {
    case media::H264PROFILE_BASELINE:
      profile_ = VAProfileH264Baseline;
      break;
    case media::H264PROFILE_MAIN:
      profile_ = VAProfileH264Main;
      break;
    case media::H264PROFILE_HIGH:
      profile_ = VAProfileH264High;
      break;
    default:
      return false;
  }
  return true;
}

class ScopedPtrXFree {
 public:
  void operator()(void* x) const {
    ::XFree(x);
  }
};

bool VaapiH264Decoder::InitializeFBConfig() {
  const int fbconfig_attr[] = {
    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
    GLX_BIND_TO_TEXTURE_TARGETS_EXT, GLX_TEXTURE_2D_BIT_EXT,
    GLX_BIND_TO_TEXTURE_RGB_EXT, GL_TRUE,
    GLX_Y_INVERTED_EXT, GL_TRUE,
    GL_NONE,
  };

  int num_fbconfigs;
  scoped_ptr_malloc<GLXFBConfig, ScopedPtrXFree> glx_fb_configs(
      glXChooseFBConfig(x_display_, DefaultScreen(x_display_), fbconfig_attr,
                        &num_fbconfigs));
  if (!glx_fb_configs.get())
    return false;
  if (!num_fbconfigs)
    return false;

  fb_config_ = glx_fb_configs.get()[0];
  return true;
}

bool VaapiH264Decoder::Initialize(
    media::VideoCodecProfile profile,
    Display* x_display,
    GLXContext glx_context,
    const base::Callback<bool(void)>& make_context_current,
    const OutputPicCB& output_pic_cb,
    const SubmitDecodeCB& submit_decode_cb) {
  DCHECK_EQ(state_, kUninitialized);

  output_pic_cb_ = output_pic_cb;
  submit_decode_cb_ = submit_decode_cb;

  x_display_ = x_display;
  make_context_current_ = make_context_current;

  if (!make_context_current_.Run())
    return false;

  if (!SetProfile(profile)) {
    DVLOG(1) << "Unsupported profile";
    return false;
  }

  if (!InitializeFBConfig()) {
    DVLOG(1) << "Could not get a usable FBConfig";
    return false;
  }

  va_display_ = VAAPI_GetDisplay(x_display_);
  if (!VAAPI_DisplayIsValid(va_display_)) {
    DVLOG(1) << "Could not get a valid VA display";
    return false;
  }

  int major_version, minor_version;
  VAStatus va_res;
  va_res = VAAPI_Initialize(va_display_, &major_version, &minor_version);
  VA_SUCCESS_OR_RETURN(va_res, "vaInitialize failed", false);
  DVLOG(1) << "VAAPI version: " << major_version << "." << minor_version;

  VAConfigAttrib attrib;
  attrib.type = VAConfigAttribRTFormat;

  VAEntrypoint entrypoint = VAEntrypointVLD;
  va_res = VAAPI_GetConfigAttributes(va_display_, profile_, entrypoint,
                                     &attrib, 1);
  VA_SUCCESS_OR_RETURN(va_res, "vaGetConfigAttributes failed", false);

  if (!(attrib.value & VA_RT_FORMAT_YUV420)) {
    DVLOG(1) << "YUV420 not supported";
    return false;
  }

  va_res = VAAPI_CreateConfig(va_display_, profile_, entrypoint,
                              &attrib, 1, &va_config_id_);
  VA_SUCCESS_OR_RETURN(va_res, "vaCreateConfig failed", false);

  state_ = kInitialized;
  return true;
}

void VaapiH264Decoder::ReusePictureBuffer(int32 picture_buffer_id) {
  DecodeSurfaces::iterator it = decode_surfaces_.find(picture_buffer_id);
  if (it == decode_surfaces_.end()) {
    DVLOG(1) << "Asked to reuse an invalid surface "
             << picture_buffer_id;
    return;
  }
  if (it->second->available()) {
    DVLOG(1) << "Asked to reuse an already available surface "
             << picture_buffer_id;
    return;
  }

  it->second->set_at_client(false);
  if (!it->second->used()) {
    it->second->Release();
    ++num_available_decode_surfaces_;
  }
}

bool VaapiH264Decoder::AssignPictureBuffer(int32 picture_buffer_id,
                                           uint32 texture_id) {
  DCHECK_EQ(state_, kDecoding);

  if (decode_surfaces_.size() >= GetRequiredNumOfPictures()) {
    DVLOG(1) << "Got more surfaces than required";
    return false;
  }

  // This will not work if we start using VDA.DismissPicture()
  linked_ptr<DecodeSurface> dec_surface(new DecodeSurface(
      fb_config_, x_display_, va_display_, make_context_current_,
      va_surface_ids_[decode_surfaces_.size()], picture_buffer_id, texture_id,
      pic_width_, pic_height_));
  if (!dec_surface->available()) {
    DVLOG(1) << "Error creating a decoding surface (binding to texture?)";
    return false;
  }

  DVLOG(2) << "New picture assigned, texture id: " << dec_surface->texture_id()
           << " pic buf id: " << dec_surface->picture_buffer_id()
           << " will use va surface " << dec_surface->va_surface_id();

  bool inserted = decode_surfaces_.insert(std::make_pair(picture_buffer_id,
                                                         dec_surface)).second;
  DCHECK(inserted);
  ++num_available_decode_surfaces_;

  return true;
}

bool VaapiH264Decoder::CreateVASurfaces() {
  DCHECK_NE(pic_width_, -1);
  DCHECK_NE(pic_height_, -1);
  if (state_ == kAfterReset)
    return true;
  DCHECK_EQ(state_, kInitialized);

  // Allocate VASurfaces in driver.
  VAStatus va_res = VAAPI_CreateSurfaces(va_display_, pic_width_,
                                         pic_height_, VA_RT_FORMAT_YUV420,
                                         GetRequiredNumOfPictures(),
                                         va_surface_ids_);
  VA_SUCCESS_OR_RETURN(va_res, "vaCreateSurfaces failed", false);

  DCHECK(decode_surfaces_.empty());
  // And create a context associated with them.
  va_res = VAAPI_CreateContext(va_display_, va_config_id_,
                               pic_width_, pic_height_, VA_PROGRESSIVE,
                               va_surface_ids_, GetRequiredNumOfPictures(),
                               &va_context_id_);

  if (va_res != VA_STATUS_SUCCESS) {
    DVLOG(1) << "Error creating a decoding surface (binding to texture?)";
    VAAPI_DestroySurfaces(va_display_, va_surface_ids_,
                          GetRequiredNumOfPictures());
    return false;
  }

  va_context_created_ = true;

  return true;
}

void VaapiH264Decoder::DestroyVASurfaces() {
  DCHECK(state_ == kDecoding || state_ == kError || state_ == kAfterReset);
  decode_surfaces_.clear();

  // This can happen if we fail during DecodeInitial.
  if (!va_context_created_)
    return;

  VAStatus va_res = VAAPI_DestroyContext(va_display_, va_context_id_);
  VA_LOG_ON_ERROR(va_res, "vaDestroyContext failed");

  va_res = VAAPI_DestroySurfaces(va_display_, va_surface_ids_,
                                 GetRequiredNumOfPictures());
  VA_LOG_ON_ERROR(va_res, "vaDestroySurfaces failed");

  va_context_created_ = false;
}

void VaapiH264Decoder::DestroyPendingBuffers() {
  while (!pending_slice_bufs_.empty()) {
    VABufferID buffer = pending_slice_bufs_.front();
    VAStatus va_res = VAAPI_DestroyBuffer(va_display_, buffer);
    VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");
    pending_slice_bufs_.pop();
  }
  while (!pending_va_bufs_.empty()) {
    VABufferID buffer = pending_va_bufs_.front();
    VAStatus va_res = VAAPI_DestroyBuffer(va_display_, buffer);
    VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");
    pending_va_bufs_.pop();
  }
}

// Fill |va_pic| with default/neutral values.
static void InitVAPicture(VAPictureH264* va_pic) {
  memset(va_pic, 0, sizeof(*va_pic));
  va_pic->picture_id = VA_INVALID_ID;
  va_pic->flags = VA_PICTURE_H264_INVALID;
}

void VaapiH264Decoder::FillVAPicture(VAPictureH264 *va_pic, H264Picture* pic) {
  DCHECK(pic);
  POCToDecodeSurfaces::iterator iter = poc_to_decode_surfaces_.find(
      pic->pic_order_cnt);
  if (iter == poc_to_decode_surfaces_.end()) {
    DVLOG(1) << "Could not find surface with POC: " << pic->pic_order_cnt;
    // Cannot provide a ref picture, will corrupt output, but may be able
    // to recover.
    InitVAPicture(va_pic);
    return;
  }

  va_pic->picture_id = iter->second->va_surface_id();
  va_pic->frame_idx = pic->frame_num;
  va_pic->flags = 0;

  switch (pic->field) {
    case H264Picture::FIELD_NONE:
      break;
    case H264Picture::FIELD_TOP:
      va_pic->flags |= VA_PICTURE_H264_TOP_FIELD;
      break;
    case H264Picture::FIELD_BOTTOM:
      va_pic->flags |= VA_PICTURE_H264_BOTTOM_FIELD;
      break;
  }

  if (pic->ref) {
    va_pic->flags |= pic->long_term ? VA_PICTURE_H264_LONG_TERM_REFERENCE
                                    : VA_PICTURE_H264_SHORT_TERM_REFERENCE;
  }

  va_pic->TopFieldOrderCnt = pic->top_field_order_cnt;
  va_pic->BottomFieldOrderCnt = pic->bottom_field_order_cnt;
}

int VaapiH264Decoder::FillVARefFramesFromDPB(VAPictureH264 *va_pics,
                                             int num_pics) {
  H264DPB::Pictures::reverse_iterator rit;
  int i;

  // Return reference frames in reverse order of insertion.
  // Libva does not document this, but other implementations (e.g. mplayer)
  // do it this way as well.
  for (rit = dpb_.rbegin(), i = 0; rit != dpb_.rend() && i < num_pics; ++rit) {
    if ((*rit)->ref)
      FillVAPicture(&va_pics[i++], *rit);
  }

  return i;
}

// Can only be called when all surfaces are already bound
// to textures (cannot be run at the same time as AssignPictureBuffer).
bool VaapiH264Decoder::AssignSurfaceToPoC(int poc) {
  DCHECK_GT(num_available_decode_surfaces_, 0) << decode_surfaces_.size();

  // Find a surface not currently holding data used for reference and/or
  // to be displayed and mark it as used.
  DecodeSurfaces::iterator iter = decode_surfaces_.begin();
  for (; iter != decode_surfaces_.end(); ++iter) {
    if (!iter->second->available())
      continue;

    --num_available_decode_surfaces_;
    DCHECK_GE(num_available_decode_surfaces_, 0);

    // Associate with input id and poc and mark as unavailable.
    iter->second->Acquire(curr_input_id_, poc);
    DVLOG(4) << "Will use surface " << iter->second->va_surface_id()
             << " for POC " << iter->second->poc()
             << " input ID: " << iter->second->input_id();
    bool inserted = poc_to_decode_surfaces_.insert(std::make_pair(
        poc, iter->second.get())).second;
    DCHECK(inserted);
    return true;
  }

  // Could not find an available surface.
  return false;
}

// Can only be called when all surfaces are already bound
// to textures (cannot be run at the same time as AssignPictureBuffer).
void VaapiH264Decoder::UnassignSurfaceFromPoC(int poc) {
  DecodeSurface* dec_surface;
  POCToDecodeSurfaces::iterator it = poc_to_decode_surfaces_.find(poc);
  if (it == poc_to_decode_surfaces_.end()) {
    DVLOG(1) << "Asked to unassign an unassigned POC " << poc;
    return;
  }
  dec_surface = it->second;
  DVLOG(4) << "POC " << poc << " no longer using surface "
           << dec_surface->va_surface_id();
  poc_to_decode_surfaces_.erase(it);

  dec_surface->set_used(false);
  if (!dec_surface->at_client()) {
    dec_surface->Release();
    ++num_available_decode_surfaces_;
  }
}

// Fill a VAPictureParameterBufferH264 to be later sent to the HW decoder.
bool VaapiH264Decoder::SendPPS() {
  const H264PPS* pps = parser_.GetPPS(curr_pps_id_);
  DCHECK(pps);

  const H264SPS* sps = parser_.GetSPS(pps->seq_parameter_set_id);
  DCHECK(sps);

  DCHECK(curr_pic_.get());

  VAPictureParameterBufferH264 pic_param;
  memset(&pic_param, 0, sizeof(VAPictureParameterBufferH264));

#define FROM_SPS_TO_PP(a) pic_param.a = sps->a;
#define FROM_SPS_TO_PP2(a, b) pic_param.b = sps->a;
  FROM_SPS_TO_PP2(pic_width_in_mbs_minus1, picture_width_in_mbs_minus1);
  // This assumes non-interlaced video
  FROM_SPS_TO_PP2(pic_height_in_map_units_minus1,
                  picture_height_in_mbs_minus1);
  FROM_SPS_TO_PP(bit_depth_luma_minus8);
  FROM_SPS_TO_PP(bit_depth_chroma_minus8);
#undef FROM_SPS_TO_PP
#undef FROM_SPS_TO_PP2

#define FROM_SPS_TO_PP_SF(a) pic_param.seq_fields.bits.a = sps->a;
#define FROM_SPS_TO_PP_SF2(a, b) pic_param.seq_fields.bits.b = sps->a;
  FROM_SPS_TO_PP_SF(chroma_format_idc);
  FROM_SPS_TO_PP_SF2(separate_colour_plane_flag,
                     residual_colour_transform_flag);
  FROM_SPS_TO_PP_SF(gaps_in_frame_num_value_allowed_flag);
  FROM_SPS_TO_PP_SF(frame_mbs_only_flag);
  FROM_SPS_TO_PP_SF(mb_adaptive_frame_field_flag);
  FROM_SPS_TO_PP_SF(direct_8x8_inference_flag);
  pic_param.seq_fields.bits.MinLumaBiPredSize8x8 = (sps->level_idc >= 31);
  FROM_SPS_TO_PP_SF(log2_max_frame_num_minus4);
  FROM_SPS_TO_PP_SF(pic_order_cnt_type);
  FROM_SPS_TO_PP_SF(log2_max_pic_order_cnt_lsb_minus4);
  FROM_SPS_TO_PP_SF(delta_pic_order_always_zero_flag);
#undef FROM_SPS_TO_PP_SF
#undef FROM_SPS_TO_PP_SF2

#define FROM_PPS_TO_PP(a) pic_param.a = pps->a;
  FROM_PPS_TO_PP(num_slice_groups_minus1);
  pic_param.slice_group_map_type = 0;
  pic_param.slice_group_change_rate_minus1 = 0;
  FROM_PPS_TO_PP(pic_init_qp_minus26);
  FROM_PPS_TO_PP(pic_init_qs_minus26);
  FROM_PPS_TO_PP(chroma_qp_index_offset);
  FROM_PPS_TO_PP(second_chroma_qp_index_offset);
#undef FROM_PPS_TO_PP

#define FROM_PPS_TO_PP_PF(a) pic_param.pic_fields.bits.a = pps->a;
#define FROM_PPS_TO_PP_PF2(a, b) pic_param.pic_fields.bits.b = pps->a;
  FROM_PPS_TO_PP_PF(entropy_coding_mode_flag);
  FROM_PPS_TO_PP_PF(weighted_pred_flag);
  FROM_PPS_TO_PP_PF(weighted_bipred_idc);
  FROM_PPS_TO_PP_PF(transform_8x8_mode_flag);

  pic_param.pic_fields.bits.field_pic_flag = 0;
  FROM_PPS_TO_PP_PF(constrained_intra_pred_flag);
  FROM_PPS_TO_PP_PF2(bottom_field_pic_order_in_frame_present_flag,
                pic_order_present_flag);
  FROM_PPS_TO_PP_PF(deblocking_filter_control_present_flag);
  FROM_PPS_TO_PP_PF(redundant_pic_cnt_present_flag);
  pic_param.pic_fields.bits.reference_pic_flag = curr_pic_->ref;
#undef FROM_PPS_TO_PP_PF
#undef FROM_PPS_TO_PP_PF2

  pic_param.frame_num = curr_pic_->frame_num;

  InitVAPicture(&pic_param.CurrPic);
  FillVAPicture(&pic_param.CurrPic, curr_pic_.get());

  // Init reference pictures' array.
  for (int i = 0; i < 16; ++i)
    InitVAPicture(&pic_param.ReferenceFrames[i]);

  // And fill it with picture info from DPB.
  FillVARefFramesFromDPB(pic_param.ReferenceFrames,
                         arraysize(pic_param.ReferenceFrames));

  pic_param.num_ref_frames = sps->max_num_ref_frames;

  // Allocate a buffer in driver for this parameter buffer and upload data.
  VABufferID pic_param_buf_id;
  VAStatus va_res = VAAPI_CreateBuffer(va_display_, va_context_id_,
                                       VAPictureParameterBufferType,
                                       sizeof(VAPictureParameterBufferH264),
                                       1, &pic_param, &pic_param_buf_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed to create a buffer for PPS", false);

  // Queue its VA buffer ID to be committed on HW decode run.
  pending_va_bufs_.push(pic_param_buf_id);

  return true;
}

// Fill a VAIQMatrixBufferH264 to be later sent to the HW decoder.
bool VaapiH264Decoder::SendIQMatrix() {
  const H264PPS* pps = parser_.GetPPS(curr_pps_id_);
  DCHECK(pps);

  VAIQMatrixBufferH264 iq_matrix_buf;
  memset(&iq_matrix_buf, 0, sizeof(VAIQMatrixBufferH264));

  if (pps->pic_scaling_matrix_present_flag) {
    for (int i = 0; i < 6; ++i) {
      for (int j = 0; j < 16; ++j)
        iq_matrix_buf.ScalingList4x4[i][j] = pps->scaling_list4x4[i][j];
    }

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 64; ++j)
        iq_matrix_buf.ScalingList8x8[i][j] = pps->scaling_list8x8[i][j];
    }
  } else {
    const H264SPS* sps = parser_.GetSPS(pps->seq_parameter_set_id);
    DCHECK(sps);
    for (int i = 0; i < 6; ++i) {
      for (int j = 0; j < 16; ++j)
        iq_matrix_buf.ScalingList4x4[i][j] = sps->scaling_list4x4[i][j];
    }

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 64; ++j)
        iq_matrix_buf.ScalingList8x8[i][j] = sps->scaling_list8x8[i][j];
    }
  }

  // Allocate a buffer in driver for this parameter buffer and upload data.
  VABufferID iq_matrix_buf_id;
  VAStatus va_res = VAAPI_CreateBuffer(va_display_, va_context_id_,
                                       VAIQMatrixBufferType,
                                       sizeof(VAIQMatrixBufferH264), 1,
                                       &iq_matrix_buf, &iq_matrix_buf_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed to create a buffer for IQMatrix",
                       false);

  // Queue its VA buffer ID to be committed on HW decode run.
  pending_va_bufs_.push(iq_matrix_buf_id);

  return true;
}

bool VaapiH264Decoder::SendVASliceParam(H264SliceHeader* slice_hdr) {
  const H264PPS* pps = parser_.GetPPS(slice_hdr->pic_parameter_set_id);
  DCHECK(pps);

  const H264SPS* sps = parser_.GetSPS(pps->seq_parameter_set_id);
  DCHECK(sps);

  VASliceParameterBufferH264 slice_param;
  memset(&slice_param, 0, sizeof(VASliceParameterBufferH264));

  slice_param.slice_data_size = slice_hdr->nalu_size;
  slice_param.slice_data_offset = 0;
  slice_param.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
  slice_param.slice_data_bit_offset = slice_hdr->header_bit_size;

#define SHDRToSP(a) slice_param.a = slice_hdr->a;
  SHDRToSP(first_mb_in_slice);
  slice_param.slice_type = slice_hdr->slice_type % 5;
  SHDRToSP(direct_spatial_mv_pred_flag);

  // TODO posciak: make sure parser sets those even when override flags
  // in slice header is off.
  SHDRToSP(num_ref_idx_l0_active_minus1);
  SHDRToSP(num_ref_idx_l1_active_minus1);
  SHDRToSP(cabac_init_idc);
  SHDRToSP(slice_qp_delta);
  SHDRToSP(disable_deblocking_filter_idc);
  SHDRToSP(slice_alpha_c0_offset_div2);
  SHDRToSP(slice_beta_offset_div2);

  if (((slice_hdr->IsPSlice() || slice_hdr->IsSPSlice()) &&
       pps->weighted_pred_flag) ||
      (slice_hdr->IsBSlice() && pps->weighted_bipred_idc == 1)) {
    SHDRToSP(luma_log2_weight_denom);
    SHDRToSP(chroma_log2_weight_denom);

    SHDRToSP(luma_weight_l0_flag);
    SHDRToSP(luma_weight_l1_flag);

    SHDRToSP(chroma_weight_l0_flag);
    SHDRToSP(chroma_weight_l1_flag);

    for (int i = 0; i <= slice_param.num_ref_idx_l0_active_minus1; ++i) {
      slice_param.luma_weight_l0[i] =
          slice_hdr->pred_weight_table_l0.luma_weight[i];
      slice_param.luma_offset_l0[i] =
          slice_hdr->pred_weight_table_l0.luma_offset[i];

      for (int j = 0; j < 2; ++j) {
        slice_param.chroma_weight_l0[i][j] =
            slice_hdr->pred_weight_table_l0.chroma_weight[i][j];
        slice_param.chroma_offset_l0[i][j] =
            slice_hdr->pred_weight_table_l0.chroma_offset[i][j];
      }
    }

    if (slice_hdr->IsBSlice()) {
      for (int i = 0; i <= slice_param.num_ref_idx_l1_active_minus1; ++i) {
        slice_param.luma_weight_l1[i] =
            slice_hdr->pred_weight_table_l1.luma_weight[i];
        slice_param.luma_offset_l1[i] =
            slice_hdr->pred_weight_table_l1.luma_offset[i];

        for (int j = 0; j < 2; ++j) {
          slice_param.chroma_weight_l1[i][j] =
              slice_hdr->pred_weight_table_l1.chroma_weight[i][j];
          slice_param.chroma_offset_l1[i][j] =
              slice_hdr->pred_weight_table_l1.chroma_offset[i][j];
        }
      }
    }
  }

  for (int i = 0; i < 32; ++i) {
    InitVAPicture(&slice_param.RefPicList0[i]);
    InitVAPicture(&slice_param.RefPicList1[i]);
  }

  int i;
  H264Picture::PtrVector::iterator it;
  for (it = ref_pic_list0_.begin(), i = 0; it != ref_pic_list0_.end() && *it;
       ++it, ++i)
    FillVAPicture(&slice_param.RefPicList0[i], *it);
  for (it = ref_pic_list1_.begin(), i = 0; it != ref_pic_list1_.end() && *it;
       ++it, ++i)
    FillVAPicture(&slice_param.RefPicList1[i], *it);

  // Allocate a buffer in driver for this parameter buffer and upload data.
  VABufferID slice_param_buf_id;
  VAStatus va_res = VAAPI_CreateBuffer(va_display_, va_context_id_,
                                       VASliceParameterBufferType,
                                       sizeof(VASliceParameterBufferH264),
                                       1, &slice_param, &slice_param_buf_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed creating a buffer for slice param",
                       false);

  // Queue its VA buffer ID to be committed on HW decode run.
  pending_slice_bufs_.push(slice_param_buf_id);

  return true;
}

bool VaapiH264Decoder::SendSliceData(const uint8* ptr, size_t size) {
    // Can't help it, blame libva...
    void* non_const_ptr = const_cast<uint8*>(ptr);

    VABufferID slice_data_buf_id;
    VAStatus va_res = VAAPI_CreateBuffer(va_display_, va_context_id_,
                                         VASliceDataBufferType, size, 1,
                                         non_const_ptr, &slice_data_buf_id);
    VA_SUCCESS_OR_RETURN(va_res, "Failed creating a buffer for slice data",
                         false);

    pending_slice_bufs_.push(slice_data_buf_id);
    return true;
}

bool VaapiH264Decoder::QueueSlice(H264SliceHeader* slice_hdr) {
  DCHECK(curr_pic_.get());

  if (!SendVASliceParam(slice_hdr))
    return false;

  if (!SendSliceData(slice_hdr->nalu_data, slice_hdr->nalu_size))
    return false;

  return true;
}

bool VaapiH264Decoder::DecodePicture() {
  DCHECK(!frame_ready_at_hw_);
  DCHECK(curr_pic_.get());

  // Find the surface associated with the picture to be decoded.
  DecodeSurface* dec_surface =
      poc_to_decode_surfaces_[curr_pic_->pic_order_cnt];
  DVLOG(4) << "Decoding POC " << curr_pic_->pic_order_cnt
           << " into surface " << dec_surface->va_surface_id();

  DVLOG(4) << "Pending VA bufs to commit: " << pending_va_bufs_.size();
  DVLOG(4) << "Pending slice bufs to commit: " << pending_slice_bufs_.size();

  DCHECK(pending_slice_bufs_.size());
  scoped_ptr<std::queue<VABufferID> > va_bufs(new std::queue<VABufferID>());
  std::swap(*va_bufs, pending_va_bufs_);
  scoped_ptr<std::queue<VABufferID> > slice_bufs(new std::queue<VABufferID>());
  std::swap(*slice_bufs, pending_slice_bufs_);

  // Fire up a parallel job on the GPU on the ChildThread to decode and put
  // the decoded/converted/scaled picture into the pixmap.
  // Callee will take care of freeing the buffer queues.
  submit_decode_cb_.Run(
      dec_surface->picture_buffer_id(), va_bufs.Pass(), slice_bufs.Pass());

  // Used to notify clients that we had sufficient data to start decoding
  // a new frame.
  frame_ready_at_hw_ = true;

  return true;
}

void VaapiH264Decoder::DestroyBuffers(size_t num_va_buffers,
                                      const VABufferID* va_buffers) {
  for (size_t i = 0; i < num_va_buffers; ++i) {
    VAStatus va_res = VAAPI_DestroyBuffer(va_display_, va_buffers[i]);
    VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");
  }
}

// TODO(posciak) start using vaMapBuffer instead of vaCreateBuffer wherever
// possible.
bool VaapiH264Decoder::SubmitDecode(
    int32 picture_buffer_id,
    scoped_ptr<std::queue<VABufferID> > va_bufs,
    scoped_ptr<std::queue<VABufferID> > slice_bufs) {

  static const size_t kMaxVABuffers = 32;
  DCHECK_LE(va_bufs->size(), kMaxVABuffers);
  DCHECK_LE(slice_bufs->size(), kMaxVABuffers);

  DecodeSurfaces::iterator it = decode_surfaces_.find(picture_buffer_id);
  if (it == decode_surfaces_.end()) {
    DVLOG(1) << "Asked to put an invalid buffer";
    return false;
  }

  // Get ready to decode into surface.
  VAStatus va_res = VAAPI_BeginPicture(va_display_, va_context_id_,
                                       it->second->va_surface_id());
  VA_SUCCESS_OR_RETURN(va_res, "vaBeginPicture failed", false);

  // Put buffer IDs for pending parameter buffers into va_buffers[].
  VABufferID va_buffers[kMaxVABuffers];
  size_t num_va_buffers = va_bufs->size();
  for (size_t i = 0; i < num_va_buffers && i < kMaxVABuffers; ++i) {
    va_buffers[i] = va_bufs->front();
    va_bufs->pop();
  }
  base::Closure va_buffers_callback =
      base::Bind(&VaapiH264Decoder::DestroyBuffers, base::Unretained(this),
                 num_va_buffers, va_buffers);
  base::ScopedClosureRunner va_buffers_deleter(va_buffers_callback);

  // And send them to the HW decoder.
  va_res = VAAPI_RenderPicture(va_display_, va_context_id_, va_buffers,
                               num_va_buffers);
  VA_SUCCESS_OR_RETURN(va_res, "vaRenderPicture for va_bufs failed", false);

  DVLOG(4) << "Committed " << num_va_buffers << "VA buffers";

  // Put buffer IDs for pending slice data buffers into slice_buffers[].
  VABufferID slice_buffers[kMaxVABuffers];
  size_t num_slice_buffers = slice_bufs->size();
  for (size_t i = 0; i < num_slice_buffers && i < kMaxVABuffers; ++i) {
    slice_buffers[i] = slice_bufs->front();
    slice_bufs->pop();
  }
  base::Closure va_slices_callback =
      base::Bind(&VaapiH264Decoder::DestroyBuffers, base::Unretained(this),
                 num_slice_buffers, slice_buffers);
  base::ScopedClosureRunner slice_buffers_deleter(va_slices_callback);

  // And send them to the Hw decoder.
  va_res = VAAPI_RenderPicture(va_display_, va_context_id_, slice_buffers,
                               num_slice_buffers);
  VA_SUCCESS_OR_RETURN(va_res, "vaRenderPicture for slices failed", false);

  DVLOG(4) << "Committed " << num_slice_buffers << "slice buffers";

  // Instruct HW decoder to start processing committed buffers (decode this
  // picture). This does not block until the end of decode.
  va_res = VAAPI_EndPicture(va_display_, va_context_id_);
  VA_SUCCESS_OR_RETURN(va_res, "vaEndPicture failed", false);

  DVLOG(3) << "Will output from VASurface " << it->second->va_surface_id()
           << " to texture id " << it->second->texture_id();

  return it->second->Sync();
}


bool VaapiH264Decoder::InitCurrPicture(H264SliceHeader* slice_hdr) {
  DCHECK(curr_pic_.get());

  memset(curr_pic_.get(), 0, sizeof(H264Picture));

  curr_pic_->idr = slice_hdr->idr_pic_flag;

  if (slice_hdr->field_pic_flag) {
    curr_pic_->field = slice_hdr->bottom_field_flag ? H264Picture::FIELD_BOTTOM
                                                    : H264Picture::FIELD_TOP;
  } else {
    curr_pic_->field = H264Picture::FIELD_NONE;
  }

  curr_pic_->ref = slice_hdr->nal_ref_idc != 0;
  // This assumes non-interlaced stream.
  curr_pic_->frame_num = curr_pic_->pic_num = slice_hdr->frame_num;

  if (!CalculatePicOrderCounts(slice_hdr))
    return false;

  // Try to get an empty surface to decode this picture to.
  if (!AssignSurfaceToPoC(curr_pic_->pic_order_cnt)) {
    DVLOG(1) << "Failed getting a free surface for a picture";
    return false;
  }

  curr_pic_->long_term_reference_flag = slice_hdr->long_term_reference_flag;
  curr_pic_->adaptive_ref_pic_marking_mode_flag =
      slice_hdr->adaptive_ref_pic_marking_mode_flag;

  // If the slice header indicates we will have to perform reference marking
  // process after this picture is decoded, store required data for that
  // purpose.
  if (slice_hdr->adaptive_ref_pic_marking_mode_flag) {
    COMPILE_ASSERT(sizeof(curr_pic_->ref_pic_marking) ==
                   sizeof(slice_hdr->ref_pic_marking),
                   ref_pic_marking_array_sizes_do_not_match);
    memcpy(curr_pic_->ref_pic_marking, slice_hdr->ref_pic_marking,
           sizeof(curr_pic_->ref_pic_marking));
  }

  return true;
}

bool VaapiH264Decoder::CalculatePicOrderCounts(H264SliceHeader* slice_hdr) {
  DCHECK_NE(curr_sps_id_, -1);
  const H264SPS* sps = parser_.GetSPS(curr_sps_id_);

  int pic_order_cnt_lsb = slice_hdr->pic_order_cnt_lsb;
  curr_pic_->pic_order_cnt_lsb = pic_order_cnt_lsb;

  switch (sps->pic_order_cnt_type) {
    case 0:
      // See spec 8.2.1.1.
      int prev_pic_order_cnt_msb, prev_pic_order_cnt_lsb;
      if (slice_hdr->idr_pic_flag) {
        prev_pic_order_cnt_msb = prev_pic_order_cnt_lsb = 0;
      } else {
        if (prev_ref_has_memmgmnt5_) {
          if (prev_ref_field_ != H264Picture::FIELD_BOTTOM) {
            prev_pic_order_cnt_msb = 0;
            prev_pic_order_cnt_lsb = prev_ref_top_field_order_cnt_;
          } else {
            prev_pic_order_cnt_msb = 0;
            prev_pic_order_cnt_lsb = 0;
          }
        } else {
          prev_pic_order_cnt_msb = prev_ref_pic_order_cnt_msb_;
          prev_pic_order_cnt_lsb = prev_ref_pic_order_cnt_lsb_;
        }
      }

      DCHECK_NE(max_pic_order_cnt_lsb_, 0);
      if ((pic_order_cnt_lsb < prev_pic_order_cnt_lsb) &&
          (prev_pic_order_cnt_lsb - pic_order_cnt_lsb >=
           max_pic_order_cnt_lsb_ / 2)) {
        curr_pic_->pic_order_cnt_msb = prev_pic_order_cnt_msb +
          max_pic_order_cnt_lsb_;
      } else if ((pic_order_cnt_lsb > prev_pic_order_cnt_lsb) &&
          (pic_order_cnt_lsb - prev_pic_order_cnt_lsb >
           max_pic_order_cnt_lsb_ / 2)) {
        curr_pic_->pic_order_cnt_msb = prev_pic_order_cnt_msb -
          max_pic_order_cnt_lsb_;
      } else {
        curr_pic_->pic_order_cnt_msb = prev_pic_order_cnt_msb;
      }

      if (curr_pic_->field != H264Picture::FIELD_BOTTOM) {
        curr_pic_->top_field_order_cnt = curr_pic_->pic_order_cnt_msb +
          pic_order_cnt_lsb;
      }

      if (curr_pic_->field != H264Picture::FIELD_TOP) {
        // TODO posciak: perhaps replace with pic->field?
        if (!slice_hdr->field_pic_flag) {
          curr_pic_->bottom_field_order_cnt = curr_pic_->top_field_order_cnt +
            slice_hdr->delta_pic_order_cnt_bottom;
        } else {
          curr_pic_->bottom_field_order_cnt = curr_pic_->pic_order_cnt_msb +
            pic_order_cnt_lsb;
        }
      }
      break;

    case 1: {
      // See spec 8.2.1.2.
      if (prev_has_memmgmnt5_)
        prev_frame_num_offset_ = 0;

      if (slice_hdr->idr_pic_flag)
        curr_pic_->frame_num_offset = 0;
      else if (prev_frame_num_ > slice_hdr->frame_num)
        curr_pic_->frame_num_offset = prev_frame_num_offset_ + max_frame_num_;
      else
        curr_pic_->frame_num_offset = prev_frame_num_offset_;

      int abs_frame_num = 0;
      if (sps->num_ref_frames_in_pic_order_cnt_cycle != 0)
        abs_frame_num = curr_pic_->frame_num_offset + slice_hdr->frame_num;
      else
        abs_frame_num = 0;

      if (slice_hdr->nal_ref_idc == 0 && abs_frame_num > 0)
        --abs_frame_num;

      int expected_pic_order_cnt = 0;
      if (abs_frame_num > 0) {
        if (sps->num_ref_frames_in_pic_order_cnt_cycle == 0) {
          DVLOG(1) << "Invalid num_ref_frames_in_pic_order_cnt_cycle "
                   << "in stream";
          return false;
        }

        int pic_order_cnt_cycle_cnt = (abs_frame_num - 1) /
            sps->num_ref_frames_in_pic_order_cnt_cycle;
        int frame_num_in_pic_order_cnt_cycle = (abs_frame_num - 1) %
            sps->num_ref_frames_in_pic_order_cnt_cycle;

        expected_pic_order_cnt = pic_order_cnt_cycle_cnt *
            sps->expected_delta_per_pic_order_cnt_cycle;
        // frame_num_in_pic_order_cnt_cycle is verified < 255 in parser
        for (int i = 0; i <= frame_num_in_pic_order_cnt_cycle; ++i)
          expected_pic_order_cnt += sps->offset_for_ref_frame[i];
      }

      if (!slice_hdr->nal_ref_idc)
        expected_pic_order_cnt += sps->offset_for_non_ref_pic;

      if (!slice_hdr->field_pic_flag) {
        curr_pic_->top_field_order_cnt = expected_pic_order_cnt +
            slice_hdr->delta_pic_order_cnt[0];
        curr_pic_->bottom_field_order_cnt = curr_pic_->top_field_order_cnt +
            sps->offset_for_top_to_bottom_field +
            slice_hdr->delta_pic_order_cnt[1];
      } else if (!slice_hdr->bottom_field_flag) {
        curr_pic_->top_field_order_cnt = expected_pic_order_cnt +
            slice_hdr->delta_pic_order_cnt[0];
      } else {
        curr_pic_->bottom_field_order_cnt = expected_pic_order_cnt +
            sps->offset_for_top_to_bottom_field +
            slice_hdr->delta_pic_order_cnt[0];
      }
      break;
    }

    case 2:
      // See spec 8.2.1.3.
      if (prev_has_memmgmnt5_)
        prev_frame_num_offset_ = 0;

      if (slice_hdr->idr_pic_flag)
        curr_pic_->frame_num_offset = 0;
      else if (prev_frame_num_ > slice_hdr->frame_num)
        curr_pic_->frame_num_offset = prev_frame_num_offset_ + max_frame_num_;
      else
        curr_pic_->frame_num_offset = prev_frame_num_offset_;

      int temp_pic_order_cnt;
      if (slice_hdr->idr_pic_flag) {
        temp_pic_order_cnt = 0;
      } else if (!slice_hdr->nal_ref_idc) {
        temp_pic_order_cnt =
            2 * (curr_pic_->frame_num_offset + slice_hdr->frame_num) - 1;
      } else {
        temp_pic_order_cnt = 2 * (curr_pic_->frame_num_offset +
            slice_hdr->frame_num);
      }

      if (!slice_hdr->field_pic_flag) {
        curr_pic_->top_field_order_cnt = temp_pic_order_cnt;
        curr_pic_->bottom_field_order_cnt = temp_pic_order_cnt;
      } else if (slice_hdr->bottom_field_flag) {
        curr_pic_->bottom_field_order_cnt = temp_pic_order_cnt;
      } else {
        curr_pic_->top_field_order_cnt = temp_pic_order_cnt;
      }
      break;

    default:
      DVLOG(1) << "Invalid pic_order_cnt_type: " << sps->pic_order_cnt_type;
      return false;
  }

  switch (curr_pic_->field) {
    case H264Picture::FIELD_NONE:
      curr_pic_->pic_order_cnt = std::min(curr_pic_->top_field_order_cnt,
                                          curr_pic_->bottom_field_order_cnt);
      break;
    case H264Picture::FIELD_TOP:
      curr_pic_->pic_order_cnt = curr_pic_->top_field_order_cnt;
      break;
    case H264Picture::FIELD_BOTTOM:
      curr_pic_->pic_order_cnt = curr_pic_->bottom_field_order_cnt;
      break;
  }

  return true;
}

void VaapiH264Decoder::UpdatePicNums() {
  for (H264DPB::Pictures::iterator it = dpb_.begin(); it != dpb_.end(); ++it) {
    H264Picture* pic = *it;
    DCHECK(pic);
    if (!pic->ref)
      continue;

    // Below assumes non-interlaced stream.
    DCHECK_EQ(pic->field, H264Picture::FIELD_NONE);
    if (pic->long_term) {
      pic->long_term_pic_num = pic->long_term_frame_idx;
    } else {
      if (pic->frame_num > frame_num_)
        pic->frame_num_wrap = pic->frame_num - max_frame_num_;
      else
        pic->frame_num_wrap = pic->frame_num;

      pic->pic_num = pic->frame_num_wrap;
    }
  }
}

struct PicNumDescCompare {
  bool operator()(const H264Picture* a, const H264Picture* b) const {
    return a->pic_num > b->pic_num;
  }
};

struct LongTermPicNumAscCompare {
  bool operator()(const H264Picture* a, const H264Picture* b) const {
    return a->long_term_pic_num < b->long_term_pic_num;
  }
};

void VaapiH264Decoder::ConstructReferencePicListsP(H264SliceHeader* slice_hdr) {
  // RefPicList0 (8.2.4.2.1) [[1] [2]], where:
  // [1] shortterm ref pics sorted by descending pic_num,
  // [2] longterm ref pics by ascending long_term_pic_num.
  DCHECK(ref_pic_list0_.empty() && ref_pic_list1_.empty());
  // First get the short ref pics...
  dpb_.GetShortTermRefPicsAppending(ref_pic_list0_);
  size_t num_short_refs = ref_pic_list0_.size();

  // and sort them to get [1].
  std::sort(ref_pic_list0_.begin(), ref_pic_list0_.end(), PicNumDescCompare());

  // Now get long term pics and sort them by long_term_pic_num to get [2].
  dpb_.GetLongTermRefPicsAppending(ref_pic_list0_);
  std::sort(ref_pic_list0_.begin() + num_short_refs, ref_pic_list0_.end(),
            LongTermPicNumAscCompare());

  // Cut off if we have more than requested in slice header.
  ref_pic_list0_.resize(slice_hdr->num_ref_idx_l0_active_minus1 + 1);
}

struct POCAscCompare {
  bool operator()(const H264Picture* a, const H264Picture* b) const {
    return a->pic_order_cnt < b->pic_order_cnt;
  }
};

struct POCDescCompare {
  bool operator()(const H264Picture* a, const H264Picture* b) const {
    return a->pic_order_cnt > b->pic_order_cnt;
  }
};

void VaapiH264Decoder::ConstructReferencePicListsB(H264SliceHeader* slice_hdr) {
  // RefPicList0 (8.2.4.2.3) [[1] [2] [3]], where:
  // [1] shortterm ref pics with POC < curr_pic's POC sorted by descending POC,
  // [2] shortterm ref pics with POC > curr_pic's POC by ascending POC,
  // [3] longterm ref pics by ascending long_term_pic_num.
  DCHECK(ref_pic_list0_.empty() && ref_pic_list1_.empty());
  dpb_.GetShortTermRefPicsAppending(ref_pic_list0_);
  size_t num_short_refs = ref_pic_list0_.size();

  // First sort ascending, this will put [1] in right place and finish [2].
  std::sort(ref_pic_list0_.begin(), ref_pic_list0_.end(), POCAscCompare());

  // Find first with POC > curr_pic's POC to get first element in [2]...
  H264Picture::PtrVector::iterator iter;
  iter = std::upper_bound(ref_pic_list0_.begin(), ref_pic_list0_.end(),
                          curr_pic_.get(), POCAscCompare());

  // and sort [1] descending, thus finishing sequence [1] [2].
  std::sort(ref_pic_list0_.begin(), iter, POCDescCompare());

  // Now add [3] and sort by ascending long_term_pic_num.
  dpb_.GetLongTermRefPicsAppending(ref_pic_list0_);
  std::sort(ref_pic_list0_.begin() + num_short_refs, ref_pic_list0_.end(),
            LongTermPicNumAscCompare());

  // RefPicList1 (8.2.4.2.4) [[1] [2] [3]], where:
  // [1] shortterm ref pics with POC > curr_pic's POC sorted by ascending POC,
  // [2] shortterm ref pics with POC < curr_pic's POC by descending POC,
  // [3] longterm ref pics by ascending long_term_pic_num.

  dpb_.GetShortTermRefPicsAppending(ref_pic_list1_);
  num_short_refs = ref_pic_list1_.size();

  // First sort by descending POC.
  std::sort(ref_pic_list1_.begin(), ref_pic_list1_.end(), POCDescCompare());

  // Find first with POC < curr_pic's POC to get first element in [2]...
  iter = std::upper_bound(ref_pic_list1_.begin(), ref_pic_list1_.end(),
                          curr_pic_.get(), POCDescCompare());

  // and sort [1] ascending.
  std::sort(ref_pic_list1_.begin(), iter, POCAscCompare());

  // Now add [3] and sort by ascending long_term_pic_num
  dpb_.GetShortTermRefPicsAppending(ref_pic_list1_);
  std::sort(ref_pic_list1_.begin() + num_short_refs, ref_pic_list1_.end(),
            LongTermPicNumAscCompare());

  // If lists identical, swap first two entries in RefPicList1 (spec 8.2.4.2.3)
  if (ref_pic_list1_.size() > 1 &&
      std::equal(ref_pic_list0_.begin(), ref_pic_list0_.end(),
                 ref_pic_list1_.begin()))
    std::swap(ref_pic_list1_[0], ref_pic_list1_[1]);

  // Per 8.2.4.2 it's possible for num_ref_idx_lX_active_minus1 to indicate
  // there should be more ref pics on list than we constructed.
  // Those superfluous ones should be treated as non-reference.
  ref_pic_list0_.resize(slice_hdr->num_ref_idx_l0_active_minus1 + 1);
  ref_pic_list1_.resize(slice_hdr->num_ref_idx_l1_active_minus1 + 1);
}

// See 8.2.4
int VaapiH264Decoder::PicNumF(H264Picture *pic) {
  if (!pic)
      return -1;

  if (!pic->long_term)
      return pic->pic_num;
  else
      return max_pic_num_;
}

// See 8.2.4
int VaapiH264Decoder::LongTermPicNumF(H264Picture *pic) {
  if (pic->ref && pic->long_term)
    return pic->long_term_pic_num;
  else
    return 2 * (max_long_term_frame_idx_ + 1);
}

// Shift elements on the |v| starting from |from| to |to|, inclusive,
// one position to the right and insert pic at |from|.
static void ShiftRightAndInsert(H264Picture::PtrVector *v,
                                int from,
                                int to,
                                H264Picture* pic) {
  DCHECK(pic);
  DCHECK((to + 1 == static_cast<int>(v->size())) ||
         (to + 2 == static_cast<int>(v->size())));

  v->resize(to + 2);

  for (int i = to + 1; i > from; --i)
    (*v)[i] = (*v)[i - 1];

  (*v)[from] = pic;
}

bool VaapiH264Decoder::ModifyReferencePicList(H264SliceHeader *slice_hdr,
                                              int list) {
  int num_ref_idx_lX_active_minus1;
  H264Picture::PtrVector* ref_pic_listx;
  H264ModificationOfPicNum* list_mod;

  // This can process either ref_pic_list0 or ref_pic_list1, depending on
  // the list argument. Set up pointers to proper list to be processed here.
  if (list == 0) {
    if (!slice_hdr->ref_pic_list_modification_flag_l0)
      return true;

    list_mod = slice_hdr->ref_list_l0_modifications;
    num_ref_idx_lX_active_minus1 = ref_pic_list0_.size() - 1;

    ref_pic_listx = &ref_pic_list0_;
  } else {
    if (!slice_hdr->ref_pic_list_modification_flag_l1)
      return true;

    list_mod = slice_hdr->ref_list_l1_modifications;
    num_ref_idx_lX_active_minus1 = ref_pic_list1_.size() - 1;

    ref_pic_listx = &ref_pic_list1_;
  }

  DCHECK_GT(num_ref_idx_lX_active_minus1, 0);

  // Spec 8.2.4.3:
  // Reorder pictures on the list in a way specified in the stream.
  int pic_num_lx_pred = curr_pic_->pic_num;
  int ref_idx_lx = 0;
  int pic_num_lx_no_wrap;
  int pic_num_lx;
  bool done = false;
  H264Picture* pic;
  for (int i = 0; i < H264SliceHeader::kRefListModSize && !done; ++i) {
    switch (list_mod->modification_of_pic_nums_idc) {
      case 0:
      case 1:
        // Modify short reference picture position.
        if (list_mod->modification_of_pic_nums_idc == 0) {
          // Subtract given value from predicted PicNum.
          pic_num_lx_no_wrap = pic_num_lx_pred -
              (static_cast<int>(list_mod->abs_diff_pic_num_minus1) + 1);
          // Wrap around max_pic_num_ if it becomes < 0 as result
          // of subtraction.
          if (pic_num_lx_no_wrap < 0)
            pic_num_lx_no_wrap += max_pic_num_;
        } else {
          // Add given value to predicted PicNum.
          pic_num_lx_no_wrap = pic_num_lx_pred +
              (static_cast<int>(list_mod->abs_diff_pic_num_minus1) + 1);
          // Wrap around max_pic_num_ if it becomes >= max_pic_num_ as result
          // of the addition.
          if (pic_num_lx_no_wrap >= max_pic_num_)
            pic_num_lx_no_wrap -= max_pic_num_;
        }

        // For use in next iteration.
        pic_num_lx_pred = pic_num_lx_no_wrap;

        if (pic_num_lx_no_wrap > curr_pic_->pic_num)
          pic_num_lx = pic_num_lx_no_wrap - max_pic_num_;
        else
          pic_num_lx = pic_num_lx_no_wrap;

        DCHECK_LT(num_ref_idx_lX_active_minus1 + 1,
                  H264SliceHeader::kRefListModSize);
        pic = dpb_.GetShortRefPicByPicNum(pic_num_lx);
        if (!pic) {
          DVLOG(1) << "Malformed stream, no pic num " << pic_num_lx;
          return false;
        }
        ShiftRightAndInsert(ref_pic_listx, ref_idx_lx,
                            num_ref_idx_lX_active_minus1, pic);
        ref_idx_lx++;

        for (int src = ref_idx_lx, dst = ref_idx_lx;
             src <= num_ref_idx_lX_active_minus1 + 1; ++src) {
          if (PicNumF((*ref_pic_listx)[src]) != pic_num_lx)
            (*ref_pic_listx)[dst++] = (*ref_pic_listx)[src];
        }
        break;

      case 2:
        // Modify long term reference picture position.
        DCHECK_LT(num_ref_idx_lX_active_minus1 + 1,
                  H264SliceHeader::kRefListModSize);
        pic = dpb_.GetLongRefPicByLongTermPicNum(list_mod->long_term_pic_num);
        if (!pic) {
          DVLOG(1) << "Malformed stream, no pic num "
                   << list_mod->long_term_pic_num;
          return false;
        }
        ShiftRightAndInsert(ref_pic_listx, ref_idx_lx,
                            num_ref_idx_lX_active_minus1, pic);
        ref_idx_lx++;

        for (int src = ref_idx_lx, dst = ref_idx_lx;
             src <= num_ref_idx_lX_active_minus1 + 1; ++src) {
          if (LongTermPicNumF((*ref_pic_listx)[src])
              != static_cast<int>(list_mod->long_term_pic_num))
            (*ref_pic_listx)[dst++] = (*ref_pic_listx)[src];
        }
        break;

      case 3:
        // End of modification list.
        done = true;
        break;

      default:
        // May be recoverable.
        DVLOG(1) << "Invalid modification_of_pic_nums_idc="
                 << list_mod->modification_of_pic_nums_idc
                 << " in position " << i;
        break;
    }

    ++list_mod;
  }

  // Per NOTE 2 in 8.2.4.3.2, the ref_pic_listx size in the above loop is
  // temporarily made one element longer than the required final list.
  // Resize the list back to its required size.
  ref_pic_listx->resize(num_ref_idx_lX_active_minus1 + 1);

  return true;
}

bool VaapiH264Decoder::OutputPic(H264Picture* pic) {
  DCHECK(!pic->outputted);
  pic->outputted = true;
  POCToDecodeSurfaces::iterator iter = poc_to_decode_surfaces_.find(
      pic->pic_order_cnt);
  if (iter == poc_to_decode_surfaces_.end())
    return false;
  DecodeSurface* dec_surface = iter->second;

  dec_surface->set_at_client(true);
  last_output_poc_ = pic->pic_order_cnt;
  // Notify the client that a picture can be output.
  DVLOG(4) << "Posting output task for POC: " << pic->pic_order_cnt
           << " input_id: " << dec_surface->input_id()
           << "output_id: " << dec_surface->picture_buffer_id();
  output_pic_cb_.Run(dec_surface->input_id(),
                     dec_surface->picture_buffer_id());

  return true;
}

bool VaapiH264Decoder::Flush() {
  // Output all pictures that are waiting to be outputted.
  FinishPrevFrameIfPresent();
  H264Picture::PtrVector to_output;
  dpb_.GetNotOutputtedPicsAppending(to_output);
  // Sort them by ascending POC to output in order.
  std::sort(to_output.begin(), to_output.end(), POCAscCompare());

  H264Picture::PtrVector::iterator it;
  for (it = to_output.begin(); it != to_output.end(); ++it) {
    if (!OutputPic(*it)) {
      DVLOG(1) << "Failed to output pic POC: " << (*it)->pic_order_cnt;
      return false;
    }
  }

  // And clear DPB contents, marking the pictures as unused first.
  // The surfaces will be released after they have been displayed and returned.
  for (H264DPB::Pictures::iterator it = dpb_.begin(); it != dpb_.end(); ++it) {
    UnassignSurfaceFromPoC((*it)->pic_order_cnt);
  }
  dpb_.Clear();
  last_output_poc_ = 0;

  return true;
}

bool VaapiH264Decoder::StartNewFrame(H264SliceHeader* slice_hdr) {
  // TODO posciak: add handling of max_num_ref_frames per spec.

  // If the new frame is an IDR, output what's left to output and clear DPB
  if (slice_hdr->idr_pic_flag) {
    // (unless we are explicitly instructed not to do so).
    if (!slice_hdr->no_output_of_prior_pics_flag) {
      // Output DPB contents.
      if (!Flush())
        return false;
    }
    dpb_.Clear();
    last_output_poc_ = 0;
  }

  // curr_pic_ should have either been added to DPB or discarded when finishing
  // the last frame. DPB is responsible for releasing that memory once it's
  // not needed anymore.
  DCHECK(!curr_pic_.get());
  curr_pic_.reset(new H264Picture);
  CHECK(curr_pic_.get());

  if (!InitCurrPicture(slice_hdr))
    return false;

  DCHECK_GT(max_frame_num_, 0);

  UpdatePicNums();

  // Prepare reference picture lists if required (B and S/SP slices).
  ref_pic_list0_.clear();
  ref_pic_list1_.clear();
  if (slice_hdr->IsPSlice() || slice_hdr->IsSPSlice()) {
    ConstructReferencePicListsP(slice_hdr);
    if (!ModifyReferencePicList(slice_hdr, 0))
      return false;
  } else if (slice_hdr->IsBSlice()) {
    ConstructReferencePicListsB(slice_hdr);
    if (!ModifyReferencePicList(slice_hdr, 0))
      return false;
    if (!ModifyReferencePicList(slice_hdr, 1))
      return false;
  }

  // Send parameter buffers before each new picture, before the first slice.
  if (!SendPPS())
    return false;

  if (!SendIQMatrix())
    return false;

  if (!QueueSlice(slice_hdr))
    return false;

  return true;
}

bool VaapiH264Decoder::HandleMemoryManagementOps() {
  // 8.2.5.4
  for (unsigned int i = 0; i < arraysize(curr_pic_->ref_pic_marking); ++i) {
    // Code below does not support interlaced stream (per-field pictures).
    H264DecRefPicMarking* ref_pic_marking = &curr_pic_->ref_pic_marking[i];
    H264Picture* to_mark;
    int pic_num_x;

    switch (ref_pic_marking->memory_mgmnt_control_operation) {
      case 0:
        // Normal end of operations' specification.
        return true;

      case 1:
        // Mark a short term reference picture as unused so it can be removed
        // if outputted.
        pic_num_x = curr_pic_->pic_num -
            (ref_pic_marking->difference_of_pic_nums_minus1 + 1);
        to_mark = dpb_.GetShortRefPicByPicNum(pic_num_x);
        if (to_mark) {
          to_mark->ref = false;
        } else {
          DVLOG(1) << "Invalid short ref pic num to unmark";
          return false;
        }
        break;

      case 2:
        // Mark a long term reference picture as unused so it can be removed
        // if outputted.
        to_mark = dpb_.GetLongRefPicByLongTermPicNum(
            ref_pic_marking->long_term_pic_num);
        if (to_mark) {
          to_mark->ref = false;
        } else {
          DVLOG(1) << "Invalid long term ref pic num to unmark";
          return false;
        }
        break;

      case 3:
        // Mark a short term reference picture as long term reference.
        pic_num_x = curr_pic_->pic_num -
            (ref_pic_marking->difference_of_pic_nums_minus1 + 1);
        to_mark = dpb_.GetShortRefPicByPicNum(pic_num_x);
        if (to_mark) {
          DCHECK(to_mark->ref && !to_mark->long_term);
          to_mark->long_term = true;
          to_mark->long_term_frame_idx = ref_pic_marking->long_term_frame_idx;
        } else {
          DVLOG(1) << "Invalid short term ref pic num to mark as long ref";
          return false;
        }
        break;

      case 4: {
        // Unmark all reference pictures with long_term_frame_idx over new max.
        max_long_term_frame_idx_
            = ref_pic_marking->max_long_term_frame_idx_plus1 - 1;
        H264Picture::PtrVector long_terms;
        dpb_.GetLongTermRefPicsAppending(long_terms);
        for (size_t i = 0; i < long_terms.size(); ++i) {
          H264Picture* pic = long_terms[i];
          DCHECK(pic->ref && pic->long_term);
          // Ok to cast, max_long_term_frame_idx is much smaller than 16bit.
          if (pic->long_term_frame_idx >
              static_cast<int>(max_long_term_frame_idx_))
            pic->ref = false;
        }
        break;
      }

      case 5:
        // Unmark all reference pictures.
        dpb_.MarkAllUnusedForRef();
        max_long_term_frame_idx_ = -1;
        curr_pic_->mem_mgmt_5 = true;
        break;

      case 6: {
        // Replace long term reference pictures with current picture.
        // First unmark if any existing with this long_term_frame_idx...
        H264Picture::PtrVector long_terms;
        dpb_.GetLongTermRefPicsAppending(long_terms);
        for (size_t i = 0; i < long_terms.size(); ++i) {
          H264Picture* pic = long_terms[i];
          DCHECK(pic->ref && pic->long_term);
          // Ok to cast, long_term_frame_idx is much smaller than 16bit.
          if (pic->long_term_frame_idx ==
              static_cast<int>(ref_pic_marking->long_term_frame_idx))
            pic->ref = false;
        }

        // and mark the current one instead.
        curr_pic_->ref = true;
        curr_pic_->long_term = true;
        curr_pic_->long_term_frame_idx = ref_pic_marking->long_term_frame_idx;
        break;
      }

      default:
        // Would indicate a bug in parser.
        NOTREACHED();
    }
  }

  return true;
}

// This method ensures that DPB does not overflow, either by removing
// reference pictures as specified in the stream, or using a sliding window
// procedure to remove the oldest one.
// It also performs marking and unmarking pictures as reference.
// See spac 8.2.5.1.
void VaapiH264Decoder::ReferencePictureMarking() {
  if (curr_pic_->idr) {
    // If current picture is an IDR, all reference pictures are unmarked.
    dpb_.MarkAllUnusedForRef();

    if (curr_pic_->long_term_reference_flag) {
      curr_pic_->long_term = true;
      curr_pic_->long_term_frame_idx = 0;
      max_long_term_frame_idx_ = 0;
    } else {
      curr_pic_->long_term = false;
      max_long_term_frame_idx_ = -1;
    }
  } else {
    if (!curr_pic_->adaptive_ref_pic_marking_mode_flag) {
      // If non-IDR, and the stream does not indicate what we should do to
      // ensure DPB doesn't overflow, discard oldest picture.
      // See spec 8.2.5.3.
      if (curr_pic_->field == H264Picture::FIELD_NONE) {
        DCHECK_LE(dpb_.CountRefPics(),
            std::max<int>(parser_.GetSPS(curr_sps_id_)->max_num_ref_frames,
                          1));
        if (dpb_.CountRefPics() ==
            std::max<int>(parser_.GetSPS(curr_sps_id_)->max_num_ref_frames,
                          1)) {
          // Max number of reference pics reached,
          // need to remove one of the short term ones.
          // Find smallest frame_num_wrap short reference picture and mark
          // it as unused.
          H264Picture* to_unmark = dpb_.GetLowestFrameNumWrapShortRefPic();
          if (to_unmark == NULL) {
            DVLOG(1) << "Couldn't find a short ref picture to unmark";
            return;
          }
          to_unmark->ref = false;
        }
      } else {
        // Shouldn't get here.
        DVLOG(1) << "Interlaced video not supported.";
      }
    } else {
      // Stream has instructions how to discard pictures from DPB and how
      // to mark/unmark existing reference pictures. Do it.
      // Spec 8.2.5.4.
      if (curr_pic_->field == H264Picture::FIELD_NONE) {
        HandleMemoryManagementOps();
      } else {
        // Shouldn't get here.
        DVLOG(1) << "Interlaced video not supported.";
      }
    }
  }
}

bool VaapiH264Decoder::FinishPicture() {
  DCHECK(curr_pic_.get());

  // Finish processing previous picture.
  // Start by storing previous reference picture data for later use,
  // if picture being finished is a reference picture.
  if (curr_pic_->ref) {
    ReferencePictureMarking();
    prev_ref_has_memmgmnt5_ = curr_pic_->mem_mgmt_5;
    prev_ref_top_field_order_cnt_ = curr_pic_->top_field_order_cnt;
    prev_ref_pic_order_cnt_msb_ = curr_pic_->pic_order_cnt_msb;
    prev_ref_pic_order_cnt_lsb_ = curr_pic_->pic_order_cnt_lsb;
    prev_ref_field_ = curr_pic_->field;
  }
  prev_has_memmgmnt5_ = curr_pic_->mem_mgmt_5;
  prev_frame_num_offset_ = curr_pic_->frame_num_offset;

  // Remove unused (for reference or later output) pictures from DPB, marking
  // them as such.
  for (H264DPB::Pictures::iterator it = dpb_.begin(); it != dpb_.end(); ++it) {
    if ((*it)->outputted && !(*it)->ref)
      UnassignSurfaceFromPoC((*it)->pic_order_cnt);
  }
  dpb_.RemoveUnused();

  DVLOG(4) << "Finishing picture, DPB entries: " << dpb_.size()
           << " Num available dec surfaces: "
           << num_available_decode_surfaces_;

  // Whatever happens below, curr_pic_ will stop managing the pointer to the
  // picture after this function returns. The ownership will either be
  // transferred to DPB, if the image is still needed (for output and/or
  // reference), or the memory will be released if we manage to output it here
  // without having to store it for future reference.
  scoped_ptr<H264Picture> pic(curr_pic_.release());

  // Get all pictures that haven't been outputted yet.
  H264Picture::PtrVector not_outputted;
  // TODO(posciak): pass as pointer, not reference (violates coding style).
  dpb_.GetNotOutputtedPicsAppending(not_outputted);
  // Include the one we've just decoded.
  not_outputted.push_back(pic.get());
  // Sort in output order.
  std::sort(not_outputted.begin(), not_outputted.end(), POCAscCompare());

  // Try to output as many pictures as we can. A picture can be output
  // if its POC is next after the previously outputted one (which means
  // last_output_poc_ + 2, because POCs are incremented by 2 to accommodate
  // fields when decoding interleaved streams). POC can also be equal to
  // last outputted picture's POC when it wraps around back to 0.
  // If the outputted picture is not a reference picture, it doesn't have
  // to remain in the DPB and can be removed.
  H264Picture::PtrVector::iterator output_candidate = not_outputted.begin();
  for (; output_candidate != not_outputted.end() &&
      (*output_candidate)->pic_order_cnt <= last_output_poc_ + 2;
      ++output_candidate) {
    DCHECK_GE((*output_candidate)->pic_order_cnt, last_output_poc_);
    if (!OutputPic(*output_candidate))
      return false;

    if (!(*output_candidate)->ref) {
      // Current picture hasn't been inserted into DPB yet, so don't remove it
      // if we managed to output it immediately.
      if (*output_candidate != pic.get())
        dpb_.RemoveByPOC((*output_candidate)->pic_order_cnt);
      // Mark as unused.
      UnassignSurfaceFromPoC((*output_candidate)->pic_order_cnt);
    }
  }

  // If we haven't managed to output the picture that we just decoded, or if
  // it's a reference picture, we have to store it in DPB.
  if (!pic->outputted || pic->ref) {
    if (dpb_.IsFull()) {
      // If we haven't managed to output anything to free up space in DPB
      // to store this picture, it's an error in the stream.
      DVLOG(1) << "Could not free up space in DPB!";
      return false;
    }

    dpb_.StorePic(pic.release());
  }

  return true;
}

bool VaapiH264Decoder::ProcessSPS(int sps_id) {
  const H264SPS* sps = parser_.GetSPS(sps_id);
  DCHECK(sps);

  if (sps->frame_mbs_only_flag == 0) {
    // Fields/interlaced video not supported.
    DVLOG(1) << "frame_mbs_only_flag != 1 not supported";
    return false;
  }

  if (sps->gaps_in_frame_num_value_allowed_flag) {
    DVLOG(1) << "Gaps in frame numbers not supported";
    return false;
  }

  curr_sps_id_ = sps->seq_parameter_set_id;

  // Calculate picture height/width (spec 7.4.2.1.1, 7.4.3).
  int width = 16 * (sps->pic_width_in_mbs_minus1 + 1);
  int height = 16 * (2 - sps->frame_mbs_only_flag) *
      (sps->pic_height_in_map_units_minus1 + 1);

  if ((pic_width_ != -1 || pic_height_ != -1) &&
      (width != pic_width_ || height != pic_height_)) {
    DVLOG(1) << "Picture size changed mid-stream";
    return false;
  }

  pic_width_ = width;
  pic_height_ = height;
  DVLOG(1) << "New picture size: " << pic_width_ << "x" << pic_height_;

  max_pic_order_cnt_lsb_ = 1 << (sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
  max_frame_num_ = 1 << (sps->log2_max_frame_num_minus4 + 4);

  return true;
}

bool VaapiH264Decoder::ProcessPPS(int pps_id) {
  const H264PPS* pps = parser_.GetPPS(pps_id);
  DCHECK(pps);

  curr_pps_id_ = pps->pic_parameter_set_id;

  return true;
}

bool VaapiH264Decoder::FinishPrevFrameIfPresent() {
  // If we already have a frame waiting to be decoded, decode it and finish.
  if (curr_pic_ != NULL) {
    if (!DecodePicture())
      return false;
    return FinishPicture();
  }

  return true;
}

bool VaapiH264Decoder::ProcessSlice(H264SliceHeader* slice_hdr) {
  prev_frame_num_ = frame_num_;
  frame_num_ = slice_hdr->frame_num;

  if (prev_frame_num_ > 0 && prev_frame_num_ < frame_num_ - 1) {
    DVLOG(1) << "Gap in frame_num!";
    return false;
  }

  if (slice_hdr->field_pic_flag == 0)
    max_pic_num_ = max_frame_num_;
  else
    max_pic_num_ = 2 * max_frame_num_;

  // TODO posciak: switch to new picture detection per 7.4.1.2.4.
  if (curr_pic_ != NULL && slice_hdr->first_mb_in_slice != 0) {
    // This is just some more slice data of the current picture, so
    // just queue it and return.
    QueueSlice(slice_hdr);
    return true;
  } else {
    // A new frame, so first finish the previous one before processing it...
    if (!FinishPrevFrameIfPresent())
      return false;

    // and then start a new one.
    return StartNewFrame(slice_hdr);
  }
}

#define SET_ERROR_AND_RETURN()             \
  do {                                     \
    DVLOG(1) << "Error during decode";     \
    state_ = kError;                       \
    return VaapiH264Decoder::kDecodeError; \
  } while (0)

VaapiH264Decoder::DecResult VaapiH264Decoder::DecodeInitial(int32 input_id) {
  // Decode enough to get required picture size (i.e. until we find an SPS),
  // if we get any slice data, we are missing the beginning of the stream.
  H264NALU nalu;
  H264Parser::Result res;

  DCHECK_NE(state_, kUninitialized);

  curr_input_id_ = input_id;

  while (1) {
    if (state_ == kAfterReset && num_available_decode_surfaces_ == 0) {
      DVLOG(4) << "No output surfaces available";
      return kNoOutputAvailable;
    }

    // Get next NALU looking for SPS or IDR if after reset.
    res = parser_.AdvanceToNextNALU(&nalu);
    if (res == H264Parser::kEOStream) {
      DVLOG(1) << "Could not find SPS before EOS";
      return kNeedMoreStreamData;
    } else if (res != H264Parser::kOk) {
      SET_ERROR_AND_RETURN();
    }

    DVLOG(4) << " NALU found: " << static_cast<int>(nalu.nal_unit_type);

    switch (nalu.nal_unit_type) {
      case H264NALU::kSPS:
        res = parser_.ParseSPS(&curr_sps_id_);
        if (res != H264Parser::kOk)
          SET_ERROR_AND_RETURN();

        if (!ProcessSPS(curr_sps_id_))
          SET_ERROR_AND_RETURN();

        // Just got information about the video size from SPS, so we can
        // now allocate surfaces and let the client now we are ready to
        // accept output buffers and decode.
        if (!CreateVASurfaces())
          SET_ERROR_AND_RETURN();

        state_ = kDecoding;
        return kReadyToDecode;

      case H264NALU::kIDRSlice:
        // If after reset, should be able to recover from an IDR.
        if (state_ == kAfterReset) {
          H264SliceHeader slice_hdr;

          res = parser_.ParseSliceHeader(nalu, &slice_hdr);
          if (res != H264Parser::kOk)
            SET_ERROR_AND_RETURN();

          if (!ProcessSlice(&slice_hdr))
            SET_ERROR_AND_RETURN();

          state_ = kDecoding;
          return kReadyToDecode;
        }  // else fallthrough
      case H264NALU::kNonIDRSlice:
      case H264NALU::kPPS:
        // Non-IDR slices cannot be used as resume points, as we may not
        // have all reference pictures that they may require.
        // fallthrough
      default:
        // Skip everything unless it's SPS or an IDR slice (if after reset).
        DVLOG(4) << "Skipping NALU";
        break;
    }
  }
}

void VaapiH264Decoder::SetStream(uint8* ptr, size_t size) {
  DCHECK(ptr);
  DCHECK(size);

  // Got new input stream data from the client.
  DVLOG(4) << "New input stream chunk at " << (void*) ptr
           << " size:  " << size;
  parser_.SetStream(ptr, size);
}

VaapiH264Decoder::DecResult VaapiH264Decoder::DecodeOneFrame(int32 input_id) {
  // Decode until one full frame is decoded or return it or until end
  // of stream (end of input data is reached).
  H264Parser::Result par_res;
  H264NALU nalu;

  curr_input_id_ = input_id;

  if (state_ != kDecoding) {
    DVLOG(1) << "Decoder not ready: error in stream or not initialized";
    return kDecodeError;
  }

  // All of the actions below might result in decoding a picture from
  // previously parsed data, but we still have to handle/parse current input
  // first.
  // Note: this may drop some already decoded frames if there are errors
  // further in the stream, but we are OK with that.
  while (1) {
    if (num_available_decode_surfaces_ == 0) {
      DVLOG(4) << "No output surfaces available";
      return kNoOutputAvailable;
    }
    par_res = parser_.AdvanceToNextNALU(&nalu);
    if (par_res == H264Parser::kEOStream)
      return kNeedMoreStreamData;
    else if (par_res != H264Parser::kOk)
      SET_ERROR_AND_RETURN();

    DVLOG(4) << "NALU found: " << static_cast<int>(nalu.nal_unit_type);

    switch (nalu.nal_unit_type) {
      case H264NALU::kNonIDRSlice:
      case H264NALU::kIDRSlice: {
        H264SliceHeader slice_hdr;

        par_res = parser_.ParseSliceHeader(nalu, &slice_hdr);
        if (par_res != H264Parser::kOk)
          SET_ERROR_AND_RETURN();

        if (!ProcessSlice(&slice_hdr))
          SET_ERROR_AND_RETURN();
        break;
      }

      case H264NALU::kSPS:
        int sps_id;

        if (!FinishPrevFrameIfPresent())
          SET_ERROR_AND_RETURN();

        par_res = parser_.ParseSPS(&sps_id);
        if (par_res != H264Parser::kOk)
          SET_ERROR_AND_RETURN();

        if (!ProcessSPS(sps_id))
          SET_ERROR_AND_RETURN();
        break;

      case H264NALU::kPPS:
        int pps_id;

        if (!FinishPrevFrameIfPresent())
          SET_ERROR_AND_RETURN();

        par_res = parser_.ParsePPS(&pps_id);
        if (par_res != H264Parser::kOk)
          SET_ERROR_AND_RETURN();

        if (!ProcessPPS(pps_id))
          SET_ERROR_AND_RETURN();
        break;

      default:
        // skip NALU
        break;
    }

    // If the last action resulted in decoding a frame, possibly from older
    // data, return. Otherwise keep reading the stream.
    if (frame_ready_at_hw_) {
      frame_ready_at_hw_ = false;
      return kDecodedFrame;
    }
  }
}

// static
size_t VaapiH264Decoder::GetRequiredNumOfPictures() {
  return kNumReqPictures;
}

// static
void VaapiH264Decoder::PreSandboxInitialization() {
  DCHECK(!pre_sandbox_init_done_);
  vaapi_handle = dlopen("libva.so", RTLD_NOW);
  vaapi_x11_handle = dlopen("libva-x11.so", RTLD_NOW);
  pre_sandbox_init_done_ = vaapi_handle && vaapi_x11_handle;
}

// static
bool VaapiH264Decoder::PostSandboxInitialization() {
  if (!pre_sandbox_init_done_)
    return false;
#define VAAPI_DLSYM(name, handle)                                       \
  VAAPI_##name = reinterpret_cast<Vaapi##name>(dlsym((handle), "va"#name)) \

  VAAPI_DLSYM(GetDisplay, vaapi_x11_handle);
  VAAPI_DLSYM(DisplayIsValid, vaapi_handle);
  VAAPI_DLSYM(Initialize, vaapi_handle);
  VAAPI_DLSYM(Terminate, vaapi_handle);
  VAAPI_DLSYM(GetConfigAttributes, vaapi_handle);
  VAAPI_DLSYM(CreateConfig, vaapi_handle);
  VAAPI_DLSYM(DestroyConfig, vaapi_handle);
  VAAPI_DLSYM(CreateSurfaces, vaapi_handle);
  VAAPI_DLSYM(DestroySurfaces, vaapi_handle);
  VAAPI_DLSYM(CreateContext, vaapi_handle);
  VAAPI_DLSYM(DestroyContext, vaapi_handle);
  VAAPI_DLSYM(PutSurface, vaapi_x11_handle);
  VAAPI_DLSYM(SyncSurface, vaapi_x11_handle);
  VAAPI_DLSYM(BeginPicture, vaapi_handle);
  VAAPI_DLSYM(RenderPicture, vaapi_handle);
  VAAPI_DLSYM(EndPicture, vaapi_handle);
  VAAPI_DLSYM(CreateBuffer, vaapi_handle);
  VAAPI_DLSYM(DestroyBuffer, vaapi_handle);
  VAAPI_DLSYM(ErrorStr, vaapi_handle);
#undef VAAPI_DLSYM

  return VAAPI_GetDisplay &&
      VAAPI_DisplayIsValid &&
      VAAPI_Initialize &&
      VAAPI_Terminate &&
      VAAPI_GetConfigAttributes &&
      VAAPI_CreateConfig &&
      VAAPI_DestroyConfig &&
      VAAPI_CreateSurfaces &&
      VAAPI_DestroySurfaces &&
      VAAPI_CreateContext &&
      VAAPI_DestroyContext &&
      VAAPI_PutSurface &&
      VAAPI_SyncSurface &&
      VAAPI_BeginPicture &&
      VAAPI_RenderPicture &&
      VAAPI_EndPicture &&
      VAAPI_CreateBuffer &&
      VAAPI_DestroyBuffer &&
      VAAPI_ErrorStr;
}

}  // namespace content
