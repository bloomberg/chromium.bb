// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <libdrm/drm_fourcc.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/posix/eintr_wrapper.h"
#include "content/common/gpu/media/exynos_video_decode_accelerator.h"
#include "content/common/gpu/media/h264_parser.h"
#include "ui/gl/scoped_binders.h"

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

namespace {

// TODO(posciak): remove once we update linux-headers.
#ifndef V4L2_EVENT_RESOLUTION_CHANGE
#define V4L2_EVENT_RESOLUTION_CHANGE 5
#endif

const char kExynosMfcDevice[] = "/dev/mfc-dec";
const char kMaliDriver[] = "libmali.so";

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
  PictureBufferArrayRef(EGLDisplay egl_display);
  ~PictureBufferArrayRef();

  struct PictureBufferRef {
    PictureBufferRef(EGLImageKHR egl_image, int32 picture_id)
        : egl_image(egl_image), picture_id(picture_id) {}
    EGLImageKHR egl_image;
    int32 picture_id;
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

struct ExynosVideoDecodeAccelerator::PictureRecord {
  PictureRecord(bool cleared, const media::Picture& picture);
  ~PictureRecord();
  bool cleared;  // Whether the texture is cleared and safe to render from.
  media::Picture picture;  // The decoded picture.
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
    EGLDisplay egl_display)
    : egl_display(egl_display) {}

ExynosVideoDecodeAccelerator::PictureBufferArrayRef::~PictureBufferArrayRef() {
  for (size_t i = 0; i < picture_buffers.size(); ++i) {
    EGLImageKHR egl_image = picture_buffers[i].egl_image;
    if (egl_image != EGL_NO_IMAGE_KHR)
      eglDestroyImageKHR(egl_display, egl_image);
  }
}

ExynosVideoDecodeAccelerator::EGLSyncKHRRef::EGLSyncKHRRef(
    EGLDisplay egl_display, EGLSyncKHR egl_sync)
    : egl_display(egl_display),
      egl_sync(egl_sync) {
}

ExynosVideoDecodeAccelerator::EGLSyncKHRRef::~EGLSyncKHRRef() {
  if (egl_sync != EGL_NO_SYNC_KHR)
    eglDestroySyncKHR(egl_display, egl_sync);
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
      at_client(false),
      egl_image(EGL_NO_IMAGE_KHR),
      egl_sync(EGL_NO_SYNC_KHR),
      picture_id(-1),
      cleared(false) {
  for (size_t i = 0; i < arraysize(fds); ++i)
    fds[i] = -1;
}

ExynosVideoDecodeAccelerator::MfcOutputRecord::~MfcOutputRecord() {}

ExynosVideoDecodeAccelerator::PictureRecord::PictureRecord(
    bool cleared,
    const media::Picture& picture)
    : cleared(cleared), picture(picture) {}

ExynosVideoDecodeAccelerator::PictureRecord::~PictureRecord() {}

ExynosVideoDecodeAccelerator::ExynosVideoDecodeAccelerator(
    EGLDisplay egl_display,
    EGLContext egl_context,
    Client* client,
    const base::WeakPtr<Client>& io_client,
    const base::Callback<bool(void)>& make_context_current,
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop_proxy)
    : child_message_loop_proxy_(base::MessageLoopProxy::current()),
      io_message_loop_proxy_(io_message_loop_proxy),
      weak_this_(base::AsWeakPtr(this)),
      client_ptr_factory_(client),
      client_(client_ptr_factory_.GetWeakPtr()),
      io_client_(io_client),
      decoder_thread_("ExynosDecoderThread"),
      decoder_state_(kUninitialized),
      decoder_delay_bitstream_buffer_id_(-1),
      decoder_current_input_buffer_(-1),
      decoder_decode_buffer_tasks_scheduled_(0),
      decoder_frames_at_client_(0),
      decoder_flushing_(false),
      resolution_change_pending_(false),
      resolution_change_reset_pending_(false),
      decoder_partial_frame_pending_(false),
      mfc_fd_(-1),
      mfc_input_streamon_(false),
      mfc_input_buffer_queued_count_(0),
      mfc_output_streamon_(false),
      mfc_output_buffer_queued_count_(0),
      mfc_output_buffer_pixelformat_(0),
      mfc_output_dpb_size_(0),
      picture_clearing_count_(0),
      device_poll_thread_("ExynosDevicePollThread"),
      device_poll_interrupt_fd_(-1),
      make_context_current_(make_context_current),
      egl_display_(egl_display),
      egl_context_(egl_context),
      video_profile_(media::VIDEO_CODEC_PROFILE_UNKNOWN) {}

ExynosVideoDecodeAccelerator::~ExynosVideoDecodeAccelerator() {
  DCHECK(!decoder_thread_.IsRunning());
  DCHECK(!device_poll_thread_.IsRunning());

  if (device_poll_interrupt_fd_ != -1) {
    HANDLE_EINTR(close(device_poll_interrupt_fd_));
    device_poll_interrupt_fd_ = -1;
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

  // We need the context to be initialized to query extensions.
  if (!make_context_current_.Run()) {
    DLOG(ERROR) << "Initialize(): could not make context current";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return false;
  }

  if (!gfx::g_driver_egl.ext.b_EGL_KHR_fence_sync) {
    DLOG(ERROR) << "Initialize(): context does not have EGL_KHR_fence_sync";
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

  if (!CreateMfcInputBuffers())
    return false;

  // MFC output format has to be setup before streaming starts.
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_S_FMT, &format);

  // Subscribe to the resolution change event.
  struct v4l2_event_subscription sub;
  memset(&sub, 0, sizeof(sub));
  sub.type = V4L2_EVENT_RESOLUTION_CHANGE;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_SUBSCRIBE_EVENT, &sub);

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
  DCHECK(io_message_loop_proxy_->BelongsToCurrentThread());

  // DecodeTask() will take care of running a DecodeBufferTask().
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &ExynosVideoDecodeAccelerator::DecodeTask, base::Unretained(this),
      bitstream_buffer));
}

void ExynosVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DVLOG(3) << "AssignPictureBuffers(): buffer_count=" << buffers.size();
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  if (buffers.size() != mfc_output_buffer_map_.size()) {
    DLOG(ERROR) << "AssignPictureBuffers(): Failed to provide requested picture"
                   " buffers. (Got " << buffers.size()
                << ", requested " << mfc_output_buffer_map_.size() << ")";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  if (!make_context_current_.Run()) {
    DLOG(ERROR) << "AssignPictureBuffers(): could not make context current";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  scoped_ptr<PictureBufferArrayRef> picture_buffers_ref(
      new PictureBufferArrayRef(egl_display_));
  gfx::ScopedTextureBinder bind_restore(GL_TEXTURE_EXTERNAL_OES, 0);
  EGLint attrs[] = {
      EGL_WIDTH,                     0, EGL_HEIGHT,                    0,
      EGL_LINUX_DRM_FOURCC_EXT,      0, EGL_DMA_BUF_PLANE0_FD_EXT,     0,
      EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0, EGL_DMA_BUF_PLANE0_PITCH_EXT,  0,
      EGL_DMA_BUF_PLANE1_FD_EXT,     0, EGL_DMA_BUF_PLANE1_OFFSET_EXT, 0,
      EGL_DMA_BUF_PLANE1_PITCH_EXT,  0, EGL_NONE, };
  attrs[1] = frame_buffer_size_.width();
  attrs[3] = frame_buffer_size_.height();
  attrs[5] = DRM_FORMAT_NV12;
  for (size_t i = 0; i < mfc_output_buffer_map_.size(); ++i) {
    DCHECK(buffers[i].size() == frame_buffer_size_);
    MfcOutputRecord& output_record = mfc_output_buffer_map_[i];
    attrs[7]  = output_record.fds[0];
    attrs[9]  = 0;
    attrs[11] = frame_buffer_size_.width();
    attrs[13] = output_record.fds[1];
    attrs[15] = 0;
    attrs[17] = frame_buffer_size_.width();
    EGLImageKHR egl_image = eglCreateImageKHR(
        egl_display_, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attrs);
    if (egl_image == EGL_NO_IMAGE_KHR) {
      DLOG(ERROR) << "AssignPictureBuffers(): could not create EGLImageKHR";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return;
    }

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, buffers[i].texture_id());
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, egl_image);
    picture_buffers_ref->picture_buffers.push_back(
        PictureBufferArrayRef::PictureBufferRef(egl_image, buffers[i].id()));
  }
  decoder_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&ExynosVideoDecodeAccelerator::AssignPictureBuffersTask,
                 base::Unretained(this),
                 base::Passed(&picture_buffers_ref)));
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
      eglCreateSyncKHR(egl_display_, EGL_SYNC_FENCE_KHR, NULL);
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

