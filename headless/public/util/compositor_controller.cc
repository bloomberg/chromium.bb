// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/compositor_controller.h"

#include <memory>

#include "base/base64.h"
#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "headless/public/util/virtual_time_controller.h"

namespace headless {

// Sends BeginFrames to advance animations while virtual time advances in
// intervals.
class CompositorController::AnimationBeginFrameTask
    : public VirtualTimeController::RepeatingTask {
 public:
  explicit AnimationBeginFrameTask(CompositorController* compositor_controller)
      : compositor_controller_(compositor_controller),
        weak_ptr_factory_(this) {}

  // VirtualTimeController::RepeatingTask implementation:
  void IntervalElapsed(base::TimeDelta virtual_time_offset,
                       const base::Closure& continue_callback) override {
    continue_callback_ = continue_callback;

    // Post a cancellable task that will issue a BeginFrame. This way, we can
    // cancel sending an animation-only BeginFrame if another virtual time task
    // sends a screenshotting BeginFrame first, or if the budget was exhausted.
    // TODO(eseckler): This won't capture screenshot requests sent
    // asynchronously.
    begin_frame_task_.Reset(
        base::Bind(&AnimationBeginFrameTask::IssueAnimationBeginFrame,
                   weak_ptr_factory_.GetWeakPtr()));
    compositor_controller_->task_runner_->PostTask(
        FROM_HERE, begin_frame_task_.callback());
  }

  void BudgetRequested(base::TimeDelta virtual_time_offset,
                       base::TimeDelta requested_budget,
                       const base::Closure& continue_callback) override {
    // Run a BeginFrame if we cancelled it because the budged expired previously
    // and no other BeginFrame was sent while virtual time was paused.
    if (needs_begin_frame_on_virtual_time_resume_) {
      continue_callback_ = continue_callback;
      IssueAnimationBeginFrame();
      return;
    }
    continue_callback.Run();
  }

  void BudgetExpired(base::TimeDelta virtual_time_offset) override {
    // Wait until a new budget was requested before sending another animation
    // BeginFrame, as it's likely that we will send a screenshotting BeginFrame.
    if (!begin_frame_task_.IsCancelled()) {
      begin_frame_task_.Cancel();
      needs_begin_frame_on_virtual_time_resume_ = true;
      BeginFrameComplete(nullptr);
    }
  }

  void CompositorControllerIssuingScreenshotBeginFrame() {
    // The screenshotting BeginFrame will replace our animation-only BeginFrame.
    // We cancel any pending animation BeginFrame to avoid sending two
    // BeginFrames within the same virtual time pause.
    needs_begin_frame_on_virtual_time_resume_ = false;
    if (!begin_frame_task_.IsCancelled()) {
      begin_frame_task_.Cancel();
      BeginFrameComplete(nullptr);
    }
  }

 private:
  void IssueAnimationBeginFrame() {
    TRACE_EVENT0("headless",
                 "CompositorController::AnimationBeginFrameTask::"
                 "IssueAnimationBeginFrame");
    begin_frame_task_.Cancel();
    needs_begin_frame_on_virtual_time_resume_ = false;
    // No need for PostBeginFrame, since the begin_frame_task_ has already been
    // posted above.
    compositor_controller_->BeginFrame(
        base::Bind(&AnimationBeginFrameTask::BeginFrameComplete,
                   weak_ptr_factory_.GetWeakPtr()),
        !compositor_controller_->update_display_for_animations_);
  }

  void BeginFrameComplete(std::unique_ptr<BeginFrameResult>) {
    TRACE_EVENT0(
        "headless",
        "CompositorController::AnimationBeginFrameTask::BeginFrameComplete");
    DCHECK(continue_callback_);
    auto callback = continue_callback_;
    continue_callback_.Reset();
    callback.Run();
  }

  CompositorController* compositor_controller_;  // NOT OWNED
  bool needs_begin_frame_on_virtual_time_resume_ = false;
  base::CancelableClosure begin_frame_task_;
  base::Closure continue_callback_;
  base::WeakPtrFactory<AnimationBeginFrameTask> weak_ptr_factory_;
};

CompositorController::CompositorController(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    HeadlessDevToolsClient* devtools_client,
    VirtualTimeController* virtual_time_controller,
    base::TimeDelta animation_begin_frame_interval,
    base::TimeDelta wait_for_compositor_ready_begin_frame_delay,
    bool update_display_for_animations)
    : task_runner_(std::move(task_runner)),
      devtools_client_(devtools_client),
      virtual_time_controller_(virtual_time_controller),
      animation_task_(std::make_unique<AnimationBeginFrameTask>(this)),
      animation_begin_frame_interval_(animation_begin_frame_interval),
      wait_for_compositor_ready_begin_frame_delay_(
          wait_for_compositor_ready_begin_frame_delay),
      update_display_for_animations_(update_display_for_animations),
      weak_ptr_factory_(this) {
  devtools_client_->GetHeadlessExperimental()->GetExperimental()->AddObserver(
      this);
  // No need to wait for completion of this, since we are waiting for the
  // setNeedsBeginFramesChanged event instead, which will be sent at some point
  // after enabling the domain.
  devtools_client_->GetHeadlessExperimental()->GetExperimental()->Enable(
      headless_experimental::EnableParams::Builder().Build());
  virtual_time_controller_->ScheduleRepeatingTask(
      animation_task_.get(), animation_begin_frame_interval_);
}

CompositorController::~CompositorController() {
  virtual_time_controller_->CancelRepeatingTask(animation_task_.get());
  devtools_client_->GetHeadlessExperimental()
      ->GetExperimental()
      ->RemoveObserver(this);
}

void CompositorController::PostBeginFrame(
    const base::Callback<void(std::unique_ptr<BeginFrameResult>)>&
        begin_frame_complete_callback,
    bool no_display_updates,
    std::unique_ptr<ScreenshotParams> screenshot) {
  // In certain nesting situations, we should not issue a BeginFrame immediately
  // - for example, issuing a new BeginFrame within a BeginFrameCompleted or
  // NeedsBeginFramesChanged event can upset the compositor. We avoid these
  // situations by issuing our BeginFrames from a separately posted task.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CompositorController::BeginFrame,
                 weak_ptr_factory_.GetWeakPtr(), begin_frame_complete_callback,
                 no_display_updates, base::Passed(&screenshot)));
}

