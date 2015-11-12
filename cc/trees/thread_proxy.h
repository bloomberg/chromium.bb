// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_THREAD_PROXY_H_
#define CC_TREES_THREAD_PROXY_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/animation/animation_events.h"
#include "cc/base/completion_event.h"
#include "cc/base/delayed_unique_notifier.h"
#include "cc/scheduler/commit_earlyout_reason.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/proxy.h"
#include "cc/trees/threaded_channel.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {

class BeginFrameSource;
class ChannelImpl;
class ChannelMain;
class ContextProvider;
class InputHandlerClient;
class LayerTreeHost;
class ProxyImpl;
class ProxyMain;
class Scheduler;
class ScopedThreadProxy;
class ThreadedChannel;

class CC_EXPORT ThreadProxy : public Proxy,
                              public ProxyMain,
                              public ProxyImpl,
                              NON_EXPORTED_BASE(LayerTreeHostImplClient),
                              NON_EXPORTED_BASE(SchedulerClient) {
 public:
  static scoped_ptr<Proxy> Create(
      LayerTreeHost* layer_tree_host,
      TaskRunnerProvider* task_runner_provider,
      scoped_ptr<BeginFrameSource> external_begin_frame_source);

  ~ThreadProxy() override;

  // Commits between the main and impl threads are processed through a pipeline
  // with the following stages. For efficiency we can early out at any stage if
  // we decide that no further processing is necessary.
  enum CommitPipelineStage {
    NO_PIPELINE_STAGE,
    ANIMATE_PIPELINE_STAGE,
    UPDATE_LAYERS_PIPELINE_STAGE,
    COMMIT_PIPELINE_STAGE,
  };

  struct MainThreadOnly {
    MainThreadOnly(ThreadProxy* proxy, LayerTreeHost* layer_tree_host);
    ~MainThreadOnly();

    const int layer_tree_host_id;

    LayerTreeHost* layer_tree_host;

    // The furthest pipeline stage which has been requested for the next
    // commit.
    CommitPipelineStage max_requested_pipeline_stage;
    // The commit pipeline stage that is currently being processed.
    CommitPipelineStage current_pipeline_stage;
    // The commit pipeline stage at which processing for the current commit
    // will stop. Only valid while we are executing the pipeline (i.e.,
    // |current_pipeline_stage| is set to a pipeline stage).
    CommitPipelineStage final_pipeline_stage;

    bool commit_waits_for_activation;

    bool started;
    bool prepare_tiles_pending;
    bool defer_commits;

    RendererCapabilities renderer_capabilities_main_thread_copy;

    // TODO(khushalsagar): Make this scoped_ptr<ChannelMain> when ProxyMain
    // and ProxyImpl are split.
    ChannelMain* channel_main;

    base::WeakPtrFactory<ThreadProxy> weak_factory;
  };

  // Accessed on the impl thread when the main thread is blocked for a commit.
  struct BlockedMainCommitOnly {
    BlockedMainCommitOnly();
    ~BlockedMainCommitOnly();
    LayerTreeHost* layer_tree_host;
  };

  struct CompositorThreadOnly {
    CompositorThreadOnly(
        ThreadProxy* proxy,
        int layer_tree_host_id,
        RenderingStatsInstrumentation* rendering_stats_instrumentation,
        scoped_ptr<BeginFrameSource> external_begin_frame_source);
    ~CompositorThreadOnly();

    const int layer_tree_host_id;

    scoped_ptr<Scheduler> scheduler;

    // Set when the main thread is waiting on a pending tree activation.
    bool next_commit_waits_for_activation;

    // Set when the main thread is waiting on a commit to complete or on a
    // pending tree activation.
    CompletionEvent* commit_completion_event;

    // Set when the next draw should post DidCommitAndDrawFrame to the main
    // thread.
    bool next_frame_is_newly_committed_frame;

    bool inside_draw;

    bool input_throttled_until_commit;

    // Whether a commit has been completed since the last time animations were
    // ticked. If this happens, we need to animate again.
    bool did_commit_after_animating;

    DelayedUniqueNotifier smoothness_priority_expiration_notifier;

    scoped_ptr<BeginFrameSource> external_begin_frame_source;

    RenderingStatsInstrumentation* rendering_stats_instrumentation;

    // Values used to keep track of frame durations. Used only in frame timing.
    BeginFrameArgs last_begin_main_frame_args;
    BeginFrameArgs last_processed_begin_main_frame_args;

    scoped_ptr<LayerTreeHostImpl> layer_tree_host_impl;

    ChannelImpl* channel_impl;

    base::WeakPtrFactory<ThreadProxy> weak_factory;
  };

  const MainThreadOnly& main() const;
  const CompositorThreadOnly& impl() const;
  TaskRunnerProvider* task_runner_provider() { return task_runner_provider_; }

  // Proxy implementation
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
  void Start() override;
  void Stop() override;
  bool SupportsImplScrolling() const override;
  bool MainFrameWillHappenForTesting() override;
  void SetChildrenNeedBeginFrames(bool children_need_begin_frames) override;
  void SetAuthoritativeVSyncInterval(const base::TimeDelta& interval) override;
  void ReleaseOutputSurface() override;
  void UpdateTopControlsState(TopControlsState constraints,
                              TopControlsState current,
                              bool animate) override;

  // LayerTreeHostImplClient implementation
  void UpdateRendererCapabilitiesOnImplThread() override;
  void DidLoseOutputSurfaceOnImplThread() override;
  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override;
  void SetEstimatedParentDrawTime(base::TimeDelta draw_time) override;
  void SetMaxSwapsPendingOnImplThread(int max) override;
  void DidSwapBuffersOnImplThread() override;
  void DidSwapBuffersCompleteOnImplThread() override;
  void OnResourcelessSoftareDrawStateChanged(bool resourceless_draw) override;
  void OnCanDrawStateChanged(bool can_draw) override;
  void NotifyReadyToActivate() override;
  void NotifyReadyToDraw() override;
  // Please call these 3 functions through
  // LayerTreeHostImpl's SetNeedsRedraw(), SetNeedsRedrawRect() and
  // SetNeedsAnimate().
  void SetNeedsRedrawOnImplThread() override;
  void SetNeedsRedrawRectOnImplThread(const gfx::Rect& dirty_rect) override;
  void SetNeedsAnimateOnImplThread() override;
  void SetNeedsPrepareTilesOnImplThread() override;
  void SetNeedsCommitOnImplThread() override;
  void SetVideoNeedsBeginFrames(bool needs_begin_frames) override;
  void PostAnimationEventsToMainThreadOnImplThread(
      scoped_ptr<AnimationEventsVector> queue) override;
  bool IsInsideDraw() override;
  void RenewTreePriority() override;
  void PostDelayedAnimationTaskOnImplThread(const base::Closure& task,
                                            base::TimeDelta delay) override;
  void DidActivateSyncTree() override;
  void WillPrepareTiles() override;
  void DidPrepareTiles() override;
  void DidCompletePageScaleAnimationOnImplThread() override;
  void OnDrawForOutputSurface() override;
  // This should only be called by LayerTreeHostImpl::PostFrameTimingEvents.
  void PostFrameTimingEventsOnImplThread(
      scoped_ptr<FrameTimingTracker::CompositeTimingSet> composite_events,
      scoped_ptr<FrameTimingTracker::MainFrameTimingSet> main_frame_events)
      override;

  // SchedulerClient implementation
  void WillBeginImplFrame(const BeginFrameArgs& args) override;
  void DidFinishImplFrame() override;
  void ScheduledActionSendBeginMainFrame(const BeginFrameArgs& args) override;
  DrawResult ScheduledActionDrawAndSwapIfPossible() override;
  DrawResult ScheduledActionDrawAndSwapForced() override;
  void ScheduledActionAnimate() override;
  void ScheduledActionCommit() override;
  void ScheduledActionActivateSyncTree() override;
  void ScheduledActionBeginOutputSurfaceCreation() override;
  void ScheduledActionPrepareTiles() override;
  void ScheduledActionInvalidateOutputSurface() override;
  void SendBeginFramesToChildren(const BeginFrameArgs& args) override;
  void SendBeginMainFrameNotExpectedSoon() override;

  // ProxyMain implementation
  void SetChannel(scoped_ptr<ThreadedChannel> threaded_channel) override;

 protected:
  ThreadProxy(LayerTreeHost* layer_tree_host,
              TaskRunnerProvider* task_runner_provider,
              scoped_ptr<BeginFrameSource> external_begin_frame_source);

 private:
  friend class ThreadProxyForTest;

  // ProxyMain implementation.
  base::WeakPtr<ProxyMain> GetMainWeakPtr() override;
  void DidCompleteSwapBuffers() override;
  void SetRendererCapabilitiesMainCopy(
      const RendererCapabilities& capabilities) override;
  void BeginMainFrameNotExpectedSoon() override;
  void DidCommitAndDrawFrame() override;
  void SetAnimationEvents(scoped_ptr<AnimationEventsVector> queue) override;
  void DidLoseOutputSurface() override;
  void RequestNewOutputSurface() override;
  void DidInitializeOutputSurface(
      bool success,
      const RendererCapabilities& capabilities) override;
  void DidCompletePageScaleAnimation() override;
  void PostFrameTimingEventsOnMain(
      scoped_ptr<FrameTimingTracker::CompositeTimingSet> composite_events,
      scoped_ptr<FrameTimingTracker::MainFrameTimingSet> main_frame_events)
      override;
  void BeginMainFrame(
      scoped_ptr<BeginMainFrameAndCommitState> begin_main_frame_state) override;

  // ProxyImpl implementation
  base::WeakPtr<ProxyImpl> GetImplWeakPtr() override;
  void SetThrottleFrameProductionOnImpl(bool throttle) override;
  void UpdateTopControlsStateOnImpl(TopControlsState constraints,
                                    TopControlsState current,
                                    bool animate) override;
  void InitializeOutputSurfaceOnImpl(OutputSurface* output_surface) override;
  void MainThreadHasStoppedFlingingOnImpl() override;
  void SetInputThrottledUntilCommitOnImpl(bool is_throttled) override;
  void SetDeferCommitsOnImpl(bool defer_commits) const override;
  void FinishAllRenderingOnImpl(CompletionEvent* completion) override;
  void SetVisibleOnImpl(bool visible) override;
  void ReleaseOutputSurfaceOnImpl(CompletionEvent* completion) override;
  void FinishGLOnImpl(CompletionEvent* completion) override;
  void MainFrameWillHappenOnImplForTesting(
      CompletionEvent* completion,
      bool* main_frame_will_happen) override;
  void SetNeedsCommitOnImpl() override;
  void SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) override;
  void BeginMainFrameAbortedOnImpl(
      CommitEarlyOutReason reason,
      base::TimeTicks main_thread_start_time) override;
  void StartCommitOnImpl(CompletionEvent* completion,
                         LayerTreeHost* layer_tree_host,
                         base::TimeTicks main_thread_start_time,
                         bool hold_commit_for_activation) override;
  void InitializeImplOnImpl(CompletionEvent* completion,
                            LayerTreeHost* layer_tree_host) override;
  void LayerTreeHostClosedOnImpl(CompletionEvent* completion) override;

  // Returns |true| if the request was actually sent, |false| if one was
  // already outstanding.
  bool SendCommitRequestToImplThreadIfNeeded(
      CommitPipelineStage required_stage);

  // Called on impl thread.
  struct SchedulerStateRequest;

  DrawResult DrawSwapInternal(bool forced_draw);

  TaskRunnerProvider* task_runner_provider_;

  // Use accessors instead of this variable directly.
  MainThreadOnly main_thread_only_vars_unsafe_;
  MainThreadOnly& main();

  // Use accessors instead of this variable directly.
  BlockedMainCommitOnly main_thread_blocked_commit_vars_unsafe_;
  BlockedMainCommitOnly& blocked_main_commit();

  // Use accessors instead of this variable directly.
  CompositorThreadOnly compositor_thread_vars_unsafe_;
  CompositorThreadOnly& impl();

  // TODO(khushalsagar): Remove this. Temporary variable to hold the channel.
  scoped_ptr<ThreadedChannel> threaded_channel_;

  base::WeakPtr<ThreadProxy> main_thread_weak_ptr_;
  base::WeakPtr<ThreadProxy> impl_thread_weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ThreadProxy);
};

}  // namespace cc

#endif  // CC_TREES_THREAD_PROXY_H_
