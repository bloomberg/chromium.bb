// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/desktop_capture_device.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/desktop_media_id.h"
#include "media/base/video_util.h"
#include "third_party/libyuv/include/libyuv/scale_argb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/window_capturer.h"

namespace content {

namespace {
const int kBytesPerPixel = 4;
}  // namespace

class DesktopCaptureDevice::Core
    : public base::RefCountedThreadSafe<Core>,
      public webrtc::DesktopCapturer::Callback {
 public:
  Core(scoped_refptr<base::SequencedTaskRunner> task_runner,
       scoped_ptr<webrtc::DesktopCapturer> capturer);

  // Implementation of VideoCaptureDevice methods.
  void Allocate(int width, int height,
                int frame_rate,
                EventHandler* event_handler);
  void Start();
  void Stop();
  void DeAllocate();

 private:
  friend class base::RefCountedThreadSafe<Core>;
  virtual ~Core();

  // webrtc::DesktopCapturer::Callback interface
  virtual webrtc::SharedMemory* CreateSharedMemory(size_t size) OVERRIDE;
  virtual void OnCaptureCompleted(webrtc::DesktopFrame* frame) OVERRIDE;

  // Helper methods that run on the |task_runner_|. Posted from the
  // corresponding public methods.
  void DoAllocate(int width, int height, int frame_rate);
  void DoStart();
  void DoStop();
  void DoDeAllocate();

  // Helper to schedule capture tasks.
  void ScheduleCaptureTimer();

  // Method that is scheduled on |task_runner_| to be called on regular interval
  // to capture a frame.
  void OnCaptureTimer();

  // Captures a single frame.
  void DoCapture();

  // Task runner used for capturing operations.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The underlying DesktopCapturer instance used to capture frames.
  scoped_ptr<webrtc::DesktopCapturer> desktop_capturer_;

  // |event_handler_lock_| must be locked whenever |event_handler_| is used.
  // It's necessary because DeAllocate() needs to reset it on the calling thread
  // to ensure that the event handler is not called once DeAllocate() returns.
  base::Lock event_handler_lock_;
  EventHandler* event_handler_;

  // Requested size specified to Allocate().
  webrtc::DesktopSize requested_size_;

  // Frame rate specified to Allocate().
  int frame_rate_;

  // Empty until the first frame has been captured, and the output dimensions
  // chosen based on the capture frame's size, and any caller-supplied
  // size constraints.
  webrtc::DesktopSize output_size_;

  // Size of the most recently received frame.
  webrtc::DesktopSize previous_frame_size_;

  // DesktopFrame into which captured frames are scaled, if the source size does
  // not match |output_size_|. If the source and output have the same dimensions
  // then this is NULL.
  scoped_ptr<webrtc::DesktopFrame> scaled_frame_;

  // True between DoStart() and DoStop(). Can't just check |event_handler_|
  // because |event_handler_| is used on the caller thread.
  bool started_;

  // True when we have delayed OnCaptureTimer() task posted on
  // |task_runner_|.
  bool capture_task_posted_;

  // True when waiting for |desktop_capturer_| to capture current frame.
  bool capture_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

DesktopCaptureDevice::Core::Core(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_ptr<webrtc::DesktopCapturer> capturer)
    : task_runner_(task_runner),
      desktop_capturer_(capturer.Pass()),
      event_handler_(NULL),
      started_(false),
      capture_task_posted_(false),
      capture_in_progress_(false) {
}

DesktopCaptureDevice::Core::~Core() {
}

void DesktopCaptureDevice::Core::Allocate(int width, int height,
                                         int frame_rate,
                                         EventHandler* event_handler) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
  DCHECK_GT(frame_rate, 0);

  {
    base::AutoLock auto_lock(event_handler_lock_);
    event_handler_ = event_handler;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Core::DoAllocate, this, width, height, frame_rate));
}

void DesktopCaptureDevice::Core::Start() {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&Core::DoStart, this));
}

void DesktopCaptureDevice::Core::Stop() {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&Core::DoStop, this));
}

void DesktopCaptureDevice::Core::DeAllocate() {
  {
    base::AutoLock auto_lock(event_handler_lock_);
    event_handler_ = NULL;
  }
  task_runner_->PostTask(FROM_HERE, base::Bind(&Core::DoDeAllocate, this));
}

