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

webrtc::DesktopRect ComputeLetterboxRect(
    const webrtc::DesktopSize& max_size,
    const webrtc::DesktopSize& source_size) {
  gfx::Rect result = media::ComputeLetterboxRegion(
      gfx::Rect(0, 0, max_size.width(), max_size.height()),
      gfx::Size(source_size.width(), source_size.height()));
  return webrtc::DesktopRect::MakeLTRB(
      result.x(), result.y(), result.right(), result.bottom());
}

}  // namespace

class DesktopCaptureDevice::Core
    : public base::RefCountedThreadSafe<Core>,
      public webrtc::DesktopCapturer::Callback {
 public:
  Core(scoped_refptr<base::SequencedTaskRunner> task_runner,
       scoped_ptr<webrtc::DesktopCapturer> capturer);

  // Implementation of VideoCaptureDevice methods.
  void Allocate(const media::VideoCaptureCapability& capture_format,
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
  void DoAllocate(const media::VideoCaptureCapability& capture_format);
  void DoStart();
  void DoStop();
  void DoDeAllocate();

  // Chooses new output properties based on the supplied source size and the
  // properties requested to Allocate(), and dispatches OnFrameInfo[Changed]
  // notifications.
  void RefreshCaptureFormat(const webrtc::DesktopSize& frame_size);

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

  // Requested video capture format (width, height, frame rate, etc).
  media::VideoCaptureCapability requested_format_;

  // Actual video capture format being generated.
  media::VideoCaptureCapability capture_format_;

  // Size of frame most recently captured from the source.
  webrtc::DesktopSize previous_frame_size_;

  // DesktopFrame into which captured frames are down-scaled and/or letterboxed,
  // depending upon the caller's requested capture capabilities. If frames can
  // be returned to the caller directly then this is NULL.
  scoped_ptr<webrtc::DesktopFrame> output_frame_;

  // Sub-rectangle of |output_frame_| into which the source will be scaled
  // and/or letterboxed.
  webrtc::DesktopRect output_rect_;

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

void DesktopCaptureDevice::Core::Allocate(
    const media::VideoCaptureCapability& capture_format,
    EventHandler* event_handler) {
  DCHECK_GT(capture_format.width, 0);
  DCHECK_GT(capture_format.height, 0);
  DCHECK_GT(capture_format.frame_rate, 0);

  {
    base::AutoLock auto_lock(event_handler_lock_);
    event_handler_ = event_handler;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Core::DoAllocate, this, capture_format));
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

  // Handle initial frame size and size changes.
  RefreshCaptureFormat(frame->size());

  if (!started_)
    return;

  webrtc::DesktopSize output_size(capture_format_.width,
                                  capture_format_.height);
  size_t output_bytes = output_size.width() * output_size.height() *
      webrtc::DesktopFrame::kBytesPerPixel;
  const uint8_t* output_data = NULL;

  if (frame->size().equals(output_size)) {
    // If the captured frame matches the output size, we can return the pixel
    // data directly, without scaling.
    output_data = frame->data();
  } else {
    // Otherwise we need to down-scale and/or letterbox to the target format.

    // Allocate a buffer of the correct size to scale the frame into.
    // |output_frame_| is cleared whenever |output_rect_| changes, so we don't
    // need to worry about clearing out stale pixel data in letterboxed areas.
    if (!output_frame_) {
      output_frame_.reset(new webrtc::BasicDesktopFrame(output_size));
      memset(output_frame_->data(), 0, output_bytes);
    }
    DCHECK(output_frame_->size().equals(output_size));

    // TODO(wez): Optimize this to scale only changed portions of the output,
    // using ARGBScaleClip().
    uint8_t* output_rect_data = output_frame_->data() +
        output_frame_->stride() * output_rect_.top() +
        webrtc::DesktopFrame::kBytesPerPixel * output_rect_.left();
    libyuv::ARGBScale(frame->data(), frame->stride(),
                      frame->size().width(), frame->size().height(),
                      output_rect_data, output_frame_->stride(),
                      output_rect_.width(), output_rect_.height(),
                      libyuv::kFilterBilinear);
    output_data = output_frame_->data();
  }

  base::AutoLock auto_lock(event_handler_lock_);
  if (event_handler_) {
    event_handler_->OnIncomingCapturedFrame(output_data, output_bytes,
                                            base::Time::Now(), 0, false, false);
  }
}

void DesktopCaptureDevice::Core::DoAllocate(
    const media::VideoCaptureCapability& capture_format) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(desktop_capturer_);

  requested_format_ = capture_format;

  // Store requested frame rate and calculate expected delay.
  capture_format_.frame_rate = requested_format_.frame_rate;
  capture_format_.expected_capture_delay =
      base::Time::kMillisecondsPerSecond / requested_format_.frame_rate;

  // Support dynamic changes in resolution only if requester also does.
  if (requested_format_.frame_size_type ==
      media::VariableResolutionVideoCaptureDevice) {
    capture_format_.frame_size_type =
        media::VariableResolutionVideoCaptureDevice;
  }

  // This capturer always outputs ARGB, non-interlaced.
  capture_format_.color = media::VideoCaptureCapability::kARGB;
  capture_format_.interlaced = false;

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
  output_frame_.reset();
  previous_frame_size_.set(0, 0);
}

void DesktopCaptureDevice::Core::DoDeAllocate() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DoStop();
  desktop_capturer_.reset();
}

void DesktopCaptureDevice::Core::RefreshCaptureFormat(
    const webrtc::DesktopSize& frame_size) {
  if (previous_frame_size_.equals(frame_size))
    return;

  // Clear the output frame, if any, since it will either need resizing, or
  // clearing of stale data in letterbox areas, anyway.
  output_frame_.reset();

  if (previous_frame_size_.is_empty() ||
      requested_format_.frame_size_type ==
          media::VariableResolutionVideoCaptureDevice) {
    // If this is the first frame, or the receiver supports variable resolution
    // then determine the output size by treating the requested width & height
    // as maxima.
    if (frame_size.width() > requested_format_.width ||
        frame_size.height() > requested_format_.height) {
      output_rect_ = ComputeLetterboxRect(
          webrtc::DesktopSize(requested_format_.width,
                              requested_format_.height),
          frame_size);
      output_rect_.Translate(-output_rect_.left(), -output_rect_.top());
    } else {
      output_rect_ = webrtc::DesktopRect::MakeSize(frame_size);
    }
    capture_format_.width = output_rect_.width();
    capture_format_.height = output_rect_.height();

    {
      base::AutoLock auto_lock(event_handler_lock_);
      if (event_handler_) {
        if (previous_frame_size_.is_empty()) {
          event_handler_->OnFrameInfo(capture_format_);
        } else {
          event_handler_->OnFrameInfoChanged(capture_format_);
        }
      }
    }
  } else {
    // Otherwise the output frame size cannot change, so just scale and
    // letterbox.
    output_rect_ = ComputeLetterboxRect(
        webrtc::DesktopSize(capture_format_.width, capture_format_.height),
        frame_size);
  }

  previous_frame_size_ = frame_size;
}

void DesktopCaptureDevice::Core::ScheduleCaptureTimer() {
  DCHECK(!capture_task_posted_);
  capture_task_posted_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&Core::OnCaptureTimer, this),
      base::TimeDelta::FromSeconds(1) / capture_format_.frame_rate);
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
    EventHandler* event_handler) {
  core_->Allocate(capture_format, event_handler);
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
