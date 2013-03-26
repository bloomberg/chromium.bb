// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_THREAD_PROXY_H_
#define CC_TREES_THREAD_PROXY_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "cc/animation/animation_events.h"
#include "cc/base/completion_event.h"
#include "cc/resources/resource_update_controller.h"
#include "cc/scheduler/scheduler.h"
#include "cc/scheduler/vsync_time_source.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/proxy.h"

namespace cc {

class ContextProvider;
class InputHandler;
class LayerTreeHost;
class ResourceUpdateQueue;
class Scheduler;
class ScopedThreadProxy;
class Thread;

class ThreadProxy : public Proxy,
                    LayerTreeHostImplClient,
                    SchedulerClient,
                    ResourceUpdateControllerClient,
                    VSyncProvider {
 public:
  static scoped_ptr<Proxy> Create(LayerTreeHost* layer_tree_host,
                                  scoped_ptr<Thread> impl_thread);

  virtual ~ThreadProxy();

  // Proxy implementation
  virtual bool CompositeAndReadback(void* pixels, gfx::Rect rect) OVERRIDE;
  virtual void StartPageScaleAnimation(gfx::Vector2d target_offset,
                                       bool use_anchor,
                                       float scale,
                                       base::TimeDelta duration) OVERRIDE;
  virtual void FinishAllRendering() OVERRIDE;
  virtual bool IsStarted() const OVERRIDE;
  virtual bool InitializeOutputSurface() OVERRIDE;
  virtual void SetSurfaceReady() OVERRIDE;
  virtual void SetVisible(bool visible) OVERRIDE;
  virtual bool InitializeRenderer() OVERRIDE;
  virtual bool RecreateOutputSurface() OVERRIDE;
  virtual const RendererCapabilities& GetRendererCapabilities() const OVERRIDE;
  virtual void SetNeedsAnimate() OVERRIDE;
  virtual void SetNeedsCommit() OVERRIDE;
  virtual void SetNeedsRedraw() OVERRIDE;
  virtual void SetDeferCommits(bool defer_commits) OVERRIDE;
  virtual bool CommitRequested() const OVERRIDE;
  virtual void MainThreadHasStoppedFlinging() OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual size_t MaxPartialTextureUpdates() const OVERRIDE;
  virtual void AcquireLayerTextures() OVERRIDE;
  virtual void ForceSerializeOnSwapBuffers() OVERRIDE;
  virtual skia::RefPtr<SkPicture> CapturePicture() OVERRIDE;
  virtual scoped_ptr<base::Value> AsValue() const OVERRIDE;
  virtual bool CommitPendingForTesting() OVERRIDE;

  // LayerTreeHostImplClient implementation
  virtual void DidLoseOutputSurfaceOnImplThread() OVERRIDE;
  virtual void OnSwapBuffersCompleteOnImplThread() OVERRIDE;
  virtual void OnVSyncParametersChanged(base::TimeTicks timebase,
                                        base::TimeDelta interval) OVERRIDE;
  virtual void DidVSync(base::TimeTicks frame_time) OVERRIDE;
  virtual void OnCanDrawStateChanged(bool can_draw) OVERRIDE;
  virtual void OnHasPendingTreeStateChanged(bool has_pending_tree) OVERRIDE;
  virtual void SetNeedsRedrawOnImplThread() OVERRIDE;
  virtual void DidInitializeVisibleTileOnImplThread() OVERRIDE;
  virtual void SetNeedsCommitOnImplThread() OVERRIDE;
  virtual void SetNeedsManageTilesOnImplThread() OVERRIDE;
  virtual void PostAnimationEventsToMainThreadOnImplThread(
      scoped_ptr<AnimationEventsVector> queue,
      base::Time wall_clock_time) OVERRIDE;
  virtual bool ReduceContentsTextureMemoryOnImplThread(size_t limit_bytes,
                                                       int priority_cutoff)
      OVERRIDE;
  virtual void ReduceWastedContentsTextureMemoryOnImplThread() OVERRIDE;
  virtual void SendManagedMemoryStats() OVERRIDE;
  virtual bool IsInsideDraw() OVERRIDE;
  virtual void RenewTreePriority() OVERRIDE;
  virtual void RequestScrollbarAnimationOnImplThread(base::TimeDelta delay)
      OVERRIDE;

  // SchedulerClient implementation
  virtual void ScheduledActionBeginFrame() OVERRIDE;
  virtual ScheduledActionDrawAndSwapResult
      ScheduledActionDrawAndSwapIfPossible() OVERRIDE;
  virtual ScheduledActionDrawAndSwapResult ScheduledActionDrawAndSwapForced()
      OVERRIDE;
  virtual void ScheduledActionCommit() OVERRIDE;
  virtual void ScheduledActionCheckForCompletedTileUploads() OVERRIDE;
  virtual void ScheduledActionActivatePendingTreeIfNeeded() OVERRIDE;
  virtual void ScheduledActionBeginContextRecreation() OVERRIDE;
  virtual void ScheduledActionAcquireLayerTexturesForMainThread() OVERRIDE;
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks time) OVERRIDE;

  // ResourceUpdateControllerClient implementation
  virtual void ReadyToFinalizeTextureUpdates() OVERRIDE;

  // VSyncProvider implementation
  virtual void RequestVSyncNotification(VSyncClient* client) OVERRIDE;

  int MaxFramesPendingForTesting() const {
    return scheduler_on_impl_thread_->MaxFramesPending();
  }

 private:
  ThreadProxy(LayerTreeHost* layer_tree_host, scoped_ptr<Thread> impl_thread);

  struct BeginFrameAndCommitState {
    BeginFrameAndCommitState();
    ~BeginFrameAndCommitState();