bool ExynosVideoDecodeAccelerator::CanDecodeOnIOThread() { return true; }

// static
void ExynosVideoDecodeAccelerator::PreSandboxInitialization() {
  DVLOG(3) << "PreSandboxInitialization()";
  dlerror();
  if (!dlopen(kMaliDriver, RTLD_LAZY | RTLD_LOCAL)) {
    DPLOG(ERROR) << "failed to dlopen()" << kMaliDriver << ": " << dlerror();
  }
}

void ExynosVideoDecodeAccelerator::DecodeTask(
    const media::BitstreamBuffer& bitstream_buffer) {
  DVLOG(3) << "DecodeTask(): input_id=" << bitstream_buffer.id();
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT1("Video Decoder", "EVDA::DecodeTask", "input_id",
               bitstream_buffer.id());

  scoped_ptr<BitstreamBufferRef> bitstream_record(new BitstreamBufferRef(
      io_client_, io_message_loop_proxy_,
      new base::SharedMemory(bitstream_buffer.handle(), true),
      bitstream_buffer.size(), bitstream_buffer.id()));
  if (!bitstream_record->shm->Map(bitstream_buffer.size())) {
    DLOG(ERROR) << "Decode(): could not map bitstream_buffer";
    NOTIFY_ERROR(UNREADABLE_INPUT);
    return;
  }
  DVLOG(3) << "Decode(): mapped to addr=" << bitstream_record->shm->memory();

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

  decoder_input_queue_.push(
      linked_ptr<BitstreamBufferRef>(bitstream_record.release()));
  decoder_decode_buffer_tasks_scheduled_++;
  DecodeBufferTask();
}

