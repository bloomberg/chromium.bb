// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation notes: This needs to work on a variety of hardware
// configurations where the speed of the CPU and GPU greatly affect overall
// performance. Spanning several threads, the process of capturing has been
// split up into four conceptual stages:
//
//   1. Reserve Buffer: Before a frame can be captured, a slot in the consumer's
//      shared-memory IPC buffer is reserved. There are only a few of these;
//      when they run out, it indicates that the downstream consumer -- likely a
//      video encoder -- is the performance bottleneck, and that the rate of
//      frame capture should be throttled back.
//
//   2. Capture: A bitmap is snapshotted/copied from the RenderView's backing
//      store. This is initiated on the UI BrowserThread, and often occurs
//      asynchronously. Where supported, the GPU scales and color converts
//      frames to our desired size, and the readback happens directly into the
//      shared-memory buffer. But this is not always possible, particularly when
//      accelerated compositing is disabled.
//
//   3. Render (if needed): If the web contents cannot be captured directly into
//      our target size and color format, scaling and colorspace conversion must
//      be done on the CPU. A dedicated thread is used for this operation, to
//      avoid blocking the UI thread. The Render stage always reads from a
//      bitmap returned by Capture, and writes into the reserved slot in the
//      shared-memory buffer.
//
//   4. Deliver: The rendered video frame is returned to the consumer (which
//      implements the VideoCaptureDevice::EventHandler interface). Because
//      all paths have written the frame into the IPC buffer, this step should
//      never need to do an additional copy of the pixel data.
//
// In the best-performing case, the Render step is bypassed: Capture produces
// ready-to-Deliver frames. But when accelerated readback is not possible, the
// system is designed so that Capture and Render may run concurrently. A timing
// diagram helps illustrate this point (@30 FPS):
//
//    Time: 0ms                 33ms                 66ms                 99ms
// thread1: |-Capture-f1------v |-Capture-f2------v  |-Capture-f3----v    |-Capt
// thread2:                   |-Render-f1-----v   |-Render-f2-----v  |-Render-f3
//
// In the above example, both capturing and rendering *each* take almost the
// full 33 ms available between frames, yet we see that the required throughput
// is obtained.
//
// Turning on verbose logging will cause the effective frame rate to be logged
// at 5-second intervals.

#include "content/browser/renderer_host/media/web_contents_video_capture_device.h"

#include <algorithm>
#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_forward.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "content/browser/renderer_host/media/web_contents_capture_util.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/base/bind_to_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/base/yuv_convert.h"
#include "media/video/capture/video_capture_types.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/skia_util.h"

// Used to self-trampoline invocation of methods to the appropriate thread. This
// should be used sparingly, only when it's not clear which thread is invoking a
// method.
#define ENSURE_INVOKED_ON_THREAD(thread, ...) {  \
  DCHECK(thread.IsRunning());  \
  if (MessageLoop::current() != thread.message_loop()) {  \
    thread.message_loop()->PostTask(FROM_HERE, base::Bind(__VA_ARGS__));  \
    return;  \
  }  \
}

namespace content {

namespace {

const int kMinFrameWidth = 2;
const int kMinFrameHeight = 2;
const int kMaxFramesInFlight = 2;
const int kMaxSnapshotsInFlight = 1;

enum SnapshotResult {
  COPIED_TO_BITMAP,
  COPIED_TO_VIDEO_FRAME,
  NO_SOURCE,
  TRANSIENT_ERROR
};

// Returns the nearest even integer closer to zero.
template<typename IntType>
IntType MakeEven(IntType x) {
  return x & static_cast<IntType>(-2);
}

// Compute a letterbox region, aligned to even coordinates.
gfx::Rect ComputeYV12LetterboxRegion(const gfx::Size& frame_size,
                                     const gfx::Size& content_size) {

  gfx::Rect result = media::ComputeLetterboxRegion(gfx::Rect(frame_size),
                                                   content_size);

  result.set_x(MakeEven(result.x()));
  result.set_y(MakeEven(result.y()));
  result.set_width(std::max(kMinFrameWidth, MakeEven(result.width())));
  result.set_height(std::max(kMinFrameHeight, MakeEven(result.height())));

  return result;
}

// Keeps track of the RenderView to be sourced, and executes copying of the
// backing store on the UI BrowserThread.
class BackingStoreCopier : public WebContentsObserver {
 public:
  typedef base::Callback<void(SnapshotResult,
                              const base::Time&,
                              const SkBitmap&)> CopyFinishedCallback;
  BackingStoreCopier(int render_process_id, int render_view_id);

  virtual ~BackingStoreCopier();

  // Starts the copy from the backing store. Must be run on the UI
  // BrowserThread. |callback| will be run when the operation completes. The
  // copy may either write directly to |target|, or it may return an SkBitmap
  // with the copy result without updating |video_frame|. The SnapshotResult
  // value passed to |callback| indicates which path was taken.
  void StartCopy(
      int frame_number,
      const scoped_refptr<media::VideoFrame>& target,
      const CopyFinishedCallback& callback);

