// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_HOOKS_H_
#define CC_TEST_TEST_HOOKS_H_

#include "base/macros.h"
#include "cc/animation/animation_delegate.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_impl.h"

namespace cc {

namespace proto {
class CompositorMessageToImpl;
}

// Used by test stubs to notify the test when something interesting happens.
class TestHooks : public AnimationDelegate {
 public:
  TestHooks();
  ~TestHooks() override;

  void ReadSettings(const LayerTreeSettings& settings);

  virtual void CreateResourceAndTileTaskWorkerPool(
      LayerTreeHostImpl* host_impl,
      scoped_ptr<TileTaskWorkerPool>* tile_task_worker_pool,
      scoped_ptr<ResourcePool>* resource_pool);
  virtual void WillBeginImplFrameOnThread(LayerTreeHostImpl* host_impl,
                                          const BeginFrameArgs& args) {}
  virtual void DidFinishImplFrameOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void BeginMainFrameAbortedOnThread(LayerTreeHostImpl* host_impl,
                                             CommitEarlyOutReason reason) {}
  virtual void WillPrepareTiles(LayerTreeHostImpl* host_impl) {}
  virtual void BeginCommitOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void WillCommitCompleteOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void CommitCompleteOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void WillActivateTreeOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void InitializedRendererOnThread(LayerTreeHostImpl* host_impl,
                                           bool success) {}
  virtual DrawResult PrepareToDrawOnThread(
      LayerTreeHostImpl* host_impl,
      LayerTreeHostImpl::FrameData* frame_data,
      DrawResult draw_result);
  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl, bool result) {}
  virtual void SwapBuffersCompleteOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void NotifyReadyToActivateOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void NotifyReadyToDrawOnThread(LayerTreeHostImpl* host_impl) {}
  virtual void NotifyAllTileTasksCompleted(LayerTreeHostImpl* host_impl) {}
  virtual void NotifyTileStateChangedOnThread(LayerTreeHostImpl* host_impl,
                                              const Tile* tile) {}
  virtual void AnimateLayers(LayerTreeHostImpl* host_impl,
                             base::TimeTicks monotonic_time) {}
  virtual void UpdateAnimationState(LayerTreeHostImpl* host_impl,
                                    bool has_unfinished_animation) {}
  virtual void WillAnimateLayers(LayerTreeHostImpl* host_impl,
                                 base::TimeTicks monotonic_time) {}
  virtual void ApplyViewportDeltas(
      const gfx::Vector2dF& inner_delta,
      const gfx::Vector2dF& outer_delta,
      const gfx::Vector2dF& elastic_overscroll_delta,
      float scale,
      float top_controls_delta) {}
  virtual void BeginMainFrame(const BeginFrameArgs& args) {}
  virtual void WillBeginMainFrame() {}
  virtual void DidBeginMainFrame() {}
  virtual void UpdateLayerTreeHost() {}
  virtual void DidInitializeOutputSurface() {}
  virtual void DidFailToInitializeOutputSurface() {}
  virtual void DidAddAnimation() {}
  virtual void WillCommit() {}
  virtual void DidCommit() {}
  virtual void DidCommitAndDrawFrame() {}
  virtual void DidCompleteSwapBuffers() {}
  virtual void DidSetVisibleOnImplTree(LayerTreeHostImpl* host_impl,
                                       bool visible) {}
  virtual void ScheduleComposite() {}
  virtual void DidSetNeedsUpdateLayers() {}
  virtual void DidActivateSyncTree() {}

  // Hooks for SchedulerClient.
  virtual void ScheduledActionWillSendBeginMainFrame() {}
  virtual void ScheduledActionSendBeginMainFrame() {}
  virtual void ScheduledActionDrawAndSwapIfPossible() {}
  virtual void ScheduledActionCommit() {}
  virtual void ScheduledActionBeginOutputSurfaceCreation() {}
  virtual void ScheduledActionPrepareTiles() {}
  virtual void ScheduledActionInvalidateOutputSurface() {}
  virtual void SendBeginFramesToChildren(const BeginFrameArgs& args) {}
  virtual void SendBeginMainFrameNotExpectedSoon() {}

  // Hooks for ProxyImpl
  virtual void SetThrottleFrameProductionOnImpl(bool throttle) {}
  virtual void UpdateTopControlsStateOnImpl(TopControlsState constraints,
                                            TopControlsState current,
                                            bool animate) {}
  virtual void InitializeOutputSurfaceOnImpl(OutputSurface* output_surface) {}
  virtual void MainThreadHasStoppedFlingingOnImpl() {}
  virtual void SetInputThrottledUntilCommitOnImpl(bool is_throttled) {}
  virtual void SetDeferCommitsOnImpl(bool defer_commits) {}
  virtual void BeginMainFrameAbortedOnImpl(CommitEarlyOutReason reason) {}
  virtual void SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) {}
  virtual void SetNeedsCommitOnImpl() {}
  virtual void FinishAllRenderingOnImpl() {}
  virtual void SetVisibleOnImpl(bool visible) {}
  virtual void ReleaseOutputSurfaceOnImpl() {}
  virtual void FinishGLOnImpl() {}
  virtual void StartCommitOnImpl() {}

  // Hooks for ProxyMain
  virtual void ReceivedDidCompleteSwapBuffers() {}
  virtual void ReceivedSetRendererCapabilitiesMainCopy(
      const RendererCapabilities& capabilities) {}
  virtual void ReceivedBeginMainFrameNotExpectedSoon() {}
  virtual void ReceivedDidCommitAndDrawFrame() {}
  virtual void ReceivedSetAnimationEvents() {}
  virtual void ReceivedDidLoseOutputSurface() {}
  virtual void ReceivedRequestNewOutputSurface() {}
  virtual void ReceivedDidInitializeOutputSurface(
      bool success,
      const RendererCapabilities& capabilities) {}
  virtual void ReceivedDidCompletePageScaleAnimation() {}
  virtual void ReceivedPostFrameTimingEventsOnMain() {}
  virtual void ReceivedBeginMainFrame() {}

  // Implementation of AnimationDelegate:
  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              Animation::TargetProperty target_property,
                              int group) override {}
  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               Animation::TargetProperty target_property,
                               int group) override {}
  void NotifyAnimationAborted(base::TimeTicks monotonic_time,
                              Animation::TargetProperty target_property,
                              int group) override {}

  virtual void RequestNewOutputSurface() = 0;

  // Used to notify the test to create the Remote client LayerTreeHost on
  // receiving a CompositorMessageToImpl of type INITIALIZE_IMPL.
  virtual void CreateRemoteClientHost(
      const proto::CompositorMessageToImpl& proto) {}

  // Used to notify the test to destroy the Remote client LayerTreeHost on
  // receiving a CompositorMessageToImpl of type CLOSE_IMPL.
  virtual void DestroyRemoteClientHost() {}
};

}  // namespace cc

#endif  // CC_TEST_TEST_HOOKS_H_
