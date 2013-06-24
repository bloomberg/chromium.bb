// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/screen_capture_device.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "media/base/video_util.h"
#include "third_party/libyuv/include/libyuv/scale_argb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_shape.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

namespace content {

namespace {
const int kBytesPerPixel = 4;
}  // namespace

class ScreenCaptureDevice::Core
    : public base::RefCountedThreadSafe<Core>,
      public webrtc::DesktopCapturer::Callback {
 public:
  explicit Core(scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Helper used in tests to supply a fake capturer.
  void SetScreenCapturerForTest(scoped_ptr<webrtc::ScreenCapturer> capturer) {
    screen_capturer_ = capturer.Pass();
  }

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
  // to capture the screen.
  void OnCaptureTimer();

  // Captures a single frame.
  void DoCapture();

  // Task runner used for screen capturing operations.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // |event_handler_lock_| must be locked whenever |event_handler_| is used.
  // It's necessary because DeAllocate() needs to reset it on the calling thread
  // to ensure that the event handler is not called once DeAllocate() returns.
  base::Lock event_handler_lock_;
  EventHandler* event_handler_;

  // Requested size specified to Allocate().
  webrtc::DesktopSize requested_size_;

  // Frame rate specified to Allocate().
  int frame_rate_;

  // The underlying ScreenCapturer instance used to capture frames.
  scoped_ptr<webrtc::ScreenCapturer> screen_capturer_;

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

  // True when waiting for |screen_capturer_| to capture current frame.
  bool capture_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

ScreenCaptureDevice::Core::Core(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner),
      event_handler_(NULL),
      started_(false),
      capture_task_posted_(false),
      capture_in_progress_(false) {
}

ScreenCaptureDevice::Core::~Core() {
}

void ScreenCaptureDevice::Core::Allocate(int width, int height,
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

void ScreenCaptureDevice::Core::Start() {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&Core::DoStart, this));
}

void ScreenCaptureDevice::Core::Stop() {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&Core::DoStop, this));
}

void ScreenCaptureDevice::Core::DeAllocate() {
  {
    base::AutoLock auto_lock(event_handler_lock_);
    event_handler_ = NULL;
  }
  task_runner_->PostTask(FROM_HERE, base::Bind(&Core::DoDeAllocate, this));
}

webrtc::SharedMemory*
ScreenCaptureDevice::Core::CreateSharedMemory(size_t size) {
  return NULL;
}

void ScreenCaptureDevice::Core::OnCaptureCompleted(
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

void ScreenCaptureDevice::Core::DoAllocate(int width,
                                           int height,
                                           int frame_rate) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  requested_size_.set(width, height);
  output_size_.set(0, 0);
  frame_rate_ = frame_rate;

  // Create and start frame capturer.
  if (!screen_capturer_) {
#if defined(OS_CHROMEOS) && !defined(ARCH_CPU_ARMEL)
    // ScreenCapturerX11 polls by default, due to poor driver support for
    // DAMAGE. ChromeOS' drivers [can be patched to] support DAMAGE properly, so
    // use it. However ARM driver seems to not support this properly, so disable
    // it for ARM. See http://crbug.com/230105.
    screen_capturer_.reset(webrtc::ScreenCapturer::CreateWithXDamage(true));
#elif defined(OS_WIN)
    // ScreenCapturerWin disables Aero by default. We don't want it disabled for
    // WebRTC screen capture, though.
    screen_capturer_.reset(
        webrtc::ScreenCapturer::CreateWithDisableAero(false));
#else
    screen_capturer_.reset(webrtc::ScreenCapturer::Create());
#endif
  }

  if (screen_capturer_)
    screen_capturer_->Start(this);

  // Capture first frame, so that we can call OnFrameInfo() callback.
  DoCapture();
}

void ScreenCaptureDevice::Core::DoStart() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  started_ = true;
  if (!capture_task_posted_) {
    ScheduleCaptureTimer();
    DoCapture();
  }
}

void ScreenCaptureDevice::Core::DoStop() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  started_ = false;
  scaled_frame_.reset();
}

void ScreenCaptureDevice::Core::DoDeAllocate() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DoStop();
  screen_capturer_.reset();
  output_size_.set(0, 0);
}

void ScreenCaptureDevice::Core::ScheduleCaptureTimer() {
  DCHECK(!capture_task_posted_);
  capture_task_posted_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&Core::OnCaptureTimer, this),
      base::TimeDelta::FromSeconds(1) / frame_rate_);
}

void ScreenCaptureDevice::Core::OnCaptureTimer() {
  DCHECK(capture_task_posted_);
  capture_task_posted_ = false;

  if (!started_)
    return;

  // Schedule a task for the next frame.
  ScheduleCaptureTimer();
  DoCapture();
}

void ScreenCaptureDevice::Core::DoCapture() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!capture_in_progress_);

  capture_in_progress_ = true;
  screen_capturer_->Capture(webrtc::DesktopRegion());

  // Currently only synchronous implementations of DesktopCapturer are
  // supported.
  DCHECK(!capture_in_progress_);
}

ScreenCaptureDevice::ScreenCaptureDevice(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : core_(new Core(task_runner)) {
  name_.device_name = "Screen";
}

ScreenCaptureDevice::~ScreenCaptureDevice() {
  DeAllocate();
}

void ScreenCaptureDevice::SetScreenCapturerForTest(
  scoped_ptr<webrtc::ScreenCapturer> capturer) {
  core_->SetScreenCapturerForTest(capturer.Pass());
}

void ScreenCaptureDevice::Allocate(int width, int height,
                                   int frame_rate,
                                   EventHandler* event_handler) {
  core_->Allocate(width, height, frame_rate, event_handler);
}

void ScreenCaptureDevice::Start() {
  core_->Start();
}

void ScreenCaptureDevice::Stop() {
  core_->Stop();
}

void ScreenCaptureDevice::DeAllocate() {
  core_->DeAllocate();
}

const media::VideoCaptureDevice::Name& ScreenCaptureDevice::device_name() {
  return name_;
}

}  // namespace content
