// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/posix/eintr_wrapper.h"
#include "base/shared_memory.h"
#include "content/common/gpu/gl_scoped_binders.h"
#include "content/common/gpu/media/exynos_video_decode_accelerator.h"
#include "content/common/gpu/media/h264_parser.h"
#include "third_party/angle/include/GLES2/gl2.h"

namespace content {

#define NOTIFY_ERROR(x)                                               \
  do {                                                                \
    SetDecoderState(kError);                                          \
    DLOG(ERROR) << "calling NotifyError(): " << x;                    \
    NotifyError(x);                                                   \
  } while (0)

#define IOCTL_OR_ERROR_RETURN(fd, type, arg)                          \
  do {                                                                \
    if (HANDLE_EINTR(ioctl(fd, type, arg) != 0)) {                    \
      DPLOG(ERROR) << __func__ << "(): ioctl() failed: " << #type;    \
      NOTIFY_ERROR(PLATFORM_FAILURE);                                 \
      return;                                                         \
    }                                                                 \
  } while (0)

#define IOCTL_OR_ERROR_RETURN_FALSE(fd, type, arg)                    \
  do {                                                                \
    if (HANDLE_EINTR(ioctl(fd, type, arg) != 0)) {                    \
      DPLOG(ERROR) << __func__ << "(): ioctl() failed: " << #type;    \
      NOTIFY_ERROR(PLATFORM_FAILURE);                                 \
      return false;                                                   \
    }                                                                 \
  } while (0)

#define POSTSANDBOX_DLSYM(lib, func, type, name)                      \
  func = reinterpret_cast<type>(dlsym(lib, name));                    \
  if (func == NULL) {                                                 \
    DPLOG(ERROR) << "PostSandboxInitialization(): failed to dlsym() " \
                 << name << ": " << dlerror();                        \
    return false;                                                     \
  }

namespace {

const char kExynosMfcDevice[] = "/dev/mfc-dec";
const char kExynosGscDevice[] = "/dev/gsc1";
const char kMaliDriver[] = "libmali.so";

// TODO(sheu): fix OpenGL ES header includes, remove unnecessary redefinitions.
// http://crbug.com/169433
typedef void* GLeglImageOES;
typedef EGLBoolean (*MaliEglImageGetBufferExtPhandleFunc)(EGLImageKHR, EGLint*,
                                                          void*);
typedef EGLImageKHR (*EglCreateImageKhrFunc)(EGLDisplay, EGLContext, EGLenum,
                                             EGLClientBuffer, const EGLint*);
typedef EGLBoolean (*EglDestroyImageKhrFunc)(EGLDisplay, EGLImageKHR);
typedef EGLSyncKHR (*EglCreateSyncKhrFunc)(EGLDisplay, EGLenum, const EGLint*);
typedef EGLBoolean (*EglDestroySyncKhrFunc)(EGLDisplay, EGLSyncKHR);
typedef EGLint (*EglClientWaitSyncKhrFunc)(EGLDisplay, EGLSyncKHR, EGLint,
                                           EGLTimeKHR);
typedef void (*GlEglImageTargetTexture2dOesFunc)(GLenum, GLeglImageOES);

void* libmali_handle = NULL;
MaliEglImageGetBufferExtPhandleFunc
    mali_egl_image_get_buffer_ext_phandle = NULL;
EglCreateImageKhrFunc egl_create_image_khr = NULL;
EglDestroyImageKhrFunc egl_destroy_image_khr = NULL;
EglCreateSyncKhrFunc egl_create_sync_khr = NULL;
EglDestroySyncKhrFunc egl_destroy_sync_khr = NULL;
EglClientWaitSyncKhrFunc egl_client_wait_sync_khr = NULL;
GlEglImageTargetTexture2dOesFunc gl_egl_image_target_texture_2d_oes = NULL;

}  // anonymous namespace

struct ExynosVideoDecodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(
      base::WeakPtr<Client>& client,
      scoped_refptr<base::MessageLoopProxy>& client_message_loop_proxy,
      base::SharedMemory* shm,
      size_t size,
      int32 input_id);
  ~BitstreamBufferRef();
  const base::WeakPtr<Client> client;
  const scoped_refptr<base::MessageLoopProxy> client_message_loop_proxy;
  const scoped_ptr<base::SharedMemory> shm;
  const size_t size;
  off_t bytes_used;
  const int32 input_id;
};

struct ExynosVideoDecodeAccelerator::PictureBufferArrayRef {
  PictureBufferArrayRef(EGLDisplay egl_display, size_t count);
  ~PictureBufferArrayRef();

  struct PictureBufferRef {
    EGLImageKHR egl_image;
    int egl_image_fd;
    int32 client_id;
  };

  EGLDisplay const egl_display;
  std::vector<PictureBufferRef> picture_buffers;
};

struct ExynosVideoDecodeAccelerator::EGLSyncKHRRef {
  EGLSyncKHRRef(EGLDisplay egl_display, EGLSyncKHR egl_sync);
  ~EGLSyncKHRRef();
  EGLDisplay const egl_display;
  EGLSyncKHR egl_sync;
};

ExynosVideoDecodeAccelerator::BitstreamBufferRef::BitstreamBufferRef(
    base::WeakPtr<Client>& client,
    scoped_refptr<base::MessageLoopProxy>& client_message_loop_proxy,
    base::SharedMemory* shm, size_t size, int32 input_id)
    : client(client),
      client_message_loop_proxy(client_message_loop_proxy),
      shm(shm),
      size(size),
      bytes_used(0),
      input_id(input_id) {
}

ExynosVideoDecodeAccelerator::BitstreamBufferRef::~BitstreamBufferRef() {
  if (input_id >= 0) {
    client_message_loop_proxy->PostTask(FROM_HERE, base::Bind(
        &Client::NotifyEndOfBitstreamBuffer, client, input_id));
  }
}

ExynosVideoDecodeAccelerator::PictureBufferArrayRef::PictureBufferArrayRef(
    EGLDisplay egl_display, size_t count)
    : egl_display(egl_display),
      picture_buffers(count) {
  for (size_t i = 0; i < picture_buffers.size(); ++i) {
    PictureBufferRef& buffer = picture_buffers[i];
    buffer.egl_image = EGL_NO_IMAGE_KHR;
    buffer.egl_image_fd = -1;
    buffer.client_id = -1;
  }
}

ExynosVideoDecodeAccelerator::PictureBufferArrayRef::~PictureBufferArrayRef() {
  for (size_t i = 0; i < picture_buffers.size(); ++i) {
    PictureBufferRef& buffer = picture_buffers[i];
    if (buffer.egl_image != EGL_NO_IMAGE_KHR)
      egl_destroy_image_khr(egl_display, buffer.egl_image);
    if (buffer.egl_image_fd != -1)
      HANDLE_EINTR(close(buffer.egl_image_fd));
  }
}

ExynosVideoDecodeAccelerator::EGLSyncKHRRef::EGLSyncKHRRef(
    EGLDisplay egl_display, EGLSyncKHR egl_sync)
    : egl_display(egl_display),
      egl_sync(egl_sync) {
}

ExynosVideoDecodeAccelerator::EGLSyncKHRRef::~EGLSyncKHRRef() {
  if (egl_sync != EGL_NO_SYNC_KHR)
    egl_destroy_sync_khr(egl_display, egl_sync);
}

ExynosVideoDecodeAccelerator::MfcInputRecord::MfcInputRecord()
    : at_device(false),
      address(NULL),
      length(0),
      bytes_used(0),
      input_id(-1) {
}

ExynosVideoDecodeAccelerator::MfcInputRecord::~MfcInputRecord() {
}

ExynosVideoDecodeAccelerator::MfcOutputRecord::MfcOutputRecord()
    : at_device(false),
      input_id(-1) {
  bytes_used[0] = 0;
  bytes_used[1] = 0;
  address[0] = NULL;
  address[1] = NULL;
  length[0] = 0;
  length[1] = 0;
}

ExynosVideoDecodeAccelerator::MfcOutputRecord::~MfcOutputRecord() {
}

ExynosVideoDecodeAccelerator::GscInputRecord::GscInputRecord()
    : at_device(false),
      mfc_output(-1) {
}

ExynosVideoDecodeAccelerator::GscInputRecord::~GscInputRecord() {
}

ExynosVideoDecodeAccelerator::GscOutputRecord::GscOutputRecord()
    : at_device(false),
      at_client(false),
      fd(-1),
      egl_image(EGL_NO_IMAGE_KHR),
      egl_sync(EGL_NO_SYNC_KHR),
      picture_id(-1) {
}

ExynosVideoDecodeAccelerator::GscOutputRecord::~GscOutputRecord() {
}

ExynosVideoDecodeAccelerator::ExynosVideoDecodeAccelerator(
    EGLDisplay egl_display,
    EGLContext egl_context,
    Client* client,
    const base::Callback<bool(void)>& make_context_current)
    : child_message_loop_proxy_(base::MessageLoopProxy::current()),
      weak_this_(base::AsWeakPtr(this)),
      client_ptr_factory_(client),
      client_(client_ptr_factory_.GetWeakPtr()),
      decoder_thread_("ExynosDecoderThread"),
      decoder_state_(kUninitialized),
      decoder_current_bitstream_buffer_(NULL),
      decoder_delay_bitstream_buffer_id_(-1),
      decoder_current_input_buffer_(-1),
      decoder_decode_buffer_tasks_scheduled_(0),
      decoder_frames_at_client_(0),
      decoder_flushing_(false),
      mfc_fd_(-1),
      mfc_input_streamon_(false),
      mfc_input_buffer_count_(0),
      mfc_input_buffer_queued_count_(0),
      mfc_output_streamon_(false),
      mfc_output_buffer_count_(0),
      mfc_output_buffer_queued_count_(0),
      mfc_output_buffer_pixelformat_(0),
      gsc_fd_(-1),
      gsc_input_streamon_(false),
      gsc_input_buffer_count_(0),
      gsc_input_buffer_queued_count_(0),
      gsc_output_streamon_(false),
      gsc_output_buffer_count_(0),
      gsc_output_buffer_queued_count_(0),
      device_poll_thread_("ExynosDevicePollThread"),
      device_poll_interrupt_fd_(-1),
      make_context_current_(make_context_current),
      egl_display_(egl_display),
      egl_context_(egl_context),
      video_profile_(media::VIDEO_CODEC_PROFILE_UNKNOWN) {
}

