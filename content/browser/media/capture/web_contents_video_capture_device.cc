// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_video_capture_device.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/media/capture/cursor_renderer.h"
#include "content/browser/media/capture/web_contents_tracker.h"
#include "content/browser/media/capture/window_activity_tracker.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_frame_subscriber.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "media/base/video_frame_metadata.h"
#include "media/capture/content/screen_capture_device_core.h"
#include "media/capture/content/thread_safe_capture_oracle.h"
#include "media/capture/content/video_capture_oracle.h"
#include "media/capture/video_capture_types.h"
#include "ui/base/layout.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace content {

namespace {

// Minimum amount of time for which there should be no animation detected
// to consider interactive mode being active. This is to prevent very brief
// periods of animated content not being detected (due to CPU fluctations for
// example) from causing a flip-flop on interactive mode.
constexpr int64_t kMinPeriodNoAnimationMillis = 3000;

// FrameSubscriber is a proxy to the ThreadSafeCaptureOracle that's compatible
// with RenderWidgetHostViewFrameSubscriber. One is created per event type.
class FrameSubscriber : public RenderWidgetHostViewFrameSubscriber {
 public:
  FrameSubscriber(media::VideoCaptureOracle::Event event_type,
                  scoped_refptr<media::ThreadSafeCaptureOracle> oracle,
                  base::WeakPtr<content::CursorRenderer> cursor_renderer,
                  base::WeakPtr<content::WindowActivityTracker> tracker)
      : event_type_(event_type),
        oracle_proxy_(std::move(oracle)),
        cursor_renderer_(cursor_renderer),
        window_activity_tracker_(tracker),
        source_id_for_copy_request_(base::UnguessableToken::Create()),
        weak_ptr_factory_(this) {}

  static void DidCaptureFrame(
      base::WeakPtr<FrameSubscriber> frame_subscriber_,
      const media::ThreadSafeCaptureOracle::CaptureFrameCallback&
          capture_frame_cb,
      scoped_refptr<media::VideoFrame> frame,
      base::TimeTicks timestamp,
      const gfx::Rect& region_in_frame,
      bool success);

  bool IsUserInteractingWithContent();

  // RenderWidgetHostViewFrameSubscriber implementation:
  bool ShouldCaptureFrame(
      const gfx::Rect& damage_rect,
      base::TimeTicks present_time,
      scoped_refptr<media::VideoFrame>* storage,
      RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback*
          deliver_frame_cb) override;
  const base::UnguessableToken& GetSourceIdForCopyRequest() override;

 private:
  const media::VideoCaptureOracle::Event event_type_;
  const scoped_refptr<media::ThreadSafeCaptureOracle> oracle_proxy_;
  // We need a weak pointer since FrameSubscriber is owned externally and
  // may outlive the cursor renderer.
  const base::WeakPtr<CursorRenderer> cursor_renderer_;
  // We need a weak pointer since FrameSubscriber is owned externally and
  // may outlive the ui activity tracker.
  const base::WeakPtr<WindowActivityTracker> window_activity_tracker_;
  base::UnguessableToken source_id_for_copy_request_;
  base::WeakPtrFactory<FrameSubscriber> weak_ptr_factory_;
};

// ContentCaptureSubscription is the relationship between a RenderWidgetHostView
// whose content is updating, a subscriber that is deciding which of these
// updates to capture (and where to deliver them to), and a callback that
// knows how to do the capture and prepare the result for delivery.
//
// In practice, this means (a) installing a RenderWidgetHostFrameSubscriber in
// the RenderWidgetHostView, to process compositor updates, and (b) occasionally
// using the capture callback to force additional captures that are needed for
// things like mouse cursor rendering updates and/or refresh frames.
//
// All of this happens on the UI thread, although the
// RenderWidgetHostViewFrameSubscriber we install may be dispatching updates
// autonomously on some other thread.
class ContentCaptureSubscription {
 public:
  using CaptureCallback = base::Callback<void(
      base::TimeTicks,
      scoped_refptr<media::VideoFrame>,
      const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&)>;