  // Stops observing an existing WebContents instance, if any.  This must be
  // called before BackingStoreCopier is destroyed.  Must be run on the UI
  // BrowserThread.
  void StopObservingWebContents();

  // content::WebContentsObserver implementation.
  virtual void DidShowFullscreenWidget(int routing_id) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    fullscreen_widget_id_ = routing_id;
  }

  virtual void DidDestroyFullscreenWidget(int routing_id) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK_EQ(fullscreen_widget_id_, routing_id);
    fullscreen_widget_id_ = MSG_ROUTING_NONE;
  }

 private:
  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE;

  void LookUpAndObserveWebContents();

  // Response callback for RenderWidgetHost::CopyFromBackingStore.
  void DidCopyFromBackingStore(int frame_number,
                               const base::Time& start_time,
                               const CopyFinishedCallback& callback,
                               bool success,
                               const SkBitmap& bitmap);

  // Response callback for RWHVP::CopyFromCompositingSurfaceToVideoFrame().
  void DidCopyFromCompositingSurfaceToVideoFrame(
      int frame_number,
      const base::Time& start_time,
      const CopyFinishedCallback& callback,
      bool success);

  // The "starting point" to find the capture source.
  const int render_process_id_;
  const int render_view_id_;

  // Routing ID of any active fullscreen render widget or MSG_ROUTING_NONE
  // otherwise.
  int fullscreen_widget_id_;

  // Last known RenderView size.
  gfx::Size last_view_size_;

  DISALLOW_COPY_AND_ASSIGN(BackingStoreCopier);
};

// Renders captures (from the backing store) into video frame buffers on a
// dedicated thread. Intended for use in the software rendering case, when GPU
// acceleration of these activities is not possible.
class VideoFrameRenderer {
 public:
  VideoFrameRenderer();

  // Render the SkBitmap |input| into the given VideoFrame buffer |output|, then
  // invoke |done_cb| to indicate success or failure. |input| is expected to be
  // ARGB. |output| must be YV12 or I420. Colorspace conversion is always done.
  // Scaling and letterboxing will be done as needed.
  void Render(int frame_number,
              const SkBitmap& input,
              const scoped_refptr<media::VideoFrame>& output,
              const base::Callback<void(bool)>& done_cb);

 private:
  void RenderOnRenderThread(int frame_number,
                            const SkBitmap& input,
                            const scoped_refptr<media::VideoFrame>& output,
                            const base::Callback<void(bool)>& done_cb);

  base::Thread render_thread_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameRenderer);
};

// Wrapper around media::VideoCaptureDevice::EventHandler to provide synchronous
// access to the underlying instance.
class SynchronizedConsumer {
 public:
  SynchronizedConsumer();

  void SetConsumer(media::VideoCaptureDevice::EventHandler* consumer);

  void OnFrameInfo(const media::VideoCaptureCapability& info);
  void OnError();
  void OnIncomingCapturedVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::Time& timestamp);

  scoped_refptr<media::VideoFrame> ReserveOutputBuffer();

 private:
  base::Lock consumer_lock_;
  media::VideoCaptureDevice::EventHandler* wrapped_consumer_;

  DISALLOW_COPY_AND_ASSIGN(SynchronizedConsumer);
};

// Responsible for logging the effective frame rate.
class VideoFrameDeliveryLog {
 public:
  VideoFrameDeliveryLog();

  // Treat |frame_number| as having been delivered, and update the
  // frame rate statistics accordingly.
  void ChronicleFrameDelivery(int frame_number);

 private:
  // The following keep track of and log the effective frame rate whenever
  // verbose logging is turned on.
  base::Time last_frame_rate_log_time_;
  int count_frames_rendered_;
  int last_frame_number_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameDeliveryLog);
};

BackingStoreCopier::BackingStoreCopier(int render_process_id,
                                       int render_view_id)
    : render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      fullscreen_widget_id_(MSG_ROUTING_NONE) {}

BackingStoreCopier::~BackingStoreCopier() {
  DCHECK(!web_contents());
}

void BackingStoreCopier::StopObservingWebContents() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (web_contents()) {
    web_contents()->DecrementCapturerCount();
    Observe(NULL);
  }
}

void BackingStoreCopier::WebContentsDestroyed(WebContents* web_contents) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  web_contents->DecrementCapturerCount();
}