    base::TimeTicks monotonic_frame_begin_time;
    scoped_ptr<ScrollAndScaleSet> scroll_info;
    gfx::Transform impl_transform;
    size_t memory_allocation_limit_bytes;
  };

  // Called on main thread.
  void BeginFrame(scoped_ptr<BeginFrameAndCommitState> begin_frame_state);
  void DidCommitAndDrawFrame();
  void DidCompleteSwapBuffers();
  void SetAnimationEvents(scoped_ptr<AnimationEventsVector> queue,
                          base::Time wall_clock_time);
  void BeginContextRecreation();
  void TryToRecreateOutputSurface();

  // Called on impl thread.
  struct ReadbackRequest {
    CompletionEvent completion;
    bool success;
    void* pixels;
    gfx::Rect rect;
  };
  struct CommitPendingRequest {
    CompletionEvent completion;
    bool commit_pending;
  };
  void ForceBeginFrameOnImplThread(CompletionEvent* completion);
  void BeginFrameCompleteOnImplThread(
      CompletionEvent* completion,
      ResourceUpdateQueue* queue,
      scoped_refptr<cc::ContextProvider> offscreen_context_provider);
  void BeginFrameAbortedOnImplThread();
  void RequestReadbackOnImplThread(ReadbackRequest* request);
  void RequestStartPageScaleAnimationOnImplThread(gfx::Vector2d target_offset,
                                                  bool use_anchor,
                                                  float scale,
                                                  base::TimeDelta duration);
  void FinishAllRenderingOnImplThread(CompletionEvent* completion);
  void InitializeImplOnImplThread(CompletionEvent* completion,
                                  InputHandler* input_handler);
  void SetSurfaceReadyOnImplThread();
  void SetVisibleOnImplThread(CompletionEvent* completion, bool visible);
  void InitializeOutputSurfaceOnImplThread(
      scoped_ptr<OutputSurface> output_surface);
  void InitializeRendererOnImplThread(CompletionEvent* completion,
                                      bool* initialize_succeeded,
                                      RendererCapabilities* capabilities);
  void LayerTreeHostClosedOnImplThread(CompletionEvent* completion);
  void ManageTilesOnImplThread();
  void SetFullRootLayerDamageOnImplThread();
  void AcquireLayerTexturesForMainThreadOnImplThread(
      CompletionEvent* completion);
  void RecreateOutputSurfaceOnImplThread(
      CompletionEvent* completion,
      scoped_ptr<OutputSurface> output_surface,
      scoped_refptr<cc::ContextProvider> offscreen_context_provider,
      bool* recreate_succeeded,
      RendererCapabilities* capabilities);
  ScheduledActionDrawAndSwapResult ScheduledActionDrawAndSwapInternal(
      bool forced_draw);
  void ForceSerializeOnSwapBuffersOnImplThread(CompletionEvent* completion);
  void SetNeedsForcedCommitOnImplThread();
  void CheckOutputSurfaceStatusOnImplThread();
  void CommitPendingOnImplThreadForTesting(CommitPendingRequest* request);
  void CapturePictureOnImplThread(CompletionEvent* completion,
                                  skia::RefPtr<SkPicture>* picture);
  void AsValueOnImplThread(CompletionEvent* completion,
                           base::DictionaryValue* state) const;
  void RenewTreePriorityOnImplThread();
  void DidSwapUseIncompleteTileOnImplThread();
  void StartScrollbarAnimationOnImplThread();

  // Accessed on main thread only.

  // Set only when SetNeedsAnimate is called.
  bool animate_requested_;
  // Set only when SetNeedsCommit is called.
  bool commit_requested_;
  // Set by SetNeedsCommit and SetNeedsAnimate.
  bool commit_request_sent_to_impl_thread_;
  // Set by BeginFrame
  bool created_offscreen_context_provider_;
  base::CancelableClosure output_surface_recreation_callback_;
  LayerTreeHost* layer_tree_host_;
  bool renderer_initialized_;
  RendererCapabilities renderer_capabilities_main_thread_copy_;
  bool started_;
  bool textures_acquired_;
  bool in_composite_and_readback_;
  bool manage_tiles_pending_;
  // Weak pointer to use when posting tasks to the impl thread.
  base::WeakPtr<ThreadProxy> impl_thread_weak_ptr_;

  base::WeakPtrFactory<ThreadProxy> weak_factory_on_impl_thread_;

  base::WeakPtr<ThreadProxy> main_thread_weak_ptr_;
  base::WeakPtrFactory<ThreadProxy> weak_factory_;

  scoped_ptr<LayerTreeHostImpl> layer_tree_host_impl_;

  scoped_ptr<InputHandler> input_handler_on_impl_thread_;

  scoped_ptr<Scheduler> scheduler_on_impl_thread_;

  // Holds on to the context we might use for compositing in between
  // InitializeContext() and InitializeRenderer() calls.
  scoped_ptr<OutputSurface>
      output_surface_before_initialization_on_impl_thread_;

  // Set when the main thread is waiting on a ScheduledActionBeginFrame to be
  // issued.
  CompletionEvent* begin_frame_completion_event_on_impl_thread_;

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

  bool render_vsync_enabled_;
  bool render_vsync_notification_enabled_;
  VSyncClient* vsync_client_;

  bool inside_draw_;

  bool defer_commits_;
  scoped_ptr<BeginFrameAndCommitState> pending_deferred_commit_;

  base::TimeTicks smoothness_takes_priority_expiration_time_;
  bool renew_tree_priority_on_impl_thread_pending_;
};

}  // namespace cc

#endif  // CC_TREES_THREAD_PROXY_H_