  // Create a subscription. Whenever a non-compositor capture is required, the
  // subscription will invoke |capture_callback| on the UI thread to do the
  // work.
  ContentCaptureSubscription(
      base::WeakPtr<RenderWidgetHostViewBase> source_view,
      scoped_refptr<media::ThreadSafeCaptureOracle> oracle_proxy,
      const CaptureCallback& capture_callback);
  ~ContentCaptureSubscription();

  // Called when a condition occurs that requires a refresh frame be
  // captured. The subscribers will determine whether a frame was recently
  // captured, or if a non-compositor initiated capture must be made.
  void MaybeCaptureForRefresh();

 private:
  // Called for active frame refresh requests, or mouse activity events.
  void OnEvent(FrameSubscriber* subscriber);

  const base::WeakPtr<RenderWidgetHostViewBase> source_view_;

  std::unique_ptr<FrameSubscriber> refresh_subscriber_;
  std::unique_ptr<FrameSubscriber> mouse_activity_subscriber_;
  CaptureCallback capture_callback_;

  // Responsible for tracking the cursor state and input events to make
  // decisions and then render the mouse cursor on the video frame after
  // capture is completed.
  std::unique_ptr<content::CursorRenderer> cursor_renderer_;

  // Responsible for tracking the UI events and making a decision on whether
  // user is actively interacting with content.
  std::unique_ptr<content::WindowActivityTracker> window_activity_tracker_;

  DISALLOW_COPY_AND_ASSIGN(ContentCaptureSubscription);
};

// Renews capture subscriptions based on feedback from WebContentsTracker, and
// also executes non-compositor-initiated frame captures.
class WebContentsCaptureMachine : public media::VideoCaptureMachine {
 public:
  WebContentsCaptureMachine(int render_process_id,
                            int main_render_frame_id,
                            bool enable_auto_throttling);
  ~WebContentsCaptureMachine() override;

  // VideoCaptureMachine overrides.
  void Start(const scoped_refptr<media::ThreadSafeCaptureOracle>& oracle_proxy,
             const media::VideoCaptureParams& params,
             const base::Callback<void(bool)> callback) override;
  void Suspend() override;
  void Resume() override;
  void Stop(const base::Closure& callback) override;
  bool IsAutoThrottlingEnabled() const override {
    return auto_throttling_enabled_;
  }
  void MaybeCaptureForRefresh() override;

 private:
  // Called for cases where events other than compositor updates require a frame
  // capture from the composited surface. Must be run on the UI BrowserThread.
  // |deliver_frame_cb| will be run when the operation completes. The copy will
  // populate |target|.
  //
  // This method is only used by a ContentCaptureSubscription::CaptureCallback.
  void Capture(base::TimeTicks start_time,
               scoped_refptr<media::VideoFrame> target,
               const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
                   deliver_frame_cb);

  bool InternalStart(scoped_refptr<media::ThreadSafeCaptureOracle> oracle_proxy,
                     const media::VideoCaptureParams& params);
  void InternalSuspend();
  void InternalResume();
  void InternalStop(const base::Closure& callback);
  void InternalMaybeCaptureForRefresh();
  bool IsStarted() const;

  // Computes the preferred size of the target RenderWidget for optimal capture.
  gfx::Size ComputeOptimalViewSize() const;

  // Response callback for RWHV::CopyFromSurfaceToVideoFrame().
  void DidCopyToVideoFrame(
      const base::TimeTicks& start_time,
      const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
          deliver_frame_cb,
      const gfx::Rect& region_in_frame,
      bool success);

  // Remove the old subscription, and attempt to start a new one. If
  // |is_still_tracking| is false, emit an error rather than attempt to start a
  // new subscription.
  void RenewFrameSubscription(bool is_still_tracking);

  // Called whenever the render widget is resized.
  void UpdateCaptureSize();

  // Parameters saved in constructor.
  const int initial_render_process_id_;
  const int initial_main_render_frame_id_;

  // Tracks events and calls back to RenewFrameSubscription() to maintain
  // capture on the correct RenderWidgetHostView.
  const scoped_refptr<WebContentsTracker> tracker_;