void BackingStoreCopier::LookUpAndObserveWebContents() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Look-up the RenderViewHost and, from that, the WebContents that wraps it.
  // If successful, begin observing the WebContents instance.  If unsuccessful,
  // stop observing and post an error.
  //
  // Why this can be unsuccessful: The request for mirroring originates in a
  // render process, and this request is based on the current RenderView
  // associated with a tab.  However, by the time we get up-and-running here,
  // there have been multiple back-and-forth IPCs between processes, as well as
  // a bit of indirection across threads.  It's easily possible that, in the
  // meantime, the original RenderView may have gone away.
  RenderViewHost* const rvh =
      RenderViewHost::FromID(render_process_id_, render_view_id_);
  DVLOG_IF(1, !rvh) << "RenderViewHost::FromID("
                    << render_process_id_ << ", " << render_view_id_
                    << ") returned NULL.";
  Observe(rvh ? WebContents::FromRenderViewHost(rvh) : NULL);
  WebContentsImpl* contents = static_cast<WebContentsImpl*>(web_contents());
  if (contents) {
    contents->IncrementCapturerCount();
    fullscreen_widget_id_ = contents->GetFullscreenWidgetRoutingID();
  } else {
    DVLOG(1) << "WebContents::FromRenderViewHost(" << rvh << ") returned NULL.";
  }
}

VideoFrameRenderer::VideoFrameRenderer()
    : render_thread_("WebContentsVideo_RenderThread") {
  render_thread_.Start();
}

void VideoFrameRenderer::Render(int frame_number,
                                const SkBitmap& input,
                                const scoped_refptr<media::VideoFrame>& output,
                                const base::Callback<void(bool)>& done_cb) {
  render_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&VideoFrameRenderer::RenderOnRenderThread,
                 base::Unretained(this), frame_number, input, output, done_cb));
}

void VideoFrameRenderer::RenderOnRenderThread(
    int frame_number,
    const SkBitmap& input,
    const scoped_refptr<media::VideoFrame>& output,
    const base::Callback<void(bool)>& done_cb) {
  DCHECK_EQ(render_thread_.message_loop(), MessageLoop::current());

  TRACE_EVENT1("mirroring", "RenderFrame", "frame_number", frame_number);

  base::ScopedClosureRunner failure_handler(base::Bind(done_cb, false));

  SkAutoLockPixels locker(input);

  // Sanity-check the captured bitmap.
  if (input.empty() ||
      !input.readyToDraw() ||
      input.config() != SkBitmap::kARGB_8888_Config ||
      input.width() < 2 || input.height() < 2) {
    DVLOG(1) << "input unacceptable (size="
             << input.getSize()
             << ", ready=" << input.readyToDraw()
             << ", config=" << input.config() << ')';
    return;
  }

  // Sanity-check the output buffer.
  if (output->format() != media::VideoFrame::YV12) {
    NOTREACHED();
    return;
  }

  // Calculate the width and height of the content region in the |output|, based
  // on the aspect ratio of |input|.
  gfx::Rect region_in_frame = ComputeYV12LetterboxRegion(
      output->coded_size(), gfx::Size(input.width(), input.height()));

  // Scale the bitmap to the required size, if necessary.
  SkBitmap scaled_bitmap;
  if (input.width() != region_in_frame.width() ||
      input.height() != region_in_frame.height()) {
    scaled_bitmap = skia::ImageOperations::Resize(
        input, skia::ImageOperations::RESIZE_BOX,
        region_in_frame.width(), region_in_frame.height());
  } else {
    scaled_bitmap = input;
  }

  {
    TRACE_EVENT0("mirroring", "RGBToYUV");

    SkAutoLockPixels scaled_bitmap_locker(scaled_bitmap);

    media::CopyRGBToVideoFrame(
        reinterpret_cast<uint8*>(scaled_bitmap.getPixels()),
        scaled_bitmap.rowBytes(),
        region_in_frame,
        output);
  }

  // The result is now ready.
  failure_handler.Release();
  done_cb.Run(true);
}

SynchronizedConsumer::SynchronizedConsumer() : wrapped_consumer_(NULL) {}

void SynchronizedConsumer::SetConsumer(
    media::VideoCaptureDevice::EventHandler* consumer) {
  base::AutoLock guard(consumer_lock_);
  wrapped_consumer_ = consumer;
}

void SynchronizedConsumer::OnFrameInfo(
    const media::VideoCaptureCapability& info) {
  base::AutoLock guard(consumer_lock_);
  if (wrapped_consumer_) {
    wrapped_consumer_->OnFrameInfo(info);
  }
}

void SynchronizedConsumer::OnError() {
  base::AutoLock guard(consumer_lock_);
  if (wrapped_consumer_) {
    wrapped_consumer_->OnError();
  }
}

scoped_refptr<media::VideoFrame> SynchronizedConsumer::ReserveOutputBuffer() {
  base::AutoLock guard(consumer_lock_);
  if (wrapped_consumer_) {
    return wrapped_consumer_->ReserveOutputBuffer();
  } else {
    return NULL;
  }
}