webrtc::SharedMemory*
DesktopCaptureDevice::Core::CreateSharedMemory(size_t size) {
  return NULL;
}

void DesktopCaptureDevice::Core::OnCaptureCompleted(
    webrtc::DesktopFrame* frame) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(capture_in_progress_);

  capture_in_progress_ = false;

  if (!frame) {
    LOG(ERROR) << "Failed to capture a frame.";
    event_handler_->OnError();
    return;
  }

  scoped_ptr<webrtc::DesktopFrame> owned_frame(frame);

  // If an |output_size_| hasn't yet been chosen then choose one, based upon
  // the source frame size and the requested size supplied to Allocate().
  if (output_size_.is_empty()) {
    // Treat the requested size as upper bounds on width & height.
    // TODO(wez): Constraints should be passed from getUserMedia to Allocate.
    output_size_.set(
        std::min(frame->size().width(), requested_size_.width()),
        std::min(frame->size().height(), requested_size_.height()));

    // Inform the EventHandler of the output dimensions, format and frame rate.
    media::VideoCaptureCapability caps;
    caps.width = output_size_.width();
    caps.height = output_size_.height();
    caps.frame_rate = frame_rate_;
    caps.color = media::VideoCaptureCapability::kARGB;
    caps.expected_capture_delay =
        base::Time::kMillisecondsPerSecond / frame_rate_;
    caps.interlaced = false;

    base::AutoLock auto_lock(event_handler_lock_);
    if (event_handler_)
      event_handler_->OnFrameInfo(caps);
  }

  if (!started_)
    return;

  size_t output_bytes = output_size_.width() * output_size_.height() *
      webrtc::DesktopFrame::kBytesPerPixel;

  if (frame->size().equals(output_size_)) {
    // If the captured frame matches the output size, we can return the pixel
    // data directly, without scaling.
    scaled_frame_.reset();

    base::AutoLock auto_lock(event_handler_lock_);
    if (event_handler_) {
      event_handler_->OnIncomingCapturedFrame(
          frame->data(), output_bytes, base::Time::Now(), 0, false, false);
    }
    return;
  }

  // If the output size differs from the frame size (e.g. the source has changed
  // from its original dimensions, or the caller specified size constraints)
  // then we need to scale the image.
  if (!scaled_frame_)
    scaled_frame_.reset(new webrtc::BasicDesktopFrame(output_size_));
  DCHECK(scaled_frame_->size().equals(output_size_));

  // If the source frame size changed then clear |scaled_frame_|'s pixels.
  if (!previous_frame_size_.equals(frame->size())) {
    previous_frame_size_ = frame->size();
    memset(scaled_frame_->data(), 0, output_bytes);
  }

  // Determine the output size preserving aspect, and center in output buffer.
  gfx::Rect scaled_rect = media::ComputeLetterboxRegion(
      gfx::Rect(0, 0, output_size_.width(), output_size_.height()),
      gfx::Size(frame->size().width(), frame->size().height()));
  uint8* scaled_data = scaled_frame_->data() +
      scaled_frame_->stride() * scaled_rect.y() +
      webrtc::DesktopFrame::kBytesPerPixel * scaled_rect.x();

  // TODO(wez): Optimize this to scale only changed portions of the output,
  // using ARGBScaleClip().
  libyuv::ARGBScale(frame->data(), frame->stride(),
                    frame->size().width(), frame->size().height(),
                    scaled_data, scaled_frame_->stride(),
                    scaled_rect.width(), scaled_rect.height(),
                    libyuv::kFilterBilinear);

  base::AutoLock auto_lock(event_handler_lock_);
  if (event_handler_) {
    event_handler_->OnIncomingCapturedFrame(
        scaled_frame_->data(), output_bytes,
        base::Time::Now(), 0, false, false);
  }
}

void DesktopCaptureDevice::Core::DoAllocate(int width,
                                            int height,
                                            int frame_rate) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(desktop_capturer_);

  requested_size_.set(width, height);
  output_size_.set(0, 0);
  frame_rate_ = frame_rate;

  desktop_capturer_->Start(this);

  // Capture first frame, so that we can call OnFrameInfo() callback.
  DoCapture();
}

