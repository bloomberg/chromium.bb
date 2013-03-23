// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/screen/screen_capture_device.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "media/video/capture/screen/mouse_cursor_shape.h"
#include "media/video/capture/screen/screen_capture_data.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace media {

namespace {
const int kBytesPerPixel = 4;
}  // namespace

class ScreenCaptureDevice::Core
    : public base::RefCountedThreadSafe<Core>,
      public ScreenCapturer::Delegate {
 public:
  explicit Core(scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Helper used in tests to supply a fake capturer.
  void SetScreenCapturerForTest(scoped_ptr<ScreenCapturer> capturer) {
    screen_capturer_ = capturer.Pass();
  }

  // Implementation of VideoCaptureDevice methods.
  void Allocate(int width, int height,
                int frame_rate,
                EventHandler* event_handler);
  void Start();
  void Stop();
  void DeAllocate();

  // ScreenCapturer::Delegate interface. Called by |screen_capturer_| on the
  // |task_runner_|.
  virtual void OnCaptureCompleted(
      scoped_refptr<ScreenCaptureData> capture_data) OVERRIDE;
  virtual void OnCursorShapeChanged(
      scoped_ptr<MouseCursorShape> cursor_shape) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<Core>;
  virtual ~Core();

  // Helper methods that run on the |task_runner_|. Posted from the
  // corresponding public methods.
  void DoAllocate(int frame_rate);
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

  // Frame rate specified to Allocate().
  int frame_rate_;

  // The underlying ScreenCapturer instance used to capture frames.
  scoped_ptr<ScreenCapturer> screen_capturer_;

  // After Allocate() is called we need to call OnFrameInfo() method of the
  // |event_handler_| to specify the size of the frames this capturer will
  // produce. In order to get screen size from |screen_capturer_| we need to
  // capture at least one frame. Once screen size is known it's stored in
  // |frame_size_|.
  bool waiting_for_frame_size_;
  SkISize frame_size_;
  SkBitmap resized_bitmap_;

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
      waiting_for_frame_size_(false),
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
      FROM_HERE, base::Bind(&Core::DoAllocate, this, frame_rate));
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

void ScreenCaptureDevice::Core::OnCaptureCompleted(
    scoped_refptr<ScreenCaptureData> capture_data) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(capture_in_progress_);
  DCHECK(!capture_data->size().isEmpty());

  capture_in_progress_ = false;

  if (waiting_for_frame_size_) {
    frame_size_ = capture_data->size();
    waiting_for_frame_size_ = false;

    // Inform the EventHandler of the video frame dimensions and format.
    VideoCaptureCapability caps;
    caps.width = frame_size_.width();
    caps.height = frame_size_.height();
    caps.frame_rate = frame_rate_;
    caps.color = VideoCaptureCapability::kARGB;
    caps.expected_capture_delay =
        base::Time::kMillisecondsPerSecond / frame_rate_;
    caps.interlaced = false;

    base::AutoLock auto_lock(event_handler_lock_);
    if (event_handler_)
      event_handler_->OnFrameInfo(caps);
  }

  if (!started_)
    return;

  size_t buffer_size = frame_size_.width() * frame_size_.height() *
      ScreenCaptureData::kBytesPerPixel;

  if (capture_data->size() == frame_size_) {
    // If the captured frame matches the requested size, we don't need to
    // resize it.
    resized_bitmap_.reset();

    base::AutoLock auto_lock(event_handler_lock_);
    if (event_handler_) {
      event_handler_->OnIncomingCapturedFrame(
          capture_data->data(), buffer_size, base::Time::Now(),
          0, false, false);
    }
    return;
  }

  // In case screen size has changed we need to resize the image. Resized image
  // is stored to |resized_bitmap_|. Only regions of the screen that are
  // changing are copied.

  SkRegion dirty_region = capture_data->dirty_region();

  if (resized_bitmap_.width() != frame_size_.width() ||
      resized_bitmap_.height() != frame_size_.height()) {
    resized_bitmap_.setConfig(SkBitmap::kARGB_8888_Config,
                              frame_size_.width(), frame_size_.height());
    resized_bitmap_.setIsOpaque(true);
    resized_bitmap_.allocPixels();
    dirty_region.setRect(SkIRect::MakeSize(frame_size_));
  }

  float scale_x = static_cast<float>(frame_size_.width()) /
      capture_data->size().width();
  float scale_y = static_cast<float>(frame_size_.height()) /
      capture_data->size().height();
  float scale;
  float x, y;
  // Center the image in case aspect ratio is different.
  if (scale_x > scale_y) {
    scale = scale_y;
    x = (scale_x - scale_y) / scale * frame_size_.width() / 2.0;
    y = 0.f;
  } else {
    scale = scale_x;
    x = 0.f;
    y = (scale_y - scale_x) / scale * frame_size_.height() / 2.0;
  }

  // Create skia device and canvas that draw to |resized_bitmap_|.
  SkDevice device(resized_bitmap_);
  SkCanvas canvas(&device);
  canvas.scale(scale, scale);

  int source_stride = capture_data->stride();
  for (SkRegion::Iterator i(dirty_region); !i.done(); i.next()) {
    SkBitmap source_bitmap;
    source_bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                            i.rect().width(), i.rect().height(),
                            source_stride);
    source_bitmap.setIsOpaque(true);
    source_bitmap.setPixels(
        capture_data->data() + i.rect().top() * source_stride +
        i.rect().left() * ScreenCaptureData::kBytesPerPixel);
    canvas.drawBitmap(source_bitmap, i.rect().left() + x / scale,
                      i.rect().top() + y / scale, NULL);
  }

  base::AutoLock auto_lock(event_handler_lock_);
  if (event_handler_) {
    event_handler_->OnIncomingCapturedFrame(
        reinterpret_cast<uint8*>(resized_bitmap_.getPixels()), buffer_size,
        base::Time::Now(), 0, false, false);
  }
}