void ExynosVideoDecodeAccelerator::DecodeBufferTask() {
  DVLOG(3) << "DecodeBufferTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT0("Video Decoder", "EVDA::DecodeBufferTask");

  decoder_decode_buffer_tasks_scheduled_--;

  if (decoder_state_ == kResetting) {
    DVLOG(2) << "DecodeBufferTask(): early out: kResetting state";
    return;
  } else if (decoder_state_ == kError) {
    DVLOG(2) << "DecodeBufferTask(): early out: kError state";
    return;
  } else if (decoder_state_ == kChangingResolution) {
    DVLOG(2) << "DecodeBufferTask(): early out: resolution change pending";
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
    decoder_input_queue_.pop();
    DVLOG(3) << "DecodeBufferTask(): reading input_id="
             << decoder_current_bitstream_buffer_->input_id
             << ", addr=" << (decoder_current_bitstream_buffer_->shm ?
                              decoder_current_bitstream_buffer_->shm->memory() :
                              NULL)
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
        decoder_partial_frame_pending_ = false;
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
    if (!AdvanceFrameFragment(data, data_size, &decoded_size)) {
      NOTIFY_ERROR(UNREADABLE_INPUT);
      return;
    }
    // AdvanceFrameFragment should not return a size larger than the buffer
    // size, even on invalid data.
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

bool ExynosVideoDecodeAccelerator::AdvanceFrameFragment(
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
    *endpos = 0;

    // Keep on peeking the next NALs while they don't indicate a frame
    // boundary.
    for (;;) {
      bool end_of_frame = false;
      result = decoder_h264_parser_->AdvanceToNextNALU(&nalu);
      if (result == content::H264Parser::kInvalidStream ||
          result == content::H264Parser::kUnsupportedStream)
        return false;
      if (result == content::H264Parser::kEOStream) {
        // We've reached the end of the buffer before finding a frame boundary.
        decoder_partial_frame_pending_ = true;
        return true;
      }
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
          if (nalu.data[1] >= 0x80) {
            end_of_frame = true;
            break;
          }
          break;
        case content::H264NALU::kSPS:
        case content::H264NALU::kPPS:
        case content::H264NALU::kEOSeq:
        case content::H264NALU::kEOStream:
          // These unconditionally signal a frame boundary.
          end_of_frame = true;
          break;
        default:
          // For all others, keep going.
          break;
      }
      if (end_of_frame) {
        if (!decoder_partial_frame_pending_ && *endpos == 0) {
          // The frame was previously restarted, and we haven't filled the
          // current frame with any contents yet.  Start the new frame here and
          // continue parsing NALs.
        } else {
          // The frame wasn't previously restarted and/or we have contents for
          // the current frame; signal the start of a new frame here: we don't
          // have a partial frame anymore.
          decoder_partial_frame_pending_ = false;
          return true;
        }
      }
      *endpos = (nalu.data + nalu.size) - data;
    }
    NOTREACHED();
    return false;
  } else {
    DCHECK_GE(video_profile_, media::VP8PROFILE_MIN);
    DCHECK_LE(video_profile_, media::VP8PROFILE_MAX);
    // For VP8, we can just dump the entire buffer.  No fragmentation needed,
    // and we never return a partial frame.
    *endpos = size;
    decoder_partial_frame_pending_ = false;
    return true;
  }
}

void ExynosVideoDecodeAccelerator::ScheduleDecodeBufferTaskIfNeeded() {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());

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
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kDecoding);
  DCHECK(!device_poll_thread_.IsRunning());
  // Initial decode.  We haven't been able to get output stream format info yet.
  // Get it, and start decoding.

  // Copy in and send to HW.
  if (!AppendToInputFrame(data, size))
    return false;

  // If we only have a partial frame, don't flush and process yet.
  if (decoder_partial_frame_pending_)
    return true;

  if (!FlushInputFrame())
    return false;

  // Recycle buffers.
  DequeueMfc();

  // Check and see if we have format info yet.
  struct v4l2_format format;
  bool again = false;
  if (!GetFormatInfo(&format, &again))
    return false;

  if (again) {
    // Need more stream to decode format, return true and schedule next buffer.
    *endpos = size;
    return true;
  }

  // Run this initialization only on first startup.
  if (decoder_state_ == kInitialized) {
    DVLOG(3) << "DecodeBufferInitial(): running initialization";
    // Success! Setup our parameters.
    if (!CreateBuffersForFormat(format))
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
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_EQ(decoder_state_, kDecoding);

  // Both of these calls will set kError state if they fail.
  // Only flush the frame if it's complete.
  return (AppendToInputFrame(data, size) &&
          (decoder_partial_frame_pending_ || FlushInputFrame()));
}

bool ExynosVideoDecodeAccelerator::AppendToInputFrame(
    const void* data, size_t size) {
  DVLOG(3) << "AppendToInputFrame()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
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

  DCHECK(data != NULL || size == 0);
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
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  DCHECK_NE(decoder_state_, kResetting);
  DCHECK_NE(decoder_state_, kError);

  if (decoder_current_input_buffer_ == -1)
    return true;

  MfcInputRecord& input_record =
      mfc_input_buffer_map_[decoder_current_input_buffer_];
  DCHECK_NE(input_record.input_id, -1);
  DCHECK(input_record.input_id != kFlushBufferId ||
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
  mfc_input_ready_queue_.push(decoder_current_input_buffer_);
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
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT0("Video Decoder", "EVDA::AssignPictureBuffersTask");

  // We run AssignPictureBuffersTask even if we're in kResetting.
  if (decoder_state_ == kError) {
    DVLOG(2) << "AssignPictureBuffersTask(): early out: kError state";
    return;
  }

  DCHECK_EQ(pic_buffers->picture_buffers.size(), mfc_output_buffer_map_.size());
  for (size_t i = 0; i < mfc_output_buffer_map_.size(); ++i) {
    MfcOutputRecord& output_record = mfc_output_buffer_map_[i];
    PictureBufferArrayRef::PictureBufferRef& buffer_ref =
        pic_buffers->picture_buffers[i];
    // We should be blank right now.
    DCHECK(!output_record.at_device);
    DCHECK(!output_record.at_client);
    DCHECK_EQ(output_record.egl_image, EGL_NO_IMAGE_KHR);
    DCHECK_EQ(output_record.egl_sync, EGL_NO_SYNC_KHR);
    DCHECK_EQ(output_record.picture_id, -1);
    DCHECK_EQ(output_record.cleared, false);
    output_record.egl_image = buffer_ref.egl_image;
    output_record.picture_id = buffer_ref.picture_id;
    mfc_free_output_buffers_.push(i);
    DVLOG(3) << "AssignPictureBuffersTask(): buffer[" << i
             << "]: picture_id=" << buffer_ref.picture_id;
  }
  pic_buffers->picture_buffers.clear();

  // We got buffers!  Kick the MFC.
  EnqueueMfc();

  if (decoder_state_ == kChangingResolution)
    ResumeAfterResolutionChange();
}

void ExynosVideoDecodeAccelerator::ServiceDeviceTask(bool mfc_event_pending) {
  DVLOG(3) << "ServiceDeviceTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
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
  } else if (decoder_state_ == kChangingResolution) {
    DVLOG(2) << "ServiceDeviceTask(): early out: kChangingResolution state";
    return;
  }

  if (mfc_event_pending)
    DequeueMfcEvents();
  DequeueMfc();
  EnqueueMfc();

  // Clear the interrupt fd.
  if (!ClearDevicePollInterrupt())
    return;

  unsigned int poll_fds = 0;
  // Add MFC fd, if we should poll on it.
  // MFC can be polled as soon as either input or output buffers are queued.
  if (mfc_input_buffer_queued_count_ + mfc_output_buffer_queued_count_ > 0)
    poll_fds |= kPollMfc;

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
           << mfc_input_buffer_map_.size() << "->"
           << mfc_free_output_buffers_.size() << "+"
           << mfc_output_buffer_queued_count_ << "/"
           << mfc_output_buffer_map_.size() << "] => VDA["
           << decoder_frames_at_client_ << "]";

  ScheduleDecodeBufferTaskIfNeeded();
  StartResolutionChangeIfNeeded();
}