ExynosVideoDecodeAccelerator::~ExynosVideoDecodeAccelerator() {
  DCHECK(!decoder_thread_.IsRunning());
  DCHECK(!device_poll_thread_.IsRunning());

  if (device_poll_interrupt_fd_ != -1) {
    HANDLE_EINTR(close(device_poll_interrupt_fd_));
    device_poll_interrupt_fd_ = -1;
  }
  if (gsc_fd_ != -1) {
    DestroyGscInputBuffers();
    DestroyGscOutputBuffers();
    HANDLE_EINTR(close(gsc_fd_));
    gsc_fd_ = -1;
  }
  if (mfc_fd_ != -1) {
    DestroyMfcInputBuffers();
    DestroyMfcOutputBuffers();
    HANDLE_EINTR(close(mfc_fd_));
    mfc_fd_ = -1;
  }

  // These maps have members that should be manually destroyed, e.g. file
  // descriptors, mmap() segments, etc.
  DCHECK(mfc_input_buffer_map_.empty());
  DCHECK(mfc_output_buffer_map_.empty());
  DCHECK(gsc_input_buffer_map_.empty());
  DCHECK(gsc_output_buffer_map_.empty());
}

bool ExynosVideoDecodeAccelerator::Initialize(
    media::VideoCodecProfile profile) {
  DVLOG(3) << "Initialize()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(decoder_state_, kUninitialized);

  switch (profile) {
    case media::H264PROFILE_BASELINE:
      DVLOG(2) << "Initialize(): profile H264PROFILE_BASELINE";
      break;
    case media::H264PROFILE_MAIN:
      DVLOG(2) << "Initialize(): profile H264PROFILE_MAIN";
      break;
    case media::H264PROFILE_HIGH:
      DVLOG(2) << "Initialize(): profile H264PROFILE_HIGH";
      break;
    case media::VP8PROFILE_MAIN:
      DVLOG(2) << "Initialize(): profile VP8PROFILE_MAIN";
      break;
    default:
      DLOG(ERROR) << "Initialize(): unsupported profile=" << profile;
      return false;
  };
  video_profile_ = profile;

  static bool sandbox_initialized = PostSandboxInitialization();
  if (!sandbox_initialized) {
    DLOG(ERROR) << "Initialize(): PostSandboxInitialization() failed";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  if (egl_display_ == EGL_NO_DISPLAY) {
    DLOG(ERROR) << "Initialize(): could not get EGLDisplay";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  if (egl_context_ == EGL_NO_CONTEXT) {
    DLOG(ERROR) << "Initialize(): could not get EGLContext";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  // Open the video devices.
  DVLOG(2) << "Initialize(): opening MFC device: " << kExynosMfcDevice;
  mfc_fd_ = HANDLE_EINTR(open(kExynosMfcDevice,
                              O_RDWR | O_NONBLOCK | O_CLOEXEC));
  if (mfc_fd_ == -1) {
    DPLOG(ERROR) << "Initialize(): could not open MFC device: "
                 << kExynosMfcDevice;
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  DVLOG(2) << "Initialize(): opening GSC device: " << kExynosGscDevice;
  gsc_fd_ = HANDLE_EINTR(open(kExynosGscDevice,
                         O_RDWR | O_NONBLOCK | O_CLOEXEC));
  if (gsc_fd_ == -1) {
    DPLOG(ERROR) << "Initialize(): could not open GSC device: "
                 << kExynosGscDevice;
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  // Create the interrupt fd.
  DCHECK_EQ(device_poll_interrupt_fd_, -1);
  device_poll_interrupt_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (device_poll_interrupt_fd_ == -1) {
    DPLOG(ERROR) << "Initialize(): eventfd() failed";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  // Capabilities check.
  struct v4l2_capability caps;
  const __u32 kCapsRequired =
      V4L2_CAP_VIDEO_CAPTURE_MPLANE |
      V4L2_CAP_VIDEO_OUTPUT_MPLANE |
      V4L2_CAP_STREAMING;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_QUERYCAP, &caps);
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    DLOG(ERROR) << "Initialize(): ioctl() failed: VIDIOC_QUERYCAP"
        ", caps check failed: 0x" << std::hex << caps.capabilities;
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_QUERYCAP, &caps);
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    DLOG(ERROR) << "Initialize(): ioctl() failed: VIDIOC_QUERYCAP"
        ", caps check failed: 0x" << std::hex << caps.capabilities;
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  // Some random ioctls that Exynos requires.
  struct v4l2_control control;
  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_MPEG_MFC51_VIDEO_DECODER_H264_DISPLAY_DELAY; // also VP8
  control.value = 8; // Magic number from Samsung folks.
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_S_CTRL, &control);

  if (!make_context_current_.Run()) {
    DLOG(ERROR) << "Initialize(): could not make context current";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  if (!CreateMfcInputBuffers())
    return false;

  // MFC output format has to be setup before streaming starts.
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12MT_16X16;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_S_FMT, &format);

  // Initialize format-specific bits.
  if (video_profile_ >= media::H264PROFILE_MIN &&
      video_profile_ <= media::H264PROFILE_MAX) {
    decoder_h264_parser_.reset(new content::H264Parser());
  }

  if (!decoder_thread_.Start()) {
    DLOG(ERROR) << "Initialize(): decoder thread failed to start";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  SetDecoderState(kInitialized);

  child_message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &Client::NotifyInitializeDone, client_));
  return true;
}

void ExynosVideoDecodeAccelerator::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  DVLOG(1) << "Decode(): input_id=" << bitstream_buffer.id()
           << ", size=" << bitstream_buffer.size();
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  scoped_ptr<BitstreamBufferRef> bitstream_record(new BitstreamBufferRef(
      client_, child_message_loop_proxy_,
      new base::SharedMemory(bitstream_buffer.handle(), true),
      bitstream_buffer.size(), bitstream_buffer.id()));
  if (!bitstream_record->shm->Map(bitstream_buffer.size())) {
    DLOG(ERROR) << "Decode(): could not map bitstream_buffer";
    NOTIFY_ERROR(UNREADABLE_INPUT);
    return;
  }
  DVLOG(3) << "Decode(): mapped to addr=" << bitstream_record->shm->memory();

  // DecodeTask() will take care of running a DecodeBufferTask().
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &ExynosVideoDecodeAccelerator::DecodeTask, base::Unretained(this),
      base::Passed(&bitstream_record)));
}

void ExynosVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DVLOG(3) << "AssignPictureBuffers(): buffer_count=" << buffers.size();
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  if (static_cast<int>(buffers.size()) != gsc_output_buffer_count_) {
    DLOG(ERROR) << "AssignPictureBuffers(): invalid buffer_count";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  if (!make_context_current_.Run()) {
    DLOG(ERROR) << "AssignPictureBuffers(): could not make context current";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  scoped_ptr<PictureBufferArrayRef> pic_buffers_ref(
      new PictureBufferArrayRef(egl_display_, buffers.size()));

  const static EGLint kImageAttrs[] = {
    EGL_IMAGE_PRESERVED_KHR, 0,
    EGL_NONE,
  };
  Display* x_display = base::MessagePumpForUI::GetDefaultXDisplay();
  ScopedTextureBinder bind_restore(0);
  for (size_t i = 0; i < pic_buffers_ref->picture_buffers.size(); ++i) {
    PictureBufferArrayRef::PictureBufferRef& buffer =
        pic_buffers_ref->picture_buffers[i];
    // Create the X pixmap and then create an EGLImageKHR from it, so we can
    // get dma_buf backing.
    Pixmap pixmap = XCreatePixmap(x_display, RootWindow(x_display, 0),
        buffers[i].size().width(), buffers[i].size().height(), 32);
    if (!pixmap) {
      DLOG(ERROR) << "AssignPictureBuffers(): could not create X pixmap";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
    glBindTexture(GL_TEXTURE_2D, buffers[i].texture_id());
    EGLImageKHR egl_image = egl_create_image_khr(
        egl_display_, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR,
        (EGLClientBuffer)pixmap, kImageAttrs);
    // We can free the X pixmap immediately -- according to the
    // EGL_KHR_image_base spec, the backing storage does not go away until the
    // last referencing EGLImage is destroyed.
    XFreePixmap(x_display, pixmap);
    if (egl_image == EGL_NO_IMAGE_KHR) {
      DLOG(ERROR) << "AssignPictureBuffers(): could not create EGLImageKHR";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
    buffer.egl_image = egl_image;
    int fd;
    if (!mali_egl_image_get_buffer_ext_phandle(buffer.egl_image, NULL, &fd)) {
      DLOG(ERROR) << "AssignPictureBuffers(): "
                  << "could not get EGLImageKHR dmabuf fd";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
    buffer.egl_image_fd = fd;
    gl_egl_image_target_texture_2d_oes(GL_TEXTURE_2D, egl_image);
    buffer.client_id = buffers[i].id();
  }
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &ExynosVideoDecodeAccelerator::AssignPictureBuffersTask,
      base::Unretained(this), base::Passed(&pic_buffers_ref)));
}

void ExynosVideoDecodeAccelerator::ReusePictureBuffer(int32 picture_buffer_id) {
  DVLOG(3) << "ReusePictureBuffer(): picture_buffer_id=" << picture_buffer_id;
  // Must be run on child thread, as we'll insert a sync in the EGL context.
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  if (!make_context_current_.Run()) {
    DLOG(ERROR) << "ReusePictureBuffer(): could not make context current";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  EGLSyncKHR egl_sync =
      egl_create_sync_khr(egl_display_, EGL_SYNC_FENCE_KHR, NULL);
  if (egl_sync == EGL_NO_SYNC_KHR) {
    DLOG(ERROR) << "ReusePictureBuffer(): eglCreateSyncKHR() failed";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  scoped_ptr<EGLSyncKHRRef> egl_sync_ref(new EGLSyncKHRRef(
      egl_display_, egl_sync));
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &ExynosVideoDecodeAccelerator::ReusePictureBufferTask,
      base::Unretained(this), picture_buffer_id, base::Passed(&egl_sync_ref)));
}

void ExynosVideoDecodeAccelerator::Flush() {
  DVLOG(3) << "Flush()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &ExynosVideoDecodeAccelerator::FlushTask, base::Unretained(this)));
}

void ExynosVideoDecodeAccelerator::Reset() {
  DVLOG(3) << "Reset()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &ExynosVideoDecodeAccelerator::ResetTask, base::Unretained(this)));
}

void ExynosVideoDecodeAccelerator::Destroy() {
  DVLOG(3) << "Destroy()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  // We're destroying; cancel all callbacks.
  client_ptr_factory_.InvalidateWeakPtrs();

  // If the decoder thread is running, destroy using posted task.
  if (decoder_thread_.IsRunning()) {
    decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
        &ExynosVideoDecodeAccelerator::DestroyTask, base::Unretained(this)));
    // DestroyTask() will cause the decoder_thread_ to flush all tasks.
    decoder_thread_.Stop();
  } else {
    // Otherwise, call the destroy task directly.
    DestroyTask();
  }

  // Set to kError state just in case.
  SetDecoderState(kError);

  delete this;
}