void CompositorController::BeginFrame(
    const base::Callback<void(std::unique_ptr<BeginFrameResult>)>&
        begin_frame_complete_callback,
    bool no_display_updates,
    std::unique_ptr<ScreenshotParams> screenshot) {
  DCHECK(!begin_frame_complete_callback_);
  begin_frame_complete_callback_ = begin_frame_complete_callback;
  if (needs_begin_frames_ || screenshot) {
    auto params_builder = headless_experimental::BeginFrameParams::Builder();

    // Use virtual time for frame time, so that rendering of animations etc. is
    // aligned with virtual time progression.
    base::Time frame_time = virtual_time_controller_->GetCurrentVirtualTime();
    if (frame_time <= last_begin_frame_time_) {
      // Frame time cannot go backwards or stop, so we issue another BeginFrame
      // with a small time offset from the last BeginFrame's time instead.
      frame_time =
          last_begin_frame_time_ + base::TimeDelta::FromMicroseconds(1);
    }
    params_builder.SetFrameTime(frame_time.ToJsTime());
    DCHECK_GT(frame_time, last_begin_frame_time_);
    DCHECK_GT(frame_time.ToJsTime(), last_begin_frame_time_.ToJsTime());
    last_begin_frame_time_ = frame_time;

    params_builder.SetInterval(
        animation_begin_frame_interval_.InMillisecondsF());

    params_builder.SetNoDisplayUpdates(no_display_updates);

    // TODO(eseckler): Set time fields. This requires obtaining the absolute
    // virtual time stamp.
    if (screenshot)
      params_builder.SetScreenshot(std::move(screenshot));

    devtools_client_->GetHeadlessExperimental()->GetExperimental()->BeginFrame(
        params_builder.Build(),
        base::Bind(&CompositorController::BeginFrameComplete,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    BeginFrameComplete(nullptr);
  }
}

void CompositorController::BeginFrameComplete(
    std::unique_ptr<BeginFrameResult> result) {
  DCHECK(begin_frame_complete_callback_);
  auto callback = begin_frame_complete_callback_;
  begin_frame_complete_callback_.Reset();
  callback.Run(std::move(result));
  if (idle_callback_) {
    auto idle_callback = idle_callback_;
    idle_callback_.Reset();
    idle_callback.Run();
  }
}

void CompositorController::OnNeedsBeginFramesChanged(
    const NeedsBeginFramesChangedParams& params) {
  needs_begin_frames_ = params.GetNeedsBeginFrames();

  // If needs_begin_frames_ became true again and we're waiting for the
  // compositor or a main frame update, continue posting BeginFrames - provided
  // there's none outstanding.
  if (compositor_ready_callback_ && needs_begin_frames_ &&
      !begin_frame_complete_callback_ &&
      wait_for_compositor_ready_begin_frame_task_.IsCancelled()) {
    PostBeginFrame(base::Bind(
        &CompositorController::WaitForCompositorReadyBeginFrameComplete,
        weak_ptr_factory_.GetWeakPtr()));
  } else if (main_frame_content_updated_callback_ && needs_begin_frames_ &&
             !begin_frame_complete_callback_) {
    PostWaitForMainFrameContentUpdateBeginFrame();
  }
}

void CompositorController::OnMainFrameReadyForScreenshots(
    const MainFrameReadyForScreenshotsParams& params) {
  TRACE_EVENT0("headless",
               "CompositorController::OnMainFrameReadyForScreenshots");
  main_frame_ready_ = true;

  // If a WaitForCompositorReadyBeginFrame is still scheduled, skip it.
  if (!wait_for_compositor_ready_begin_frame_task_.IsCancelled()) {
    wait_for_compositor_ready_begin_frame_task_.Cancel();
    auto callback = compositor_ready_callback_;
    compositor_ready_callback_.Reset();
    callback.Run();
  }
}

void CompositorController::WaitForCompositorReady(
    const base::Closure& compositor_ready_callback) {
  // We need to wait for the mainFrameReadyForScreenshots event, which will be
  // issued once the renderer has submitted its first CompositorFrame in
  // response to a BeginFrame. At that point, we know that the renderer
  // compositor has initialized. We do this by issuing BeginFrames until we
  // receive the event. To avoid bogging down the system with a flood of
  // BeginFrames, we add a short delay between them.
  // TODO(eseckler): Investigate if we can remove the need for these initial
  // BeginFrames and the mainFrameReadyForScreenshots event, by making the
  // compositor wait for the renderer in the very first BeginFrame, even if it
  // isn't yet present in the surface hierarchy. Maybe surface synchronization
  // can help here?
  DCHECK(!begin_frame_complete_callback_);
  DCHECK(!compositor_ready_callback_);

  if (main_frame_ready_) {
    compositor_ready_callback.Run();
    return;
  }

  compositor_ready_callback_ = compositor_ready_callback;
  if (needs_begin_frames_) {
    // Post BeginFrames with a delay until the main frame becomes ready.
    PostWaitForCompositorReadyBeginFrameTask();
  }
}

void CompositorController::PostWaitForCompositorReadyBeginFrameTask() {
  TRACE_EVENT0(
      "headless",
      "CompositorController::PostWaitForCompositorReadyBeginFrameTask");
  wait_for_compositor_ready_begin_frame_task_.Reset(
      base::Bind(&CompositorController::IssueWaitForCompositorReadyBeginFrame,
                 weak_ptr_factory_.GetWeakPtr()));

  // We may receive the mainFrameReadyForScreenshots event before this task
  // is run. In that case, we cancel it in OnMainFrameReadyForScreenshots to
  // avoid another unnecessary BeginFrame.
  task_runner_->PostDelayedTask(
      FROM_HERE, wait_for_compositor_ready_begin_frame_task_.callback(),
      wait_for_compositor_ready_begin_frame_delay_);
}

void CompositorController::IssueWaitForCompositorReadyBeginFrame() {
  TRACE_EVENT0("headless",
               "CompositorController::IssueWaitForCompositorReadyBeginFrame");
  // No need for PostBeginFrame, since
  // wait_for_compositor_ready_begin_frame_task_ has already been posted.
  wait_for_compositor_ready_begin_frame_task_.Cancel();
  BeginFrame(base::Bind(
      &CompositorController::WaitForCompositorReadyBeginFrameComplete,
      weak_ptr_factory_.GetWeakPtr()));
}

void CompositorController::WaitForCompositorReadyBeginFrameComplete(
    std::unique_ptr<BeginFrameResult>) {
  TRACE_EVENT0(
      "headless",
      "CompositorController::WaitForCompositorReadyBeginFrameComplete");
  DCHECK(compositor_ready_callback_);

  if (main_frame_ready_) {
    auto callback = compositor_ready_callback_;
    compositor_ready_callback_.Reset();
    callback.Run();
    return;
  }

  // Continue posting more BeginFrames with a delay until the main frame
  // becomes ready. If needs_begin_frames_ is false, it will eventually turn
  // true again once the renderer's compositor has started up.
  if (needs_begin_frames_)
    PostWaitForCompositorReadyBeginFrameTask();
}

void CompositorController::WaitUntilIdle(const base::Closure& idle_callback) {
  TRACE_EVENT_INSTANT1("headless", "CompositorController::WaitUntilIdle",
                       TRACE_EVENT_SCOPE_THREAD, "begin_frame_in_flight",
                       !!begin_frame_complete_callback_);
  DCHECK(!idle_callback_);

  if (!begin_frame_complete_callback_) {
    idle_callback.Run();
    return;
  }

  idle_callback_ = idle_callback;
}

void CompositorController::WaitForMainFrameContentUpdate(
    const base::Closure& main_frame_content_updated_callback) {
  TRACE_EVENT0("headless",
               "CompositorController::WaitForMainFrameContentUpdate");
  DCHECK(!begin_frame_complete_callback_);
  DCHECK(!main_frame_content_updated_callback_);
  main_frame_content_updated_callback_ = main_frame_content_updated_callback;

  // Post BeginFrames until we see a main frame update.
  if (needs_begin_frames_)
    PostWaitForMainFrameContentUpdateBeginFrame();
}

void CompositorController::PostWaitForMainFrameContentUpdateBeginFrame() {
  TRACE_EVENT0(
      "headless",
      "CompositorController::PostWaitForMainFrameContentUpdateBeginFrame");
  DCHECK(main_frame_content_updated_callback_);
  PostBeginFrame(base::Bind(
      &CompositorController::WaitForMainFrameContentUpdateBeginFrameComplete,
      weak_ptr_factory_.GetWeakPtr()));
}

void CompositorController::WaitForMainFrameContentUpdateBeginFrameComplete(
    std::unique_ptr<BeginFrameResult> result) {
  if (!result)
    return;

  TRACE_EVENT1(
      "headless",
      "CompositorController::WaitForMainFrameContentUpdateBeginFrameComplete",
      "main_frame_content_updated", result->GetMainFrameContentUpdated());
  DCHECK(!begin_frame_complete_callback_);
  DCHECK(main_frame_content_updated_callback_);

  if (result->GetMainFrameContentUpdated()) {
    auto callback = main_frame_content_updated_callback_;
    main_frame_content_updated_callback_.Reset();
    callback.Run();
    return;
  }

  // Continue posting BeginFrames until we see a main frame update.
  if (needs_begin_frames_)
    PostWaitForMainFrameContentUpdateBeginFrame();
}

void CompositorController::CaptureScreenshot(
    ScreenshotParamsFormat format,
    int quality,
    const base::Callback<void(const std::string&)>&
        screenshot_captured_callback) {
  TRACE_EVENT0("headless", "CompositorController::CaptureScreenshot");
  DCHECK(!begin_frame_complete_callback_);
  DCHECK(!screenshot_captured_callback_);
  DCHECK(main_frame_ready_);

  screenshot_captured_callback_ = screenshot_captured_callback;

  // Let AnimationBeginFrameTask know that it doesn't need to issue an
  // animation BeginFrame for the current virtual time pause.
  animation_task_->CompositorControllerIssuingScreenshotBeginFrame();

  const bool no_display_updates = false;
  PostBeginFrame(
      base::Bind(&CompositorController::CaptureScreenshotBeginFrameComplete,
                 weak_ptr_factory_.GetWeakPtr()),
      no_display_updates,
      ScreenshotParams::Builder()
          .SetFormat(format)
          .SetQuality(quality)
          .Build());
}

void CompositorController::CaptureScreenshotBeginFrameComplete(
    std::unique_ptr<BeginFrameResult> result) {
  TRACE_EVENT1(
      "headless", "CompositorController::CaptureScreenshotBeginFrameComplete",
      "hasScreenshotData",
      result ? std::to_string(result->HasScreenshotData()) : "invalid");
  DCHECK(screenshot_captured_callback_);
  if (result && result->HasScreenshotData()) {
    // TODO(eseckler): Look into returning binary screenshot data via DevTools.
    std::string decoded_data;
    base::Base64Decode(result->GetScreenshotData(), &decoded_data);
    auto callback = screenshot_captured_callback_;
    screenshot_captured_callback_.Reset();
    callback.Run(decoded_data);
  } else {
    LOG(ERROR) << "Screenshotting failed, BeginFrameResult has no data and "
                  "hasDamage is "
               << (result ? std::to_string(result->HasScreenshotData())
                          : "invalid");
    auto callback = screenshot_captured_callback_;
    screenshot_captured_callback_.Reset();
    callback.Run(std::string());
  }
}

}  // namespace headless