void ExynosVideoDecodeAccelerator::EnqueueMfc() {
  DVLOG(3) << "EnqueueMfc()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
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

void ExynosVideoDecodeAccelerator::DequeueMfcEvents() {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  DVLOG(3) << "DequeueMfcEvents()";

  struct v4l2_event ev;
  memset(&ev, 0, sizeof(ev));

  while (ioctl(mfc_fd_, VIDIOC_DQEVENT, &ev) == 0) {
    if (ev.type == V4L2_EVENT_RESOLUTION_CHANGE) {
      DVLOG(3) << "DequeueMfcEvents(): got resolution change event.";
      DCHECK(!resolution_change_pending_);
      resolution_change_pending_ = true;
    } else {
      DLOG(FATAL) << "DequeueMfcEvents(): got an event (" << ev.type
                  << ") we haven't subscribed to.";
    }
  }
}

void ExynosVideoDecodeAccelerator::DequeueMfc() {
  DVLOG(3) << "DequeueMfc()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(decoder_state_, kUninitialized);
  TRACE_EVENT0("Video Decoder", "EVDA::DequeueMfc");

  // Dequeue completed MFC input (VIDEO_OUTPUT) buffers, and recycle to the free
  // list.
  struct v4l2_buffer dqbuf;
  struct v4l2_plane planes[2];
  while (mfc_input_buffer_queued_count_ > 0) {
    DCHECK(mfc_input_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(planes, 0, sizeof(planes));
    dqbuf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dqbuf.memory = V4L2_MEMORY_MMAP;
    dqbuf.m.planes = planes;
    dqbuf.length = 1;
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
    DCHECK(!output_record.at_client);
    DCHECK_NE(output_record.egl_image, EGL_NO_IMAGE_KHR);
    DCHECK_NE(output_record.picture_id, -1);
    output_record.at_device = false;
    if (dqbuf.m.planes[0].bytesused + dqbuf.m.planes[1].bytesused == 0) {
      // This is an empty output buffer returned as part of a flush.
      mfc_free_output_buffers_.push(dqbuf.index);
    } else {
      DCHECK_GE(dqbuf.timestamp.tv_sec, 0);
      output_record.at_client = true;
      DVLOG(3) << "DequeueMfc(): returning input_id=" << dqbuf.timestamp.tv_sec
               << " as picture_id=" << output_record.picture_id;
      const media::Picture& picture =
          media::Picture(output_record.picture_id, dqbuf.timestamp.tv_sec);
      pending_picture_ready_.push(
          PictureRecord(output_record.cleared, picture));
      SendPictureReady();
      output_record.cleared = true;
      decoder_frames_at_client_++;
    }
    mfc_output_buffer_queued_count_--;
  }

  NotifyFlushDoneIfNeeded();
}

bool ExynosVideoDecodeAccelerator::EnqueueMfcInputRecord() {
  DVLOG(3) << "EnqueueMfcInputRecord()";
  DCHECK(!mfc_input_ready_queue_.empty());

  // Enqueue a MFC input (VIDEO_OUTPUT) buffer.
  const int buffer = mfc_input_ready_queue_.front();
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
  mfc_input_ready_queue_.pop();
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
  const int buffer = mfc_free_output_buffers_.front();
  MfcOutputRecord& output_record = mfc_output_buffer_map_[buffer];
  DCHECK(!output_record.at_device);
  DCHECK(!output_record.at_client);
  DCHECK_NE(output_record.egl_image, EGL_NO_IMAGE_KHR);
  DCHECK_NE(output_record.picture_id, -1);
  if (output_record.egl_sync != EGL_NO_SYNC_KHR) {
    TRACE_EVENT0("Video Decoder",
                 "EVDA::EnqueueMfcOutputRecord: eglClientWaitSyncKHR");
    // If we have to wait for completion, wait.  Note that
    // mfc_free_output_buffers_ is a FIFO queue, so we always wait on the
    // buffer that has been in the queue the longest.
    eglClientWaitSyncKHR(egl_display_, output_record.egl_sync, 0,
        EGL_FOREVER_KHR);
    eglDestroySyncKHR(egl_display_, output_record.egl_sync);
    output_record.egl_sync = EGL_NO_SYNC_KHR;
  }
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_planes[arraysize(output_record.fds)];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes, 0, sizeof(qbuf_planes));
  qbuf.index    = buffer;
  qbuf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  qbuf.memory   = V4L2_MEMORY_MMAP;
  qbuf.m.planes = qbuf_planes;
  qbuf.length   = arraysize(output_record.fds);
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_QBUF, &qbuf);
  mfc_free_output_buffers_.pop();
  output_record.at_device = true;
  mfc_output_buffer_queued_count_++;
  return true;
}