// static
void ExynosVideoDecodeAccelerator::PreSandboxInitialization() {
  DVLOG(3) << "PreSandboxInitialization()";
  dlerror();

  libmali_handle = dlopen(kMaliDriver, RTLD_LAZY | RTLD_LOCAL);
  if (libmali_handle == NULL) {
    DPLOG(ERROR) << "failed to dlopen() " << kMaliDriver << ": " << dlerror();
  }
}

// static
bool ExynosVideoDecodeAccelerator::PostSandboxInitialization() {
  DVLOG(3) << "PostSandboxInitialization()";
  if (libmali_handle == NULL) {
    DLOG(ERROR) << "PostSandboxInitialization(): no " << kMaliDriver
                << " driver handle";
    return false;
  }

  dlerror();

  POSTSANDBOX_DLSYM(libmali_handle,
                    mali_egl_image_get_buffer_ext_phandle,
                    MaliEglImageGetBufferExtPhandleFunc,
                    "mali_egl_image_get_buffer_ext_phandle");

  POSTSANDBOX_DLSYM(libmali_handle,
                    egl_create_image_khr,
                    EglCreateImageKhrFunc,
                    "eglCreateImageKHR");

  POSTSANDBOX_DLSYM(libmali_handle,
                    egl_destroy_image_khr,
                    EglDestroyImageKhrFunc,
                    "eglDestroyImageKHR");

  POSTSANDBOX_DLSYM(libmali_handle,
                    egl_create_sync_khr,
                    EglCreateSyncKhrFunc,
                    "eglCreateSyncKHR");

  POSTSANDBOX_DLSYM(libmali_handle,
                    egl_destroy_sync_khr,
                    EglDestroySyncKhrFunc,
                    "eglDestroySyncKHR");

  POSTSANDBOX_DLSYM(libmali_handle,
                    egl_client_wait_sync_khr,
                    EglClientWaitSyncKhrFunc,
                    "eglClientWaitSyncKHR");

  POSTSANDBOX_DLSYM(libmali_handle,
                    gl_egl_image_target_texture_2d_oes,
                    GlEglImageTargetTexture2dOesFunc,
                    "glEGLImageTargetTexture2DOES");

  return true;
}

void ExynosVideoDecodeAccelerator::DecodeTask(
    scoped_ptr<BitstreamBufferRef> bitstream_record) {
  DVLOG(3) << "DecodeTask(): input_id=" << bitstream_record->input_id;
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT1("Video Decoder", "EVDA::DecodeTask", "input_id",
               bitstream_record->input_id);

  if (decoder_state_ == kResetting || decoder_flushing_) {
    // In the case that we're resetting or flushing, we need to delay decoding
    // the BitstreamBuffers that come after the Reset() or Flush() call.  When
    // we're here, we know that this DecodeTask() was scheduled by a Decode()
    // call that came after (in the client thread) the Reset() or Flush() call;
    // thus set up the delay if necessary.
    if (decoder_delay_bitstream_buffer_id_ == -1)
      decoder_delay_bitstream_buffer_id_ = bitstream_record->input_id;
  } else if (decoder_state_ == kError) {
    DVLOG(2) << "DecodeTask(): early out: kError state";
    return;
  }

  decoder_input_queue_.push_back(
      linked_ptr<BitstreamBufferRef>(bitstream_record.release()));
  decoder_decode_buffer_tasks_scheduled_++;
  DecodeBufferTask();
}

void ExynosVideoDecodeAccelerator::DecodeBufferTask() {
  DVLOG(3) << "DecodeBufferTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT0("Video Decoder", "EVDA::DecodeBufferTask");

  decoder_decode_buffer_tasks_scheduled_--;

  if (decoder_state_ == kResetting) {
    DVLOG(2) << "DecodeBufferTask(): early out: kResetting state";
    return;
  } else if (decoder_state_ == kError) {
    DVLOG(2) << "DecodeBufferTask(): early out: kError state";
    return;
  }

  if (decoder_current_bitstream_buffer_ == NULL) {
    if (decoder_input_queue_.empty()) {
      // We're waiting for a new buffer -- exit without scheduling a new task.
      return;
    }
    linked_ptr<BitstreamBufferRef>& buffer_ref = decoder_input_queue_.front();
    if (decoder_delay_bitstream_buffer_id_ == buffer_ref->input_id) {
      // We're asked to delay decoding on this and subsequent buffers.
      return;
    }

    // Setup to use the next buffer.
    decoder_current_bitstream_buffer_.reset(buffer_ref.release());
    decoder_input_queue_.pop_front();
    DVLOG(3) << "DecodeBufferTask(): reading input_id="
             << decoder_current_bitstream_buffer_->input_id
             << ", addr=" << decoder_current_bitstream_buffer_->shm->memory()
             << ", size=" << decoder_current_bitstream_buffer_->size;
  }
  bool schedule_task = false;
  const size_t size = decoder_current_bitstream_buffer_->size;
  size_t decoded_size = 0;
  if (size == 0) {
    const int32 input_id = decoder_current_bitstream_buffer_->input_id;
    if (input_id >= 0) {
      // This is a buffer queued from the client that has zero size.  Skip.
      schedule_task = true;
    } else {
      // This is a buffer of zero size, queued to flush the pipe.  Flush.
      DCHECK_EQ(decoder_current_bitstream_buffer_->shm.get(),
                static_cast<base::SharedMemory*>(NULL));
      // Enqueue a buffer guaranteed to be empty.  To do that, we flush the
      // current input, enqueue no data to the next frame, then flush that down.
      schedule_task = true;
      if (decoder_current_input_buffer_ != -1 &&
          mfc_input_buffer_map_[decoder_current_input_buffer_].input_id !=
              kFlushBufferId)
        schedule_task = FlushInputFrame();

      if (schedule_task && AppendToInputFrame(NULL, 0) && FlushInputFrame()) {
        DVLOG(2) << "DecodeBufferTask(): enqueued flush buffer";
        schedule_task = true;
      } else {
        // If we failed to enqueue the empty buffer (due to pipeline
        // backpressure), don't advance the bitstream buffer queue, and don't
        // schedule the next task.  This bitstream buffer queue entry will get
        // reprocessed when the pipeline frees up.
        schedule_task = false;
      }
    }
  } else {
    // This is a buffer queued from the client, with actual contents.  Decode.
    const uint8* const data =
        reinterpret_cast<const uint8*>(
            decoder_current_bitstream_buffer_->shm->memory()) +
        decoder_current_bitstream_buffer_->bytes_used;
    const size_t data_size =
        decoder_current_bitstream_buffer_->size -
        decoder_current_bitstream_buffer_->bytes_used;
    if (!FindFrameFragment(data, data_size, &decoded_size)) {
      NOTIFY_ERROR(UNREADABLE_INPUT);
      return;
    }
    // FindFrameFragment should not return a size larger than the buffer size,
    // even on invalid data.
    CHECK_LE(decoded_size, data_size);

    switch (decoder_state_) {
      case kInitialized:
      case kAfterReset:
        schedule_task = DecodeBufferInitial(data, decoded_size, &decoded_size);
        break;
      case kDecoding:
        schedule_task = DecodeBufferContinue(data, decoded_size);
        break;
      default:
        NOTIFY_ERROR(ILLEGAL_STATE);
        return;
    }
  }
  if (decoder_state_ == kError) {
    // Failed during decode.
    return;
  }

  if (schedule_task) {
    decoder_current_bitstream_buffer_->bytes_used += decoded_size;
    if (decoder_current_bitstream_buffer_->bytes_used ==
        decoder_current_bitstream_buffer_->size) {
      // Our current bitstream buffer is done; return it.
      int32 input_id = decoder_current_bitstream_buffer_->input_id;
      DVLOG(3) << "DecodeBufferTask(): finished input_id=" << input_id;
      // BitstreamBufferRef destructor calls NotifyEndOfBitstreamBuffer().
      decoder_current_bitstream_buffer_.reset();
    }
    ScheduleDecodeBufferTaskIfNeeded();
  }
}

bool ExynosVideoDecodeAccelerator::FindFrameFragment(
    const uint8* data,
    size_t size,
    size_t* endpos) {
  if (video_profile_ >= media::H264PROFILE_MIN &&
      video_profile_ <= media::H264PROFILE_MAX) {
    // For H264, we need to feed HW one frame at a time.  This is going to take
    // some parsing of our input stream.
    decoder_h264_parser_->SetStream(data, size);
    content::H264NALU nalu;
    content::H264Parser::Result result;

    // Find the first NAL.
    result = decoder_h264_parser_->AdvanceToNextNALU(&nalu);
    if (result == content::H264Parser::kInvalidStream ||
        result == content::H264Parser::kUnsupportedStream)
      return false;
    *endpos = (nalu.data + nalu.size) - data;
    if (result == content::H264Parser::kEOStream)
      return true;

    // Keep on peeking the next NALs while they don't indicate a frame
    // boundary.
    for (;;) {
      result = decoder_h264_parser_->AdvanceToNextNALU(&nalu);
      if (result == content::H264Parser::kInvalidStream ||
          result == content::H264Parser::kUnsupportedStream)
        return false;
      if (result == content::H264Parser::kEOStream)
        return true;
      switch (nalu.nal_unit_type) {
        case content::H264NALU::kNonIDRSlice:
        case content::H264NALU::kIDRSlice:
          if (nalu.size < 1)
            return false;
          // For these two, if the "first_mb_in_slice" field is zero, start a
          // new frame and return.  This field is Exp-Golomb coded starting on
          // the eighth data bit of the NAL; a zero value is encoded with a
          // leading '1' bit in the byte, which we can detect as the byte being
          // (unsigned) greater than or equal to 0x80.
          if (nalu.data[1] >= 0x80)
            return true;
          break;
        case content::H264NALU::kSPS:
        case content::H264NALU::kPPS:
        case content::H264NALU::kEOSeq:
        case content::H264NALU::kEOStream:
          // These unconditionally signal a frame boundary.
          return true;
        default:
          // For all others, keep going.
          break;
      }
      *endpos = (nalu.data + nalu.size) - data;
    }
    NOTREACHED();
    return false;
  } else {
    DCHECK_GE(video_profile_, media::VP8PROFILE_MIN);
    DCHECK_LE(video_profile_, media::VP8PROFILE_MAX);
    // For VP8, we can just dump the entire buffer.  No fragmentation needed.
    *endpos = size;
    return true;
  }
}

