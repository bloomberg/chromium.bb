// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/exynos_video_encode_accelerator.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/posix/eintr_wrapper.h"
#include "content/public/common/content_switches.h"
#include "media/base/bitstream_buffer.h"

#define NOTIFY_ERROR(x)                                            \
  do {                                                             \
    SetEncoderState(kError);                                       \
    DLOG(ERROR) << "calling NotifyError(): " << x;                 \
    NotifyError(x);                                                \
  } while (0)

#define IOCTL_OR_ERROR_RETURN_VALUE(fd, type, arg, value)          \
  do {                                                             \
    if (HANDLE_EINTR(ioctl(fd, type, arg) != 0)) {                 \
      DPLOG(ERROR) << __func__ << "(): ioctl() failed: " << #type; \
      NOTIFY_ERROR(kPlatformFailureError);                         \
      return value;                                                \
    }                                                              \
  } while (0)

#define IOCTL_OR_ERROR_RETURN(fd, type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(fd, type, arg, ((void)0))

#define IOCTL_OR_ERROR_RETURN_FALSE(fd, type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(fd, type, arg, false)

namespace content {

namespace {

const char kExynosGscDevice[] = "/dev/gsc1";
const char kExynosMfcDevice[] = "/dev/mfc-enc";

// File descriptors we need to poll, one-bit flag for each.
enum PollFds {
  kPollGsc = (1 << 0),
  kPollMfc = (1 << 1),
};

}  // anonymous namespace

struct ExynosVideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(int32 id, scoped_ptr<base::SharedMemory> shm, size_t size)
      : id(id), shm(shm.Pass()), size(size) {}
  const int32 id;
  const scoped_ptr<base::SharedMemory> shm;
  const size_t size;
};


ExynosVideoEncodeAccelerator::GscInputRecord::GscInputRecord()
    : at_device(false) {}

ExynosVideoEncodeAccelerator::GscOutputRecord::GscOutputRecord()
    : at_device(false), mfc_input(-1) {}

ExynosVideoEncodeAccelerator::MfcInputRecord::MfcInputRecord()
    : at_device(false) {
  fd[0] = fd[1] = -1;
}

ExynosVideoEncodeAccelerator::MfcOutputRecord::MfcOutputRecord()
    : at_device(false), address(NULL), length(0) {}

ExynosVideoEncodeAccelerator::ExynosVideoEncodeAccelerator()
    : child_message_loop_proxy_(base::MessageLoopProxy::current()),
      weak_this_ptr_factory_(this),
      weak_this_(weak_this_ptr_factory_.GetWeakPtr()),
      encoder_thread_("ExynosEncoderThread"),
      encoder_state_(kUninitialized),
      output_buffer_byte_size_(0),
      stream_header_size_(0),
      input_format_fourcc_(0),
      output_format_fourcc_(0),
      gsc_fd_(-1),
      gsc_input_streamon_(false),
      gsc_input_buffer_queued_count_(0),
      gsc_output_streamon_(false),
      gsc_output_buffer_queued_count_(0),
      mfc_fd_(-1),
      mfc_input_streamon_(false),
      mfc_input_buffer_queued_count_(0),
      mfc_output_streamon_(false),
      mfc_output_buffer_queued_count_(0),
      device_poll_thread_("ExynosEncoderDevicePollThread"),
      device_poll_interrupt_fd_(-1) {}

ExynosVideoEncodeAccelerator::~ExynosVideoEncodeAccelerator() {
  DCHECK(!encoder_thread_.IsRunning());
  DCHECK(!device_poll_thread_.IsRunning());

  if (device_poll_interrupt_fd_ != -1) {
    close(device_poll_interrupt_fd_);
    device_poll_interrupt_fd_ = -1;
  }
  if (gsc_fd_ != -1) {
    DestroyGscInputBuffers();
    DestroyGscOutputBuffers();
    close(gsc_fd_);
    gsc_fd_ = -1;
  }
  if (mfc_fd_ != -1) {
    DestroyMfcInputBuffers();
    DestroyMfcOutputBuffers();
    close(mfc_fd_);
    mfc_fd_ = -1;
  }
}

