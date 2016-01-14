// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_PROXY_MAIN_H_
#define CC_TREES_PROXY_MAIN_H_

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/debug/frame_timing_tracker.h"
#include "cc/input/top_controls_state.h"
#include "cc/output/output_surface.h"
#include "cc/output/renderer_capabilities.h"
#include "cc/trees/channel_main.h"
#include "cc/trees/proxy.h"
#include "cc/trees/proxy_common.h"

namespace cc {

class AnimationEvents;
class BeginFrameSource;
class ChannelMain;
class LayerTreeHost;

// This class aggregates all interactions that the impl side of the compositor
// needs to have with the main side.
// The class is created and lives on the main thread.
class CC_EXPORT ProxyMain : public Proxy {
 public:
  static scoped_ptr<ProxyMain> CreateThreaded(
      LayerTreeHost* layer_tree_host,
      TaskRunnerProvider* task_runner_provider);

  ~ProxyMain() override;

  // Commits between the main and impl threads are processed through a pipeline
  // with the following stages. For efficiency we can early out at any stage if
  // we decide that no further processing is necessary.
  enum CommitPipelineStage {
    NO_PIPELINE_STAGE,
    ANIMATE_PIPELINE_STAGE,
    UPDATE_LAYERS_PIPELINE_STAGE,
    COMMIT_PIPELINE_STAGE,
  };

  // Virtual for testing.
  virtual void DidCompleteSwapBuffers();
  virtual void SetRendererCapabilities(
      const RendererCapabilities& capabilities);
  virtual void BeginMainFrameNotExpectedSoon();
  virtual void DidCommitAndDrawFrame();
  virtual void SetAnimationEvents(scoped_ptr<AnimationEvents> events);
  virtual void DidLoseOutputSurface();
  virtual void RequestNewOutputSurface();
  virtual void DidInitializeOutputSurface(
      bool success,
      const RendererCapabilities& capabilities);
  virtual void DidCompletePageScaleAnimation();
  virtual void PostFrameTimingEventsOnMain(
      scoped_ptr<FrameTimingTracker::CompositeTimingSet> composite_events,
      scoped_ptr<FrameTimingTracker::MainFrameTimingSet> main_frame_events);
  virtual void BeginMainFrame(
      scoped_ptr<BeginMainFrameAndCommitState> begin_main_frame_state);

  ChannelMain* channel_main() const { return channel_main_.get(); }
  CommitPipelineStage max_requested_pipeline_stage() const {
    return max_requested_pipeline_stage_;
  }
  CommitPipelineStage current_pipeline_stage() const {
    return current_pipeline_stage_;
  }
  CommitPipelineStage final_pipeline_stage() const {
    return final_pipeline_stage_;
  }

 protected:
  ProxyMain(LayerTreeHost* layer_tree_host,
            TaskRunnerProvider* task_runner_provider);

 private:
  friend class ProxyMainForTest;

  // Proxy implementation.
  void FinishAllRendering() override;
  bool IsStarted() const override;
  bool CommitToActiveTree() const override;
  void SetOutputSurface(OutputSurface* output_surface) override;
  void SetVisible(bool visible) override;
  void SetThrottleFrameProduction(bool throttle) override;
  const RendererCapabilities& GetRendererCapabilities() const override;
  void SetNeedsAnimate() override;
  void SetNeedsUpdateLayers() override;
  void SetNeedsCommit() override;
  void SetNeedsRedraw(const gfx::Rect& damage_rect) override;
  void SetNextCommitWaitsForActivation() override;
  void NotifyInputThrottledUntilCommit() override;
  void SetDeferCommits(bool defer_commits) override;
  bool CommitRequested() const override;
  bool BeginMainFrameRequested() const override;
  void MainThreadHasStoppedFlinging() override;
  void Start(scoped_ptr<BeginFrameSource> external_begin_frame_source) override;
  void Stop() override;
  bool SupportsImplScrolling() const override;
  bool MainFrameWillHappenForTesting() override;
  void SetChildrenNeedBeginFrames(bool children_need_begin_frames) override;
  void SetAuthoritativeVSyncInterval(const base::TimeDelta& interval) override;
  void ReleaseOutputSurface() override;
  void UpdateTopControlsState(TopControlsState constraints,
                              TopControlsState current,
                              bool animate) override;

  // This sets the channel used by ProxyMain to communicate with ProxyImpl.
  void SetChannel(scoped_ptr<ChannelMain> channel_main);

  // Returns |true| if the request was actually sent, |false| if one was
  // already outstanding.
  bool SendCommitRequestToImplThreadIfNeeded(
      CommitPipelineStage required_stage);
  bool IsMainThread() const;

  LayerTreeHost* layer_tree_host_;

  TaskRunnerProvider* task_runner_provider_;

  const int layer_tree_host_id_;

  // The furthest pipeline stage which has been requested for the next
  // commit.
  CommitPipelineStage max_requested_pipeline_stage_;
  // The commit pipeline stage that is currently being processed.
  CommitPipelineStage current_pipeline_stage_;
  // The commit pipeline stage at which processing for the current commit
  // will stop. Only valid while we are executing the pipeline (i.e.,
  // |current_pipeline_stage| is set to a pipeline stage).
  CommitPipelineStage final_pipeline_stage_;

  bool commit_waits_for_activation_;

  // Set when the Proxy is started using Proxy::Start() and reset when it is
  // stopped using Proxy::Stop().
  bool started_;

  bool defer_commits_;

  RendererCapabilities renderer_capabilities_;

  scoped_ptr<ChannelMain> channel_main_;

  DISALLOW_COPY_AND_ASSIGN(ProxyMain);
};

}  // namespace cc

#endif  // CC_TREES_PROXY_MAIN_H_