void ExynosVideoDecodeAccelerator::ScheduleDecodeBufferTaskIfNeeded() {
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());

  // If we're behind on tasks, schedule another one.
  int buffers_to_decode = decoder_input_queue_.size();
  if (decoder_current_bitstream_buffer_ != NULL)
    buffers_to_decode++;
  if (decoder_decode_buffer_tasks_scheduled_ < buffers_to_decode) {
    decoder_decode_buffer_tasks_scheduled_++;
    decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
        &ExynosVideoDecodeAccelerator::DecodeBufferTask,
        base::Unretained(this)));
  }
}

bool ExynosVideoDecodeAccelerator::DecodeBufferInitial(
    const void* data, size_t size, size_t* endpos) {
  DVLOG(3) << "DecodeBufferInitial(): data=" << data << ", size=" << size;
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kDecoding);
  DCHECK(!device_poll_thread_.IsRunning());
  // Initial decode.  We haven't been able to get output stream format info yet.
  // Get it, and start decoding.

  // Copy in and send to HW.
  if (!AppendToInputFrame(data, size) || !FlushInputFrame())
    return false;

  // Recycle buffers.
  DequeueMfc();

  // Check and see if we have format info yet.
  struct v4l2_format format;
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  if (ioctl(mfc_fd_, VIDIOC_G_FMT, &format) != 0) {
    if (errno == EINVAL) {
      // We will get EINVAL if we haven't seen sufficient stream to decode the
      // format.  Return true and schedule the next buffer.
      *endpos = size;
      return true;
    } else {
      DPLOG(ERROR) << "DecodeBufferInitial(): ioctl() failed: VIDIOC_G_FMT";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return false;
    }
  }

  // Run this initialization only on first startup.
  if (decoder_state_ == kInitialized) {
    DVLOG(3) << "DecodeBufferInitial(): running one-time initialization";
    // Success! Setup our parameters.
    CHECK_EQ(format.fmt.pix_mp.num_planes, 2);
    frame_buffer_size_.SetSize(
        format.fmt.pix_mp.width, format.fmt.pix_mp.height);
    mfc_output_buffer_size_[0] = format.fmt.pix_mp.plane_fmt[0].sizeimage;
    mfc_output_buffer_size_[1] = format.fmt.pix_mp.plane_fmt[1].sizeimage;
    mfc_output_buffer_pixelformat_ = format.fmt.pix_mp.pixelformat;
    DCHECK_EQ(mfc_output_buffer_pixelformat_, V4L2_PIX_FMT_NV12MT_16X16);

    // Create our other buffers.
    if (!CreateMfcOutputBuffers() || !CreateGscInputBuffers() ||
        !CreateGscOutputBuffers())
      return false;

    // MFC expects to process the initial buffer once during stream init to
    // configure stream parameters, but will not consume the steam data on that
    // iteration.  Subsequent iterations (including after reset) do not require
    // the stream init step.
    *endpos = 0;
  } else {
    *endpos = size;
  }

  // StartDevicePoll will raise the error if there is one.
  if (!StartDevicePoll())
    return false;

  decoder_state_ = kDecoding;
  ScheduleDecodeBufferTaskIfNeeded();
  return true;
}

bool ExynosVideoDecodeAccelerator::DecodeBufferContinue(
    const void* data, size_t size) {
  DVLOG(3) << "DecodeBufferContinue(): data=" << data << ", size=" << size;
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  DCHECK_EQ(decoder_state_, kDecoding);

  // Both of these calls will set kError state if they fail.
  return (AppendToInputFrame(data, size) && FlushInputFrame());
}

bool ExynosVideoDecodeAccelerator::AppendToInputFrame(
    const void* data, size_t size) {
  DVLOG(3) << "AppendToInputFrame()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kResetting);
  DCHECK_NE(decoder_state_, kError);
  // This routine can handle data == NULL and size == 0, which occurs when
  // we queue an empty buffer for the purposes of flushing the pipe.

  // Flush if we're too big
  if (decoder_current_input_buffer_ != -1) {
    MfcInputRecord& input_record =
        mfc_input_buffer_map_[decoder_current_input_buffer_];
    if (input_record.bytes_used + size > input_record.length) {
      if (!FlushInputFrame())
        return false;
      decoder_current_input_buffer_ = -1;
    }
  }

  // Try to get an available input buffer
  if (decoder_current_input_buffer_ == -1) {
    if (mfc_free_input_buffers_.empty()) {
      // See if we can get more free buffers from HW
      DequeueMfc();
      if (mfc_free_input_buffers_.empty()) {
        // Nope!
        DVLOG(2) << "AppendToInputFrame(): stalled for input buffers";
        return false;
      }
    }
    decoder_current_input_buffer_ = mfc_free_input_buffers_.back();
    mfc_free_input_buffers_.pop_back();
    MfcInputRecord& input_record =
        mfc_input_buffer_map_[decoder_current_input_buffer_];
    DCHECK_EQ(input_record.bytes_used, 0);
    DCHECK_EQ(input_record.input_id, -1);
    DCHECK(decoder_current_bitstream_buffer_ != NULL);
    input_record.input_id = decoder_current_bitstream_buffer_->input_id;
  }

  DCHECK_EQ(data == NULL, size == 0);
  if (size == 0) {
    // If we asked for an empty buffer, return now.  We return only after
    // getting the next input buffer, since we might actually want an empty
    // input buffer for flushing purposes.
    return true;
  }

  // Copy in to the buffer.
  MfcInputRecord& input_record =
      mfc_input_buffer_map_[decoder_current_input_buffer_];
  if (size > input_record.length - input_record.bytes_used) {
    LOG(ERROR) << "AppendToInputFrame(): over-size frame, erroring";
    NOTIFY_ERROR(UNREADABLE_INPUT);
    return false;
  }
  memcpy(
      reinterpret_cast<uint8*>(input_record.address) + input_record.bytes_used,
      data,
      size);
  input_record.bytes_used += size;

  return true;
}

bool ExynosVideoDecodeAccelerator::FlushInputFrame() {
  DVLOG(3) << "FlushInputFrame()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kResetting);
  DCHECK_NE(decoder_state_, kError);

  if (decoder_current_input_buffer_ == -1)
    return true;

  MfcInputRecord& input_record =
      mfc_input_buffer_map_[decoder_current_input_buffer_];
  DCHECK_NE(input_record.input_id, -1);
  DCHECK_EQ(input_record.input_id == kFlushBufferId,
            input_record.bytes_used == 0);
  // * if input_id >= 0, this input buffer was prompted by a bitstream buffer we
  //   got from the client.  We can skip it if it is empty.
  // * if input_id < 0 (should be kFlushBufferId in this case), this input
  //   buffer was prompted by a flush buffer, and should be queued even when
  //   empty.
  if (input_record.input_id >= 0 && input_record.bytes_used == 0) {
    input_record.input_id = -1;
    mfc_free_input_buffers_.push_back(decoder_current_input_buffer_);
    decoder_current_input_buffer_ = -1;
    return true;
  }

  // Queue it to MFC.
  mfc_input_ready_queue_.push_back(decoder_current_input_buffer_);
  decoder_current_input_buffer_ = -1;
  DVLOG(3) << "FlushInputFrame(): submitting input_id="
           << input_record.input_id;
  // Kick the MFC once since there's new available input for it.
  EnqueueMfc();

  return (decoder_state_ != kError);
}

void ExynosVideoDecodeAccelerator::AssignPictureBuffersTask(
    scoped_ptr<PictureBufferArrayRef> pic_buffers) {
  DVLOG(3) << "AssignPictureBuffersTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT0("Video Decoder", "EVDA::AssignPictureBuffersTask");

  // We run AssignPictureBuffersTask even if we're in kResetting.
  if (decoder_state_ == kError) {
    DVLOG(2) << "AssignPictureBuffersTask(): early out: kError state";
    return;
  }

  DCHECK_EQ(pic_buffers->picture_buffers.size(), gsc_output_buffer_map_.size());
  for (size_t i = 0; i < gsc_output_buffer_map_.size(); ++i) {
    // We should be blank right now.
    GscOutputRecord& output_record = gsc_output_buffer_map_[i];
    DCHECK_EQ(output_record.fd, -1);
    DCHECK_EQ(output_record.egl_image, EGL_NO_IMAGE_KHR);
    DCHECK_EQ(output_record.egl_sync, EGL_NO_SYNC_KHR);
    DCHECK_EQ(output_record.picture_id, -1);
    PictureBufferArrayRef::PictureBufferRef& buffer =
        pic_buffers->picture_buffers[i];
    output_record.fd = buffer.egl_image_fd;
    output_record.egl_image = buffer.egl_image;
    output_record.picture_id = buffer.client_id;

    // Take ownership of the EGLImage and fd.
    buffer.egl_image = EGL_NO_IMAGE_KHR;
    buffer.egl_image_fd = -1;
    // And add this buffer to the free list.
    gsc_free_output_buffers_.push_back(i);
  }

  // We got buffers!  Kick the GSC.
  EnqueueGsc();
}