bool ExynosVideoEncodeAccelerator::Initialize(
    media::VideoFrame::Format input_format,
    const gfx::Size& input_visible_size,
    media::VideoCodecProfile output_profile,
    uint32 initial_bitrate,
    Client* client) {
  DVLOG(3) << "Initialize(): input_format=" << input_format
           << ", input_visible_size=" << input_visible_size.ToString()
           << ", output_profile=" << output_profile
           << ", initial_bitrate=" << initial_bitrate;

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();

  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(encoder_state_, kUninitialized);

  input_visible_size_ = input_visible_size;
  input_allocated_size_.SetSize((input_visible_size_.width() + 0xF) & ~0xF,
                                (input_visible_size_.height() + 0xF) & ~0xF);
  converted_visible_size_.SetSize((input_visible_size_.width() + 0x1) & ~0x1,
                                  (input_visible_size_.height() + 0x1) & ~0x1);
  converted_allocated_size_.SetSize(
      (converted_visible_size_.width() + 0xF) & ~0xF,
      (converted_visible_size_.height() + 0xF) & ~0xF);
  output_visible_size_ = converted_visible_size_;

  switch (input_format) {
    case media::VideoFrame::I420:
      input_format_fourcc_ = V4L2_PIX_FMT_YUV420M;
      break;
    default:
      DLOG(ERROR) << "Initialize(): invalid input_format=" << input_format;
      return false;
  }

  if (output_profile >= media::H264PROFILE_MIN &&
      output_profile <= media::H264PROFILE_MAX) {
    output_format_fourcc_ = V4L2_PIX_FMT_H264;
  } else if (output_profile >= media::VP8PROFILE_MIN &&
             output_profile <= media::VP8PROFILE_MAX) {
    output_format_fourcc_ = V4L2_PIX_FMT_VP8;
  } else {
    DLOG(ERROR) << "Initialize(): invalid output_profile=" << output_profile;
    return false;
  }

  // Open the color conversion device.
  DVLOG(2) << "Initialize(): opening GSC device: " << kExynosGscDevice;
  gsc_fd_ =
      HANDLE_EINTR(open(kExynosGscDevice, O_RDWR | O_NONBLOCK | O_CLOEXEC));
  if (gsc_fd_ == -1) {
    DPLOG(ERROR) << "Initialize(): could not open GSC device: "
                 << kExynosGscDevice;
    return false;
  }

  // Capabilities check.
  struct v4l2_capability caps;
  memset(&caps, 0, sizeof(caps));
  const __u32 kCapsRequired = V4L2_CAP_VIDEO_CAPTURE_MPLANE |
                              V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING;
  if (HANDLE_EINTR(ioctl(gsc_fd_, VIDIOC_QUERYCAP, &caps))) {
    DPLOG(ERROR) << "Initialize(): ioctl() failed: VIDIOC_QUERYCAP";
    return false;
  }
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    DLOG(ERROR) << "Initialize(): ioctl() failed: VIDIOC_QUERYCAP: "
                   "caps check failed: 0x" << std::hex << caps.capabilities;
    return false;
  }

  // Open the video encoder device.
  DVLOG(2) << "Initialize(): opening MFC device: " << kExynosMfcDevice;
  mfc_fd_ =
      HANDLE_EINTR(open(kExynosMfcDevice, O_RDWR | O_NONBLOCK | O_CLOEXEC));
  if (mfc_fd_ == -1) {
    DPLOG(ERROR) << "Initialize(): could not open MFC device: "
                 << kExynosMfcDevice;
    return false;
  }

  memset(&caps, 0, sizeof(caps));
  if (HANDLE_EINTR(ioctl(mfc_fd_, VIDIOC_QUERYCAP, &caps))) {
    DPLOG(ERROR) << "Initialize(): ioctl() failed: VIDIOC_QUERYCAP";
    return false;
  }
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    DLOG(ERROR) << "Initialize(): ioctl() failed: VIDIOC_QUERYCAP: "
                   "caps check failed: 0x" << std::hex << caps.capabilities;
    return false;
  }

  // Create the interrupt fd.
  DCHECK_EQ(device_poll_interrupt_fd_, -1);
  device_poll_interrupt_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (device_poll_interrupt_fd_ == -1) {
    DPLOG(ERROR) << "Initialize(): eventfd() failed";
    return false;
  }

  DVLOG(3)
      << "Initialize(): input_visible_size_=" << input_visible_size_.ToString()
      << ", input_allocated_size_=" << input_allocated_size_.ToString()
      << ", converted_visible_size_=" << converted_visible_size_.ToString()
      << ", converted_allocated_size_=" << converted_allocated_size_.ToString()
      << ", output_visible_size_=" << output_visible_size_.ToString();

  if (!CreateGscInputBuffers() || !CreateGscOutputBuffers())
    return false;

  // MFC setup for encoding is rather particular in ordering:
  //
  // 1. Format (VIDIOC_S_FMT) set first on OUTPUT and CAPTURE queues.
  // 2. VIDIOC_REQBUFS, VIDIOC_QBUF, and VIDIOC_STREAMON on CAPTURE queue.
  // 3. VIDIOC_REQBUFS (and later VIDIOC_QBUF and VIDIOC_STREAMON) on OUTPUT
  //    queue.
  //
  // Unfortunately, we cannot do (3) in Initialize() here since we have no
  // buffers to QBUF in step (2) until the client has provided output buffers
  // through UseOutputBitstreamBuffer().  So, we just do (1), and the
  // VIDIOC_REQBUFS part of (2) here.  The rest is done the first time we get
  // a UseOutputBitstreamBuffer() callback.

  if (!SetMfcFormats())
    return false;

  if (!InitMfcControls())
    return false;

  // VIDIOC_REQBUFS on CAPTURE queue.
  if (!CreateMfcOutputBuffers())
    return false;

  if (!encoder_thread_.Start()) {
    DLOG(ERROR) << "Initialize(): encoder thread failed to start";
    return false;
  }

  RequestEncodingParametersChange(initial_bitrate, kInitialFramerate);

  SetEncoderState(kInitialized);

  child_message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&Client::RequireBitstreamBuffers,
                 client_,
                 gsc_input_buffer_map_.size(),
                 input_allocated_size_,
                 output_buffer_byte_size_));
  return true;
}

void ExynosVideoEncodeAccelerator::Encode(
    const scoped_refptr<media::VideoFrame>& frame,
    bool force_keyframe) {
  DVLOG(3) << "Encode(): force_keyframe=" << force_keyframe;
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  encoder_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&ExynosVideoEncodeAccelerator::EncodeTask,
                 base::Unretained(this),
                 frame,
                 force_keyframe));
}

void ExynosVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    const media::BitstreamBuffer& buffer) {
  DVLOG(3) << "UseOutputBitstreamBuffer(): id=" << buffer.id();
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  if (buffer.size() < output_buffer_byte_size_) {
    NOTIFY_ERROR(kInvalidArgumentError);
    return;
  }

  scoped_ptr<base::SharedMemory> shm(
      new base::SharedMemory(buffer.handle(), false));
  if (!shm->Map(buffer.size())) {
    NOTIFY_ERROR(kPlatformFailureError);
    return;
  }

  scoped_ptr<BitstreamBufferRef> buffer_ref(
      new BitstreamBufferRef(buffer.id(), shm.Pass(), buffer.size()));
  encoder_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&ExynosVideoEncodeAccelerator::UseOutputBitstreamBufferTask,
                 base::Unretained(this),
                 base::Passed(&buffer_ref)));
}

void ExynosVideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32 bitrate,
    uint32 framerate) {
  DVLOG(3) << "RequestEncodingParametersChange(): bitrate=" << bitrate
           << ", framerate=" << framerate;
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  encoder_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(
          &ExynosVideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          base::Unretained(this),
          bitrate,
          framerate));
}

void ExynosVideoEncodeAccelerator::Destroy() {
  DVLOG(3) << "Destroy()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());

  // We're destroying; cancel all callbacks.
  client_ptr_factory_.reset();

  // If the encoder thread is running, destroy using posted task.
  if (encoder_thread_.IsRunning()) {
    encoder_thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&ExynosVideoEncodeAccelerator::DestroyTask,
                   base::Unretained(this)));
    // DestroyTask() will put the encoder into kError state and cause all tasks
    // to no-op.
    encoder_thread_.Stop();
  } else {
    // Otherwise, call the destroy task directly.
    DestroyTask();
  }

  // Set to kError state just in case.
  SetEncoderState(kError);

  delete this;
}

// static
std::vector<media::VideoEncodeAccelerator::SupportedProfile>
ExynosVideoEncodeAccelerator::GetSupportedProfiles() {
  std::vector<SupportedProfile> profiles;

  SupportedProfile profile;

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableWebRtcHWVp8Encoding)) {
    profile.profile = media::VP8PROFILE_MAIN;
    profile.max_resolution.SetSize(1920, 1088);
    profile.max_framerate.numerator = 30;
    profile.max_framerate.denominator = 1;
    profiles.push_back(profile);
  } else {
    profile.profile = media::H264PROFILE_MAIN;
    profile.max_resolution.SetSize(1920, 1088);
    profile.max_framerate.numerator = 30;
    profile.max_framerate.denominator = 1;
    profiles.push_back(profile);
  }

  return profiles;
}

