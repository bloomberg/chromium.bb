// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_device.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "media/base/video_util.h"
#include "third_party/libyuv/include/libyuv/scale_argb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_and_cursor_composer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/window_capturer.h"

namespace content {

namespace {

// Maximum CPU time percentage of a single core that can be consumed for desktop
// capturing. This means that on systems where screen scraping is slow we may
// need to capture at frame rate lower than requested. This is necessary to keep
// UI responsive.
const int kMaximumCpuConsumptionPercentage = 50;

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
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        scoped_ptr<Client> client);
  void StopAndDeAllocate();

 private:
  friend class base::RefCountedThreadSafe<Core>;
  virtual ~Core();

  // webrtc::DesktopCapturer::Callback interface
  virtual webrtc::SharedMemory* CreateSharedMemory(size_t size) OVERRIDE;
  virtual void OnCaptureCompleted(webrtc::DesktopFrame* frame) OVERRIDE;

  // Helper methods that run on the |task_runner_|. Posted from the
  // corresponding public methods.
  void DoAllocateAndStart(const media::VideoCaptureParams& params,
                          scoped_ptr<Client> client);
  void DoStopAndDeAllocate();

  // Chooses new output properties based on the supplied source size and the
  // properties requested to Allocate(), and dispatches OnFrameInfo[Changed]
  // notifications.
  void RefreshCaptureFormat(const webrtc::DesktopSize& frame_size);

  // Method that is scheduled on |task_runner_| to be called on regular interval
  // to capture a frame.
  void OnCaptureTimer();

  // Captures a frame and schedules timer for the next one.
  void CaptureFrameAndScheduleNext();

  // Captures a single frame.
  void DoCapture();

  // Task runner used for capturing operations.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The underlying DesktopCapturer instance used to capture frames.
  scoped_ptr<webrtc::DesktopCapturer> desktop_capturer_;

  // The device client which proxies device events to the controller. Accessed
  // on the task_runner_ thread.
  scoped_ptr<Client> client_;

  // Requested video capture format (width, height, frame rate, etc).
  media::VideoCaptureParams requested_params_;

  // Actual video capture format being generated.
  media::VideoCaptureFormat capture_format_;

  // Size of frame most recently captured from the source.
  webrtc::DesktopSize previous_frame_size_;

  // DesktopFrame into which captured frames are down-scaled and/or letterboxed,
  // depending upon the caller's requested capture capabilities. If frames can
  // be returned to the caller directly then this is NULL.
  scoped_ptr<webrtc::DesktopFrame> output_frame_;

  // Sub-rectangle of |output_frame_| into which the source will be scaled
  // and/or letterboxed.
  webrtc::DesktopRect output_rect_;

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
      capture_task_posted_(false),
      capture_in_progress_(false) {}

DesktopCaptureDevice::Core::~Core() {
}

void DesktopCaptureDevice::Core::AllocateAndStart(
    const media::VideoCaptureParams& params,
    scoped_ptr<Client> client) {
  DCHECK_GT(params.requested_format.frame_size.GetArea(), 0);
  DCHECK_GT(params.requested_format.frame_rate, 0);

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &Core::DoAllocateAndStart, this, params, base::Passed(&client)));
}

void DesktopCaptureDevice::Core::StopAndDeAllocate() {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&Core::DoStopAndDeAllocate, this));
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
    std::string log("Failed to capture a frame.");
    LOG(ERROR) << log;
    client_->OnError(log);
    return;
  }

  if (!client_)
    return;

  scoped_ptr<webrtc::DesktopFrame> owned_frame(frame);

  // Handle initial frame size and size changes.
  RefreshCaptureFormat(frame->size());

  webrtc::DesktopSize output_size(capture_format_.frame_size.width(),
                                  capture_format_.frame_size.height());
  size_t output_bytes = output_size.width() * output_size.height() *
      webrtc::DesktopFrame::kBytesPerPixel;
  const uint8_t* output_data = NULL;
  scoped_ptr<uint8_t[]> flipped_frame_buffer;

  if (frame->size().equals(output_size)) {
    // If the captured frame matches the output size, we can return the pixel
    // data directly, without scaling.
    output_data = frame->data();

    // If the |frame| generated by the screen capturer is inverted then we need
    // to flip |frame|.
    // This happens only on a specific platform. Refer to crbug.com/306876.
    if (frame->stride() < 0) {
      int height = frame->size().height();
      int bytes_per_row =
          frame->size().width() * webrtc::DesktopFrame::kBytesPerPixel;
      flipped_frame_buffer.reset(new uint8_t[output_bytes]);
      uint8_t* dest = flipped_frame_buffer.get();
      for (int row = 0; row < height; ++row) {
        memcpy(dest, output_data, bytes_per_row);
        dest += bytes_per_row;
        output_data += frame->stride();
      }
      output_data = flipped_frame_buffer.get();
    }
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

  client_->OnIncomingCapturedData(
      output_data, output_bytes, capture_format_, 0, base::TimeTicks::Now());
}

