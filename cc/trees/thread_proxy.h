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
#include "cc/resources/resource_update_controller.h"
#include "cc/scheduler/rolling_time_delta_history.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/proxy.h"

namespace base { class SingleThreadTaskRunner; }

namespace cc {

class ContextProvider;
class InputHandlerClient;
class LayerTreeHost;
class ResourceUpdateQueue;
class Scheduler;
class ScopedThreadProxy;

class ThreadProxy : public Proxy,
                    LayerTreeHostImplClient,
                    SchedulerClient,
                    ResourceUpdateControllerClient {
 public:
  static scoped_ptr<Proxy> Create(
      LayerTreeHost* layer_tree_host,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);

  virtual ~ThreadProxy();

  // Proxy implementation
  virtual bool CompositeAndReadback(void* pixels, gfx::Rect rect) OVERRIDE;
  virtual void FinishAllRendering() OVERRIDE;
  virtual bool IsStarted() const OVERRIDE;
  virtual void SetLayerTreeHostClientReady() OVERRIDE;
  virtual void SetVisible(bool visible) OVERRIDE;
  virtual void CreateAndInitializeOutputSurface() OVERRIDE;
  virtual const RendererCapabilities& GetRendererCapabilities() const OVERRIDE;
  virtual void SetNeedsAnimate() OVERRIDE;
  virtual void SetNeedsUpdateLayers() OVERRIDE;
  virtual void SetNeedsCommit() OVERRIDE;
  virtual void SetNeedsRedraw(gfx::Rect damage_rect) OVERRIDE;
  virtual void SetNextCommitWaitsForActivation() OVERRIDE;
  virtual void NotifyInputThrottledUntilCommit() OVERRIDE;
  virtual void SetDeferCommits(bool defer_commits) OVERRIDE;
  virtual bool CommitRequested() const OVERRIDE;
  virtual bool BeginMainFrameRequested() const OVERRIDE;
  virtual void MainThreadHasStoppedFlinging() OVERRIDE;
  virtual void Start(scoped_ptr<OutputSurface> first_output_surface) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual size_t MaxPartialTextureUpdates() const OVERRIDE;
  virtual void AcquireLayerTextures() OVERRIDE;
  virtual void ForceSerializeOnSwapBuffers() OVERRIDE;
  virtual scoped_ptr<base::Value> AsValue() const OVERRIDE;
  virtual bool CommitPendingForTesting() OVERRIDE;
  virtual scoped_ptr<base::Value> SchedulerStateAsValueForTesting() OVERRIDE;

  // LayerTreeHostImplClient implementation
  virtual void DidLoseOutputSurfaceOnImplThread() OVERRIDE;
  virtual void OnSwapBuffersCompleteOnImplThread() OVERRIDE;
  virtual void BeginImplFrame(const BeginFrameArgs& args) OVERRIDE;
  virtual void OnCanDrawStateChanged(bool can_draw) OVERRIDE;
  virtual void NotifyReadyToActivate() OVERRIDE;
  virtual void SetNeedsRedrawOnImplThread() OVERRIDE;
  virtual void SetNeedsRedrawRectOnImplThread(gfx::Rect dirty_rect) OVERRIDE;
  virtual void SetNeedsManageTilesOnImplThread() OVERRIDE;
  virtual void DidInitializeVisibleTileOnImplThread() OVERRIDE;
  virtual void SetNeedsCommitOnImplThread() OVERRIDE;
  virtual void PostAnimationEventsToMainThreadOnImplThread(
      scoped_ptr<AnimationEventsVector> queue,
      base::Time wall_clock_time) OVERRIDE;
  virtual bool ReduceContentsTextureMemoryOnImplThread(size_t limit_bytes,
                                                       int priority_cutoff)
      OVERRIDE;
  virtual void SendManagedMemoryStats() OVERRIDE;
  virtual bool IsInsideDraw() OVERRIDE;
  virtual void RenewTreePriority() OVERRIDE;
  virtual void RequestScrollbarAnimationOnImplThread(base::TimeDelta delay)
      OVERRIDE;
  virtual void DidActivatePendingTree() OVERRIDE;
  virtual void DidManageTiles() OVERRIDE;

  // SchedulerClient implementation
  virtual void SetNeedsBeginImplFrame(bool enable) OVERRIDE;
  virtual void ScheduledActionSendBeginMainFrame() OVERRIDE;
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapIfPossible()
      OVERRIDE;
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapForced() OVERRIDE;
  virtual DrawSwapReadbackResult ScheduledActionDrawAndReadback() OVERRIDE;
  virtual void ScheduledActionCommit() OVERRIDE;
  virtual void ScheduledActionUpdateVisibleTiles() OVERRIDE;
  virtual void ScheduledActionActivatePendingTree() OVERRIDE;
  virtual void ScheduledActionBeginOutputSurfaceCreation() OVERRIDE;
  virtual void ScheduledActionAcquireLayerTexturesForMainThread() OVERRIDE;
  virtual void ScheduledActionManageTiles() OVERRIDE;
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks time) OVERRIDE;
  virtual base::TimeDelta DrawDurationEstimate() OVERRIDE;
  virtual base::TimeDelta BeginMainFrameToCommitDurationEstimate() OVERRIDE;
  virtual base::TimeDelta CommitToActivateDurationEstimate() OVERRIDE;
  virtual void PostBeginImplFrameDeadline(const base::Closure& closure,
                                          base::TimeTicks deadline) OVERRIDE;
  virtual void DidBeginImplFrameDeadline() OVERRIDE;