void ExynosVideoEncodeAccelerator::EncodeTask(
    const scoped_refptr<media::VideoFrame>& frame, bool force_keyframe) {
  DVLOG(3) << "EncodeTask(): force_keyframe=" << force_keyframe;
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(encoder_state_, kUninitialized);

  if (encoder_state_ == kError) {
    DVLOG(2) << "EncodeTask(): early out: kError state";
    return;
  }

  encoder_input_queue_.push_back(frame);
  EnqueueGsc();

  if (force_keyframe) {
    // TODO(sheu): this presently makes for slightly imprecise encoding
    // parameters updates.  To precisely align the parameter updates with the
    // incoming input frame, we should track the parameters through the GSC
    // pipeline and only apply them when the MFC input is about to be queued.
    struct v4l2_ext_control ctrls[1];
    struct v4l2_ext_controls control;
    memset(&ctrls, 0, sizeof(ctrls));
    memset(&control, 0, sizeof(control));
    ctrls[0].id    = V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE;
    ctrls[0].value = V4L2_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE_I_FRAME;
    control.ctrl_class = V4L2_CTRL_CLASS_MPEG;
    control.count = 1;
    control.controls = ctrls;
    IOCTL_OR_ERROR_RETURN(mfc_fd_, VIDIOC_S_EXT_CTRLS, &control);
  }
}

void ExynosVideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    scoped_ptr<BitstreamBufferRef> buffer_ref) {
  DVLOG(3) << "UseOutputBitstreamBufferTask(): id=" << buffer_ref->id;
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());

  encoder_output_queue_.push_back(
      linked_ptr<BitstreamBufferRef>(buffer_ref.release()));
  EnqueueMfc();

  if (encoder_state_ == kInitialized) {
    // Finish setting up our MFC OUTPUT queue.  See: Initialize().
    // VIDIOC_REQBUFS on OUTPUT queue.
    if (!CreateMfcInputBuffers())
      return;
    if (!StartDevicePoll())
      return;
    encoder_state_ = kEncoding;
  }
}

void ExynosVideoEncodeAccelerator::DestroyTask() {
  DVLOG(3) << "DestroyTask()";

  // DestroyTask() should run regardless of encoder_state_.

  // Stop streaming and the device_poll_thread_.
  StopDevicePoll();

  // Set our state to kError, and early-out all tasks.
  encoder_state_ = kError;
}

void ExynosVideoEncodeAccelerator::ServiceDeviceTask() {
  DVLOG(3) << "ServiceDeviceTask()";
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(encoder_state_, kUninitialized);
  DCHECK_NE(encoder_state_, kInitialized);

  if (encoder_state_ == kError) {
    DVLOG(2) << "ServiceDeviceTask(): early out: kError state";
    return;
  }

  DequeueGsc();
  DequeueMfc();
  EnqueueGsc();
  EnqueueMfc();

  // Clear the interrupt fd.
  if (!ClearDevicePollInterrupt())
    return;

  unsigned int poll_fds = 0;
  // Add GSC fd, if we should poll on it.
  // GSC has to wait until both input and output buffers are queued.
  if (gsc_input_buffer_queued_count_ > 0 && gsc_output_buffer_queued_count_ > 0)
    poll_fds |= kPollGsc;
  // Add MFC fd, if we should poll on it.
  // MFC can be polled as soon as either input or output buffers are queued.
  if (mfc_input_buffer_queued_count_ + mfc_output_buffer_queued_count_ > 0)
    poll_fds |= kPollMfc;

  // ServiceDeviceTask() should only ever be scheduled from DevicePollTask(),
  // so either:
  // * device_poll_thread_ is running normally
  // * device_poll_thread_ scheduled us, but then a DestroyTask() shut it down,
  //   in which case we're in kError state, and we should have early-outed
  //   already.
  DCHECK(device_poll_thread_.message_loop());
  // Queue the DevicePollTask() now.
  device_poll_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&ExynosVideoEncodeAccelerator::DevicePollTask,
                 base::Unretained(this),
                 poll_fds));

  DVLOG(2) << "ServiceDeviceTask(): buffer counts: ENC["
           << encoder_input_queue_.size() << "] => GSC["
           << gsc_free_input_buffers_.size() << "+"
           << gsc_input_buffer_queued_count_ << "/"
           << gsc_input_buffer_map_.size() << "->"
           << gsc_free_output_buffers_.size() << "+"
           << gsc_output_buffer_queued_count_ << "/"
           << gsc_output_buffer_map_.size() << "] => "
           << mfc_ready_input_buffers_.size() << " => MFC["
           << mfc_free_input_buffers_.size() << "+"
           << mfc_input_buffer_queued_count_ << "/"
           << mfc_input_buffer_map_.size() << "->"
           << mfc_free_output_buffers_.size() << "+"
           << mfc_output_buffer_queued_count_ << "/"
           << mfc_output_buffer_map_.size() << "] => OUT["
           << encoder_output_queue_.size() << "]";
}

