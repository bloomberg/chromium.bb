// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_device.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
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

class DesktopCaptureDevice::Core : public webrtc::DesktopCapturer::Callback {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
       scoped_ptr<webrtc::DesktopCapturer> capturer,
       DesktopMediaID::Type type);
  virtual ~Core();

  // Implementation of VideoCaptureDevice methods.
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        scoped_ptr<Client> client);

  void SetNotificationWindowId(gfx::NativeViewId window_id);

 private:

  // webrtc::DesktopCapturer::Callback interface
  virtual webrtc::SharedMemory* CreateSharedMemory(size_t size) OVERRIDE;
  virtual void OnCaptureCompleted(webrtc::DesktopFrame* frame) OVERRIDE;

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
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

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

  // Timer used to capture the frame.
  base::OneShotTimer<Core> capture_timer_;

  // True when waiting for |desktop_capturer_| to capture current frame.
  bool capture_in_progress_;

  // True if the first capture call has returned. Used to log the first capture
  // result.
  bool first_capture_returned_;

  // The type of the capturer.
  DesktopMediaID::Type capturer_type_;

  scoped_ptr<webrtc::BasicDesktopFrame> black_frame_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

DesktopCaptureDevice::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_ptr<webrtc::DesktopCapturer> capturer,
    DesktopMediaID::Type type)
    : task_runner_(task_runner),
      desktop_capturer_(capturer.Pass()),
      capture_in_progress_(false),
      first_capture_returned_(false),
      capturer_type_(type) {
}

DesktopCaptureDevice::Core::~Core() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_.reset();
  output_frame_.reset();
  previous_frame_size_.set(0, 0);
  desktop_capturer_.reset();
}

void DesktopCaptureDevice::Core::AllocateAndStart(
    const media::VideoCaptureParams& params,
    scoped_ptr<Client> client) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_GT(params.requested_format.frame_size.GetArea(), 0);
  DCHECK_GT(params.requested_format.frame_rate, 0);
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

void DesktopCaptureDevice::Core::SetNotificationWindowId(
    gfx::NativeViewId window_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(window_id);
  desktop_capturer_->SetExcludedWindow(window_id);
}

webrtc::SharedMemory*
DesktopCaptureDevice::Core::CreateSharedMemory(size_t size) {
  return NULL;
}

void DesktopCaptureDevice::Core::OnCaptureCompleted(
    webrtc::DesktopFrame* frame) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(capture_in_progress_);

  if (!first_capture_returned_) {
    first_capture_returned_ = true;
    if (capturer_type_ == DesktopMediaID::TYPE_SCREEN) {
      IncrementDesktopCaptureCounter(frame ? FIRST_SCREEN_CAPTURE_SUCCEEDED
                                           : FIRST_SCREEN_CAPTURE_FAILED);
    } else {
      IncrementDesktopCaptureCounter(frame ? FIRST_WINDOW_CAPTURE_SUCCEEDED
                                           : FIRST_WINDOW_CAPTURE_FAILED);
    }
  }

  capture_in_progress_ = false;

  if (!frame) {
    std::string log("Failed to capture a frame.");
    LOG(ERROR) << log;
    client_->OnError(log);
    return;
  }

  if (!client_)
    return;

  base::TimeDelta capture_time(
      base::TimeDelta::FromMilliseconds(frame->capture_time_ms()));
  UMA_HISTOGRAM_TIMES(
      capturer_type_ == DesktopMediaID::TYPE_SCREEN ? kUmaScreenCaptureTime
                                                    : kUmaWindowCaptureTime,
      capture_time);

  scoped_ptr<webrtc::DesktopFrame> owned_frame(frame);

  // On OSX We receive a 1x1 frame when the shared window is minimized. It
  // cannot be subsampled to I420 and will be dropped downstream. So we replace
  // it with a black frame to avoid the video appearing frozen at the last
  // frame.
  if (frame->size().width() == 1 || frame->size().height() == 1) {
    if (!black_frame_.get()) {
      black_frame_.reset(
          new webrtc::BasicDesktopFrame(
              webrtc::DesktopSize(capture_format_.frame_size.width(),
                                  capture_format_.frame_size.height())));
      memset(black_frame_->data(),
             0,
             black_frame_->stride() * black_frame_->size().height());
    }
    owned_frame.reset();
    frame = black_frame_.get();
  }

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
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!client_)
    return;

  CaptureFrameAndScheduleNext();
}