void SynchronizedConsumer::OnIncomingCapturedVideoFrame(
    const scoped_refptr<media::VideoFrame>& video_frame,
    const base::Time& timestamp) {
  base::AutoLock guard(consumer_lock_);
  if (wrapped_consumer_) {
    wrapped_consumer_->OnIncomingCapturedVideoFrame(video_frame, timestamp);
  }
}

VideoFrameDeliveryLog::VideoFrameDeliveryLog()
    : last_frame_rate_log_time_(),
      count_frames_rendered_(0),
      last_frame_number_(0) {
}

void VideoFrameDeliveryLog::ChronicleFrameDelivery(int frame_number) {
  // Log frame rate, if verbose logging is turned on.
  static const base::TimeDelta kFrameRateLogInterval =
      base::TimeDelta::FromSeconds(10);
  const base::Time& now = base::Time::Now();
  if (last_frame_rate_log_time_.is_null()) {
    last_frame_rate_log_time_ = now;
    count_frames_rendered_ = 0;
    last_frame_number_ = frame_number;
  } else {
    ++count_frames_rendered_;
    const base::TimeDelta elapsed = now - last_frame_rate_log_time_;
    if (elapsed >= kFrameRateLogInterval) {
      const double measured_fps =
          count_frames_rendered_ / elapsed.InSecondsF();
      const int frames_elapsed = frame_number - last_frame_number_;
      const int count_frames_dropped = frames_elapsed - count_frames_rendered_;
      DCHECK_LE(0, count_frames_dropped);
      UMA_HISTOGRAM_PERCENTAGE(
          "TabCapture.FrameDropPercentage",
          (count_frames_dropped * 100 + frames_elapsed / 2) / frames_elapsed);
      UMA_HISTOGRAM_COUNTS(
          "TabCapture.FrameRate",
          static_cast<int>(measured_fps));
      VLOG(1) << "Current measured frame rate for CaptureMachine@" << this
              << " is " << measured_fps << " FPS.";
      last_frame_rate_log_time_ = now;
      count_frames_rendered_ = 0;
      last_frame_number_ = frame_number;
    }
  }
}

}  // namespace

// The "meat" of the video capture implementation, which is a ref-counted class.
// Separating this from the "shell class" WebContentsVideoCaptureDevice allows
// safe destruction without needing to block any threads (e.g., the IO
// BrowserThread).
//
// CaptureMachine manages a simple state machine and the pipeline (see notes at
// top of this file).  It times the start of successive captures and
// facilitates the processing of each through the stages of the pipeline.
class CaptureMachine
    : public base::RefCountedThreadSafe<CaptureMachine, CaptureMachine> {
 public:

  // |destroy_cb| will be invoked after CaptureMachine is fully destroyed,
  // to synchronize tear-down.
  CaptureMachine(int render_process_id,
                 int render_view_id,
                 const base::Closure& destroy_cb);

  // Synchronously sets/unsets the consumer.  Pass |consumer| as NULL to remove
  // the reference to the consumer; then, once this method returns,
  // CaptureMachine will no longer invoke callbacks on the old consumer from any
  // thread.
  void SetConsumer(media::VideoCaptureDevice::EventHandler* consumer);

  // Asynchronous requests to change CaptureMachine state.
  void Allocate(int width, int height, int frame_rate);
  void Start();
  void Stop();
  void DeAllocate();

 private:
  friend class base::RefCountedThreadSafe<CaptureMachine, CaptureMachine>;

  // Flag indicating current state.
  enum State {
    kIdle,
    kAllocated,
    kCapturing,
    kError,
    kDestroyed
  };

  virtual ~CaptureMachine();

  void TransitionStateTo(State next_state);

  // Stops capturing and notifies consumer_ of an error state.
  void Error();

  // Schedules the next frame capture off of the system clock, skipping frames
  // to catch-up if necessary.
  void ScheduleNextFrameCapture();

  // The glue between the pipeline stages.
  void StartSnapshot();
  void SnapshotComplete(int frame_number,
                        const scoped_refptr<media::VideoFrame>& target,
                        SnapshotResult result,
                        const base::Time& frame_timestamp,
                        const SkBitmap& bitmap);
  void RenderComplete(int frame_number,
                      const base::Time& capture_time,
                      const scoped_refptr<media::VideoFrame>& frame,
                      bool success);

  // Deliver |frame| to the video codec.
  void DeliverVideoFrame(int frame_number,
                         const scoped_refptr<media::VideoFrame>& frame,
                         const base::Time& frame_timestamp);

  // Indicate that capturing a frame failed for some reason. |frame|
  // is returned to the pool of buffers available for capture.
  void AbortVideoFrame(int frame_number,
                       const scoped_refptr<media::VideoFrame>& frame);

  void DoShutdownTasksOnUIThread();

  void TryResumeSuspendedCapture();

  // Specialized RefCounted traits for CaptureMachine, so that operator delete
  // is called from an "outside" thread.  See comments for "traits" in
  // base/memory/ref_counted.h.
  static void Destruct(const CaptureMachine* x);
  static void DeleteFromOutsideThread(const CaptureMachine* x);

  SynchronizedConsumer consumer_;  // Recipient of frames.

  // Used to ensure state machine transitions occur synchronously, and that
  // capturing executes at regular intervals.
  base::Thread manager_thread_;

  State state_;  // Current lifecycle state.
  media::VideoCaptureCapability settings_;  // Capture settings.
  base::Time next_start_capture_time_;  // When to start capturing next frame.
  int frame_number_;  // Counter of frames, including skipped frames.
  base::TimeDelta capture_period_;  // Time between frames.

  int snapshots_in_flight_;  // The number of captures enqueued in the snapshot
                             // phase of the pipeline.
  int frames_in_flight_;  // The number of captures enqueued in any stage
                          // of the pipeline.
  bool capture_suspended_;

  // The two pipeline stages.
  BackingStoreCopier copier_;
  VideoFrameRenderer renderer_;

  VideoFrameDeliveryLog delivery_log_;

  base::Closure destroy_cb_;  // Invoked once CaptureMachine is destroyed.

  DISALLOW_COPY_AND_ASSIGN(CaptureMachine);
};