void ExynosVideoEncodeAccelerator::EnqueueGsc() {
  DVLOG(3) << "EnqueueGsc()";
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());

  const int old_gsc_inputs_queued = gsc_input_buffer_queued_count_;
  while (!encoder_input_queue_.empty() && !gsc_free_input_buffers_.empty()) {
    if (!EnqueueGscInputRecord())
      return;
  }
  if (old_gsc_inputs_queued == 0 && gsc_input_buffer_queued_count_ != 0) {
    // We started up a previously empty queue.
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

  // Enqueue a GSC output, only if we need one.  GSC output buffers write
  // directly to MFC input buffers, so we'll have to check for free MFC input
  // buffers as well.
  // GSC is liable to race conditions if more than one output buffer is
  // simultaneously enqueued, so enqueue just one.
  if (gsc_input_buffer_queued_count_ != 0 &&
      gsc_output_buffer_queued_count_ == 0 &&
      !gsc_free_output_buffers_.empty() && !mfc_free_input_buffers_.empty()) {
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
  DCHECK_LE(gsc_output_buffer_queued_count_, 1);
}

void ExynosVideoEncodeAccelerator::DequeueGsc() {
  DVLOG(3) << "DequeueGsc()";
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());

  // Dequeue completed GSC input (VIDEO_OUTPUT) buffers, and recycle to the free
  // list.
  struct v4l2_buffer dqbuf;
  struct v4l2_plane planes[3];
  while (gsc_input_buffer_queued_count_ > 0) {
    DCHECK(gsc_input_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(&planes, 0, sizeof(planes));
    dqbuf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dqbuf.memory   = V4L2_MEMORY_USERPTR;
    dqbuf.m.planes = planes;
    dqbuf.length   = arraysize(planes);
    if (HANDLE_EINTR(ioctl(gsc_fd_, VIDIOC_DQBUF, &dqbuf)) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      DPLOG(ERROR) << "DequeueGsc(): ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR(kPlatformFailureError);
      return;
    }
    GscInputRecord& input_record = gsc_input_buffer_map_[dqbuf.index];
    DCHECK(input_record.at_device);
    DCHECK(input_record.frame.get());
    input_record.at_device = false;
    input_record.frame = NULL;
    gsc_free_input_buffers_.push_back(dqbuf.index);
    gsc_input_buffer_queued_count_--;
  }

  // Dequeue completed GSC output (VIDEO_CAPTURE) buffers, and recycle to the
  // free list.  Queue the corresponding MFC buffer to the GSC->MFC holding
  // queue.
  while (gsc_output_buffer_queued_count_ > 0) {
    DCHECK(gsc_output_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(&planes, 0, sizeof(planes));
    dqbuf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dqbuf.memory   = V4L2_MEMORY_DMABUF;
    dqbuf.m.planes = planes;
    dqbuf.length   = 2;
    if (HANDLE_EINTR(ioctl(gsc_fd_, VIDIOC_DQBUF, &dqbuf)) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      DPLOG(ERROR) << "DequeueGsc(): ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR(kPlatformFailureError);
      return;
    }
    GscOutputRecord& output_record = gsc_output_buffer_map_[dqbuf.index];
    DCHECK(output_record.at_device);
    DCHECK(output_record.mfc_input != -1);
    mfc_ready_input_buffers_.push_back(output_record.mfc_input);
    output_record.at_device = false;
    output_record.mfc_input = -1;
    gsc_free_output_buffers_.push_back(dqbuf.index);
    gsc_output_buffer_queued_count_--;
  }
}
void ExynosVideoEncodeAccelerator::EnqueueMfc() {
  DVLOG(3) << "EnqueueMfc()";
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());

  // Enqueue all the MFC inputs we can.
  const int old_mfc_inputs_queued = mfc_input_buffer_queued_count_;
  while (!mfc_ready_input_buffers_.empty()) {
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
  while (!mfc_free_output_buffers_.empty() && !encoder_output_queue_.empty()) {
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

void ExynosVideoEncodeAccelerator::DequeueMfc() {
  DVLOG(3) << "DequeueMfc()";
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());

  // Dequeue completed MFC input (VIDEO_OUTPUT) buffers, and recycle to the free
  // list.
  struct v4l2_buffer dqbuf;
  struct v4l2_plane planes[2];
  while (mfc_input_buffer_queued_count_ > 0) {
    DCHECK(mfc_input_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(&planes, 0, sizeof(planes));
    dqbuf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    dqbuf.memory   = V4L2_MEMORY_MMAP;
    dqbuf.m.planes = planes;
    dqbuf.length   = 2;
    if (HANDLE_EINTR(ioctl(mfc_fd_, VIDIOC_DQBUF, &dqbuf)) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      DPLOG(ERROR) << "DequeueMfc(): ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR(kPlatformFailureError);
      return;
    }
    MfcInputRecord& input_record = mfc_input_buffer_map_[dqbuf.index];
    DCHECK(input_record.at_device);
    input_record.at_device = false;
    mfc_free_input_buffers_.push_back(dqbuf.index);
    mfc_input_buffer_queued_count_--;
  }

  // Dequeue completed MFC output (VIDEO_CAPTURE) buffers, and recycle to the
  // free list.  Notify the client that an output buffer is complete.
  while (mfc_output_buffer_queued_count_ > 0) {
    DCHECK(mfc_output_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    memset(planes, 0, sizeof(planes));
    dqbuf.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    dqbuf.memory   = V4L2_MEMORY_MMAP;
    dqbuf.m.planes = planes;
    dqbuf.length   = 1;
    if (HANDLE_EINTR(ioctl(mfc_fd_, VIDIOC_DQBUF, &dqbuf)) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      DPLOG(ERROR) << "DequeueMfc(): ioctl() failed: VIDIOC_DQBUF";
      NOTIFY_ERROR(kPlatformFailureError);
      return;
    }
    const bool key_frame = ((dqbuf.flags & V4L2_BUF_FLAG_KEYFRAME) != 0);
    MfcOutputRecord& output_record = mfc_output_buffer_map_[dqbuf.index];
    DCHECK(output_record.at_device);
    DCHECK(output_record.buffer_ref.get());

    void* output_data = output_record.address;
    size_t output_size = dqbuf.m.planes[0].bytesused;
    // This shouldn't happen, but just in case. We should be able to recover
    // after next keyframe after showing some corruption.
    DCHECK_LE(output_size, output_buffer_byte_size_);
    if (output_size > output_buffer_byte_size_)
      output_size = output_buffer_byte_size_;
    uint8* target_data =
        reinterpret_cast<uint8*>(output_record.buffer_ref->shm->memory());
    if (output_format_fourcc_ == V4L2_PIX_FMT_H264) {
      if (stream_header_size_ == 0) {
        // Assume that the first buffer dequeued is the stream header.
        stream_header_size_ = output_size;
        stream_header_.reset(new uint8[stream_header_size_]);
        memcpy(stream_header_.get(), output_data, stream_header_size_);
      }
      if (key_frame &&
          output_buffer_byte_size_ - stream_header_size_ >= output_size) {
        // Insert stream header before every keyframe.
        memcpy(target_data, stream_header_.get(), stream_header_size_);
        memcpy(target_data + stream_header_size_, output_data, output_size);
        output_size += stream_header_size_;
      } else {
        memcpy(target_data, output_data, output_size);
      }
    } else {
      memcpy(target_data, output_data, output_size);
    }

    DVLOG(3) << "DequeueMfc(): returning "
                "bitstream_buffer_id=" << output_record.buffer_ref->id
             << ", key_frame=" << key_frame;
    child_message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&Client::BitstreamBufferReady,
                   client_,
                   output_record.buffer_ref->id,
                   output_size,
                   key_frame));
    output_record.at_device = false;
    output_record.buffer_ref.reset();
    mfc_free_output_buffers_.push_back(dqbuf.index);
    mfc_output_buffer_queued_count_--;
  }
}

bool ExynosVideoEncodeAccelerator::EnqueueGscInputRecord() {
  DVLOG(3) << "EnqueueGscInputRecord()";
  DCHECK(!encoder_input_queue_.empty());
  DCHECK(!gsc_free_input_buffers_.empty());

  // Enqueue a GSC input (VIDEO_OUTPUT) buffer for an input video frame
  scoped_refptr<media::VideoFrame> frame = encoder_input_queue_.front();
  const int gsc_buffer = gsc_free_input_buffers_.back();
  GscInputRecord& input_record = gsc_input_buffer_map_[gsc_buffer];
  DCHECK(!input_record.at_device);
  DCHECK(!input_record.frame.get());
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_planes[3];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes, 0, sizeof(qbuf_planes));
  qbuf.index    = gsc_buffer;
  qbuf.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  qbuf.memory   = V4L2_MEMORY_USERPTR;
  qbuf.m.planes = qbuf_planes;
  switch (input_format_fourcc_) {
    case V4L2_PIX_FMT_YUV420M: {
      qbuf.m.planes[0].bytesused = input_allocated_size_.GetArea();
      qbuf.m.planes[0].length    = input_allocated_size_.GetArea();
      qbuf.m.planes[0].m.userptr = reinterpret_cast<unsigned long>(
          frame->data(media::VideoFrame::kYPlane));
      qbuf.m.planes[1].bytesused = input_allocated_size_.GetArea() / 4;
      qbuf.m.planes[1].length    = input_allocated_size_.GetArea() / 4;
      qbuf.m.planes[1].m.userptr = reinterpret_cast<unsigned long>(
          frame->data(media::VideoFrame::kUPlane));
      qbuf.m.planes[2].bytesused = input_allocated_size_.GetArea() / 4;
      qbuf.m.planes[2].length    = input_allocated_size_.GetArea() / 4;
      qbuf.m.planes[2].m.userptr = reinterpret_cast<unsigned long>(
          frame->data(media::VideoFrame::kVPlane));
      qbuf.length = 3;
      break;
    }
    default:
      NOTREACHED();
      NOTIFY_ERROR(kIllegalStateError);
      return false;
  }
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_QBUF, &qbuf);
  input_record.at_device = true;
  input_record.frame = frame;
  encoder_input_queue_.pop_front();
  gsc_free_input_buffers_.pop_back();
  gsc_input_buffer_queued_count_++;
  return true;
}

bool ExynosVideoEncodeAccelerator::EnqueueGscOutputRecord() {
  DVLOG(3) << "EnqueueGscOutputRecord()";
  DCHECK(!gsc_free_output_buffers_.empty());
  DCHECK(!mfc_free_input_buffers_.empty());

  // Enqueue a GSC output (VIDEO_CAPTURE) buffer.
  const int gsc_buffer = gsc_free_output_buffers_.back();
  const int mfc_buffer = mfc_free_input_buffers_.back();
  GscOutputRecord& output_record = gsc_output_buffer_map_[gsc_buffer];
  MfcInputRecord& input_record = mfc_input_buffer_map_[mfc_buffer];
  DCHECK(!output_record.at_device);
  DCHECK_EQ(output_record.mfc_input, -1);
  DCHECK(!input_record.at_device);
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_planes[2];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes, 0, sizeof(qbuf_planes));
  qbuf.index            = gsc_buffer;
  qbuf.type             = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  qbuf.memory           = V4L2_MEMORY_DMABUF;
  qbuf.m.planes         = qbuf_planes;
  qbuf.m.planes[0].m.fd = input_record.fd[0];
  qbuf.m.planes[1].m.fd = input_record.fd[1];
  qbuf.length           = 2;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_QBUF, &qbuf);
  output_record.at_device = true;
  output_record.mfc_input = mfc_buffer;
  mfc_free_input_buffers_.pop_back();
  gsc_free_output_buffers_.pop_back();
  gsc_output_buffer_queued_count_++;
  return true;
}