  // Set to false to prevent the capture size from automatically adjusting in
  // response to end-to-end utilization.  This is enabled via the throttling
  // option in the WebContentsVideoCaptureDevice device ID.
  const bool auto_throttling_enabled_;

  // Makes all the decisions about which frames to copy, and how.
  scoped_refptr<media::ThreadSafeCaptureOracle> oracle_proxy_;

  // Video capture parameters that this machine is started with.
  media::VideoCaptureParams capture_params_;

  // Responsible for forwarding events from the active RenderWidgetHost to the
  // oracle, and initiating captures accordingly.
  std::unique_ptr<ContentCaptureSubscription> subscription_;

  // False while frame capture has been suspended. This prevents subscriptions
  // from being created by RenewFrameSubscription() until frame capture is
  // resumed.
  bool frame_capture_active_;

  // Weak pointer factory used to invalidate callbacks.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<WebContentsCaptureMachine> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsCaptureMachine);
};

void FrameSubscriber::DidCaptureFrame(
    base::WeakPtr<FrameSubscriber> frame_subscriber_,
    const media::ThreadSafeCaptureOracle::CaptureFrameCallback&
        capture_frame_cb,
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks timestamp,
    const gfx::Rect& region_in_frame,
    bool success) {
  if (success) {
    // We can get a callback in the shutdown sequence for the browser main loop
    // and this can result in a DCHECK failure. Avoid this by doing DCHECK only
    // on success.
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (frame_subscriber_ && frame_subscriber_->cursor_renderer_) {
      CursorRenderer* cursor_renderer =
          frame_subscriber_->cursor_renderer_.get();
      if (cursor_renderer->SnapshotCursorState(region_in_frame))
        cursor_renderer->RenderOnVideoFrame(frame.get());
      frame->metadata()->SetBoolean(
          media::VideoFrameMetadata::INTERACTIVE_CONTENT,
          frame_subscriber_->IsUserInteractingWithContent());
    }
  }
  capture_frame_cb.Run(std::move(frame), timestamp, success);
}

bool FrameSubscriber::IsUserInteractingWithContent() {
  bool ui_activity = window_activity_tracker_ &&
                     window_activity_tracker_->IsUiInteractionActive();
  bool interactive_mode = false;
  if (cursor_renderer_) {
    bool animation_active =
        (base::TimeTicks::Now() -
         oracle_proxy_->last_time_animation_was_detected()) <
        base::TimeDelta::FromMilliseconds(kMinPeriodNoAnimationMillis);
    if (ui_activity && !animation_active) {
      interactive_mode = true;
    } else if (animation_active && window_activity_tracker_) {
      window_activity_tracker_->Reset();
    }
  }
  return interactive_mode;
}

bool FrameSubscriber::ShouldCaptureFrame(
    const gfx::Rect& damage_rect,
    base::TimeTicks present_time,
    scoped_refptr<media::VideoFrame>* storage,
    DeliverFrameCallback* deliver_frame_cb) {
  TRACE_EVENT1("gpu.capture", "FrameSubscriber::ShouldCaptureFrame", "instance",
               this);

  media::ThreadSafeCaptureOracle::CaptureFrameCallback capture_frame_cb;
  if (!oracle_proxy_->ObserveEventAndDecideCapture(
          event_type_, damage_rect, present_time, storage, &capture_frame_cb)) {
    return false;
  }

  DCHECK(*storage);
  DCHECK(!capture_frame_cb.is_null());
  *deliver_frame_cb =
      base::Bind(&FrameSubscriber::DidCaptureFrame,
                 weak_ptr_factory_.GetWeakPtr(), capture_frame_cb, *storage);
  return true;
}

const base::UnguessableToken& FrameSubscriber::GetSourceIdForCopyRequest() {
  return source_id_for_copy_request_;
}

ContentCaptureSubscription::ContentCaptureSubscription(
    base::WeakPtr<RenderWidgetHostViewBase> source_view,
    scoped_refptr<media::ThreadSafeCaptureOracle> oracle_proxy,
    const CaptureCallback& capture_callback)
    : source_view_(source_view), capture_callback_(capture_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(source_view_);

#if defined(USE_AURA) || defined(OS_MACOSX)
  cursor_renderer_ = CursorRenderer::Create(source_view_->GetNativeView());
  window_activity_tracker_ =
      WindowActivityTracker::Create(source_view_->GetNativeView());
#endif
  // Subscriptions for refresh frames and mouse cursor updates. When events
  // outside of the normal content change/compositing workflow occur, these
  // decide whether extra frames need to be captured. Such extra frame captures
  // are initiated by running the |capture_callback_|.
  refresh_subscriber_.reset(new FrameSubscriber(
      media::VideoCaptureOracle::kActiveRefreshRequest, oracle_proxy,
      cursor_renderer_ ? cursor_renderer_->GetWeakPtr()
                       : base::WeakPtr<CursorRenderer>(),
      window_activity_tracker_ ? window_activity_tracker_->GetWeakPtr()
                               : base::WeakPtr<WindowActivityTracker>()));
  mouse_activity_subscriber_.reset(new FrameSubscriber(
      media::VideoCaptureOracle::kMouseCursorUpdate, oracle_proxy,
      cursor_renderer_ ? cursor_renderer_->GetWeakPtr()
                       : base::WeakPtr<CursorRenderer>(),
      window_activity_tracker_ ? window_activity_tracker_->GetWeakPtr()
                               : base::WeakPtr<WindowActivityTracker>()));

  // Main capture path: Subscribing to compositor updates. This means that any
  // time the tab content has changed and compositing has taken place, the
  // RenderWidgetHostView will consult the FrameSubscriber (which consults the
  // oracle) to determine whether a frame should be captured. If so, the
  // RenderWidgetHostView will initiate the frame capture and NOT the
  // |capture_callback_|.
  std::unique_ptr<RenderWidgetHostViewFrameSubscriber> subscriber(
      new FrameSubscriber(
          media::VideoCaptureOracle::kCompositorUpdate, oracle_proxy,
          cursor_renderer_ ? cursor_renderer_->GetWeakPtr()
                           : base::WeakPtr<CursorRenderer>(),
          window_activity_tracker_ ? window_activity_tracker_->GetWeakPtr()
                                   : base::WeakPtr<WindowActivityTracker>()));
  source_view_->BeginFrameSubscription(std::move(subscriber));

  // Begin monitoring for user activity that is used to signal "interactive
  // content."
  if (window_activity_tracker_) {
    window_activity_tracker_->RegisterMouseInteractionObserver(
        base::Bind(&ContentCaptureSubscription::OnEvent, base::Unretained(this),
                   mouse_activity_subscriber_.get()));
  }
}

ContentCaptureSubscription::~ContentCaptureSubscription() {
  // If the BrowserThreads have been torn down, then the browser is in the final
  // stages of exiting and it is dangerous to take any further action.  We must
  // return early.  http://crbug.com/396413
  if (!BrowserThread::IsMessageLoopValid(BrowserThread::UI))
    return;

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If the RWHV weak pointer is still valid, make sure the view is still
  // associated with a RWH before attempting to end the frame subscription. This
  // is because a null RWH indicates the RWHV has had its Destroy() method
  // called, which makes it invalid to call any of its methods that assume the
  // compositor is present.
  if (source_view_ && source_view_->GetRenderWidgetHost())
    source_view_->EndFrameSubscription();
}

void ContentCaptureSubscription::MaybeCaptureForRefresh() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  OnEvent(refresh_subscriber_.get());
}