CaptureMachine::CaptureMachine(int render_process_id,
                               int render_view_id,
                               const base::Closure& destroy_cb)
    : manager_thread_("WebContentsVideo_ManagerThread"),
      state_(kIdle),
      snapshots_in_flight_(0),
      frames_in_flight_(0),
      capture_suspended_(false),
      copier_(render_process_id, render_view_id),
      destroy_cb_(destroy_cb) {
  manager_thread_.Start();
}

void CaptureMachine::SetConsumer(
    media::VideoCaptureDevice::EventHandler* consumer) {
  consumer_.SetConsumer(consumer);
}

void CaptureMachine::Allocate(int width, int height, int frame_rate) {
  ENSURE_INVOKED_ON_THREAD(manager_thread_,
                           &CaptureMachine::Allocate, this,
                           width, height, frame_rate);

  if (state_ != kIdle) {
    DVLOG(1) << "Allocate() invoked when not in state Idle.";
    return;
  }

  if (frame_rate <= 0) {
    DVLOG(1) << "invalid frame_rate: " << frame_rate;
    Error();
    return;
  }

  // Frame dimensions must each be a positive, even integer, since the consumer
  // wants (or will convert to) YUV420.
  width = MakeEven(width);
  height = MakeEven(height);
  if (width < kMinFrameWidth || height < kMinFrameHeight) {
    DVLOG(1) << "invalid width (" << width << ") and/or height ("
             << height << ")";
    Error();
    return;
  }

  settings_.width = width;
  settings_.height = height;
  settings_.frame_rate = frame_rate;
  // Sets the color format used by OnIncomingCapturedFrame().
  // Does not apply to OnIncomingCapturedVideoFrame().
  settings_.color = media::VideoCaptureCapability::kARGB;
  settings_.expected_capture_delay = 0;
  settings_.interlaced = false;

  capture_period_ = base::TimeDelta::FromMicroseconds(
      1000000.0 / settings_.frame_rate + 0.5);

  consumer_.OnFrameInfo(settings_);

  TransitionStateTo(kAllocated);
}

void CaptureMachine::Start() {
  ENSURE_INVOKED_ON_THREAD(manager_thread_, &CaptureMachine::Start, this);

  if (state_ != kAllocated) {
    return;
  }

  TransitionStateTo(kCapturing);

  next_start_capture_time_ = base::Time::Now();
  frame_number_ = 0;
  ScheduleNextFrameCapture();
}

void CaptureMachine::Stop() {
  ENSURE_INVOKED_ON_THREAD(manager_thread_, &CaptureMachine::Stop, this);

  if (state_ != kCapturing) {
    return;
  }

  TransitionStateTo(kAllocated);
}

void CaptureMachine::DeAllocate() {
  ENSURE_INVOKED_ON_THREAD(manager_thread_,
                           &CaptureMachine::DeAllocate, this);

  if (state_ == kCapturing) {
    Stop();
  }
  if (state_ == kAllocated) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&CaptureMachine::DoShutdownTasksOnUIThread, this));

    TransitionStateTo(kIdle);
  }
}

CaptureMachine::~CaptureMachine() {
  DVLOG(1) << "CaptureMachine@" << this << " destroying.";
  state_ = kDestroyed;
  // Note: Implicit destructors will be called after this, which will block the
  // current thread while joining on the other threads.  However, this should be
  // instantaneous since the other threads' task queues *must* be empty at this
  // point (because CaptureMachine's ref-count is zero).
}