bool ExynosVideoEncodeAccelerator::EnqueueMfcInputRecord() {
  DVLOG(3) << "EnqueueMfcInputRecord()";
  DCHECK(!mfc_ready_input_buffers_.empty());

  // Enqueue a MFC input (VIDEO_OUTPUT) buffer.
  const int mfc_buffer = mfc_ready_input_buffers_.front();
  MfcInputRecord& input_record = mfc_input_buffer_map_[mfc_buffer];
  DCHECK(!input_record.at_device);
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_planes[2];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes, 0, sizeof(qbuf_planes));
  qbuf.index     = mfc_buffer;
  qbuf.type      = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  qbuf.memory    = V4L2_MEMORY_MMAP;
  qbuf.m.planes  = qbuf_planes;
  qbuf.length    = 2;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_QBUF, &qbuf);
  input_record.at_device = true;
  mfc_ready_input_buffers_.pop_front();
  mfc_input_buffer_queued_count_++;
  return true;
}

bool ExynosVideoEncodeAccelerator::EnqueueMfcOutputRecord() {
  DVLOG(3) << "EnqueueMfcOutputRecord()";
  DCHECK(!mfc_free_output_buffers_.empty());
  DCHECK(!encoder_output_queue_.empty());

  // Enqueue a MFC output (VIDEO_CAPTURE) buffer.
  linked_ptr<BitstreamBufferRef> output_buffer = encoder_output_queue_.back();
  const int mfc_buffer = mfc_free_output_buffers_.back();
  MfcOutputRecord& output_record = mfc_output_buffer_map_[mfc_buffer];
  DCHECK(!output_record.at_device);
  DCHECK(!output_record.buffer_ref.get());
  struct v4l2_buffer qbuf;
  struct v4l2_plane qbuf_planes[1];
  memset(&qbuf, 0, sizeof(qbuf));
  memset(qbuf_planes, 0, sizeof(qbuf_planes));
  qbuf.index                 = mfc_buffer;
  qbuf.type                  = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  qbuf.memory                = V4L2_MEMORY_MMAP;
  qbuf.m.planes              = qbuf_planes;
  qbuf.length                = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_QBUF, &qbuf);
  output_record.at_device = true;
  output_record.buffer_ref = output_buffer;
  encoder_output_queue_.pop_back();
  mfc_free_output_buffers_.pop_back();
  mfc_output_buffer_queued_count_++;
  return true;
}

bool ExynosVideoEncodeAccelerator::StartDevicePoll() {
  DVLOG(3) << "StartDevicePoll()";
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK(!device_poll_thread_.IsRunning());

  // Start up the device poll thread and schedule its first DevicePollTask().
  if (!device_poll_thread_.Start()) {
    DLOG(ERROR) << "StartDevicePoll(): Device thread failed to start";
    NOTIFY_ERROR(kPlatformFailureError);
    return false;
  }
  // Enqueue a poll task with no devices to poll on -- it will wait only on the
  // interrupt fd.
  device_poll_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&ExynosVideoEncodeAccelerator::DevicePollTask,
                 base::Unretained(this),
                 0));

  return true;
}

bool ExynosVideoEncodeAccelerator::StopDevicePoll() {
  DVLOG(3) << "StopDevicePoll()";

  // Signal the DevicePollTask() to stop, and stop the device poll thread.
  if (!SetDevicePollInterrupt())
    return false;
  device_poll_thread_.Stop();
  // Clear the interrupt now, to be sure.
  if (!ClearDevicePollInterrupt())
    return false;

  // Stop streaming.
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

  // Reset all our accounting info.
  encoder_input_queue_.clear();
  gsc_free_input_buffers_.clear();
  for (size_t i = 0; i < gsc_input_buffer_map_.size(); ++i) {
    GscInputRecord& input_record = gsc_input_buffer_map_[i];
    input_record.at_device = false;
    input_record.frame = NULL;
    gsc_free_input_buffers_.push_back(i);
  }
  gsc_input_buffer_queued_count_ = 0;
  gsc_free_output_buffers_.clear();
  for (size_t i = 0; i < gsc_output_buffer_map_.size(); ++i) {
    GscOutputRecord& output_record = gsc_output_buffer_map_[i];
    output_record.at_device = false;
    output_record.mfc_input = -1;
    gsc_free_output_buffers_.push_back(i);
  }
  gsc_output_buffer_queued_count_ = 0;
  mfc_ready_input_buffers_.clear();
  mfc_free_input_buffers_.clear();
  for (size_t i = 0; i < mfc_input_buffer_map_.size(); ++i) {
    MfcInputRecord& input_record = mfc_input_buffer_map_[i];
    input_record.at_device = false;
    mfc_free_input_buffers_.push_back(i);
  }
  mfc_input_buffer_queued_count_ = 0;
  mfc_free_output_buffers_.clear();
  for (size_t i = 0; i < mfc_output_buffer_map_.size(); ++i) {
    MfcOutputRecord& output_record = mfc_output_buffer_map_[i];
    output_record.at_device = false;
    output_record.buffer_ref.reset();
    mfc_free_output_buffers_.push_back(i);
  }
  mfc_output_buffer_queued_count_ = 0;
  encoder_output_queue_.clear();

  DVLOG(3) << "StopDevicePoll(): device poll stopped";
  return true;
}