void ExynosVideoDecodeAccelerator::ReusePictureBufferTask(
    int32 picture_buffer_id, scoped_ptr<EGLSyncKHRRef> egl_sync_ref) {
  DVLOG(3) << "ReusePictureBufferTask(): picture_buffer_id="
           << picture_buffer_id;
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "EVDA::ReusePictureBufferTask");

  // We run ReusePictureBufferTask even if we're in kResetting.
  if (decoder_state_ == kError) {
    DVLOG(2) << "ReusePictureBufferTask(): early out: kError state";
    return;
  }

  if (decoder_state_ == kChangingResolution) {
    DVLOG(2) << "ReusePictureBufferTask(): early out: kChangingResolution";
    return;
  }

  size_t index;
  for (index = 0; index < mfc_output_buffer_map_.size(); ++index)
    if (mfc_output_buffer_map_[index].picture_id == picture_buffer_id)
      break;

  if (index >= mfc_output_buffer_map_.size()) {
    DLOG(ERROR) << "ReusePictureBufferTask(): picture_buffer_id not found";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  MfcOutputRecord& output_record = mfc_output_buffer_map_[index];
  if (output_record.at_device || !output_record.at_client) {
    DLOG(ERROR) << "ReusePictureBufferTask(): picture_buffer_id not reusable";
    NOTIFY_ERROR(INVALID_ARGUMENT);
    return;
  }

  DCHECK_EQ(output_record.egl_sync, EGL_NO_SYNC_KHR);
  output_record.at_client = false;
  output_record.egl_sync = egl_sync_ref->egl_sync;
  mfc_free_output_buffers_.push(index);
  decoder_frames_at_client_--;
  // Take ownership of the EGLSync.
  egl_sync_ref->egl_sync = EGL_NO_SYNC_KHR;
  // We got a buffer back, so kick the MFC.
  EnqueueMfc();
}

void ExynosVideoDecodeAccelerator::FlushTask() {
  DVLOG(3) << "FlushTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "EVDA::FlushTask");

  // Flush outstanding buffers.
  if (decoder_state_ == kInitialized || decoder_state_ == kAfterReset) {
    // There's nothing in the pipe, so return done immediately.
    DVLOG(3) << "FlushTask(): returning flush";
    child_message_loop_proxy_->PostTask(
        FROM_HERE, base::Bind(&Client::NotifyFlushDone, client_));
    return;
  } else if (decoder_state_ == kError) {
    DVLOG(2) << "FlushTask(): early out: kError state";
    return;
  }

  // We don't support stacked flushing.
  DCHECK(!decoder_flushing_);

  // Queue up an empty buffer -- this triggers the flush.
  decoder_input_queue_.push(
      linked_ptr<BitstreamBufferRef>(new BitstreamBufferRef(
          io_client_, io_message_loop_proxy_, NULL, 0, kFlushBufferId)));
  decoder_flushing_ = true;
  SendPictureReady();  // Send all pending PictureReady.

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
  if (!decoder_input_queue_.empty()) {
    if (decoder_input_queue_.front()->input_id !=
        decoder_delay_bitstream_buffer_id_)
      return;
  }
  if (decoder_current_input_buffer_ != -1)
    return;
  if ((mfc_input_ready_queue_.size() + mfc_input_buffer_queued_count_) != 0)
    return;

  // TODO(posciak): crbug.com/270039. MFC requires a streamoff-streamon
  // sequence after flush to continue, even if we are not resetting. This would
  // make sense, because we don't really want to resume from a non-resume point
  // (e.g. not from an IDR) if we are flushed.
  // MSE player however triggers a Flush() on chunk end, but never Reset(). One
  // could argue either way, or even say that Flush() is not needed/harmful when
  // transitioning to next chunk.
  // For now, do the streamoff-streamon cycle to satisfy MFC and not freeze when
  // doing MSE. This should be harmless otherwise.
  if (!StopDevicePoll(false))
    return;

  if (!StartDevicePoll())
    return;

  decoder_delay_bitstream_buffer_id_ = -1;
  decoder_flushing_ = false;
  DVLOG(3) << "NotifyFlushDoneIfNeeded(): returning flush";
  child_message_loop_proxy_->PostTask(
      FROM_HERE, base::Bind(&Client::NotifyFlushDone, client_));

  // While we were flushing, we early-outed DecodeBufferTask()s.
  ScheduleDecodeBufferTaskIfNeeded();
}