void ExynosVideoDecodeAccelerator::ServiceDeviceTask() {
  DVLOG(3) << "ServiceDeviceTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kInitialized);
  DCHECK_NE(decoder_state_, kAfterReset);
  TRACE_EVENT0("Video Decoder", "EVDA::ServiceDeviceTask");

  if (decoder_state_ == kResetting) {
    DVLOG(2) << "ServiceDeviceTask(): early out: kResetting state";
    return;
  } else if (decoder_state_ == kError) {
    DVLOG(2) << "ServiceDeviceTask(): early out: kError state";
    return;
  }

  DequeueMfc();
  DequeueGsc();
  EnqueueMfc();
  EnqueueGsc();

  // Clear the interrupt fd.
  if (!ClearDevicePollInterrupt())
    return;

  unsigned int poll_fds = 0;
  // Add MFC fd, if we should poll on it.
  // MFC can be polled as soon as either input or output buffers are queued.
  if (mfc_input_buffer_queued_count_ + mfc_output_buffer_queued_count_ > 0)
    poll_fds |= kPollMfc;
  // Add GSC fd, if we should poll on it.
  // GSC has to wait until both input and output buffers are queued.
  if (gsc_input_buffer_queued_count_ > 0 && gsc_output_buffer_queued_count_ > 0)
    poll_fds |= kPollGsc;

  // ServiceDeviceTask() should only ever be scheduled from DevicePollTask(),
  // so either:
  // * device_poll_thread_ is running normally
  // * device_poll_thread_ scheduled us, but then a ResetTask() or DestroyTask()
  //   shut it down, in which case we're either in kResetting or kError states
  //   respectively, and we should have early-outed already.
  DCHECK(device_poll_thread_.message_loop());
  // Queue the DevicePollTask() now.
  device_poll_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &ExynosVideoDecodeAccelerator::DevicePollTask,
      base::Unretained(this),
      poll_fds));

  DVLOG(1) << "ServiceDeviceTask(): buffer counts: DEC["
           << decoder_input_queue_.size() << "->"
           << mfc_input_ready_queue_.size() << "] => MFC["
           << mfc_free_input_buffers_.size() << "+"
           << mfc_input_buffer_queued_count_ << "/"
           << mfc_input_buffer_count_ << "->"
           << mfc_free_output_buffers_.size() << "+"
           << mfc_output_buffer_queued_count_ << "/"
           << mfc_output_buffer_count_ << "] => "
           << mfc_output_gsc_input_queue_.size() << " => GSC["
           << gsc_free_input_buffers_.size() << "+"
           << gsc_input_buffer_queued_count_ << "/"
           << gsc_input_buffer_count_ << "->"
           << gsc_free_output_buffers_.size() << "+"
           << gsc_output_buffer_queued_count_ << "/"
           << gsc_output_buffer_count_ << "] => VDA["
           << decoder_frames_at_client_ << "]";

  ScheduleDecodeBufferTaskIfNeeded();
}

void ExynosVideoDecodeAccelerator::EnqueueMfc() {
  DVLOG(3) << "EnqueueMfc()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT0("Video Decoder", "EVDA::EnqueueMfc");

  // Drain the pipe of completed decode buffers.
  const int old_mfc_inputs_queued = mfc_input_buffer_queued_count_;
  while (!mfc_input_ready_queue_.empty()) {
    if (!EnqueueMfcInputRecord())
      return;
  }
  if (old_mfc_inputs_queued == 0 && mfc_input_buffer_queued_count_ != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!SetDevicePollInterrupt())
      return;
    // Start VIDIOC_STREAMON if we haven't yet.
    if (!mfc_input_streamon_) {
      __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      IOCTL_OR_ERROR_RETURN(mfc_fd_, VIDIOC_STREAMON, &type);
      mfc_input_streamon_ = true;
    }
  }

  // Enqueue all the MFC outputs we can.
  const int old_mfc_outputs_queued = mfc_output_buffer_queued_count_;
  while (!mfc_free_output_buffers_.empty()) {
    if (!EnqueueMfcOutputRecord())
      return;
  }
  if (old_mfc_outputs_queued == 0 && mfc_output_buffer_queued_count_ != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!SetDevicePollInterrupt())
      return;
    // Start VIDIOC_STREAMON if we haven't yet.
    if (!mfc_output_streamon_) {
      __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      IOCTL_OR_ERROR_RETURN(mfc_fd_, VIDIOC_STREAMON, &type);
      mfc_output_streamon_ = true;
    }
  }
}

