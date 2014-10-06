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
  virtual void FinishAllRendering() override;
  virtual bool IsStarted() const override;
  virtual void SetOutputSurface(scoped_ptr<OutputSurface>) override;
  virtual void SetLayerTreeHostClientReady() override;
  virtual void SetVisible(bool visible) override;
  virtual const RendererCapabilities& GetRendererCapabilities() const override;
  virtual void SetNeedsAnimate() override;
  virtual void SetNeedsUpdateLayers() override;
  virtual void SetNeedsCommit() override;
  virtual void SetNeedsRedraw(const gfx::Rect& damage_rect) override;
  virtual void SetNextCommitWaitsForActivation() override;
  virtual void NotifyInputThrottledUntilCommit() override {}
  virtual void SetDeferCommits(bool defer_commits) override;
  virtual bool CommitRequested() const override;
  virtual bool BeginMainFrameRequested() const override;
  virtual void MainThreadHasStoppedFlinging() override {}
  virtual void Start() override;
  virtual void Stop() override;
  virtual size_t MaxPartialTextureUpdates() const override;
  virtual void ForceSerializeOnSwapBuffers() override;
  virtual bool SupportsImplScrolling() const override;
  virtual void AsValueInto(base::debug::TracedValue* state) const override;
  virtual bool MainFrameWillHappenForTesting() override;

  // SchedulerClient implementation
  virtual BeginFrameSource* ExternalBeginFrameSource() override;
  virtual void WillBeginImplFrame(const BeginFrameArgs& args) override;
  virtual void ScheduledActionSendBeginMainFrame() override;
  virtual DrawResult ScheduledActionDrawAndSwapIfPossible() override;
  virtual DrawResult ScheduledActionDrawAndSwapForced() override;
  virtual void ScheduledActionCommit() override;
  virtual void ScheduledActionAnimate() override;
  virtual void ScheduledActionUpdateVisibleTiles() override;
  virtual void ScheduledActionActivateSyncTree() override;
  virtual void ScheduledActionBeginOutputSurfaceCreation() override;
  virtual void ScheduledActionManageTiles() override;
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks time) override;
  virtual base::TimeDelta DrawDurationEstimate() override;
  virtual base::TimeDelta BeginMainFrameToCommitDurationEstimate() override;
  virtual base::TimeDelta CommitToActivateDurationEstimate() override;
  virtual void DidBeginImplFrameDeadline() override;

  // LayerTreeHostImplClient implementation
  virtual void UpdateRendererCapabilitiesOnImplThread() override;
  virtual void DidLoseOutputSurfaceOnImplThread() override;
  virtual void CommitVSyncParameters(base::TimeTicks timebase,
                                     base::TimeDelta interval) override {}
  virtual void SetEstimatedParentDrawTime(base::TimeDelta draw_time) override {}
  virtual void SetMaxSwapsPendingOnImplThread(int max) override {}
  virtual void DidSwapBuffersOnImplThread() override;
  virtual void DidSwapBuffersCompleteOnImplThread() override;
  virtual void OnCanDrawStateChanged(bool can_draw) override;
  virtual void NotifyReadyToActivate() override;
  virtual void SetNeedsRedrawOnImplThread() override;
  virtual void SetNeedsRedrawRectOnImplThread(
      const gfx::Rect& dirty_rect) override;
  virtual void SetNeedsAnimateOnImplThread() override;
  virtual void SetNeedsManageTilesOnImplThread() override;
  virtual void DidInitializeVisibleTileOnImplThread() override;
  virtual void SetNeedsCommitOnImplThread() override;
  virtual void PostAnimationEventsToMainThreadOnImplThread(
      scoped_ptr<AnimationEventsVector> events) override;
  virtual bool ReduceContentsTextureMemoryOnImplThread(
      size_t limit_bytes,
      int priority_cutoff) override;
  virtual bool IsInsideDraw() override;
  virtual void RenewTreePriority() override {}
  virtual void PostDelayedScrollbarFadeOnImplThread(
      const base::Closure& start_fade,
      base::TimeDelta delay) override {}
  virtual void DidActivateSyncTree() override {}
  virtual void DidManageTiles() override;
  virtual void SetDebugState(const LayerTreeDebugState& debug_state) override {}

  void RequestNewOutputSurface();

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