// static
void CaptureMachine::Destruct(const CaptureMachine* x) {
  // The current thread is very likely to be one owned by CaptureMachine.  When
  // ~CaptureMachine() is called, it will attempt to join with the
  // CaptureMachine-owned threads, including itself.  Since it's illegal for a
  // thread to join with itself, we need to trampoline the destructor call to
  // another thread.
  BrowserThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(&DeleteFromOutsideThread, x));
}

// static
void CaptureMachine::DeleteFromOutsideThread(const CaptureMachine* x) {
  const base::Closure run_after_delete = x->destroy_cb_;
  // Note: Thread joins are about to happen here (in ~CaptureThread()).
  delete x;
  if (!run_after_delete.is_null())
    run_after_delete.Run();
}

void CaptureMachine::TransitionStateTo(State next_state) {
  DCHECK_EQ(manager_thread_.message_loop(), MessageLoop::current());

#ifndef NDEBUG
  static const char* kStateNames[] = {
    "Idle", "Allocated", "Capturing", "Error", "Destroyed"
  };
  DVLOG(1) << "State change: " << kStateNames[state_]
           << " --> " << kStateNames[next_state];
#endif

  state_ = next_state;
}

void CaptureMachine::Error() {
  DCHECK_EQ(manager_thread_.message_loop(), MessageLoop::current());

  if (state_ == kCapturing) {
    Stop();
  }
  TransitionStateTo(kError);

  consumer_.OnError();
}

void CaptureMachine::ScheduleNextFrameCapture() {
  DCHECK_EQ(manager_thread_.message_loop(), MessageLoop::current());

  if (state_ != kCapturing) {
    return;
  }

  DCHECK_LT(0, settings_.frame_rate);
  next_start_capture_time_ += capture_period_;
  ++frame_number_;
  const base::Time& now = base::Time::Now();
  if (next_start_capture_time_ < now) {
    // One or more frame captures were missed.  Skip ahead.
    const base::TimeDelta& behind_by = now - next_start_capture_time_;
    const int64 num_frames_missed = (behind_by / capture_period_) + 1;
    VLOG(1) << "Ran behind by " << num_frames_missed << " frames.";
    next_start_capture_time_ += capture_period_ * num_frames_missed;
    frame_number_ += num_frames_missed;
  } else if (now + capture_period_ < next_start_capture_time_) {
    // Note: This should only happen if the system clock has been reset
    // backwards in time.
    VLOG(1) << "Resetting next capture start time due to clock skew.";
    next_start_capture_time_ = now + capture_period_;
  }

  manager_thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CaptureMachine::StartSnapshot, this),
      next_start_capture_time_ - now);
}

void CaptureMachine::StartSnapshot() {
  DCHECK_EQ(manager_thread_.message_loop(), MessageLoop::current());

  capture_suspended_ = false;

  if (state_ != kCapturing) {
    return;
  }

  if (frames_in_flight_ < kMaxFramesInFlight &&
      snapshots_in_flight_ < kMaxSnapshotsInFlight) {

    scoped_refptr<media::VideoFrame> frame = consumer_.ReserveOutputBuffer();

    if (frame) {
      frames_in_flight_++;
      snapshots_in_flight_++;

      // Initiate a StartCopy() operation (on the UI thread) that will report
      // back to SnapshotComplete() (on the current thread) when done.
      BackingStoreCopier::CopyFinishedCallback snapshot_complete =
          media::BindToLoop(manager_thread_.message_loop_proxy(),
              base::Bind(&CaptureMachine::SnapshotComplete, this, frame_number_,
                         frame));

      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&BackingStoreCopier::StartCopy, base::Unretained(&copier_),
                     frame_number_, frame, snapshot_complete));
    }
    ScheduleNextFrameCapture();
  } else {
    capture_suspended_ = true;
  }
}

void CaptureMachine::TryResumeSuspendedCapture() {
  DCHECK_EQ(manager_thread_.message_loop(), MessageLoop::current());
  if (capture_suspended_) {
    // Set capture_suspended_ to false here to ensure there is never more than
    // one StartSnapshot operation scheduled to run (it's safe to call this
    // function twice in a row). Use PostTask here instead of simply calling
    // StartSnapshot, so that any VideoFrames that are about to fall out of
    // scope to be returned to the pool of usable buffers.
    capture_suspended_ = false;
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&CaptureMachine::StartSnapshot, this));
  }
}