void ExynosVideoDecodeAccelerator::DequeueMfc() {
  DVLOG(3) << "DequeueMfc()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT0("Video Decoder", "EVDA::DequeueMfc");

  // Dequeue completed MFC input (VIDEO_OUTPUT) buffers, and recycle to the free
  // list.
  struct v4l2_buffer dqbuf;
  struct v4l2_plane planes[2];
  while (mfc_input_buffer_queued_count_ > 0) {
    DCHECK(mfc_input_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    dqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dqbuf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(mfc_fd_, VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      DPLOG(ERROR) << "DequeueMfc(): ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
    MfcInputRecord& input_record = mfc_input_buffer_map_[dqbuf.index];
    DCHECK(input_record.at_device);
    mfc_free_input_buffers_.push_back(dqbuf.index);
    input_record.at_device = false;
    input_record.bytes_used = 0;
    input_record.input_id = -1;
    mfc_input_buffer_queued_count_--;
  }

  // Dequeue completed MFC output (VIDEO_CAPTURE) buffers, and queue to the
  // completed queue.
  while (mfc_output_buffer_queued_count_ > 0) {
    DCHECK(mfc_output_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(planes, 0, sizeof(planes));
    dqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dqbuf.memory = V4L2_MEMORY_MMAP;
    dqbuf.m.planes = planes;
    dqbuf.length = 2;
    if (ioctl(mfc_fd_, VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      DPLOG(ERROR) << "DequeueMfc(): ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
    MfcOutputRecord& output_record = mfc_output_buffer_map_[dqbuf.index];
    DCHECK(output_record.at_device);
    output_record.at_device = false;
    output_record.bytes_used[0] = dqbuf.m.planes[0].bytesused;
    output_record.bytes_used[1] = dqbuf.m.planes[1].bytesused;
    if (output_record.bytes_used[0] + output_record.bytes_used[1] == 0) {
      // This is an empty output buffer returned as part of a flush.
      mfc_free_output_buffers_.push_back(dqbuf.index);
      output_record.input_id = -1;
    } else {
      // This is an output buffer with contents to pass down the pipe.
      mfc_output_gsc_input_queue_.push_back(dqbuf.index);
      output_record.input_id = dqbuf.timestamp.tv_sec;
      DCHECK(output_record.input_id >= 0);
      DVLOG(3) << "DequeueMfc(): dequeued input_id=" << output_record.input_id;
      // We don't count this output buffer dequeued yet, or add it to the free
      // list, as it has data GSC needs to process.

      // We have new frames in mfc_output_gsc_input_queue_.  Kick the pipe.
      SetDevicePollInterrupt();
    }
    mfc_output_buffer_queued_count_--;
  }

  NotifyFlushDoneIfNeeded();
}

void ExynosVideoDecodeAccelerator::EnqueueGsc() {
  DVLOG(3) << "EnqueueGsc()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kInitialized);
  TRACE_EVENT0("Video Decoder", "EVDA::EnqueueGsc");

  // Drain the pipe of completed MFC output buffers.
  const int old_gsc_inputs_queued = gsc_input_buffer_queued_count_;
  while (!mfc_output_gsc_input_queue_.empty() &&
         !gsc_free_input_buffers_.empty()) {
    if (!EnqueueGscInputRecord())
      return;
  }
  if (old_gsc_inputs_queued == 0 && gsc_input_buffer_queued_count_ != 0) {
    // We just started up a previously empty queue.
    // Queue state changed; signal interrupt.
    if (!SetDevicePollInterrupt())
      return;
    // Start VIDIOC_STREAMON if we haven't yet.
    if (!gsc_input_streamon_) {
      __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      IOCTL_OR_ERROR_RETURN(gsc_fd_, VIDIOC_STREAMON, &type);
      gsc_input_streamon_ = true;
    }
  }

  // Enqueue a GSC output, only if we need one
  if (gsc_input_buffer_queued_count_ != 0 &&
      gsc_output_buffer_queued_count_ == 0 &&
      !gsc_free_output_buffers_.empty()) {
    const int old_gsc_outputs_queued = gsc_output_buffer_queued_count_;
    if (!EnqueueGscOutputRecord())
      return;
    if (old_gsc_outputs_queued == 0 && gsc_output_buffer_queued_count_ != 0) {
      // We just started up a previously empty queue.
      // Queue state changed; signal interrupt.
      if (!SetDevicePollInterrupt())
        return;
      // Start VIDIOC_STREAMON if we haven't yet.
      if (!gsc_output_streamon_) {
        __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        IOCTL_OR_ERROR_RETURN(gsc_fd_, VIDIOC_STREAMON, &type);
        gsc_output_streamon_ = true;
      }
    }
  }
  // Bug check: GSC is liable to race conditions if more than one buffer is
  // simultaneously queued.
  DCHECK_GE(1, gsc_output_buffer_queued_count_);
}

void ExynosVideoDecodeAccelerator::DequeueGsc() {
  DVLOG(3) << "DequeueGsc()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kInitialized);
  DCHECK_NE(decoder_state_, kAfterReset);
  TRACE_EVENT0("Video Decoder", "EVDA::DequeueGsc");

  // Dequeue completed GSC input (VIDEO_OUTPUT) buffers, and recycle to the free
  // list.  Also recycle the corresponding MFC output buffers at this time.
  struct v4l2_buffer dqbuf;
  while (gsc_input_buffer_queued_count_ > 0) {
    DCHECK(gsc_input_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    dqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dqbuf.memory = V4L2_MEMORY_DMABUF;
    if (ioctl(gsc_fd_, VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      DPLOG(ERROR) << "DequeueGsc(): ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
    GscInputRecord& input_record = gsc_input_buffer_map_[dqbuf.index];
    MfcOutputRecord& output_record =
        mfc_output_buffer_map_[input_record.mfc_output];
    DCHECK(input_record.at_device);
    gsc_free_input_buffers_.push_back(dqbuf.index);
    mfc_free_output_buffers_.push_back(input_record.mfc_output);
    input_record.at_device = false;
    input_record.mfc_output = -1;
    output_record.input_id = -1;
    gsc_input_buffer_queued_count_--;
  }

  // Dequeue completed GSC output (VIDEO_CAPTURE) buffers, and send them off to
  // the client.  Don't recycle to its free list yet -- we can't do that until
  // ReusePictureBuffer() returns it to us.
  while (gsc_output_buffer_queued_count_ > 0) {
    DCHECK(gsc_output_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    dqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dqbuf.memory = V4L2_MEMORY_DMABUF;
    if (ioctl(gsc_fd_, VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      DPLOG(ERROR) << "DequeueGsc(): ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }
    GscOutputRecord& output_record = gsc_output_buffer_map_[dqbuf.index];
    DCHECK(output_record.at_device);
    DCHECK(!output_record.at_client);
    DCHECK_EQ(output_record.egl_sync, EGL_NO_SYNC_KHR);
    output_record.at_device = false;
    output_record.at_client = true;
    gsc_output_buffer_queued_count_--;
    child_message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &Client::PictureReady, client_, media::Picture(
            output_record.picture_id, dqbuf.timestamp.tv_sec)));
    decoder_frames_at_client_++;
  }

  NotifyFlushDoneIfNeeded();
}

bool ExynosVideoDecodeAccelerator::EnqueueMfcInputRecord() {
  DVLOG(3) << "EnqueueMfcInputRecord()";
  DCHECK(!mfc_input_ready_queue_.empty());

  // Enqueue a MFC input (VIDEO_OUTPUT) buffer.
  const int buffer = mfc_input_ready_queue_.back();
  MfcInputRecord& input_record = mfc_input_buffer_map_[buffer];
  DCHECK(!input_record.at_device);
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_plane;
  memset(&qbuf, 0, sizeof(qbuf));
  memset(&qbuf_plane, 0, sizeof(qbuf_plane));
  qbuf.index                 = buffer;
  qbuf.type                  = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  qbuf.timestamp.tv_sec      = input_record.input_id;
  qbuf.memory                = V4L2_MEMORY_MMAP;
  qbuf.m.planes              = &qbuf_plane;
  qbuf.m.planes[0].bytesused = input_record.bytes_used;
  qbuf.length                = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_QBUF, &qbuf);
  mfc_input_ready_queue_.pop_back();
  input_record.at_device = true;
  mfc_input_buffer_queued_count_++;
  DVLOG(3) << "EnqueueMfcInputRecord(): enqueued input_id="
           << input_record.input_id;
  return true;
}

bool ExynosVideoDecodeAccelerator::EnqueueMfcOutputRecord() {
  DVLOG(3) << "EnqueueMfcOutputRecord()";
  DCHECK(!mfc_free_output_buffers_.empty());

  // Enqueue a MFC output (VIDEO_CAPTURE) buffer.
  const int buffer = mfc_free_output_buffers_.back();
  MfcOutputRecord& output_record = mfc_output_buffer_map_[buffer];
  DCHECK(!output_record.at_device);
  DCHECK_EQ(output_record.input_id, -1);
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_planes[2];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes, 0, sizeof(qbuf_planes));
  qbuf.index    = buffer;
  qbuf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  qbuf.memory   = V4L2_MEMORY_MMAP;
  qbuf.m.planes = qbuf_planes;
  qbuf.length   = 2;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_QBUF, &qbuf);
  mfc_free_output_buffers_.pop_back();
  output_record.at_device = true;
  mfc_output_buffer_queued_count_++;
  return true;
}

bool ExynosVideoDecodeAccelerator::EnqueueGscInputRecord() {
  DVLOG(3) << "EnqueueGscInputRecord()";
  DCHECK(!gsc_free_input_buffers_.empty());

  // Enqueue a GSC input (VIDEO_OUTPUT) buffer for a complete MFC output
  // (VIDEO_CAPTURE) buffer.
  const int mfc_buffer = mfc_output_gsc_input_queue_.front();
  const int gsc_buffer = gsc_free_input_buffers_.back();
  MfcOutputRecord& output_record = mfc_output_buffer_map_[mfc_buffer];
  DCHECK(!output_record.at_device);
  GscInputRecord& input_record = gsc_input_buffer_map_[gsc_buffer];
  DCHECK(!input_record.at_device);
  DCHECK_EQ(input_record.mfc_output, -1);
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_planes[2];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes, 0, sizeof(qbuf_planes));
  qbuf.index                 = gsc_buffer;
  qbuf.type                  = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  qbuf.timestamp.tv_sec      = output_record.input_id;
  qbuf.memory                = V4L2_MEMORY_USERPTR;
  qbuf.m.planes              = qbuf_planes;
  qbuf.m.planes[0].bytesused = output_record.bytes_used[0];
  qbuf.m.planes[0].length    = mfc_output_buffer_size_[0];
  qbuf.m.planes[0].m.userptr = (unsigned long)output_record.address[0];
  qbuf.m.planes[1].bytesused = output_record.bytes_used[1];
  qbuf.m.planes[1].length    = mfc_output_buffer_size_[1];
  qbuf.m.planes[1].m.userptr = (unsigned long)output_record.address[1];
  qbuf.length                = 2;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_QBUF, &qbuf);
  mfc_output_gsc_input_queue_.pop_front();
  gsc_free_input_buffers_.pop_back();
  input_record.at_device = true;
  input_record.mfc_output = mfc_buffer;
  output_record.bytes_used[0] = 0;
  output_record.bytes_used[1] = 0;
  gsc_input_buffer_queued_count_++;
  DVLOG(3) << "EnqueueGscInputRecord(): enqueued input_id="
           << output_record.input_id;
  return true;
}

bool ExynosVideoDecodeAccelerator::EnqueueGscOutputRecord() {
  DVLOG(3) << "EnqueueGscOutputRecord()";
  DCHECK(!gsc_free_output_buffers_.empty());

  // Enqueue a GSC output (VIDEO_CAPTURE) buffer.
  const int buffer = gsc_free_output_buffers_.front();
  GscOutputRecord& output_record = gsc_output_buffer_map_[buffer];
  DCHECK(!output_record.at_device);
  DCHECK(!output_record.at_client);
  if (output_record.egl_sync != EGL_NO_SYNC_KHR) {
    TRACE_EVENT0(
        "Video Decoder",
        "EVDA::EnqueueGscOutputRecord: eglClientWaitSyncKHR");
    // If we have to wait for completion, wait.  Note that
    // gsc_free_output_buffers_ is a FIFO queue, so we always wait on the
    // buffer that has been in the queue the longest.
    egl_client_wait_sync_khr(egl_display_, output_record.egl_sync, 0,
        EGL_FOREVER_KHR);
    egl_destroy_sync_khr(egl_display_, output_record.egl_sync);
    output_record.egl_sync = EGL_NO_SYNC_KHR;
  }
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_plane;
  memset(&qbuf, 0, sizeof(qbuf));
  memset(&qbuf_plane, 0, sizeof(qbuf_plane));
  qbuf.index            = buffer;
  qbuf.type             = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  qbuf.memory           = V4L2_MEMORY_DMABUF;
  qbuf.m.planes         = &qbuf_plane;
  qbuf.m.planes[0].m.fd = output_record.fd;
  qbuf.length           = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_QBUF, &qbuf);
  gsc_free_output_buffers_.pop_front();
  output_record.at_device = true;
  gsc_output_buffer_queued_count_++;
  return true;
}

void ExynosVideoDecodeAccelerator::ReusePictureBufferTask(
    int32 picture_buffer_id, scoped_ptr<EGLSyncKHRRef> egl_sync_ref) {
  DVLOG(3) << "ReusePictureBufferTask(): picture_buffer_id="
           << picture_buffer_id;
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "EVDA::ReusePictureBufferTask");

  // We run ReusePictureBufferTask even if we're in kResetting.
  if (decoder_state_ == kError) {
    DVLOG(2) << "ReusePictureBufferTask(): early out: kError state";
    return;
  }

  size_t index;
  for (index = 0; index < gsc_output_buffer_map_.size(); ++index)
    if (gsc_output_buffer_map_[index].picture_id == picture_buffer_id)
      break;

  if (index >= gsc_output_buffer_map_.size()) {
    DLOG(ERROR) << "ReusePictureBufferTask(): picture_buffer_id not found";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  GscOutputRecord& output_record = gsc_output_buffer_map_[index];
  if (output_record.at_device || !output_record.at_client) {
    DLOG(ERROR) << "ReusePictureBufferTask(): picture_buffer_id not reusable";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  DCHECK_EQ(output_record.egl_sync, EGL_NO_SYNC_KHR);
  output_record.at_client = false;
  output_record.egl_sync = egl_sync_ref->egl_sync;
  gsc_free_output_buffers_.push_back(index);
  decoder_frames_at_client_--;
  // Take ownership of the EGLSync.
  egl_sync_ref->egl_sync = EGL_NO_SYNC_KHR;
  // We got a buffer back, so kick the GSC.
  EnqueueGsc();
}

void ExynosVideoDecodeAccelerator::FlushTask() {
  DVLOG(3) << "FlushTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "EVDA::FlushTask");

  // Flush outstanding buffers.
  if (decoder_state_ == kInitialized || decoder_state_ == kAfterReset) {
    // There's nothing in the pipe, so return done immediately.
    child_message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &Client::NotifyFlushDone, client_));
    return;
  } else if (decoder_state_ == kError) {
    DVLOG(2) << "FlushTask(): early out: kError state";
    return;
  }

  // We don't support stacked flushing.
  DCHECK(!decoder_flushing_);

  // Queue up an empty buffer -- this triggers the flush.
  decoder_input_queue_.push_back(linked_ptr<BitstreamBufferRef>(
      new BitstreamBufferRef(client_, child_message_loop_proxy_, NULL, 0,
                             kFlushBufferId)));
  decoder_flushing_ = true;

  ScheduleDecodeBufferTaskIfNeeded();
}

void ExynosVideoDecodeAccelerator::NotifyFlushDoneIfNeeded() {
  if (!decoder_flushing_)
    return;

  // Pipeline is empty when:
  // * Decoder input queue is empty of non-delayed buffers.
  // * There is no currently filling input buffer.
  // * MFC input holding queue is empty.
  // * All MFC input (VIDEO_OUTPUT) buffers are returned.
  // * MFC -> GSC holding queue is empty.
  // * All GSC input (VIDEO_OUTPUT) buffers are returned.
  if (!decoder_input_queue_.empty()) {
    if (decoder_input_queue_.front()->input_id !=
        decoder_delay_bitstream_buffer_id_)
      return;
  }
  if (decoder_current_input_buffer_ != -1)
    return;
  if ((mfc_input_ready_queue_.size() +
      mfc_input_buffer_queued_count_ + mfc_output_gsc_input_queue_.size() +
      gsc_input_buffer_queued_count_ + gsc_output_buffer_queued_count_ ) != 0)
    return;

  decoder_delay_bitstream_buffer_id_ = -1;
  decoder_flushing_ = false;
  child_message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
    &Client::NotifyFlushDone, client_));

  // While we were flushing, we early-outed DecodeBufferTask()s.
  ScheduleDecodeBufferTaskIfNeeded();
}

void ExynosVideoDecodeAccelerator::ResetTask() {
  DVLOG(3) << "ResetTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "EVDA::ResetTask");

  if (decoder_state_ == kError) {
    DVLOG(2) << "ResetTask(): early out: kError state";
    return;
  }

  // We stop streaming, but we _don't_ destroy our buffers.
  if (!StopDevicePoll())
    return;

  decoder_current_bitstream_buffer_.reset();
  decoder_input_queue_.clear();

  decoder_current_input_buffer_ = -1;

  // If we were flushing, we'll never return any more BitstreamBuffers or
  // PictureBuffers; they have all been dropped and returned by now.
  NotifyFlushDoneIfNeeded();

  // Mark that we're resetting, then enqueue a ResetDoneTask().  All intervening
  // jobs will early-out in the kResetting state.
  decoder_state_ = kResetting;
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &ExynosVideoDecodeAccelerator::ResetDoneTask, base::Unretained(this)));
}