bool ExynosVideoEncodeAccelerator::SetDevicePollInterrupt() {
  DVLOG(3) << "SetDevicePollInterrupt()";

  // We might get called here if we fail during initialization, in which case we
  // don't have a file descriptor.
  if (device_poll_interrupt_fd_ == -1)
    return true;

  const uint64 buf = 1;
  if (HANDLE_EINTR((write(device_poll_interrupt_fd_, &buf, sizeof(buf)))) <
      static_cast<ssize_t>(sizeof(buf))) {
    DPLOG(ERROR) << "SetDevicePollInterrupt(): write() failed";
    NOTIFY_ERROR(kPlatformFailureError);
    return false;
  }
  return true;
}

bool ExynosVideoEncodeAccelerator::ClearDevicePollInterrupt() {
  DVLOG(3) << "ClearDevicePollInterrupt()";

  // We might get called here if we fail during initialization, in which case we
  // don't have a file descriptor.
  if (device_poll_interrupt_fd_ == -1)
    return true;

  uint64 buf;
  if (HANDLE_EINTR(read(device_poll_interrupt_fd_, &buf, sizeof(buf))) <
      static_cast<ssize_t>(sizeof(buf))) {
    if (errno == EAGAIN) {
      // No interrupt flag set, and we're reading nonblocking.  Not an error.
      return true;
    } else {
      DPLOG(ERROR) << "ClearDevicePollInterrupt(): read() failed";
      NOTIFY_ERROR(kPlatformFailureError);
      return false;
    }
  }
  return true;
}

void ExynosVideoEncodeAccelerator::DevicePollTask(unsigned int poll_fds) {
  DVLOG(3) << "DevicePollTask()";
  DCHECK_EQ(device_poll_thread_.message_loop(), base::MessageLoop::current());
  DCHECK_NE(device_poll_interrupt_fd_, -1);

  // This routine just polls the set of device fds, and schedules a
  // ServiceDeviceTask() on encoder_thread_ when processing needs to occur.
  // Other threads may notify this task to return early by writing to
  // device_poll_interrupt_fd_.
  struct pollfd pollfds[3];
  nfds_t nfds;

  // Add device_poll_interrupt_fd_;
  pollfds[0].fd = device_poll_interrupt_fd_;
  pollfds[0].events = POLLIN | POLLERR;
  nfds = 1;

  // Add GSC fd, if we should poll on it.
  // GSC has to wait until both input and output buffers are queued.
  if (poll_fds & kPollGsc) {
    DVLOG(3) << "DevicePollTask(): adding GSC to poll() set";
    pollfds[nfds].fd = gsc_fd_;
    pollfds[nfds].events = POLLIN | POLLOUT | POLLERR;
    nfds++;
  }
  if (poll_fds & kPollMfc) {
    DVLOG(3) << "DevicePollTask(): adding MFC to poll() set";
    pollfds[nfds].fd = mfc_fd_;
    pollfds[nfds].events = POLLIN | POLLOUT | POLLERR;
    nfds++;
  }

  // Poll it!
  if (HANDLE_EINTR(poll(pollfds, nfds, -1)) == -1) {
    DPLOG(ERROR) << "DevicePollTask(): poll() failed";
    NOTIFY_ERROR(kPlatformFailureError);
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch encoder state from this thread.
  encoder_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&ExynosVideoEncodeAccelerator::ServiceDeviceTask,
                 base::Unretained(this)));
}

void ExynosVideoEncodeAccelerator::NotifyError(Error error) {
  DVLOG(1) << "NotifyError(): error=" << error;

  if (!child_message_loop_proxy_->BelongsToCurrentThread()) {
    child_message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(
            &ExynosVideoEncodeAccelerator::NotifyError, weak_this_, error));
    return;
  }

  if (client_) {
    client_->NotifyError(error);
    client_ptr_factory_.reset();
  }
}

void ExynosVideoEncodeAccelerator::SetEncoderState(State state) {
  DVLOG(3) << "SetEncoderState(): state=" << state;

  // We can touch encoder_state_ only if this is the encoder thread or the
  // encoder thread isn't running.
  if (encoder_thread_.message_loop() != NULL &&
      encoder_thread_.message_loop() != base::MessageLoop::current()) {
    encoder_thread_.message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&ExynosVideoEncodeAccelerator::SetEncoderState,
                   base::Unretained(this),
                   state));
  } else {
    encoder_state_ = state;
  }
}

void ExynosVideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    uint32 bitrate,
    uint32 framerate) {
  DVLOG(3) << "RequestEncodingParametersChangeTask(): bitrate=" << bitrate
           << ", framerate=" << framerate;
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());

  if (bitrate < 1)
    bitrate = 1;
  if (framerate < 1)
    framerate = 1;

  struct v4l2_ext_control ctrls[1];
  struct v4l2_ext_controls control;
  memset(&ctrls, 0, sizeof(ctrls));
  memset(&control, 0, sizeof(control));
  ctrls[0].id    = V4L2_CID_MPEG_VIDEO_BITRATE;
  ctrls[0].value = bitrate;
  control.ctrl_class = V4L2_CTRL_CLASS_MPEG;
  control.count = arraysize(ctrls);
  control.controls = ctrls;
  IOCTL_OR_ERROR_RETURN(mfc_fd_, VIDIOC_S_EXT_CTRLS, &control);

  struct v4l2_streamparm parms;
  memset(&parms, 0, sizeof(parms));
  parms.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  // Note that we are provided "frames per second" but V4L2 expects "time per
  // frame"; hence we provide the reciprocal of the framerate here.
  parms.parm.output.timeperframe.numerator = 1;
  parms.parm.output.timeperframe.denominator = framerate;
  IOCTL_OR_ERROR_RETURN(mfc_fd_, VIDIOC_S_PARM, &parms);
}