void CaptureMachine::SnapshotComplete(
    int frame_number,
    const scoped_refptr<media::VideoFrame>& target,
    SnapshotResult result,
    const base::Time& frame_timestamp,
    const SkBitmap& bitmap) {
  DCHECK_EQ(manager_thread_.message_loop(), MessageLoop::current());

  snapshots_in_flight_--;
  TryResumeSuspendedCapture();

  if (state_ != kCapturing) {
    AbortVideoFrame(frame_number, target);
    return;
  }

  switch (result) {
    case NO_SOURCE:
      AbortVideoFrame(frame_number, target);
      Error();
      break;

    case TRANSIENT_ERROR:
      AbortVideoFrame(frame_number, target);
      break;

    case COPIED_TO_BITMAP:
      // |bitmap| contains the copy result, but we must still draw it into
      // |target|. The Renderer will do that.
      renderer_.Render(
          frame_number, bitmap, target,
          media::BindToLoop(manager_thread_.message_loop_proxy(),
                            base::Bind(&CaptureMachine::RenderComplete, this,
                                       frame_number, frame_timestamp, target)));
      break;

    case COPIED_TO_VIDEO_FRAME:
      // Capture is done; deliver the result.
      DeliverVideoFrame(frame_number, target, frame_timestamp);
      break;
  }
}

void CaptureMachine::RenderComplete(
    int frame_number,
    const base::Time& frame_timestamp,
    const scoped_refptr<media::VideoFrame>& frame,
    bool success) {
  DCHECK_EQ(manager_thread_.message_loop(), MessageLoop::current());

  TryResumeSuspendedCapture();

  if (state_ != kCapturing || !success) {
    AbortVideoFrame(frame_number, frame);
  } else {
    DCHECK(!frame_timestamp.is_null());
    DeliverVideoFrame(frame_number, frame, frame_timestamp);
  }
}

void CaptureMachine::DeliverVideoFrame(
    int frame_number,
    const scoped_refptr<media::VideoFrame>& frame,
    const base::Time& frame_timestamp) {
  DCHECK_EQ(manager_thread_.message_loop(), MessageLoop::current());
  TRACE_EVENT1("mirroring", "DeliverVideoFrame", "frame_number", frame_number);

  // Send the frame to the consumer.
  consumer_.OnIncomingCapturedVideoFrame(frame, frame_timestamp);

  // Log some statistics.
  delivery_log_.ChronicleFrameDelivery(frame_number);

  // Turn the crank on our capture machine.
  DCHECK_GE(frames_in_flight_, 1);
  DCHECK_LE(frames_in_flight_, kMaxFramesInFlight);
  frames_in_flight_--;
  TryResumeSuspendedCapture();
}

void CaptureMachine::AbortVideoFrame(
    int frame_number,
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK_EQ(manager_thread_.message_loop(), MessageLoop::current());
  TRACE_EVENT1("mirroring", "AbortVideoFrame", "frame_number", frame_number);

  DCHECK_GE(frames_in_flight_, 1);
  DCHECK_LE(frames_in_flight_, kMaxFramesInFlight);
  frames_in_flight_--;
  TryResumeSuspendedCapture();
}

void CaptureMachine::DoShutdownTasksOnUIThread() {
  copier_.StopObservingWebContents();
}

void BackingStoreCopier::StartCopy(
    int frame_number,
    const scoped_refptr<media::VideoFrame>& target,
    const CopyFinishedCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  gfx::Size video_size = target->coded_size();

  if (!web_contents()) {  // No source yet.
    LookUpAndObserveWebContents();
    if (!web_contents()) {  // No source ever.
      callback.Run(NO_SOURCE, base::Time(), SkBitmap());
      return;
    }
  }

  RenderWidgetHost* rwh;
  if (fullscreen_widget_id_ != MSG_ROUTING_NONE) {
    RenderProcessHost* process = web_contents()->GetRenderProcessHost();
    rwh = process ? process->GetRenderWidgetHostByID(fullscreen_widget_id_)
                  : NULL;
  } else {
    rwh = web_contents()->GetRenderViewHost();
  }

  if (!rwh) {
    // Transient failure state (e.g., a RenderView is being replaced).
    callback.Run(TRANSIENT_ERROR, base::Time(), SkBitmap());
    return;
  }

  RenderWidgetHostViewPort* view =
      RenderWidgetHostViewPort::FromRWHV(rwh->GetView());

  gfx::Size fitted_size;
  if (view) {
    gfx::Size view_size = view->GetViewBounds().size();
    if (!view_size.IsEmpty()) {
      fitted_size = ComputeYV12LetterboxRegion(video_size, view_size).size();
    }
    if (view_size != last_view_size_) {
      last_view_size_ = view_size;

      // Measure the number of kilopixels.
      UMA_HISTOGRAM_COUNTS_10000(
          "TabCapture.ViewChangeKiloPixels",
          view_size.width() * view_size.height() / 1024);
    }
  }

  TRACE_EVENT_ASYNC_BEGIN1("mirroring", "Capture", this,
                           "frame_number", frame_number);

  if (view && !view->IsSurfaceAvailableForCopy()) {
    // Fallback to the more expensive renderer-side copy if the surface and
    // backing store are not accessible.
    rwh->GetSnapshotFromRenderer(
        gfx::Rect(),
        base::Bind(&BackingStoreCopier::DidCopyFromBackingStore,
                   base::Unretained(this), frame_number,
                   base::Time::Now(), callback));
  } else if (view && view->CanCopyToVideoFrame()) {
    gfx::Size view_size = view->GetViewBounds().size();

    view->CopyFromCompositingSurfaceToVideoFrame(
        gfx::Rect(view_size),
        target,
        base::Bind(
            &BackingStoreCopier::DidCopyFromCompositingSurfaceToVideoFrame,
            base::Unretained(this), frame_number,
            base::Time::Now(), callback));
  } else {
    rwh->CopyFromBackingStore(
        gfx::Rect(),
        fitted_size,  // Size here is a request not always honored.
        base::Bind(&BackingStoreCopier::DidCopyFromBackingStore,
                   base::Unretained(this), frame_number,
                   base::Time::Now(), callback));
  }
}