void DesktopCaptureDevice::Core::DoStart() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  started_ = true;
  if (!capture_task_posted_) {
    ScheduleCaptureTimer();
    DoCapture();
  }
}

void DesktopCaptureDevice::Core::DoStop() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  started_ = false;
  scaled_frame_.reset();
}

void DesktopCaptureDevice::Core::DoDeAllocate() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DoStop();
  desktop_capturer_.reset();
  output_size_.set(0, 0);
}

void DesktopCaptureDevice::Core::ScheduleCaptureTimer() {
  DCHECK(!capture_task_posted_);
  capture_task_posted_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&Core::OnCaptureTimer, this),
      base::TimeDelta::FromSeconds(1) / frame_rate_);
}

void DesktopCaptureDevice::Core::OnCaptureTimer() {
  DCHECK(capture_task_posted_);
  capture_task_posted_ = false;

  if (!started_)
    return;

  // Schedule a task for the next frame.
  ScheduleCaptureTimer();
  DoCapture();
}

void DesktopCaptureDevice::Core::DoCapture() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!capture_in_progress_);

  capture_in_progress_ = true;
  desktop_capturer_->Capture(webrtc::DesktopRegion());

  // Currently only synchronous implementations of DesktopCapturer are
  // supported.
  DCHECK(!capture_in_progress_);
}

// static
scoped_ptr<media::VideoCaptureDevice> DesktopCaptureDevice::Create(
    const DesktopMediaID& source) {
  scoped_refptr<base::SequencedWorkerPool> blocking_pool =
      BrowserThread::GetBlockingPool();
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      blocking_pool->GetSequencedTaskRunner(
          blocking_pool->GetSequenceToken());

  switch (source.type) {
    case DesktopMediaID::TYPE_SCREEN: {
      scoped_ptr<webrtc::DesktopCapturer> capturer;

#if defined(OS_CHROMEOS) && !defined(ARCH_CPU_ARMEL) && defined(USE_X11)
      // ScreenCapturerX11 polls by default, due to poor driver support for
      // DAMAGE. ChromeOS' drivers [can be patched to] support DAMAGE properly,
      // so use it. However ARM driver seems to not support this properly, so
      // disable it for ARM. See http://crbug.com/230105 .
      capturer.reset(webrtc::ScreenCapturer::CreateWithXDamage(true));
#elif defined(OS_WIN)
      // ScreenCapturerWin disables Aero by default. We don't want it disabled
      // for WebRTC screen capture, though.
      capturer.reset(
          webrtc::ScreenCapturer::CreateWithDisableAero(false));
#else
      capturer.reset(webrtc::ScreenCapturer::Create());
#endif

      return scoped_ptr<media::VideoCaptureDevice>(new DesktopCaptureDevice(
          task_runner, capturer.Pass()));
    }

    case DesktopMediaID::TYPE_WINDOW: {
      scoped_ptr<webrtc::WindowCapturer> capturer(
          webrtc::WindowCapturer::Create());

      if (!capturer || !capturer->SelectWindow(source.id)) {
        return scoped_ptr<media::VideoCaptureDevice>();
      }

      return scoped_ptr<media::VideoCaptureDevice>(new DesktopCaptureDevice(
          task_runner, capturer.PassAs<webrtc::DesktopCapturer>()));
    }

    default: {
      NOTREACHED();
      return scoped_ptr<media::VideoCaptureDevice>();
    }
  }
}

DesktopCaptureDevice::DesktopCaptureDevice(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_ptr<webrtc::DesktopCapturer> capturer)
    : core_(new Core(task_runner, capturer.Pass())),
      name_("", "") {
}

DesktopCaptureDevice::~DesktopCaptureDevice() {
  DeAllocate();
}

void DesktopCaptureDevice::Allocate(
    const media::VideoCaptureCapability& capture_format,
    EventHandler* observer) {
  core_->Allocate(capture_format.width,
                  capture_format.height,
                  capture_format.frame_rate,
                  observer);
}

void DesktopCaptureDevice::Start() {
  core_->Start();
}

void DesktopCaptureDevice::Stop() {
  core_->Stop();
}

void DesktopCaptureDevice::DeAllocate() {
  core_->DeAllocate();
}

const media::VideoCaptureDevice::Name& DesktopCaptureDevice::device_name() {
  return name_;
}

}  // namespace content