void ExynosVideoDecodeAccelerator::ResetTask() {
  DVLOG(3) << "ResetTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "EVDA::ResetTask");

  if (decoder_state_ == kError) {
    DVLOG(2) << "ResetTask(): early out: kError state";
    return;
  }

  // If we are in the middle of switching resolutions, postpone reset until
  // it's done. We don't have to worry about timing of this wrt to decoding,
  // because MFC input pipe is already stopped if we are changing resolution.
  // We will come back here after we are done with the resolution change.
  DCHECK(!resolution_change_reset_pending_);
  if (resolution_change_pending_ || decoder_state_ == kChangingResolution) {
    resolution_change_reset_pending_ = true;
    return;
  }

  // We stop streaming and clear buffer tracking info (not preserving
  // MFC inputs).
  // StopDevicePoll() unconditionally does _not_ destroy buffers, however.
  if (!StopDevicePoll(false))
    return;

  decoder_current_bitstream_buffer_.reset();
  while (!decoder_input_queue_.empty())
    decoder_input_queue_.pop();

  decoder_current_input_buffer_ = -1;

  // If we were flushing, we'll never return any more BitstreamBuffers or
  // PictureBuffers; they have all been dropped and returned by now.
  NotifyFlushDoneIfNeeded();

  // Mark that we're resetting, then enqueue a ResetDoneTask().  All intervening
  // jobs will early-out in the kResetting state.
  decoder_state_ = kResetting;
  SendPictureReady();  // Send all pending PictureReady.
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &ExynosVideoDecodeAccelerator::ResetDoneTask, base::Unretained(this)));
}

void ExynosVideoDecodeAccelerator::ResetDoneTask() {
  DVLOG(3) << "ResetDoneTask()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "EVDA::ResetDoneTask");

  if (decoder_state_ == kError) {
    DVLOG(2) << "ResetDoneTask(): early out: kError state";
    return;
  }

  // We might have received a resolution change event while we were waiting
  // for the reset to finish. The codec will not post another event if the
  // resolution after reset remains the same as the one to which were just
  // about to switch, so preserve the event across reset so we can address
  // it after resuming.

  // Reset format-specific bits.
  if (video_profile_ >= media::H264PROFILE_MIN &&
      video_profile_ <= media::H264PROFILE_MAX) {
    decoder_h264_parser_.reset(new content::H264Parser());
  }

  // Jobs drained, we're finished resetting.
  DCHECK_EQ(decoder_state_, kResetting);
  decoder_state_ = kAfterReset;
  decoder_partial_frame_pending_ = false;
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
  StopDevicePoll(false);

  decoder_current_bitstream_buffer_.reset();
  decoder_current_input_buffer_ = -1;
  decoder_decode_buffer_tasks_scheduled_ = 0;
  decoder_frames_at_client_ = 0;
  while (!decoder_input_queue_.empty())
    decoder_input_queue_.pop();
  decoder_flushing_ = false;

  // Set our state to kError.  Just in case.
  decoder_state_ = kError;
}

bool ExynosVideoDecodeAccelerator::StartDevicePoll() {
  DVLOG(3) << "StartDevicePoll()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
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

bool ExynosVideoDecodeAccelerator::StopDevicePoll(bool keep_mfc_input_state) {
  DVLOG(3) << "StopDevicePoll()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());

  // Signal the DevicePollTask() to stop, and stop the device poll thread.
  if (!SetDevicePollInterrupt())
    return false;
  device_poll_thread_.Stop();
  // Clear the interrupt now, to be sure.
  if (!ClearDevicePollInterrupt())
    return false;

  // Stop streaming.
  if (!keep_mfc_input_state) {
    if (mfc_input_streamon_) {
      __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_STREAMOFF, &type);
    }
    mfc_input_streamon_ = false;
  }
  if (mfc_output_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_STREAMOFF, &type);
  }
  mfc_output_streamon_ = false;

  // Reset all our accounting info.
  if (!keep_mfc_input_state) {
    while (!mfc_input_ready_queue_.empty())
      mfc_input_ready_queue_.pop();
    mfc_free_input_buffers_.clear();
    for (size_t i = 0; i < mfc_input_buffer_map_.size(); ++i) {
      mfc_free_input_buffers_.push_back(i);
      mfc_input_buffer_map_[i].at_device = false;
      mfc_input_buffer_map_[i].bytes_used = 0;
      mfc_input_buffer_map_[i].input_id = -1;
    }
    mfc_input_buffer_queued_count_ = 0;
  }
  while (!mfc_free_output_buffers_.empty())
    mfc_free_output_buffers_.pop();
  for (size_t i = 0; i < mfc_output_buffer_map_.size(); ++i) {
    MfcOutputRecord& output_record = mfc_output_buffer_map_[i];
    // Only mark those free that aren't being held by the VDA client.
    if (!output_record.at_client) {
      DCHECK_EQ(output_record.egl_sync, EGL_NO_SYNC_KHR);
      mfc_free_output_buffers_.push(i);
      mfc_output_buffer_map_[i].at_device = false;
    }
  }
  mfc_output_buffer_queued_count_ = 0;

  DVLOG(3) << "StopDevicePoll(): device poll stopped";
  return true;
}

bool ExynosVideoDecodeAccelerator::SetDevicePollInterrupt() {
  DVLOG(3) << "SetDevicePollInterrupt()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());

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
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());

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

void ExynosVideoDecodeAccelerator::StartResolutionChangeIfNeeded() {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_EQ(decoder_state_, kDecoding);

  if (!resolution_change_pending_)
    return;

  DVLOG(3) << "No more work, initiate resolution change";

  // Keep MFC input queue.
  if (!StopDevicePoll(true))
    return;

  decoder_state_ = kChangingResolution;
  DCHECK(resolution_change_pending_);
  resolution_change_pending_ = false;

  // Post a task to clean up buffers on child thread. This will also ensure
  // that we won't accept ReusePictureBuffer() anymore after that.
  child_message_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &ExynosVideoDecodeAccelerator::ResolutionChangeDestroyBuffers,
      weak_this_));
}