void ScreenCaptureDevice::Core::OnCursorShapeChanged(
    scoped_ptr<MouseCursorShape> cursor_shape) {
  // TODO(sergeyu): Store mouse cursor shape and then render it to each captured
  // frame. crbug.com/173265 .
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
}

void ScreenCaptureDevice::Core::DoAllocate(int frame_rate) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  frame_rate_ = frame_rate;

  // Create and start frame capturer.
#if defined(OS_CHROMEOS)
  // ScreenCapturerX11 polls by default, due to poor driver support for DAMAGE.
  // ChromeOS' drivers [can be patched to] support DAMAGE properly, so use it.
  if (!screen_capturer_)
    screen_capturer_ = ScreenCapturer::CreateWithXDamage(true);
#else
  if (!screen_capturer_)
    screen_capturer_ = ScreenCapturer::Create();
#endif
  if (screen_capturer_)
    screen_capturer_->Start(this);

  // Capture first frame, so that we can call OnFrameInfo() callback.
  waiting_for_frame_size_ = true;
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
  resized_bitmap_.reset();
}

void ScreenCaptureDevice::Core::DoDeAllocate() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DoStop();
  if (screen_capturer_) {
    screen_capturer_->Stop();
    screen_capturer_.reset();
  }
  waiting_for_frame_size_ = false;
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
  DCHECK(!capture_in_progress_);

  capture_in_progress_ = true;
  screen_capturer_->CaptureFrame();

  // Assume that ScreenCapturer always calls OnCaptureCompleted()
  // callback before it returns.
  //
  // TODO(sergeyu): Fix ScreenCapturer to return video frame
  // synchronously instead of using Delegate interface.
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
  scoped_ptr<ScreenCapturer> capturer) {
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

const VideoCaptureDevice::Name& ScreenCaptureDevice::device_name() {
  return name_;
}

}  // namespace media