void ContentCaptureSubscription::OnEvent(FrameSubscriber* subscriber) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT0("gpu.capture", "ContentCaptureSubscription::OnEvent");

  scoped_refptr<media::VideoFrame> frame;
  RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback deliver_frame_cb;

  const base::TimeTicks start_time = base::TimeTicks::Now();
  DCHECK(subscriber == refresh_subscriber_.get() ||
         subscriber == mouse_activity_subscriber_.get());
  if (subscriber->ShouldCaptureFrame(gfx::Rect(), start_time, &frame,
                                     &deliver_frame_cb)) {
    capture_callback_.Run(start_time, frame, deliver_frame_cb);
  }
}

WebContentsCaptureMachine::WebContentsCaptureMachine(
    int render_process_id,
    int main_render_frame_id,
    bool enable_auto_throttling)
    : initial_render_process_id_(render_process_id),
      initial_main_render_frame_id_(main_render_frame_id),
      tracker_(new WebContentsTracker(true)),
      auto_throttling_enabled_(enable_auto_throttling),
      frame_capture_active_(true),
      weak_ptr_factory_(this) {
  DVLOG(1) << "Created WebContentsCaptureMachine for " << render_process_id
           << ':' << main_render_frame_id
           << (auto_throttling_enabled_ ? " with auto-throttling enabled" : "");
}