void ExynosVideoDecodeAccelerator::FinishResolutionChange() {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DVLOG(3) << "FinishResolutionChange()";

  if (decoder_state_ == kError) {
    DVLOG(2) << "FinishResolutionChange(): early out: kError state";
    return;
  }

  struct v4l2_format format;
  bool again;
  bool ret = GetFormatInfo(&format, &again);
  if (!ret || again) {
    DVLOG(3) << "Couldn't get format information after resolution change";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  if (!CreateBuffersForFormat(format)) {
    DVLOG(3) << "Couldn't reallocate buffers after resolution change";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  // From here we stay in kChangingResolution and wait for
  // AssignPictureBuffers() before we can resume.
}

void ExynosVideoDecodeAccelerator::ResumeAfterResolutionChange() {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DVLOG(3) << "ResumeAfterResolutionChange()";

  decoder_state_ = kDecoding;

  if (resolution_change_reset_pending_) {
    resolution_change_reset_pending_ = false;
    ResetTask();
    return;
  }

  if (!StartDevicePoll())
    return;

  EnqueueMfc();
  ScheduleDecodeBufferTaskIfNeeded();
}

void ExynosVideoDecodeAccelerator::DevicePollTask(unsigned int poll_fds) {
  DVLOG(3) << "DevicePollTask()";
  DCHECK_EQ(device_poll_thread_.message_loop(), base::MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "EVDA::DevicePollTask");

  // This routine just polls the set of device fds, and schedules a
  // ServiceDeviceTask() on decoder_thread_ when processing needs to occur.
  // Other threads may notify this task to return early by writing to
  // device_poll_interrupt_fd_.
  struct pollfd pollfds[3];
  nfds_t nfds;
  int mfc_pollfd = -1;

  // Add device_poll_interrupt_fd_;
  pollfds[0].fd = device_poll_interrupt_fd_;
  pollfds[0].events = POLLIN | POLLERR;
  nfds = 1;

  if (poll_fds & kPollMfc) {
    DVLOG(3) << "DevicePollTask(): adding MFC to poll() set";
    pollfds[nfds].fd = mfc_fd_;
    pollfds[nfds].events = POLLIN | POLLOUT | POLLERR | POLLPRI;
    mfc_pollfd = nfds;
    nfds++;
  }

  // Poll it!
  if (HANDLE_EINTR(poll(pollfds, nfds, -1)) == -1) {
    DPLOG(ERROR) << "DevicePollTask(): poll() failed";
    NOTIFY_ERROR(PLATFORM_FAILURE);
    return;
  }

  bool mfc_event_pending = (mfc_pollfd != -1 &&
                            pollfds[mfc_pollfd].revents & POLLPRI);

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch decoder state from this thread.
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &ExynosVideoDecodeAccelerator::ServiceDeviceTask,
      base::Unretained(this), mfc_event_pending));
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
  DVLOG(3) << "SetDecoderState(): state=" << state;

  // We can touch decoder_state_ only if this is the decoder thread or the
  // decoder thread isn't running.
  if (decoder_thread_.message_loop() != NULL &&
      decoder_thread_.message_loop() != base::MessageLoop::current()) {
    decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
        &ExynosVideoDecodeAccelerator::SetDecoderState,
        base::Unretained(this), state));
  } else {
    decoder_state_ = state;
  }
}

bool ExynosVideoDecodeAccelerator::GetFormatInfo(struct v4l2_format* format,
                                                 bool* again) {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());

  *again = false;
  memset(format, 0, sizeof(*format));
  format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  if (HANDLE_EINTR(ioctl(mfc_fd_, VIDIOC_G_FMT, format)) != 0) {
    if (errno == EINVAL) {
      // EINVAL means we haven't seen sufficient stream to decode the format.
      *again = true;
      return true;
    } else {
      DPLOG(ERROR) << "DecodeBufferInitial(): ioctl() failed: VIDIOC_G_FMT";
      NOTIFY_ERROR(PLATFORM_FAILURE);
      return false;
    }
  }

  return true;
}

bool ExynosVideoDecodeAccelerator::CreateBuffersForFormat(
    const struct v4l2_format& format) {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  CHECK_EQ(format.fmt.pix_mp.num_planes, 2);
  frame_buffer_size_.SetSize(
      format.fmt.pix_mp.width, format.fmt.pix_mp.height);
  mfc_output_buffer_pixelformat_ = format.fmt.pix_mp.pixelformat;
  DCHECK_EQ(mfc_output_buffer_pixelformat_, V4L2_PIX_FMT_NV12M);
  DVLOG(3) << "CreateBuffersForFormat(): new resolution: "
           << frame_buffer_size_.ToString();

  if (!CreateMfcOutputBuffers())
    return false;

  return true;
}

