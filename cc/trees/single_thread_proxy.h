// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_SINGLE_THREAD_PROXY_H_
#define CC_TREES_SINGLE_THREAD_PROXY_H_

#include <limits>

#include "base/time/time.h"
#include "cc/animation/animation_events.h"
#include "cc/output/begin_frame_args.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/proxy.h"
#include "cc/trees/proxy_timing_history.h"

namespace cc {

class ContextProvider;
class LayerTreeHost;
class LayerTreeHostSingleThreadClient;

class CC_EXPORT SingleThreadProxy : public Proxy,
                                    NON_EXPORTED_BASE(LayerTreeHostImplClient),
                                    SchedulerClient {
 public:
  static scoped_ptr<Proxy> Create(
      LayerTreeHost* layer_tree_host,
      LayerTreeHostSingleThreadClient* client,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);
  virtual ~SingleThreadProxy();

  // Proxy implementation
  virtual void FinishAllRendering() OVERRIDE;
  virtual bool IsStarted() const OVERRIDE;
  virtual void SetLayerTreeHostClientReady() OVERRIDE;
  virtual void SetVisible(bool visible) OVERRIDE;
  virtual const RendererCapabilities& GetRendererCapabilities() const OVERRIDE;
  virtual void SetNeedsAnimate() OVERRIDE;
  virtual void SetNeedsUpdateLayers() OVERRIDE;
  virtual void SetNeedsCommit() OVERRIDE;
  virtual void SetNeedsRedraw(const gfx::Rect& damage_rect) OVERRIDE;
  virtual void SetNextCommitWaitsForActivation() OVERRIDE;
  virtual void NotifyInputThrottledUntilCommit() OVERRIDE {}
  virtual void SetDeferCommits(bool defer_commits) OVERRIDE;
  virtual bool CommitRequested() const OVERRIDE;
  virtual bool BeginMainFrameRequested() const OVERRIDE;
  virtual void MainThreadHasStoppedFlinging() OVERRIDE {}
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual size_t MaxPartialTextureUpdates() const OVERRIDE;
  virtual void ForceSerializeOnSwapBuffers() OVERRIDE;
  virtual bool SupportsImplScrolling() const OVERRIDE;
  virtual void AsValueInto(base::debug::TracedValue* state) const OVERRIDE;
  virtual bool MainFrameWillHappenForTesting() OVERRIDE;

  // SchedulerClient implementation
  virtual void SetNeedsBeginFrame(bool enable) OVERRIDE;
  virtual void WillBeginImplFrame(const BeginFrameArgs& args) OVERRIDE;
  virtual void ScheduledActionSendBeginMainFrame() OVERRIDE;
  virtual DrawResult ScheduledActionDrawAndSwapIfPossible() OVERRIDE;
  virtual DrawResult ScheduledActionDrawAndSwapForced() OVERRIDE;
  virtual void ScheduledActionCommit() OVERRIDE;
  virtual void ScheduledActionAnimate() OVERRIDE;
  virtual void ScheduledActionUpdateVisibleTiles() OVERRIDE;
  virtual void ScheduledActionActivateSyncTree() OVERRIDE;
  virtual void ScheduledActionBeginOutputSurfaceCreation() OVERRIDE;
  virtual void ScheduledActionManageTiles() OVERRIDE;
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks time) OVERRIDE;
  virtual base::TimeDelta DrawDurationEstimate() OVERRIDE;
  virtual base::TimeDelta BeginMainFrameToCommitDurationEstimate() OVERRIDE;
  virtual base::TimeDelta CommitToActivateDurationEstimate() OVERRIDE;
  virtual void DidBeginImplFrameDeadline() OVERRIDE;