WebContentsCaptureMachine::~WebContentsCaptureMachine() {}

bool WebContentsCaptureMachine::IsStarted() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return weak_ptr_factory_.HasWeakPtrs();
}

void WebContentsCaptureMachine::Start(
    const scoped_refptr<media::ThreadSafeCaptureOracle>& oracle_proxy,
    const media::VideoCaptureParams& params,
    const base::Callback<void(bool)> callback) {
  // Starts the capture machine asynchronously.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WebContentsCaptureMachine::InternalStart,
                 base::Unretained(this), oracle_proxy, params),
      callback);
}

bool WebContentsCaptureMachine::InternalStart(
    scoped_refptr<media::ThreadSafeCaptureOracle> oracle_proxy,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!IsStarted());

  DCHECK(oracle_proxy);
  oracle_proxy_ = std::move(oracle_proxy);
  capture_params_ = params;

  // Note: Creation of the first WeakPtr in the following statement will cause
  // IsStarted() to return true from now on.
  tracker_->SetResizeChangeCallback(
      base::Bind(&WebContentsCaptureMachine::UpdateCaptureSize,
                 weak_ptr_factory_.GetWeakPtr()));
  tracker_->Start(initial_render_process_id_, initial_main_render_frame_id_,
                  base::Bind(&WebContentsCaptureMachine::RenewFrameSubscription,
                             weak_ptr_factory_.GetWeakPtr()));
  if (WebContents* contents = tracker_->web_contents())
    contents->IncrementCapturerCount(ComputeOptimalViewSize());

  return true;
}

void WebContentsCaptureMachine::Suspend() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WebContentsCaptureMachine::InternalSuspend,
                 base::Unretained(this)));
}

void WebContentsCaptureMachine::InternalSuspend() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!frame_capture_active_)
    return;
  frame_capture_active_ = false;
  if (IsStarted())
    RenewFrameSubscription(tracker_->is_still_tracking());
}

void WebContentsCaptureMachine::Resume() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&WebContentsCaptureMachine::InternalResume,
                                     base::Unretained(this)));
}

void WebContentsCaptureMachine::InternalResume() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (frame_capture_active_)
    return;
  frame_capture_active_ = true;
  if (IsStarted())
    RenewFrameSubscription(tracker_->is_still_tracking());
}

void WebContentsCaptureMachine::Stop(const base::Closure& callback) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&WebContentsCaptureMachine::InternalStop,
                                     base::Unretained(this), callback));
}

void WebContentsCaptureMachine::InternalStop(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::ScopedClosureRunner done_runner(callback);

  if (!IsStarted())
    return;

  // The following cancels any outstanding callbacks and causes IsStarted() to
  // return false from here onward.
  weak_ptr_factory_.InvalidateWeakPtrs();

  RenewFrameSubscription(false);
  if (WebContents* contents = tracker_->web_contents())
    contents->DecrementCapturerCount();
  tracker_->Stop();
}

void WebContentsCaptureMachine::MaybeCaptureForRefresh() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WebContentsCaptureMachine::InternalMaybeCaptureForRefresh,
                 // Use of Unretained() is safe here since this task must run
                 // before InternalStop().
                 base::Unretained(this)));
}

void WebContentsCaptureMachine::InternalMaybeCaptureForRefresh() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (IsStarted() && subscription_)
    subscription_->MaybeCaptureForRefresh();
}