void ExynosVideoDecodeAccelerator::ResetDoneTask() {
  DVLOG(3) << "ResetDoneTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "EVDA::ResetDoneTask");

  if (decoder_state_ == kError) {
    DVLOG(2) << "ResetDoneTask(): early out: kError state";
    return;
  }

  // Reset format-specific bits.
  if (video_profile_ >= media::H264PROFILE_MIN &&
      video_profile_ <= media::H264PROFILE_MAX) {
    decoder_h264_parser_.reset(new content::H264Parser());
  }

  // Jobs drained, we're finished resetting.
  DCHECK_EQ(decoder_state_, kResetting);
  decoder_state_ = kAfterReset;
  decoder_delay_bitstream_buffer_id_ = -1;
  child_message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &Client::NotifyResetDone, client_));

  // While we were resetting, we early-outed DecodeBufferTask()s.
  ScheduleDecodeBufferTaskIfNeeded();
}

void ExynosVideoDecodeAccelerator::DestroyTask() {
  DVLOG(3) << "DestroyTask()";
  TRACE_EVENT0("Video Decoder", "EVDA::DestroyTask");

  // DestroyTask() should run regardless of decoder_state_.

  // Stop streaming and the device_poll_thread_.
  StopDevicePoll();

  decoder_current_bitstream_buffer_.reset();
  decoder_current_input_buffer_ = -1;
  decoder_decode_buffer_tasks_scheduled_ = 0;
  decoder_frames_at_client_ = 0;
  decoder_input_queue_.clear();
  decoder_flushing_ = false;

  // Set our state to kError.  Just in case.
  decoder_state_ = kError;
}

bool ExynosVideoDecodeAccelerator::StartDevicePoll() {
  DVLOG(3) << "StartDevicePoll()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());
  DCHECK(!device_poll_thread_.IsRunning());

  // Start up the device poll thread and schedule its first DevicePollTask().
  if (!device_poll_thread_.Start()) {
    DLOG(ERROR) << "StartDevicePoll(): Device thread failed to start";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  device_poll_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &ExynosVideoDecodeAccelerator::DevicePollTask,
      base::Unretained(this),
      0));

  return true;
}

bool ExynosVideoDecodeAccelerator::StopDevicePoll() {
  DVLOG(3) << "StopDevicePoll()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());

  // Signal the DevicePollTask() to stop, and stop the device poll thread.
  if (!SetDevicePollInterrupt())
    return false;
  device_poll_thread_.Stop();
  // Clear the interrupt now, to be sure.
  if (!ClearDevicePollInterrupt())
    return false;

  // Stop streaming.
  if (mfc_input_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_STREAMOFF, &type);
  }
  mfc_input_streamon_ = false;
  if (mfc_output_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_STREAMOFF, &type);
  }
  mfc_output_streamon_ = false;
  if (gsc_input_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_STREAMOFF, &type);
  }
  gsc_input_streamon_ = false;
  if (gsc_output_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_STREAMOFF, &type);
  }
  gsc_output_streamon_ = false;

  // Reset all our accounting info.
  mfc_input_ready_queue_.clear();
  mfc_free_input_buffers_.clear();
  DCHECK_EQ(mfc_input_buffer_count_,
      static_cast<int>(mfc_input_buffer_map_.size()));
  for (size_t i = 0; i < mfc_input_buffer_map_.size(); ++i) {
    mfc_free_input_buffers_.push_back(i);
    mfc_input_buffer_map_[i].at_device = false;
    mfc_input_buffer_map_[i].bytes_used = 0;
    mfc_input_buffer_map_[i].input_id = -1;
  }
  mfc_input_buffer_queued_count_ = 0;
  mfc_free_output_buffers_.clear();
  DCHECK_EQ(mfc_output_buffer_count_,
     static_cast<int>(mfc_output_buffer_map_.size()));
  for (size_t i = 0; i < mfc_output_buffer_map_.size(); ++i) {
    mfc_free_output_buffers_.push_back(i);
    mfc_output_buffer_map_[i].at_device = false;
    mfc_output_buffer_map_[i].input_id = -1;
  }
  mfc_output_buffer_queued_count_ = 0;
  mfc_output_gsc_input_queue_.clear();
  gsc_free_input_buffers_.clear();
  DCHECK_EQ(gsc_input_buffer_count_,
      static_cast<int>(gsc_input_buffer_map_.size()));
  for (size_t i = 0; i < gsc_input_buffer_map_.size(); ++i) {
    gsc_free_input_buffers_.push_back(i);
    gsc_input_buffer_map_[i].at_device = false;
    gsc_input_buffer_map_[i].mfc_output = -1;
  }
  gsc_input_buffer_queued_count_ = 0;
  gsc_free_output_buffers_.clear();
  DCHECK_EQ(gsc_output_buffer_count_,
      static_cast<int>(gsc_output_buffer_map_.size()));
  for (size_t i = 0; i < gsc_output_buffer_map_.size(); ++i) {
    // Only mark those free that aren't being held by the VDA.
    if (!gsc_output_buffer_map_[i].at_client) {
      gsc_free_output_buffers_.push_back(i);
      gsc_output_buffer_map_[i].at_device = false;
    }
  }
  gsc_output_buffer_queued_count_ = 0;

  DVLOG(3) << "StopDevicePoll(): device poll stopped";
  return true;
}

bool ExynosVideoDecodeAccelerator::SetDevicePollInterrupt() {
  DVLOG(3) << "SetDevicePollInterrupt()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());

  const uint64 buf = 1;
  if (HANDLE_EINTR(write(device_poll_interrupt_fd_, &buf, sizeof(buf))) == -1) {
    DPLOG(ERROR) << "SetDevicePollInterrupt(): write() failed";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }
  return true;
}

bool ExynosVideoDecodeAccelerator::ClearDevicePollInterrupt() {
  DVLOG(3) << "ClearDevicePollInterrupt()";
  DCHECK_EQ(decoder_thread_.message_loop(), MessageLoop::current());

  uint64 buf;
  if (HANDLE_EINTR(read(device_poll_interrupt_fd_, &buf, sizeof(buf))) == -1) {
    if (errno == EAGAIN) {
      // No interrupt flag set, and we're reading nonblocking.  Not an error.
      return true;
    } else {
      DPLOG(ERROR) << "ClearDevicePollInterrupt(): read() failed";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return false;
    }
  }
  return true;
}

void ExynosVideoDecodeAccelerator::DevicePollTask(unsigned int poll_fds) {
  DVLOG(3) << "DevicePollTask()";
  DCHECK_EQ(device_poll_thread_.message_loop(), MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "EVDA::DevicePollTask");

  // This routine just polls the set of device fds, and schedules a
  // ServiceDeviceTask() on decoder_thread_ when processing needs to occur.
  // Other threads may notify this task to return early by writing to
  // device_poll_interrupt_fd_.
  struct pollfd pollfds[3];
  nfds_t nfds;

  // Add device_poll_interrupt_fd_;
  pollfds[0].fd = device_poll_interrupt_fd_;
  pollfds[0].events = POLLIN | POLLERR;
  nfds = 1;

  if (poll_fds & kPollMfc) {
    DVLOG(3) << "DevicePollTask(): adding MFC to poll() set";
    pollfds[nfds].fd = mfc_fd_;
    pollfds[nfds].events = POLLIN | POLLOUT | POLLERR;
    nfds++;
  }
  // Add GSC fd, if we should poll on it.
  // GSC has to wait until both input and output buffers are queued.
  if (poll_fds & kPollGsc) {
    DVLOG(3) << "DevicePollTask(): adding GSC to poll() set";
    pollfds[nfds].fd = gsc_fd_;
    pollfds[nfds].events = POLLIN | POLLOUT | POLLERR;
    nfds++;
  }

  // Poll it!
  if (HANDLE_EINTR(poll(pollfds, nfds, -1)) == -1) {
    DPLOG(ERROR) << "DevicePollTask(): poll() failed";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch decoder state from this thread.
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &ExynosVideoDecodeAccelerator::ServiceDeviceTask,
      base::Unretained(this)));
}

void ExynosVideoDecodeAccelerator::NotifyError(Error error) {
  DVLOG(2) << "NotifyError()";

  if (!child_message_loop_proxy_->BelongsToCurrentThread()) {
    child_message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &ExynosVideoDecodeAccelerator::NotifyError, weak_this_, error));
    return;
  }

  if (client_) {
    client_->NotifyError(error);
    client_ptr_factory_.InvalidateWeakPtrs();
  }
}

void ExynosVideoDecodeAccelerator::SetDecoderState(State state) {
  DVLOG(3) << "SetDecoderState(): state=%d" << state;

  // We can touch decoder_state_ only if this is the decoder thread or the
  // decoder thread isn't running.
  if (decoder_thread_.message_loop() != NULL &&
      decoder_thread_.message_loop() != MessageLoop::current()) {
    decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
        &ExynosVideoDecodeAccelerator::SetDecoderState,
        base::Unretained(this), state));
  } else {
    decoder_state_ = state;
  }
}