void BackingStoreCopier::DidCopyFromBackingStore(
    int frame_number,
    const base::Time& start_time,
    const CopyFinishedCallback& callback,
    bool success,
    const SkBitmap& bitmap) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Note: No restriction on which thread invokes this method but, currently,
  // it's always the UI BrowserThread.
  TRACE_EVENT_ASYNC_END1("mirroring", "Capture", this,
                         "frame_number", frame_number);

  if (success) {
    base::Time now = base::Time::Now();
    UMA_HISTOGRAM_TIMES("TabCapture.CopyTimeBitmap", now - start_time);
    callback.Run(COPIED_TO_BITMAP, now, bitmap);
  } else {
    // Capture can fail due to transient issues, so just skip this frame.
    DVLOG(1) << "CopyFromBackingStore failed; skipping frame.";
    callback.Run(TRANSIENT_ERROR, base::Time(), SkBitmap());
  }
}

void BackingStoreCopier::DidCopyFromCompositingSurfaceToVideoFrame(
    int frame_number,
    const base::Time& start_time,
    const CopyFinishedCallback& callback,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Note: No restriction on which thread invokes this method but, currently,
  // it's always the UI BrowserThread.
  TRACE_EVENT_ASYNC_END1("mirroring", "Capture", this,
                         "frame_number", frame_number);

  if (success) {
    base::Time now = base::Time::Now();
    UMA_HISTOGRAM_TIMES("TabCapture.CopyTimeVideoFrame", now - start_time);
    callback.Run(COPIED_TO_VIDEO_FRAME, now, SkBitmap());
  } else {
    // Capture can fail due to transient issues, so just skip this frame.
    DVLOG(1) << "CopyFromCompositingSurface failed; skipping frame.";
    callback.Run(TRANSIENT_ERROR, base::Time(), SkBitmap());
  }
}

WebContentsVideoCaptureDevice::WebContentsVideoCaptureDevice(
    const media::VideoCaptureDevice::Name& name,
    int render_process_id,
    int render_view_id,
    const base::Closure& destroy_cb)
    : device_name_(name),
      capturer_(new CaptureMachine(render_process_id, render_view_id,
                                   destroy_cb)) {}

WebContentsVideoCaptureDevice::~WebContentsVideoCaptureDevice() {
  DVLOG(2) << "WebContentsVideoCaptureDevice@" << this << " destroying.";
}

// static
media::VideoCaptureDevice* WebContentsVideoCaptureDevice::Create(
    const std::string& device_id,
    const base::Closure& destroy_cb) {
  // Parse device_id into render_process_id and render_view_id.
  int render_process_id = -1;
  int render_view_id = -1;
  if (!WebContentsCaptureUtil::ExtractTabCaptureTarget(device_id,
                                                       &render_process_id,
                                                       &render_view_id))
    return NULL;

  media::VideoCaptureDevice::Name name;
  base::SStringPrintf(&name.device_name,
                      "WebContents[%.*s]",
                      static_cast<int>(device_id.size()), device_id.data());
  name.unique_id = device_id;

  return new WebContentsVideoCaptureDevice(
      name, render_process_id, render_view_id, destroy_cb);
}

void WebContentsVideoCaptureDevice::Allocate(
    int width, int height, int frame_rate,
    VideoCaptureDevice::EventHandler* consumer) {
  DCHECK(capturer_);
  capturer_->SetConsumer(consumer);
  capturer_->Allocate(width, height, frame_rate);
}

void WebContentsVideoCaptureDevice::Start() {
  DCHECK(capturer_);
  capturer_->Start();
}

void WebContentsVideoCaptureDevice::Stop() {
  DCHECK(capturer_);
  capturer_->Stop();
}

void WebContentsVideoCaptureDevice::DeAllocate() {
  DCHECK(capturer_);
  capturer_->SetConsumer(NULL);
  capturer_->DeAllocate();
}

const media::VideoCaptureDevice::Name&
WebContentsVideoCaptureDevice::device_name() {
  return device_name_;
}

}  // namespace content