void WebContentsCaptureMachine::Capture(
    base::TimeTicks start_time,
    scoped_refptr<media::VideoFrame> target,
    const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
        deliver_frame_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderWidgetHostView* const view = tracker_->GetTargetView();
  if (!view)
    return;
  view->CopyFromSurfaceToVideoFrame(
      gfx::Rect(), std::move(target),
      base::Bind(&WebContentsCaptureMachine::DidCopyToVideoFrame,
                 weak_ptr_factory_.GetWeakPtr(), start_time, deliver_frame_cb));
}

gfx::Size WebContentsCaptureMachine::ComputeOptimalViewSize() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(miu): Propagate capture frame size changes as new "preferred size"
  // updates, rather than just using the max frame size.
  // http://crbug.com/350491
  gfx::Size optimal_size = oracle_proxy_->max_frame_size();

  switch (capture_params_.resolution_change_policy) {
    case media::RESOLUTION_POLICY_FIXED_RESOLUTION:
      break;
    case media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO:
    case media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT: {
      // If the max frame size is close to a common video aspect ratio, compute
      // a standard resolution for that aspect ratio.  For example, given
      // 1365x768, which is very close to 16:9, the optimal size would be
      // 1280x720.  The purpose of this logic is to prevent scaling quality
      // issues caused by "one pixel stretching" and/or odd-to-even dimension
      // scaling, and to improve the performance of consumers of the captured
      // video.
      const auto HasIntendedAspectRatio = [](
          const gfx::Size& size, int width_units, int height_units) {
        const int a = height_units * size.width();
        const int b = width_units * size.height();
        const int percentage_diff = 100 * std::abs((a - b)) / b;
        return percentage_diff <= 1;  // Effectively, anything strictly <2%.
      };
      const auto RoundToExactAspectRatio = [](const gfx::Size& size,
                                              int width_step, int height_step) {
        const int adjusted_height = std::max(
            size.height() - (size.height() % height_step), height_step);
        DCHECK_EQ((adjusted_height * width_step) % height_step, 0);
        return gfx::Size(adjusted_height * width_step / height_step,
                         adjusted_height);
      };
      if (HasIntendedAspectRatio(optimal_size, 16, 9))
        optimal_size = RoundToExactAspectRatio(optimal_size, 160, 90);
      else if (HasIntendedAspectRatio(optimal_size, 4, 3))
        optimal_size = RoundToExactAspectRatio(optimal_size, 64, 48);
      // Else, do not make an adjustment.
      break;
    }
  }

  // If the ratio between physical and logical pixels is greater than 1:1,
  // shrink |optimal_size| by that amount.  Then, when external code resizes the
  // render widget to the "preferred size," the widget will be physically
  // rendered at the exact capture size, thereby eliminating unnecessary scaling
  // operations in the graphics pipeline.
  if (auto* rwhv = tracker_->GetTargetView()) {
    const gfx::NativeView view = rwhv->GetNativeView();
    const float scale = ui::GetScaleFactorForNativeView(view);
    if (scale > 1.0f) {
      const gfx::Size shrunk_size =
          gfx::ScaleToFlooredSize(optimal_size, 1.0f / scale);
      if (shrunk_size.width() > 0 && shrunk_size.height() > 0)
        optimal_size = shrunk_size;
    }
  }

  VLOG(1) << "Computed optimal target size: " << optimal_size.ToString();
  return optimal_size;
}

void WebContentsCaptureMachine::DidCopyToVideoFrame(
    const base::TimeTicks& start_time,
    const RenderWidgetHostViewFrameSubscriber::DeliverFrameCallback&
        deliver_frame_cb,
    const gfx::Rect& region_in_frame,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Capture can fail due to transient issues, so just skip this frame.
  DVLOG_IF(1, !success)
      << "CopyFromSurfaceToVideoFrame() failed; skipping frame.";
  deliver_frame_cb.Run(start_time, region_in_frame, success);
}

