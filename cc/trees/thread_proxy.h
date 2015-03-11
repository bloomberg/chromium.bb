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
#include "cc/resources/resource_update_controller.h"
#include "cc/scheduler/commit_earlyout_reason.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/proxy.h"
#include "cc/trees/proxy_timing_history.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {

class BeginFrameSource;
class ContextProvider;
class InputHandlerClient;
class LayerTreeHost;
class ResourceUpdateQueue;
class Scheduler;
class ScopedThreadProxy;

class CC_EXPORT ThreadProxy : public Proxy,
                    NON_EXPORTED_BASE(LayerTreeHostImplClient),
                    NON_EXPORTED_BASE(SchedulerClient),
                    NON_EXPORTED_BASE(ResourceUpdateControllerClient) {
 public:
  static scoped_ptr<Proxy> Create(
      LayerTreeHost* layer_tree_host,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      scoped_ptr<BeginFrameSource> external_begin_frame_source);

  ~ThreadProxy() override;

  struct BeginMainFrameAndCommitState {
    BeginMainFrameAndCommitState();
    ~BeginMainFrameAndCommitState();

    unsigned int begin_frame_id;
    BeginFrameArgs begin_frame_args;
    scoped_ptr<ScrollAndScaleSet> scroll_info;
    size_t memory_allocation_limit_bytes;
    int memory_allocation_priority_cutoff;
    bool evicted_ui_resources;
  };

  struct MainThreadOnly {
    MainThreadOnly(ThreadProxy* proxy, int layer_tree_host_id);
    ~MainThreadOnly();

    const int layer_tree_host_id;

    // Set only when SetNeedsAnimate is called.
    bool animate_requested;
    // Set only when SetNeedsCommit is called.
    bool commit_requested;
    // Set by SetNeedsAnimate, SetNeedsUpdateLayers, and SetNeedsCommit.
    bool commit_request_sent_to_impl_thread;

    bool started;
    bool prepare_tiles_pending;
    bool can_cancel_commit;
    bool defer_commits;

    RendererCapabilities renderer_capabilities_main_thread_copy;

    base::WeakPtrFactory<ThreadProxy> weak_factory;
  };

  // Accessed on the main thread, or when main thread is blocked.
  struct MainThreadOrBlockedMainThread {
    explicit MainThreadOrBlockedMainThread(LayerTreeHost* host);
    ~MainThreadOrBlockedMainThread();

    PrioritizedResourceManager* contents_texture_manager();

    LayerTreeHost* layer_tree_host;
    bool commit_waits_for_activation;
    bool main_thread_inside_commit;
  };

  struct CompositorThreadOnly {
    CompositorThreadOnly(
        ThreadProxy* proxy,
        int layer_tree_host_id,
        RenderingStatsInstrumentation* rendering_stats_instrumentation,
        scoped_ptr<BeginFrameSource> external_begin_frame_source);
    ~CompositorThreadOnly();

    const int layer_tree_host_id;

    // Copy of the main thread side contents texture manager for work
    // that needs to be done on the compositor thread.
    PrioritizedResourceManager* contents_texture_manager;

    scoped_ptr<Scheduler> scheduler;

    // Set when the main thread is waiting on a
    // ScheduledActionSendBeginMainFrame to be issued.
    CompletionEvent* begin_main_frame_sent_completion_event;

    // Set when the main thread is waiting on a commit to complete.
    CompletionEvent* commit_completion_event;

    // Set when the main thread is waiting on a pending tree activation.
    CompletionEvent* completion_event_for_commit_held_on_tree_activation;

    scoped_ptr<ResourceUpdateController> current_resource_update_controller;

    // Set when the next draw should post DidCommitAndDrawFrame to the main
    // thread.
    bool next_frame_is_newly_committed_frame;

    bool inside_draw;

    bool input_throttled_until_commit;

    base::TimeTicks animation_time;

    // Whether a commit has been completed since the last time animations were
    // ticked. If this happens, we need to animate again.
    bool did_commit_after_animating;

    DelayedUniqueNotifier smoothness_priority_expiration_notifier;

    ProxyTimingHistory timing_history;

    scoped_ptr<BeginFrameSource> external_begin_frame_source;

    scoped_ptr<LayerTreeHostImpl> layer_tree_host_impl;
    base::WeakPtrFactory<ThreadProxy> weak_factory;
  };

  const MainThreadOnly& main() const;
  const MainThreadOrBlockedMainThread& blocked_main() const;
  const CompositorThreadOnly& impl() const;

  // Proxy implementation
  void FinishAllRendering() override;
  bool IsStarted() const override;
  bool CommitToActiveTree() const override;
  void SetOutputSurface(scoped_ptr<OutputSurface>) override;
  void SetLayerTreeHostClientReady() override;
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
  size_t MaxPartialTextureUpdates() const override;
  void ForceSerializeOnSwapBuffers() override;
  bool SupportsImplScrolling() const override;
  void SetDebugState(const LayerTreeDebugState& debug_state) override;
  bool MainFrameWillHappenForTesting() override;
  void SetChildrenNeedBeginFrames(bool children_need_begin_frames) override;

  // LayerTreeHostImplClient implementation
  void UpdateRendererCapabilitiesOnImplThread() override;
  void DidLoseOutputSurfaceOnImplThread() override;
  void CommitVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval) override;
  void SetEstimatedParentDrawTime(base::TimeDelta draw_time) override;
  void SetMaxSwapsPendingOnImplThread(int max) override;
  void DidSwapBuffersOnImplThread() override;
  void DidSwapBuffersCompleteOnImplThread() override;
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
  void PostAnimationEventsToMainThreadOnImplThread(
      scoped_ptr<AnimationEventsVector> queue) override;
  bool ReduceContentsTextureMemoryOnImplThread(size_t limit_bytes,
                                               int priority_cutoff) override;
  bool IsInsideDraw() override;
  void RenewTreePriority() override;
  void PostDelayedAnimationTaskOnImplThread(const base::Closure& task,
                                            base::TimeDelta delay) override;
  void DidActivateSyncTree() override;
  void DidPrepareTiles() override;
  void DidCompletePageScaleAnimationOnImplThread() override;

  // SchedulerClient implementation
  void WillBeginImplFrame(const BeginFrameArgs& args) override;
  void ScheduledActionSendBeginMainFrame() override;
  DrawResult ScheduledActionDrawAndSwapIfPossible() override;
  DrawResult ScheduledActionDrawAndSwapForced() override;
  void ScheduledActionAnimate() override;
  void ScheduledActionCommit() override;
  void ScheduledActionActivateSyncTree() override;
  void ScheduledActionBeginOutputSurfaceCreation() override;
  void ScheduledActionPrepareTiles() override;
  void DidAnticipatedDrawTimeChange(base::TimeTicks time) override;
  base::TimeDelta DrawDurationEstimate() override;
  base::TimeDelta BeginMainFrameToCommitDurationEstimate() override;
  base::TimeDelta CommitToActivateDurationEstimate() override;
  void DidBeginImplFrameDeadline() override;
  void SendBeginFramesToChildren(const BeginFrameArgs& args) override;
  void SendBeginMainFrameNotExpectedSoon() override;

  // ResourceUpdateControllerClient implementation
  void ReadyToFinalizeTextureUpdates() override;

 protected:
  ThreadProxy(
      LayerTreeHost* layer_tree_host,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner,
      scoped_ptr<BeginFrameSource> external_begin_frame_source);

 private:
  // Called on main thread.
  void SetRendererCapabilitiesMainThreadCopy(
      const RendererCapabilities& capabilities);
  void BeginMainFrame(
      scoped_ptr<BeginMainFrameAndCommitState> begin_main_frame_state);
  void BeginMainFrameNotExpectedSoon();
  void DidCommitAndDrawFrame();
  void DidCompleteSwapBuffers();
  void SetAnimationEvents(scoped_ptr<AnimationEventsVector> queue);
  void DidLoseOutputSurface();
  void RequestNewOutputSurface();
  void DidInitializeOutputSurface(bool success,
                                  const RendererCapabilities& capabilities);
  void SendCommitRequestToImplThreadIfNeeded();
  void DidCompletePageScaleAnimation();

  // Called on impl thread.
  struct SchedulerStateRequest;

  void StartCommitOnImplThread(CompletionEvent* completion,
                               ResourceUpdateQueue* queue);
  void BeginMainFrameAbortedOnImplThread(CommitEarlyOutReason reason);
  void FinishAllRenderingOnImplThread(CompletionEvent* completion);
  void InitializeImplOnImplThread(CompletionEvent* completion);
  void SetLayerTreeHostClientReadyOnImplThread();
  void SetVisibleOnImplThread(CompletionEvent* completion, bool visible);
  void SetThrottleFrameProductionOnImplThread(bool throttle);
  void HasInitializedOutputSurfaceOnImplThread(
      CompletionEvent* completion,
      bool* has_initialized_output_surface);
  void DeleteContentsTexturesOnImplThread(CompletionEvent* completion);
  void InitializeOutputSurfaceOnImplThread(
      scoped_ptr<OutputSurface> output_surface);
  void FinishGLOnImplThread(CompletionEvent* completion);
  void LayerTreeHostClosedOnImplThread(CompletionEvent* completion);
  DrawResult DrawSwapInternal(bool forced_draw);
  void ForceSerializeOnSwapBuffersOnImplThread(CompletionEvent* completion);
  void MainFrameWillHappenOnImplThreadForTesting(CompletionEvent* completion,
                                                 bool* main_frame_will_happen);
  void SetSwapUsedIncompleteTileOnImplThread(bool used_incomplete_tile);
  void MainThreadHasStoppedFlingingOnImplThread();
  void SetInputThrottledUntilCommitOnImplThread(bool is_throttled);
  void SetDebugStateOnImplThread(const LayerTreeDebugState& debug_state);
  void SetDeferCommitsOnImplThread(bool defer_commits) const;

  LayerTreeHost* layer_tree_host();
  const LayerTreeHost* layer_tree_host() const;

  // Use accessors instead of this variable directly.
  MainThreadOnly main_thread_only_vars_unsafe_;
  MainThreadOnly& main();

  // Use accessors instead of this variable directly.
  MainThreadOrBlockedMainThread main_thread_or_blocked_vars_unsafe_;
  MainThreadOrBlockedMainThread& blocked_main();

  // Use accessors instead of this variable directly.
  CompositorThreadOnly compositor_thread_vars_unsafe_;
  CompositorThreadOnly& impl();

  base::WeakPtr<ThreadProxy> main_thread_weak_ptr_;
  base::WeakPtr<ThreadProxy> impl_thread_weak_ptr_;

  DISALLOW_COPY_AND_ASSIGN(ThreadProxy);
};

}  // namespace cc

#endif  // CC_TREES_THREAD_PROXY_H_