  // ResourceUpdateControllerClient implementation
  virtual void ReadyToFinalizeTextureUpdates() OVERRIDE;

 private:
  ThreadProxy(LayerTreeHost* layer_tree_host,
              scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);

  struct BeginMainFrameAndCommitState {
    BeginMainFrameAndCommitState();
    ~BeginMainFrameAndCommitState();

    base::TimeTicks monotonic_frame_begin_time;
    scoped_ptr<ScrollAndScaleSet> scroll_info;
    size_t memory_allocation_limit_bytes;
    int memory_allocation_priority_cutoff;
    bool evicted_ui_resources;
  };

  // Called on main thread.
  void BeginMainFrame(
      scoped_ptr<BeginMainFrameAndCommitState> begin_main_frame_state);
  void DidCommitAndDrawFrame();
  void DidCompleteSwapBuffers();
  void SetAnimationEvents(scoped_ptr<AnimationEventsVector> queue,
                          base::Time wall_clock_time);
  void DoCreateAndInitializeOutputSurface();
  // |capabilities| is set only when |success| is true.
  void OnOutputSurfaceInitializeAttempted(
      bool success,
      const RendererCapabilities& capabilities);
  void SendCommitRequestToImplThreadIfNeeded();

  // Called on impl thread.
  struct ReadbackRequest;
  struct CommitPendingRequest;
  struct SchedulerStateRequest;

  void ForceCommitForReadbackOnImplThread(
      CompletionEvent* begin_main_frame_sent_completion,
      ReadbackRequest* request);
  void StartCommitOnImplThread(
      CompletionEvent* completion,
      ResourceUpdateQueue* queue,
      scoped_refptr<cc::ContextProvider> offscreen_context_provider);
  void BeginMainFrameAbortedOnImplThread(bool did_handle);
  void RequestReadbackOnImplThread(ReadbackRequest* request);
  void FinishAllRenderingOnImplThread(CompletionEvent* completion);
  void InitializeImplOnImplThread(CompletionEvent* completion);
  void SetLayerTreeHostClientReadyOnImplThread();
  void SetVisibleOnImplThread(CompletionEvent* completion, bool visible);
  void UpdateBackgroundAnimateTicking();
  void HasInitializedOutputSurfaceOnImplThread(
      CompletionEvent* completion,
      bool* has_initialized_output_surface);
  void InitializeOutputSurfaceOnImplThread(
      CompletionEvent* completion,
      scoped_ptr<OutputSurface> output_surface,
      scoped_refptr<ContextProvider> offscreen_context_provider,
      bool* success,
      RendererCapabilities* capabilities);
  void FinishGLOnImplThread(CompletionEvent* completion);
  void LayerTreeHostClosedOnImplThread(CompletionEvent* completion);
  void AcquireLayerTexturesForMainThreadOnImplThread(
      CompletionEvent* completion);
  DrawSwapReadbackResult DrawSwapReadbackInternal(bool forced_draw,
                                                  bool swap_requested,
                                                  bool readback_requested);
  void ForceSerializeOnSwapBuffersOnImplThread(CompletionEvent* completion);
  void CheckOutputSurfaceStatusOnImplThread();
  void CommitPendingOnImplThreadForTesting(CommitPendingRequest* request);
  void SchedulerStateAsValueOnImplThreadForTesting(
      SchedulerStateRequest* request);
  void AsValueOnImplThread(CompletionEvent* completion,
                           base::DictionaryValue* state) const;
  void RenewTreePriorityOnImplThread();
  void SetSwapUsedIncompleteTileOnImplThread(bool used_incomplete_tile);
  void StartScrollbarAnimationOnImplThread();
  void MainThreadHasStoppedFlingingOnImplThread();
  void SetInputThrottledUntilCommitOnImplThread(bool is_throttled);