void WebContentsCaptureMachine::RenewFrameSubscription(bool is_still_tracking) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Always destroy the old subscription before creating a new one.
  if (subscription_) {
    DVLOG(1) << "Cancelling existing subscription.";
    subscription_.reset();
  }

  if (!is_still_tracking) {
    if (IsStarted()) {
      // Tracking of WebContents and/or its main frame has failed before Stop()
      // was called, so report this as an error:
      oracle_proxy_->ReportError(FROM_HERE,
                                 "WebContents and/or main frame are gone.");
    }
    return;
  }

  if (!frame_capture_active_) {
    DVLOG(1) << "Not renewing subscription: Frame capture is suspended.";
    return;
  }

  auto* const view =
      static_cast<RenderWidgetHostViewBase*>(tracker_->GetTargetView());
  if (!view) {
    DVLOG(1) << "Cannot renew subscription: No view to capture.";
    return;
  }

  DVLOG(1) << "Renewing subscription to RenderWidgetHostView@" << view;
  subscription_.reset(new ContentCaptureSubscription(
      view->GetWeakPtr(), oracle_proxy_,
      base::Bind(&WebContentsCaptureMachine::Capture,
                 weak_ptr_factory_.GetWeakPtr())));
  // Capture a refresh frame immediately to make sure the latest frame in the
  // video stream has the correct content.
  subscription_->MaybeCaptureForRefresh();
}

void WebContentsCaptureMachine::UpdateCaptureSize() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!oracle_proxy_)
    return;
  RenderWidgetHostView* const view = tracker_->GetTargetView();
  if (!view)
    return;

  // Convert the view's size from the DIP coordinate space to the pixel
  // coordinate space.  When the view is being rendered on a high-DPI display,
  // this allows the high-resolution image detail to propagate through to the
  // captured video.
  const gfx::Size view_size = view->GetViewBounds().size();
  const gfx::Size physical_size = gfx::ConvertSizeToPixel(
      ui::GetScaleFactorForNativeView(view->GetNativeView()), view_size);
  VLOG(1) << "Computed physical capture size (" << physical_size.ToString()
          << ") from view size (" << view_size.ToString() << ").";

  oracle_proxy_->UpdateCaptureSize(physical_size);
}

}  // namespace

WebContentsVideoCaptureDevice::WebContentsVideoCaptureDevice(
    int render_process_id,
    int main_render_frame_id,
    bool enable_auto_throttling)
    : core_(new media::ScreenCaptureDeviceCore(
          std::unique_ptr<media::VideoCaptureMachine>(
              new WebContentsCaptureMachine(render_process_id,
                                            main_render_frame_id,
                                            enable_auto_throttling)))) {}

WebContentsVideoCaptureDevice::~WebContentsVideoCaptureDevice() {
  DVLOG(2) << "WebContentsVideoCaptureDevice@" << this << " destroying.";
}

// static
std::unique_ptr<media::VideoCaptureDevice>
WebContentsVideoCaptureDevice::Create(const std::string& device_id) {
  // Parse device_id into render_process_id and main_render_frame_id.
  WebContentsMediaCaptureId media_id;
  if (!WebContentsMediaCaptureId::Parse(device_id, &media_id)) {
    return nullptr;
  }

  return std::unique_ptr<media::VideoCaptureDevice>(
      new WebContentsVideoCaptureDevice(media_id.render_process_id,
                                        media_id.main_render_frame_id,
                                        media_id.enable_auto_throttling));
}

void WebContentsVideoCaptureDevice::AllocateAndStart(
    const media::VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  DVLOG(1) << "Allocating " << params.requested_format.frame_size.ToString();
  core_->AllocateAndStart(params, std::move(client));
}

void WebContentsVideoCaptureDevice::RequestRefreshFrame() {
  core_->RequestRefreshFrame();
}

void WebContentsVideoCaptureDevice::MaybeSuspend() {
  core_->Suspend();
}

void WebContentsVideoCaptureDevice::Resume() {
  core_->Resume();
}

void WebContentsVideoCaptureDevice::StopAndDeAllocate() {
  core_->StopAndDeAllocate();
}

void WebContentsVideoCaptureDevice::OnUtilizationReport(int frame_feedback_id,
                                                        double utilization) {
  core_->OnConsumerReportingUtilization(frame_feedback_id, utilization);
}

}  // namespace content