bool ExynosVideoDecodeAccelerator::CreateMfcInputBuffers() {
  DVLOG(3) << "CreateMfcInputBuffers()";
  // We always run this as we prepare to initialize.
  DCHECK_EQ(decoder_state_, kUninitialized);
  DCHECK(!mfc_input_streamon_);
  DCHECK_EQ(mfc_input_buffer_count_, 0);

  __u32 pixelformat = 0;
  if (video_profile_ >= media::H264PROFILE_MIN &&
      video_profile_ <= media::H264PROFILE_MAX) {
    pixelformat = V4L2_PIX_FMT_H264;
  } else if (video_profile_ >= media::VP8PROFILE_MIN &&
             video_profile_ <= media::VP8PROFILE_MAX) {
    pixelformat = V4L2_PIX_FMT_VP8;
  } else {
    NOTREACHED();
  }

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type                              = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.pixelformat            = pixelformat;
  format.fmt.pix_mp.plane_fmt[0].sizeimage = kMfcInputBufferMaxSize;
  format.fmt.pix_mp.num_planes             = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_S_FMT, &format);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count  = kMfcInputBufferCount;
  reqbufs.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_REQBUFS, &reqbufs);
  mfc_input_buffer_count_ = reqbufs.count;
  mfc_input_buffer_map_.resize(mfc_input_buffer_count_);
  for (int i = 0; i < mfc_input_buffer_count_; ++i) {
    mfc_free_input_buffers_.push_back(i);

    // Query for the MEMORY_MMAP pointer.
    struct v4l2_plane planes[1];
    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    memset(planes, 0, sizeof(planes));
    buffer.index    = i;
    buffer.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buffer.memory   = V4L2_MEMORY_MMAP;
    buffer.m.planes = planes;
    buffer.length   = 1;
    IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_QUERYBUF, &buffer);
    void* address = mmap(NULL, buffer.m.planes[0].length,
        PROT_READ | PROT_WRITE, MAP_SHARED, mfc_fd_,
        buffer.m.planes[0].m.mem_offset);
    if (address == MAP_FAILED) {
      DPLOG(ERROR) << "CreateMfcInputBuffers(): mmap() failed";
      return false;
    }
    mfc_input_buffer_map_[i].address = address;
    mfc_input_buffer_map_[i].length = buffer.m.planes[0].length;
  }

  return true;
}

bool ExynosVideoDecodeAccelerator::CreateMfcOutputBuffers() {
  DVLOG(3) << "CreateMfcOutputBuffers()";
  DCHECK_EQ(decoder_state_, kInitialized);
  DCHECK(!mfc_output_streamon_);
  DCHECK_EQ(mfc_output_buffer_count_, 0);

  // Number of MFC output buffers we need.
  struct v4l2_control ctrl;
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_G_CTRL, &ctrl);

  // Output format setup in Initialize().

  // Allocate the output buffers.
  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count  = ctrl.value + kMfcOutputBufferExtraCount;
  reqbufs.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_REQBUFS, &reqbufs);

  // Fill our free-buffers list, and create DMABUFs from them.
  mfc_output_buffer_count_ = reqbufs.count;
  mfc_output_buffer_map_.resize(mfc_output_buffer_count_);
  for (int i = 0; i < mfc_output_buffer_count_; ++i) {
    mfc_free_output_buffers_.push_back(i);

    // Query for the MEMORY_MMAP pointer.
    struct v4l2_plane planes[2];
    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    memset(planes, 0, sizeof(planes));
    buffer.index    = i;
    buffer.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buffer.memory   = V4L2_MEMORY_MMAP;
    buffer.m.planes = planes;
    buffer.length   = 2;
    IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_QUERYBUF, &buffer);

    // Get their user memory for GSC input.
    for (int j = 0; j < 2; ++j) {
      void* address = mmap(NULL, buffer.m.planes[j].length,
          PROT_READ | PROT_WRITE, MAP_SHARED, mfc_fd_,
          buffer.m.planes[j].m.mem_offset);
      if (address == MAP_FAILED) {
        DPLOG(ERROR) << "CreateMfcInputBuffers(): mmap() failed";
        return false;
      }
      mfc_output_buffer_map_[i].address[j] = address;
      mfc_output_buffer_map_[i].length[j] = buffer.m.planes[j].length;
    }
  }

  return true;
}

bool ExynosVideoDecodeAccelerator::CreateGscInputBuffers() {
  DVLOG(3) << "CreateGscInputBuffers()";
  DCHECK_EQ(decoder_state_, kInitialized);
  DCHECK(!gsc_input_streamon_);
  DCHECK_EQ(gsc_input_buffer_count_, 0);

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.width  = frame_buffer_size_.width();
  format.fmt.pix_mp.height = frame_buffer_size_.height();
  format.fmt.pix_mp.pixelformat = mfc_output_buffer_pixelformat_;
  format.fmt.pix_mp.plane_fmt[0].sizeimage = mfc_output_buffer_size_[0];
  format.fmt.pix_mp.plane_fmt[1].sizeimage = mfc_output_buffer_size_[1];
  // NV12MT_16X16 is a tiled format for which bytesperline doesn't make too much
  // sense.  Convention seems to be to assume 8bpp for these tiled formats.
  format.fmt.pix_mp.plane_fmt[0].bytesperline = frame_buffer_size_.width();
  format.fmt.pix_mp.plane_fmt[1].bytesperline = frame_buffer_size_.width();
  format.fmt.pix_mp.num_planes = 2;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_S_FMT, &format);

  struct v4l2_control control;
  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_ROTATE;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_HFLIP;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_VFLIP;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id = V4L2_CID_GLOBAL_ALPHA;
  control.value = 255;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_S_CTRL, &control);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = kGscInputBufferCount;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_USERPTR;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_REQBUFS, &reqbufs);

  gsc_input_buffer_count_ = reqbufs.count;
  gsc_input_buffer_map_.resize(gsc_input_buffer_count_);
  for (int i = 0; i < gsc_input_buffer_count_; ++i) {
    gsc_free_input_buffers_.push_back(i);
    gsc_input_buffer_map_[i].mfc_output = -1;
  }

  return true;
}

bool ExynosVideoDecodeAccelerator::CreateGscOutputBuffers() {
  DVLOG(3) << "CreateGscOutputBuffers()";
  DCHECK_EQ(decoder_state_, kInitialized);
  DCHECK(!gsc_output_streamon_);
  DCHECK_EQ(gsc_output_buffer_count_, 0);

  // GSC outputs into the EGLImages we create from the textures we are
  // assigned.  Assume RGBA8888 format.
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.width  = frame_buffer_size_.width();
  format.fmt.pix_mp.height = frame_buffer_size_.height();
  format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_RGB32;
  format.fmt.pix_mp.plane_fmt[0].sizeimage =
      frame_buffer_size_.width() * frame_buffer_size_.height() * 4;
  format.fmt.pix_mp.plane_fmt[0].bytesperline = frame_buffer_size_.width() * 4;
  format.fmt.pix_mp.num_planes = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_S_FMT, &format);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = kGscOutputBufferCount;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_DMABUF;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_REQBUFS, &reqbufs);

  // We don't actually fill in the freelist or the map here.  That happens once
  // we have actual usable buffers, after AssignPictureBuffers();
  gsc_output_buffer_count_ = reqbufs.count;
  gsc_output_buffer_map_.resize(gsc_output_buffer_count_);

  DVLOG(3) << "CreateGscOutputBuffers(): ProvidePictureBuffers(): "
           << "buffer_count=" << gsc_output_buffer_count_
           << ", width=" << frame_buffer_size_.width()
           << ", height=" << frame_buffer_size_.height();
  child_message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &Client::ProvidePictureBuffers, client_, gsc_output_buffer_count_,
      gfx::Size(frame_buffer_size_.width(), frame_buffer_size_.height()),
      GL_TEXTURE_2D));

  return true;
}

void ExynosVideoDecodeAccelerator::DestroyMfcInputBuffers() {
  DVLOG(3) << "DestroyMfcInputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!mfc_input_streamon_);

  for (size_t i = 0; i < mfc_input_buffer_map_.size(); ++i) {
    if (mfc_input_buffer_map_[i].address != NULL) {
      munmap(mfc_input_buffer_map_[i].address,
          mfc_input_buffer_map_[i].length);
    }
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  if (ioctl(mfc_fd_, VIDIOC_REQBUFS, &reqbufs) != 0)
    DPLOG(ERROR) << "DestroyMfcInputBuffers(): ioctl() failed: VIDIOC_REQBUFS";

  mfc_input_buffer_map_.clear();
  mfc_free_input_buffers_.clear();
  mfc_input_buffer_count_ = 0;
}

void ExynosVideoDecodeAccelerator::DestroyMfcOutputBuffers() {
  DVLOG(3) << "DestroyMfcOutputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!mfc_output_streamon_);

  for (size_t i = 0; i < mfc_output_buffer_map_.size(); ++i) {
    if (mfc_output_buffer_map_[i].address[0] != NULL)
      munmap(mfc_output_buffer_map_[i].address[0],
          mfc_output_buffer_map_[i].length[0]);
    if (mfc_output_buffer_map_[i].address[1] != NULL)
      munmap(mfc_output_buffer_map_[i].address[1],
          mfc_output_buffer_map_[i].length[1]);
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  if (ioctl(mfc_fd_, VIDIOC_REQBUFS, &reqbufs) != 0)
    DPLOG(ERROR) << "DestroyMfcOutputBuffers() ioctl() failed: VIDIOC_REQBUFS";

  mfc_output_buffer_map_.clear();
  mfc_free_output_buffers_.clear();
  mfc_output_buffer_count_ = 0;
}

void ExynosVideoDecodeAccelerator::DestroyGscInputBuffers() {
  DVLOG(3) << "DestroyGscInputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!gsc_input_streamon_);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_DMABUF;
  if (ioctl(gsc_fd_, VIDIOC_REQBUFS, &reqbufs) != 0)
    DPLOG(ERROR) << "DestroyGscInputBuffers(): ioctl() failed: VIDIOC_REQBUFS";

  gsc_input_buffer_map_.clear();
  gsc_free_input_buffers_.clear();
  gsc_input_buffer_count_ = 0;
}

void ExynosVideoDecodeAccelerator::DestroyGscOutputBuffers() {
  DVLOG(3) << "DestroyGscOutputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!gsc_output_streamon_);

  if (gsc_output_buffer_map_.size() != 0) {
    if (!make_context_current_.Run())
      DLOG(ERROR) << "DestroyGscOutputBuffers(): "
                  << "could not make context current";

    size_t i = 0;
    do {
      GscOutputRecord& output_record = gsc_output_buffer_map_[i];
      if (output_record.fd != -1)
        HANDLE_EINTR(close(output_record.fd));
      if (output_record.egl_image != EGL_NO_IMAGE_KHR)
        egl_destroy_image_khr(egl_display_, output_record.egl_image);
      if (output_record.egl_sync != EGL_NO_SYNC_KHR)
        egl_destroy_sync_khr(egl_display_, output_record.egl_sync);
      if (client_)
        client_->DismissPictureBuffer(output_record.picture_id);
      ++i;
    } while (i < gsc_output_buffer_map_.size());
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_DMABUF;
  if (ioctl(gsc_fd_, VIDIOC_REQBUFS, &reqbufs) != 0)
    DPLOG(ERROR) << "DestroyGscOutputBuffers(): ioctl() failed: VIDIOC_REQBUFS";

  gsc_output_buffer_map_.clear();
  gsc_free_output_buffers_.clear();
  gsc_output_buffer_count_ = 0;
}

}  // namespace content
