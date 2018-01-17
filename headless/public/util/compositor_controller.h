// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_UTIL_COMPOSITOR_CONTROLLER_H_
#define HEADLESS_PUBLIC_UTIL_COMPOSITOR_CONTROLLER_H_

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "headless/public/devtools/domains/headless_experimental.h"
#include "headless/public/headless_devtools_client.h"

namespace headless {

class VirtualTimeController;

// Issues BeginFrames (Chromium's vsync signal) while virtual time advances and
// and takes screenshots.
class HEADLESS_EXPORT CompositorController
    : public headless_experimental::ExperimentalObserver {
 public:
  using BeginFrameResult = headless_experimental::BeginFrameResult;
  using MainFrameReadyForScreenshotsParams =
      headless_experimental::MainFrameReadyForScreenshotsParams;
  using NeedsBeginFramesChangedParams =
      headless_experimental::NeedsBeginFramesChangedParams;
  using ScreenshotParams = headless_experimental::ScreenshotParams;
  using ScreenshotParamsFormat = headless_experimental::ScreenshotParamsFormat;

  // |animation_begin_frame_interval| specifies the virtual time between
  // individual BeginFrames while virtual time advances.
  // |wait_for_compositor_ready_begin_frame_delay| is the real time delay
  // between BeginFrames that are sent while waiting for the main frame
  // compositor to become ready (real time).
  // If |update_display_for_animations| is false, animation BeginFrames will not
  // commit or draw visual updates to the display. This can be used to reduce
  // the overhead of such BeginFrames in the common case that screenshots will
  // be taken from separate BeginFrames.
  CompositorController(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      HeadlessDevToolsClient* devtools_client,
      VirtualTimeController* virtual_time_controller,
      base::TimeDelta animation_begin_frame_interval,
      base::TimeDelta wait_for_compositor_ready_begin_frame_delay,
      bool update_display_for_animations = true);
  ~CompositorController() override;

  // Issues BeginFrames until the main frame's compositor has completed
  // initialization. Should not be called again until
  // |compositor_ready_callback| was run. Should only be called while no other
  // BeginFrame is in flight.
  void WaitForCompositorReady(const base::Closure& compositor_ready_callback);

  // Executes |idle_callback| when no BeginFrames are in flight.
  void WaitUntilIdle(const base::Closure& idle_callback);

  // Issues BeginFrames until a new main frame update was committed. Should not
  // be called again until |main_frame_content_updated_callback| was run. Should
  // only be called while no other BeginFrame is in flight.
  //
  // This can be used in situations where e.g. the main frame size changes and
  // we need to wait for the update to propagate down into a new surface before
  // taking a screenshot.
  // TODO(eseckler): Investigate whether we can replace this with surface
  // synchronization or some other mechanism that avoids the need for additional
  // BeginFrames.
  void WaitForMainFrameContentUpdate(
      const base::Closure& main_frame_content_updated_callback);

  // Captures a screenshot by issuing a BeginFrame. |quality| is only valid for
  // jpeg format screenshots, in range 0..100. Should not be called again until
  // |screenshot_captured_callback| was run. Should only be called while no
  // other BeginFrame is in flight and after the compositor is ready.
  void CaptureScreenshot(ScreenshotParamsFormat format,
                         int quality,
                         const base::Callback<void(const std::string&)>&
                             screenshot_captured_callback);

 private:
  class AnimationBeginFrameTask;

  // headless_experimental_::Observer implementation:
  void OnNeedsBeginFramesChanged(
      const NeedsBeginFramesChangedParams& params) override;
  void OnMainFrameReadyForScreenshots(
      const MainFrameReadyForScreenshotsParams& params) override;

  // Posts a BeginFrame as a new task to avoid nesting it inside the current
  // callstack, which can upset the compositor.
  void PostBeginFrame(
      const base::Callback<void(std::unique_ptr<BeginFrameResult>)>&
          begin_frame_complete_callback,
      bool no_display_updates = false,
      std::unique_ptr<ScreenshotParams> screenshot = nullptr);
  // Issues a BeginFrame synchronously and runs |begin_frame_complete_callback|
  // when done. Should not be called again until |begin_frame_complete_callback|
  // was run.
  void BeginFrame(const base::Callback<void(std::unique_ptr<BeginFrameResult>)>&
                      begin_frame_complete_callback,
                  bool no_display_updates = false,
                  std::unique_ptr<ScreenshotParams> screenshot = nullptr);
  // Runs the |begin_frame_complete_callback_| and the |idle_callback_| if set.
  void BeginFrameComplete(std::unique_ptr<BeginFrameResult>);

  // Posts a task to issue a BeginFrame while waiting for the
  // mainFrameReadyForScreenshots event. The taks may be cancelled by the event.
  void PostWaitForCompositorReadyBeginFrameTask();
  void IssueWaitForCompositorReadyBeginFrame();
  void WaitForCompositorReadyBeginFrameComplete(
      std::unique_ptr<BeginFrameResult>);

  // Posts a BeginFrame while waiting for a main frame content update.
  void PostWaitForMainFrameContentUpdateBeginFrame();
  void WaitForMainFrameContentUpdateBeginFrameComplete(
      std::unique_ptr<BeginFrameResult> result);

  void CaptureScreenshotBeginFrameComplete(
      std::unique_ptr<BeginFrameResult> result);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  HeadlessDevToolsClient* devtools_client_;         // NOT OWNED
  VirtualTimeController* virtual_time_controller_;  // NOT OWNED
  std::unique_ptr<AnimationBeginFrameTask> animation_task_;
  base::Closure compositor_ready_callback_;
  base::Closure idle_callback_;
  base::Closure main_frame_content_updated_callback_;
  base::Callback<void(const std::string&)> screenshot_captured_callback_;
  base::Callback<void(std::unique_ptr<BeginFrameResult>)>
      begin_frame_complete_callback_;
  base::CancelableClosure wait_for_compositor_ready_begin_frame_task_;
  base::TimeDelta animation_begin_frame_interval_;
  base::TimeDelta wait_for_compositor_ready_begin_frame_delay_;
  bool update_display_for_animations_;
  bool needs_begin_frames_ = false;
  bool main_frame_ready_ = false;
  base::Time last_begin_frame_time_ = base::Time::UnixEpoch();
  base::WeakPtrFactory<CompositorController> weak_ptr_factory_;
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_UTIL_COMPOSITOR_CONTROLLER_H_