bool ExynosVideoEncodeAccelerator::CreateGscInputBuffers() {
  DVLOG(3) << "CreateGscInputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(encoder_state_, kUninitialized);
  DCHECK(!gsc_input_streamon_);

  struct v4l2_control control;
  memset(&control, 0, sizeof(control));
  control.id    = V4L2_CID_ROTATE;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_S_CTRL, &control);

  // HFLIP actually seems to control vertical mirroring for GSC, and vice-versa.
  memset(&control, 0, sizeof(control));
  control.id    = V4L2_CID_HFLIP;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id    = V4L2_CID_VFLIP;
  control.value = 0;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_S_CTRL, &control);

  memset(&control, 0, sizeof(control));
  control.id    = V4L2_CID_ALPHA_COMPONENT;
  control.value = 255;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_S_CTRL, &control);

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.width  = input_allocated_size_.width();
  format.fmt.pix_mp.height = input_allocated_size_.height();
  format.fmt.pix_mp.pixelformat = input_format_fourcc_;
  switch (input_format_fourcc_) {
    case V4L2_PIX_FMT_RGB32:
      format.fmt.pix_mp.plane_fmt[0].sizeimage =
          input_allocated_size_.GetArea() * 4;
      format.fmt.pix_mp.plane_fmt[0].bytesperline =
          input_allocated_size_.width() * 4;
      format.fmt.pix_mp.num_planes = 1;
      break;
    case V4L2_PIX_FMT_YUV420M:
      format.fmt.pix_mp.plane_fmt[0].sizeimage =
          input_allocated_size_.GetArea();
      format.fmt.pix_mp.plane_fmt[0].bytesperline =
          input_allocated_size_.width();
      format.fmt.pix_mp.plane_fmt[1].sizeimage =
          input_allocated_size_.GetArea() / 4;
      format.fmt.pix_mp.plane_fmt[1].bytesperline =
          input_allocated_size_.width() / 2;
      format.fmt.pix_mp.plane_fmt[2].sizeimage =
          input_allocated_size_.GetArea() / 4;
      format.fmt.pix_mp.plane_fmt[2].bytesperline =
          input_allocated_size_.width() / 2;
      format.fmt.pix_mp.num_planes = 3;
      break;
    default:
      NOTREACHED();
      NOTIFY_ERROR(kIllegalStateError);
      return false;
  }
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_S_FMT, &format);

  struct v4l2_crop crop;
  memset(&crop, 0, sizeof(crop));
  crop.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  crop.c.left   = 0;
  crop.c.top    = 0;
  crop.c.width  = input_visible_size_.width();
  crop.c.height = input_visible_size_.height();
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_S_CROP, &crop);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count  = kGscInputBufferCount;
  reqbufs.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_USERPTR;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_REQBUFS, &reqbufs);

  DCHECK(gsc_input_buffer_map_.empty());
  gsc_input_buffer_map_.resize(reqbufs.count);
  for (size_t i = 0; i < gsc_input_buffer_map_.size(); ++i)
    gsc_free_input_buffers_.push_back(i);

  return true;
}

bool ExynosVideoEncodeAccelerator::CreateGscOutputBuffers() {
  DVLOG(3) << "CreateGscOutputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(encoder_state_, kUninitialized);
  DCHECK(!gsc_output_streamon_);

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.width = converted_allocated_size_.width();
  format.fmt.pix_mp.height = converted_allocated_size_.height();
  format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
  format.fmt.pix_mp.plane_fmt[0].sizeimage =
      converted_allocated_size_.GetArea();
  format.fmt.pix_mp.plane_fmt[1].sizeimage =
      converted_allocated_size_.GetArea() / 2;
  format.fmt.pix_mp.plane_fmt[0].bytesperline =
      converted_allocated_size_.width();
  format.fmt.pix_mp.plane_fmt[1].bytesperline =
      converted_allocated_size_.width();
  format.fmt.pix_mp.num_planes = 2;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_S_FMT, &format);

  struct v4l2_crop crop;
  memset(&crop, 0, sizeof(crop));
  crop.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  crop.c.left   = 0;
  crop.c.top    = 0;
  crop.c.width  = converted_visible_size_.width();
  crop.c.height = converted_visible_size_.height();
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_S_CROP, &crop);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count  = kGscOutputBufferCount;
  reqbufs.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_DMABUF;
  IOCTL_OR_ERROR_RETURN_FALSE(gsc_fd_, VIDIOC_REQBUFS, &reqbufs);

  DCHECK(gsc_output_buffer_map_.empty());
  gsc_output_buffer_map_.resize(reqbufs.count);
  for (size_t i = 0; i < gsc_output_buffer_map_.size(); ++i)
    gsc_free_output_buffers_.push_back(i);
  return true;
}

bool ExynosVideoEncodeAccelerator::SetMfcFormats() {
  DVLOG(3) << "SetMfcFormats()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!mfc_input_streamon_);
  DCHECK(!mfc_output_streamon_);

  // VIDIOC_S_FMT on OUTPUT queue.
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  format.fmt.pix_mp.width = input_allocated_size_.width();
  format.fmt.pix_mp.height = input_allocated_size_.height();
  format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12M;
  format.fmt.pix_mp.num_planes = 2;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_S_FMT, &format);
  // We read direct from GSC, so we rely on the HW not changing our set
  // size/stride.
  DCHECK_EQ(format.fmt.pix_mp.plane_fmt[0].sizeimage,
            static_cast<__u32>(input_allocated_size_.GetArea()));
  DCHECK_EQ(format.fmt.pix_mp.plane_fmt[0].bytesperline,
            static_cast<__u32>(input_allocated_size_.width()));
  DCHECK_EQ(format.fmt.pix_mp.plane_fmt[1].sizeimage,
            static_cast<__u32>(input_allocated_size_.GetArea() / 2));
  DCHECK_EQ(format.fmt.pix_mp.plane_fmt[1].bytesperline,
            static_cast<__u32>(input_allocated_size_.width()));

  struct v4l2_crop crop;
  memset(&crop, 0, sizeof(crop));
  crop.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  crop.c.left   = 0;
  crop.c.top    = 0;
  crop.c.width  = input_visible_size_.width();
  crop.c.height = input_visible_size_.height();
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_S_CROP, &crop);

  // VIDIOC_S_FMT on CAPTURE queue.
  output_buffer_byte_size_ = kMfcOutputBufferSize;
  memset(&format, 0, sizeof(format));
  format.type                   = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  format.fmt.pix_mp.width       = output_visible_size_.width();
  format.fmt.pix_mp.height      = output_visible_size_.height();
  format.fmt.pix_mp.pixelformat = output_format_fourcc_;
  format.fmt.pix_mp.plane_fmt[0].sizeimage = output_buffer_byte_size_;
  format.fmt.pix_mp.num_planes  = 1;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_S_FMT, &format);

  return true;
}