  // LayerTreeHostImplClient implementation
  virtual void UpdateRendererCapabilitiesOnImplThread() OVERRIDE;
  virtual void DidLoseOutputSurfaceOnImplThread() OVERRIDE;
  virtual void CommitVSyncParameters(base::TimeTicks timebase,
                                     base::TimeDelta interval) OVERRIDE {}
  virtual void SetEstimatedParentDrawTime(base::TimeDelta draw_time) OVERRIDE {}
  virtual void SetMaxSwapsPendingOnImplThread(int max) OVERRIDE {}
  virtual void DidSwapBuffersOnImplThread() OVERRIDE;
  virtual void DidSwapBuffersCompleteOnImplThread() OVERRIDE;
  virtual void BeginFrame(const BeginFrameArgs& args) OVERRIDE;
  virtual void OnCanDrawStateChanged(bool can_draw) OVERRIDE;
  virtual void NotifyReadyToActivate() OVERRIDE;
  virtual void SetNeedsRedrawOnImplThread() OVERRIDE;
  virtual void SetNeedsRedrawRectOnImplThread(
      const gfx::Rect& dirty_rect) OVERRIDE;
  virtual void SetNeedsAnimateOnImplThread() OVERRIDE;
  virtual void SetNeedsManageTilesOnImplThread() OVERRIDE;
  virtual void DidInitializeVisibleTileOnImplThread() OVERRIDE;
  virtual void SetNeedsCommitOnImplThread() OVERRIDE;
  virtual void PostAnimationEventsToMainThreadOnImplThread(
      scoped_ptr<AnimationEventsVector> events) OVERRIDE;
  virtual bool ReduceContentsTextureMemoryOnImplThread(
      size_t limit_bytes,
      int priority_cutoff) OVERRIDE;
  virtual bool IsInsideDraw() OVERRIDE;
  virtual void RenewTreePriority() OVERRIDE {}
  virtual void PostDelayedScrollbarFadeOnImplThread(
      const base::Closure& start_fade,
      base::TimeDelta delay) OVERRIDE {}
  virtual void DidActivateSyncTree() OVERRIDE {}
  virtual void DidManageTiles() OVERRIDE;
  virtual void SetDebugState(const LayerTreeDebugState& debug_state) OVERRIDE {}

  // Attempts to create the context and renderer synchronously. Calls
  // LayerTreeHost::OnCreateAndInitializeOutputSurfaceAttempted with the result.
  void CreateAndInitializeOutputSurface();

  // Called by the legacy path where RenderWidget does the scheduling.
  void CompositeImmediately(base::TimeTicks frame_begin_time);

 private:
  SingleThreadProxy(
      LayerTreeHost* layer_tree_host,
      LayerTreeHostSingleThreadClient* client,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);

  void BeginMainFrame();
  void BeginMainFrameAbortedOnImplThread();
  void DoCommit(const BeginFrameArgs& begin_frame_args);
  DrawResult DoComposite(base::TimeTicks frame_begin_time,
                         LayerTreeHostImpl::FrameData* frame);
  void DoSwap();
  void DidCommitAndDrawFrame();

  bool ShouldComposite() const;
  void UpdateBackgroundAnimateTicking();

  // Accessed on main thread only.
  LayerTreeHost* layer_tree_host_;
  LayerTreeHostSingleThreadClient* client_;

  // Used on the Thread, but checked on main thread during
  // initialization/shutdown.
  scoped_ptr<LayerTreeHostImpl> layer_tree_host_impl_;
  RendererCapabilities renderer_capabilities_for_main_thread_;

  // Accessed from both threads.
  scoped_ptr<Scheduler> scheduler_on_impl_thread_;
  ProxyTimingHistory timing_history_;

  bool next_frame_is_newly_committed_frame_;

  bool inside_draw_;
  bool defer_commits_;
  bool commit_was_deferred_;
  bool commit_requested_;

  base::WeakPtrFactory<SingleThreadProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SingleThreadProxy);
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the impl thread to satisfy assertion checks.
class DebugScopedSetImplThread {
 public:
  explicit DebugScopedSetImplThread(Proxy* proxy) : proxy_(proxy) {
#if DCHECK_IS_ON
    previous_value_ = proxy_->impl_thread_is_overridden_;
    proxy_->SetCurrentThreadIsImplThread(true);
#endif
  }
  ~DebugScopedSetImplThread() {
#if DCHECK_IS_ON
    proxy_->SetCurrentThreadIsImplThread(previous_value_);
#endif
  }

 private:
  bool previous_value_;
  Proxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(DebugScopedSetImplThread);
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the main thread to satisfy assertion checks.
class DebugScopedSetMainThread {
 public:
  explicit DebugScopedSetMainThread(Proxy* proxy) : proxy_(proxy) {
#if DCHECK_IS_ON
    previous_value_ = proxy_->impl_thread_is_overridden_;
    proxy_->SetCurrentThreadIsImplThread(false);
#endif
  }
  ~DebugScopedSetMainThread() {
#if DCHECK_IS_ON
    proxy_->SetCurrentThreadIsImplThread(previous_value_);
#endif
  }

 private:
  bool previous_value_;
  Proxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(DebugScopedSetMainThread);
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the impl thread and that the main thread is blocked to
// satisfy assertion checks
class DebugScopedSetImplThreadAndMainThreadBlocked {
 public:
  explicit DebugScopedSetImplThreadAndMainThreadBlocked(Proxy* proxy)
      : impl_thread_(proxy), main_thread_blocked_(proxy) {}

 private:
  DebugScopedSetImplThread impl_thread_;
  DebugScopedSetMainThreadBlocked main_thread_blocked_;

  DISALLOW_COPY_AND_ASSIGN(DebugScopedSetImplThreadAndMainThreadBlocked);
};

}  // namespace cc

#endif  // CC_TREES_SINGLE_THREAD_PROXY_H_