  // Accessed on main thread only.

  // Set only when SetNeedsAnimate is called.
  bool animate_requested_;
  // Set only when SetNeedsCommit is called.
  bool commit_requested_;
  // Set by SetNeedsAnimate, SetNeedsUpdateLayers, and SetNeedsCommit.
  bool commit_request_sent_to_impl_thread_;
  // Set by BeginMainFrame
  bool created_offscreen_context_provider_;
  base::CancelableClosure output_surface_creation_callback_;
  LayerTreeHost* layer_tree_host_;
  RendererCapabilities renderer_capabilities_main_thread_copy_;
  bool started_;
  bool textures_acquired_;
  bool in_composite_and_readback_;
  bool manage_tiles_pending_;
  // Weak pointer to use when posting tasks to the impl thread.
  base::WeakPtr<ThreadProxy> impl_thread_weak_ptr_;
  // Holds the first output surface passed from Start. Should not be used for
  // anything else.
  scoped_ptr<OutputSurface> first_output_surface_;

  // Accessed on the main thread, or when main thread is blocked.
  bool commit_waits_for_activation_;
  bool inside_commit_;

  scoped_ptr<LayerTreeHostImpl> layer_tree_host_impl_;

  scoped_ptr<Scheduler> scheduler_on_impl_thread_;

  // Set when the main thread is waiting on a
  // ScheduledActionSendBeginMainFrame to be issued.
  CompletionEvent*
      begin_main_frame_sent_completion_event_on_impl_thread_;

  // Set when the main thread is waiting on a readback.
  ReadbackRequest* readback_request_on_impl_thread_;

  // Set when the main thread is waiting on a commit to complete.
  CompletionEvent* commit_completion_event_on_impl_thread_;

  // Set when the main thread is waiting on a pending tree activation.
  CompletionEvent* completion_event_for_commit_held_on_tree_activation_;

  // Set when the main thread is waiting on layers to be drawn.
  CompletionEvent* texture_acquisition_completion_event_on_impl_thread_;

  scoped_ptr<ResourceUpdateController>
      current_resource_update_controller_on_impl_thread_;

  // Set when the next draw should post DidCommitAndDrawFrame to the main
  // thread.
  bool next_frame_is_newly_committed_frame_on_impl_thread_;

  bool throttle_frame_production_;
  bool begin_impl_frame_scheduling_enabled_;
  bool using_synchronous_renderer_compositor_;

  bool inside_draw_;

  bool can_cancel_commit_;

  bool defer_commits_;
  bool input_throttled_until_commit_;
  scoped_ptr<BeginMainFrameAndCommitState> pending_deferred_commit_;

  base::TimeTicks smoothness_takes_priority_expiration_time_;
  bool renew_tree_priority_on_impl_thread_pending_;

  RollingTimeDeltaHistory draw_duration_history_;
  RollingTimeDeltaHistory begin_main_frame_to_commit_duration_history_;
  RollingTimeDeltaHistory commit_to_activate_duration_history_;

  // Used for computing samples added to
  // begin_main_frame_to_commit_duration_history_ and
  // activation_duration_history_.
  base::TimeTicks begin_main_frame_sent_time_;
  base::TimeTicks commit_complete_time_;

  base::WeakPtr<ThreadProxy> main_thread_weak_ptr_;
  base::WeakPtrFactory<ThreadProxy> weak_factory_on_impl_thread_;
  base::WeakPtrFactory<ThreadProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThreadProxy);
};

}  // namespace cc

#endif  // CC_TREES_THREAD_PROXY_H_