bool ExynosVideoEncodeAccelerator::InitMfcControls() {
  struct v4l2_ext_control ctrls[9];
  struct v4l2_ext_controls control;
  memset(&ctrls, 0, sizeof(ctrls));
  memset(&control, 0, sizeof(control));
  // No B-frames, for lowest decoding latency.
  ctrls[0].id    = V4L2_CID_MPEG_VIDEO_B_FRAMES;
  ctrls[0].value = 0;
  // Enable frame-level bitrate control.
  ctrls[1].id    = V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE;
  ctrls[1].value = 1;
  // Enable "tight" bitrate mode. For this to work properly, frame- and mb-level
  // bitrate controls have to be enabled as well.
  ctrls[2].id    = V4L2_CID_MPEG_MFC51_VIDEO_RC_REACTION_COEFF;
  ctrls[2].value = 1;
  // Force bitrate control to average over a GOP (for tight bitrate
  // tolerance).
  ctrls[3].id    = V4L2_CID_MPEG_MFC51_VIDEO_RC_FIXED_TARGET_BIT;
  ctrls[3].value = 1;
  // Quantization parameter maximum value (for variable bitrate control).
  ctrls[4].id    = V4L2_CID_MPEG_VIDEO_H264_MAX_QP;
  ctrls[4].value = 51;
  // Separate stream header so we can cache it and insert into the stream.
  ctrls[5].id    = V4L2_CID_MPEG_VIDEO_HEADER_MODE;
  ctrls[5].value = V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE;
  // Enable macroblock-level bitrate control.
  ctrls[6].id    = V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE;
  ctrls[6].value = 1;
  // Use H.264 level 4.0 to match the supported max resolution.
  ctrls[7].id    = V4L2_CID_MPEG_VIDEO_H264_LEVEL;
  ctrls[7].value = V4L2_MPEG_VIDEO_H264_LEVEL_4_0;
  // Disable periodic key frames.
  ctrls[8].id    = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
  ctrls[8].value = 0;
  control.ctrl_class = V4L2_CTRL_CLASS_MPEG;
  control.count = arraysize(ctrls);
  control.controls = ctrls;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_S_EXT_CTRLS, &control);

  return true;
}

bool ExynosVideoEncodeAccelerator::CreateMfcInputBuffers() {
  DVLOG(3) << "CreateMfcInputBuffers()";
  // This function runs on encoder_thread_ after output buffers have been
  // provided by the client.
  DCHECK_EQ(encoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK(!mfc_input_streamon_);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 1;  // Driver will allocate the appropriate number of buffers.
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_REQBUFS, &reqbufs);

  DCHECK(mfc_input_buffer_map_.empty());
  mfc_input_buffer_map_.resize(reqbufs.count);
  for (size_t i = 0; i < mfc_input_buffer_map_.size(); ++i) {
    MfcInputRecord& input_record = mfc_input_buffer_map_[i];
    for (int j = 0; j < 2; ++j) {
      // Export the DMABUF fd so GSC can write to it.
      struct v4l2_exportbuffer expbuf;
      memset(&expbuf, 0, sizeof(expbuf));
      expbuf.type  = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
      expbuf.index = i;
      expbuf.plane = j;
      expbuf.flags = O_CLOEXEC;
      IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_EXPBUF, &expbuf);
      input_record.fd[j] = expbuf.fd;
    }
    mfc_free_input_buffers_.push_back(i);
  }

  return true;
}

bool ExynosVideoEncodeAccelerator::CreateMfcOutputBuffers() {
  DVLOG(3) << "CreateMfcOutputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!mfc_output_streamon_);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = kMfcOutputBufferCount;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_REQBUFS, &reqbufs);

  DCHECK(mfc_output_buffer_map_.empty());
  mfc_output_buffer_map_.resize(reqbufs.count);
  for (size_t i = 0; i < mfc_output_buffer_map_.size(); ++i) {
    struct v4l2_plane planes[1];
    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    memset(planes, 0, sizeof(planes));
    buffer.index    = i;
    buffer.type     = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buffer.memory   = V4L2_MEMORY_MMAP;
    buffer.m.planes = planes;
    buffer.length   = arraysize(planes);
    IOCTL_OR_ERROR_RETURN_FALSE(mfc_fd_, VIDIOC_QUERYBUF, &buffer);
    void* address = mmap(NULL, buffer.m.planes[0].length,
        PROT_READ | PROT_WRITE, MAP_SHARED, mfc_fd_,
        buffer.m.planes[0].m.mem_offset);
    if (address == MAP_FAILED) {
      DPLOG(ERROR) << "CreateMfcOutputBuffers(): mmap() failed";
      return false;
    }
    mfc_output_buffer_map_[i].address = address;
    mfc_output_buffer_map_[i].length = buffer.m.planes[0].length;
    mfc_free_output_buffers_.push_back(i);
  }

  return true;
}

void ExynosVideoEncodeAccelerator::DestroyGscInputBuffers() {
  DVLOG(3) << "DestroyGscInputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!gsc_input_streamon_);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_USERPTR;
  if (HANDLE_EINTR(ioctl(gsc_fd_, VIDIOC_REQBUFS, &reqbufs)) != 0)
    DPLOG(ERROR) << "DestroyGscInputBuffers(): ioctl() failed: VIDIOC_REQBUFS";

  gsc_input_buffer_map_.clear();
  gsc_free_input_buffers_.clear();
}

void ExynosVideoEncodeAccelerator::DestroyGscOutputBuffers() {
  DVLOG(3) << "DestroyGscOutputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!gsc_output_streamon_);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_DMABUF;
  if (HANDLE_EINTR(ioctl(gsc_fd_, VIDIOC_REQBUFS, &reqbufs)) != 0)
    DPLOG(ERROR) << "DestroyGscOutputBuffers(): ioctl() failed: VIDIOC_REQBUFS";

  gsc_output_buffer_map_.clear();
  gsc_free_output_buffers_.clear();
}

void ExynosVideoEncodeAccelerator::DestroyMfcInputBuffers() {
  DVLOG(3) << "DestroyMfcInputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!mfc_input_streamon_);

  for (size_t buf = 0; buf < mfc_input_buffer_map_.size(); ++buf) {
    MfcInputRecord& input_record = mfc_input_buffer_map_[buf];

    for (size_t plane = 0; plane < arraysize(input_record.fd); ++plane)
      close(mfc_input_buffer_map_[buf].fd[plane]);
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  if (HANDLE_EINTR(ioctl(mfc_fd_, VIDIOC_REQBUFS, &reqbufs)) != 0)
    DPLOG(ERROR) << "DestroyMfcInputBuffers(): ioctl() failed: VIDIOC_REQBUFS";

  mfc_input_buffer_map_.clear();
  mfc_free_input_buffers_.clear();
}

void ExynosVideoEncodeAccelerator::DestroyMfcOutputBuffers() {
  DVLOG(3) << "DestroyMfcOutputBuffers()";
  DCHECK(child_message_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!mfc_output_streamon_);

  for (size_t i = 0; i < mfc_output_buffer_map_.size(); ++i) {
    if (mfc_output_buffer_map_[i].address != NULL) {
      munmap(mfc_output_buffer_map_[i].address,
             mfc_output_buffer_map_[i].length);
    }
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  if (HANDLE_EINTR(ioctl(mfc_fd_, VIDIOC_REQBUFS, &reqbufs)) != 0)
    DPLOG(ERROR) << "DestroyMfcOutputBuffers(): ioctl() failed: VIDIOC_REQBUFS";

  mfc_output_buffer_map_.clear();
  mfc_free_output_buffers_.clear();
}

}  // namespace content