void DesktopCaptureDevice::Core::CaptureFrameAndScheduleNext() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::TimeTicks started_time = base::TimeTicks::Now();
  DoCapture();
  base::TimeDelta last_capture_duration = base::TimeTicks::Now() - started_time;

  // Limit frame-rate to reduce CPU consumption.
  base::TimeDelta capture_period = std::max(
    (last_capture_duration * 100) / kMaximumCpuConsumptionPercentage,
    base::TimeDelta::FromSeconds(1) / capture_format_.frame_rate);

  // Schedule a task for the next frame.
  capture_timer_.Start(FROM_HERE, capture_period - last_capture_duration,
                       this, &Core::OnCaptureTimer);
}

void DesktopCaptureDevice::Core::DoCapture() {
  DCHECK(task_runner_->BelongsToCurrentThread());
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
  webrtc::DesktopCaptureOptions options =
      webrtc::DesktopCaptureOptions::CreateDefault();
  // Leave desktop effects enabled during WebRTC captures.
  options.set_disable_effects(false);

  scoped_ptr<webrtc::DesktopCapturer> capturer;

  switch (source.type) {
    case DesktopMediaID::TYPE_SCREEN: {
#if defined(OS_WIN)
      options.set_allow_use_magnification_api(true);
#endif
      scoped_ptr<webrtc::ScreenCapturer> screen_capturer(
          webrtc::ScreenCapturer::Create(options));
      if (screen_capturer && screen_capturer->SelectScreen(source.id)) {
        capturer.reset(new webrtc::DesktopAndCursorComposer(
            screen_capturer.release(),
            webrtc::MouseCursorMonitor::CreateForScreen(options, source.id)));
        IncrementDesktopCaptureCounter(SCREEN_CAPTURER_CREATED);
      }
      break;
    }

    case DesktopMediaID::TYPE_WINDOW: {
      scoped_ptr<webrtc::WindowCapturer> window_capturer(
          webrtc::WindowCapturer::Create(options));
      if (window_capturer && window_capturer->SelectWindow(source.id)) {
        window_capturer->BringSelectedWindowToFront();
        capturer.reset(new webrtc::DesktopAndCursorComposer(
            window_capturer.release(),
            webrtc::MouseCursorMonitor::CreateForWindow(options, source.id)));
        IncrementDesktopCaptureCounter(WINDOW_CAPTURER_CREATED);
      }
      break;
    }

    default: {
      NOTREACHED();
    }
  }

  scoped_ptr<media::VideoCaptureDevice> result;
  if (capturer)
    result.reset(new DesktopCaptureDevice(capturer.Pass(), source.type));

  return result.Pass();
}

DesktopCaptureDevice::~DesktopCaptureDevice() {
  DCHECK(!core_);
}

void DesktopCaptureDevice::AllocateAndStart(
    const media::VideoCaptureParams& params,
    scoped_ptr<Client> client) {
  thread_.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&Core::AllocateAndStart, base::Unretained(core_.get()), params,
                 base::Passed(&client)));
}

void DesktopCaptureDevice::StopAndDeAllocate() {
  if (core_) {
    thread_.message_loop_proxy()->DeleteSoon(FROM_HERE, core_.release());
    thread_.Stop();
  }
}

void DesktopCaptureDevice::SetNotificationWindowId(
    gfx::NativeViewId window_id) {
  thread_.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&Core::SetNotificationWindowId, base::Unretained(core_.get()),
                 window_id));
}

DesktopCaptureDevice::DesktopCaptureDevice(
    scoped_ptr<webrtc::DesktopCapturer> capturer,
    DesktopMediaID::Type type)
    : thread_("desktopCaptureThread") {
#if defined(OS_WIN)
  // On Windows the thread must be a UI thread.
  base::MessageLoop::Type thread_type = base::MessageLoop::TYPE_UI;
#else
  base::MessageLoop::Type thread_type = base::MessageLoop::TYPE_DEFAULT;
#endif

  thread_.StartWithOptions(base::Thread::Options(thread_type, 0));

  core_.reset(new Core(thread_.message_loop_proxy(), capturer.Pass(), type));
}

}  // namespace content