void DesktopCaptureDevice::Core::DoAllocateAndStart(
    const media::VideoCaptureParams& params,
    scoped_ptr<Client> client) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(desktop_capturer_);
  DCHECK(client.get());
  DCHECK(!client_.get());

  client_ = client.Pass();
  requested_params_ = params;

  capture_format_ = requested_params_.requested_format;

  // This capturer always outputs ARGB, non-interlaced.
  capture_format_.pixel_format = media::PIXEL_FORMAT_ARGB;

  desktop_capturer_->Start(this);

  CaptureFrameAndScheduleNext();
}

void DesktopCaptureDevice::Core::DoStopAndDeAllocate() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  client_.reset();
  output_frame_.reset();
  previous_frame_size_.set(0, 0);
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
      requested_params_.allow_resolution_change) {
    // If this is the first frame, or the receiver supports variable resolution
    // then determine the output size by treating the requested width & height
    // as maxima.
    if (frame_size.width() >
            requested_params_.requested_format.frame_size.width() ||
        frame_size.height() >
            requested_params_.requested_format.frame_size.height()) {
      output_rect_ = ComputeLetterboxRect(
          webrtc::DesktopSize(
              requested_params_.requested_format.frame_size.width(),
              requested_params_.requested_format.frame_size.height()),
          frame_size);
      output_rect_.Translate(-output_rect_.left(), -output_rect_.top());
    } else {
      output_rect_ = webrtc::DesktopRect::MakeSize(frame_size);
    }
    capture_format_.frame_size.SetSize(output_rect_.width(),
                                       output_rect_.height());
  } else {
    // Otherwise the output frame size cannot change, so just scale and
    // letterbox.
    output_rect_ = ComputeLetterboxRect(
        webrtc::DesktopSize(capture_format_.frame_size.width(),
                            capture_format_.frame_size.height()),
        frame_size);
  }

  previous_frame_size_ = frame_size;
}

void DesktopCaptureDevice::Core::OnCaptureTimer() {
  DCHECK(capture_task_posted_);
  capture_task_posted_ = false;

  if (!client_)
    return;

  CaptureFrameAndScheduleNext();
}

void DesktopCaptureDevice::Core::CaptureFrameAndScheduleNext() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!capture_task_posted_);

  base::TimeTicks started_time = base::TimeTicks::Now();
  DoCapture();
  base::TimeDelta last_capture_duration = base::TimeTicks::Now() - started_time;

  // Limit frame-rate to reduce CPU consumption.
  base::TimeDelta capture_period = std::max(
    (last_capture_duration * 100) / kMaximumCpuConsumptionPercentage,
    base::TimeDelta::FromSeconds(1) / capture_format_.frame_rate);

  // Schedule a task for the next frame.
  capture_task_posted_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&Core::OnCaptureTimer, this),
      capture_period - last_capture_duration);
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

  webrtc::DesktopCaptureOptions options =
      webrtc::DesktopCaptureOptions::CreateDefault();
  // Leave desktop effects enabled during WebRTC captures.
  options.set_disable_effects(false);

  scoped_ptr<webrtc::DesktopCapturer> capturer;

  switch (source.type) {
    case DesktopMediaID::TYPE_SCREEN: {
      scoped_ptr<webrtc::ScreenCapturer> screen_capturer;
      screen_capturer.reset(webrtc::ScreenCapturer::Create(options));
      if (screen_capturer && screen_capturer->SelectScreen(source.id)) {
        capturer.reset(new webrtc::DesktopAndCursorComposer(
            screen_capturer.release(),
            webrtc::MouseCursorMonitor::CreateForScreen(options, source.id)));
      }
      break;
    }

    case DesktopMediaID::TYPE_WINDOW: {
      scoped_ptr<webrtc::WindowCapturer> window_capturer(
          webrtc::WindowCapturer::Create(options));
      if (window_capturer && window_capturer->SelectWindow(source.id)) {
        capturer.reset(new webrtc::DesktopAndCursorComposer(
            window_capturer.release(),
            webrtc::MouseCursorMonitor::CreateForWindow(options, source.id)));
      }
      break;
    }

    default: {
      NOTREACHED();
    }
  }

  scoped_ptr<media::VideoCaptureDevice> result;
  if (capturer)
    result.reset(new DesktopCaptureDevice(task_runner, capturer.Pass()));

  return result.Pass();
}

DesktopCaptureDevice::DesktopCaptureDevice(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    scoped_ptr<webrtc::DesktopCapturer> capturer)
    : core_(new Core(task_runner, capturer.Pass())) {}

DesktopCaptureDevice::~DesktopCaptureDevice() {
  StopAndDeAllocate();
}

void DesktopCaptureDevice::AllocateAndStart(
    const media::VideoCaptureParams& params,
    scoped_ptr<Client> client) {
  core_->AllocateAndStart(params, client.Pass());
}

void DesktopCaptureDevice::StopAndDeAllocate() {
  core_->StopAndDeAllocate();
}

}  // namespace content