bool ExynosVideoDecodeAccelerator::CreateMfcInputBuffers() {
  DVLOG(3) << "CreateMfcInputBuffers()";
  // We always run this as we prepare to initialize.
  DCHECK_EQ(decoder_state_, kUninitialized);
  DCHECK(!mfc_input_streamon_);
  DCHECK(mfc_input_buffer_map_.empty());

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
  mfc_input_buffer_map_.resize(reqbufs.count);
  for (size_t i = 0; i < mfc_input_buffer_map_.size(); ++i) {
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
  DCHECK(decoder_state_ == kInitialized ||
         decoder_state_ == kChangingResolution);
  DCHECK(!mfc_output_streamon_);
  DCHECK(mfc_output_buffer_map_.empty());

  // Number of MFC output buffers we need.
  struct v4l2_control ctrl;
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_G_CTRL, &ctrl);
  mfc_output_dpb_size_ = ctrl.value;

  // Output format setup in Initialize().

  // Allocate the output buffers.
  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count  = mfc_output_dpb_size_ + kDpbOutputBufferExtraCount;
  reqbufs.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_REQBUFS, &reqbufs);

  // Create DMABUFs from output buffers.
  mfc_output_buffer_map_.resize(reqbufs.count);
  for (size_t i = 0; i < mfc_output_buffer_map_.size(); ++i) {
    MfcOutputRecord& output_record = mfc_output_buffer_map_[i];
    for (size_t j = 0; j < arraysize(output_record.fds); ++j) {
      // Export the DMABUF fd so we can export it as a texture.
      struct v4l2_exportbuffer expbuf;
      memset(&expbuf, 0, sizeof(expbuf));
      expbuf.type  = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
      expbuf.index = i;
      expbuf.plane = j;
      expbuf.flags = O_CLOEXEC;
      IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_EXPBUF, &expbuf);
      output_record.fds[j] = expbuf.fd;
    }
  }

  DVLOG(3) << "CreateMfcOutputBuffers(): ProvidePictureBuffers(): "
           << "buffer_count=" << mfc_output_buffer_map_.size()
           << ", width=" << frame_buffer_size_.width()
           << ", height=" << frame_buffer_size_.height();
  child_message_loop_proxy_->PostTask(FROM_HERE,
                                      base::Bind(&Client::ProvidePictureBuffers,
                                                 client_,
                                                 mfc_output_buffer_map_.size(),
                                                 frame_buffer_size_,
                                                 GL_TEXTURE_EXTERNAL_OES));

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
}

void ExynosVideoDecodeAccelerator::DestroyMfcOutputBuffers() {
  DVLOG(3) << "DestroyMfcOutputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!mfc_output_streamon_);

  if (mfc_output_buffer_map_.size() != 0) {
    if (!make_context_current_.Run()) {
      DLOG(ERROR) << "DestroyMfcOutputBuffers(): "
                  << "could not make context current";
    } else {
      size_t i = 0;
      do {
        MfcOutputRecord& output_record = mfc_output_buffer_map_[i];
        for (size_t j = 0; j < arraysize(output_record.fds); ++j) {
          if (output_record.fds[j] != -1)
            HANDLE_EINTR(close(output_record.fds[j]));
          if (output_record.egl_image != EGL_NO_IMAGE_KHR)
            eglDestroyImageKHR(egl_display_, output_record.egl_image);
          if (output_record.egl_sync != EGL_NO_SYNC_KHR)
            eglDestroySyncKHR(egl_display_, output_record.egl_sync);
        }
        DVLOG(1) << "DestroyMfcOutputBuffers(): dismissing PictureBuffer id="
                 << output_record.picture_id;
        child_message_loop_proxy_->PostTask(
            FROM_HERE,
            base::Bind(&Client::DismissPictureBuffer,
                       client_,
                       output_record.picture_id));
        i++;
      } while (i < mfc_output_buffer_map_.size());
    }
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  if (ioctl(mfc_fd_, VIDIOC_REQBUFS, &reqbufs) != 0)
    DPLOG(ERROR) << "DestroyMfcOutputBuffers() ioctl() failed: VIDIOC_REQBUFS";

  mfc_output_buffer_map_.clear();
  while (!mfc_free_output_buffers_.empty())
    mfc_free_output_buffers_.pop();
}

void ExynosVideoDecodeAccelerator::ResolutionChangeDestroyBuffers() {
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DVLOG(3) << "ResolutionChangeDestroyBuffers()";

  DestroyMfcOutputBuffers();

  // Finish resolution change on decoder thread.
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &ExynosVideoDecodeAccelerator::FinishResolutionChange,
      base::Unretained(this)));
}

void ExynosVideoDecodeAccelerator::SendPictureReady() {
  DVLOG(3) << "SendPictureReady()";
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  bool resetting_or_flushing =
      (decoder_state_ == kResetting || decoder_flushing_);
  while (pending_picture_ready_.size() > 0) {
    bool cleared = pending_picture_ready_.front().cleared;
    const media::Picture& picture = pending_picture_ready_.front().picture;
    if (cleared && picture_clearing_count_ == 0) {
      // This picture is cleared. Post it to IO thread to reduce latency. This
      // should be the case after all pictures are cleared at the beginning.
      io_message_loop_proxy_->PostTask(
          FROM_HERE, base::Bind(&Client::PictureReady, io_client_, picture));
      pending_picture_ready_.pop();
    } else if (!cleared || resetting_or_flushing) {
      DVLOG(3) << "SendPictureReady()"
               << ". cleared=" << pending_picture_ready_.front().cleared
               << ", decoder_state_=" << decoder_state_
               << ", decoder_flushing_=" << decoder_flushing_
               << ", picture_clearing_count_=" << picture_clearing_count_;
      // If the picture is not cleared, post it to the child thread because it
      // has to be cleared in the child thread. A picture only needs to be
      // cleared once. If the decoder is resetting or flushing, send all
      // pictures to ensure PictureReady arrive before reset or flush done.
      child_message_loop_proxy_->PostTaskAndReply(
          FROM_HERE,
          base::Bind(&Client::PictureReady, client_, picture),
          // Unretained is safe. If Client::PictureReady gets to run, |this| is
          // alive. Destroy() will wait the decode thread to finish.
          base::Bind(&ExynosVideoDecodeAccelerator::PictureCleared,
                     base::Unretained(this)));
      picture_clearing_count_++;
      pending_picture_ready_.pop();
    } else {
      // This picture is cleared. But some pictures are about to be cleared on
      // the child thread. To preserve the order, do not send this until those
      // pictures are cleared.
      break;
    }
  }
}

void ExynosVideoDecodeAccelerator::PictureCleared() {
  DVLOG(3) << "PictureCleared(). clearing count=" << picture_clearing_count_;
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_GT(picture_clearing_count_, 0);
  picture_clearing_count_--;
  SendPictureReady();
}

}  // namespace content
