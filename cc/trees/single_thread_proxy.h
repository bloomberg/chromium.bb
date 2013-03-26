// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_SINGLE_THREAD_PROXY_H_
#define CC_TREES_SINGLE_THREAD_PROXY_H_

#include <limits>

#include "base/time.h"
#include "cc/animation/animation_events.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/proxy.h"

namespace cc {

class ContextProvider;
class LayerTreeHost;

class SingleThreadProxy : public Proxy, LayerTreeHostImplClient {
 public:
  static scoped_ptr<Proxy> Create(LayerTreeHost* layer_tree_host);
  virtual ~SingleThreadProxy();

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
  virtual void MainThreadHasStoppedFlinging() OVERRIDE {}
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual size_t MaxPartialTextureUpdates() const OVERRIDE;
  virtual void AcquireLayerTextures() OVERRIDE {}
  virtual void ForceSerializeOnSwapBuffers() OVERRIDE;
  virtual skia::RefPtr<SkPicture> CapturePicture() OVERRIDE;
  virtual scoped_ptr<base::Value> AsValue() const OVERRIDE;
  virtual bool CommitPendingForTesting() OVERRIDE;

  // LayerTreeHostImplClient implementation
  virtual void DidLoseOutputSurfaceOnImplThread() OVERRIDE;
  virtual void OnSwapBuffersCompleteOnImplThread() OVERRIDE;
  virtual void OnVSyncParametersChanged(base::TimeTicks timebase,
                                        base::TimeDelta interval) OVERRIDE {}
  virtual void DidVSync(base::TimeTicks frame_time) OVERRIDE {}
  virtual void OnCanDrawStateChanged(bool can_draw) OVERRIDE {}
  virtual void OnHasPendingTreeStateChanged(bool have_pending_tree) OVERRIDE;
  virtual void SetNeedsRedrawOnImplThread() OVERRIDE;
  virtual void DidInitializeVisibleTileOnImplThread() OVERRIDE;
  virtual void SetNeedsCommitOnImplThread() OVERRIDE;
  virtual void SetNeedsManageTilesOnImplThread() OVERRIDE;
  virtual void PostAnimationEventsToMainThreadOnImplThread(
      scoped_ptr<AnimationEventsVector> events,
      base::Time wall_clock_time) OVERRIDE;
  virtual bool ReduceContentsTextureMemoryOnImplThread(
      size_t limit_bytes,
      int priority_cutoff) OVERRIDE;
  virtual void ReduceWastedContentsTextureMemoryOnImplThread() OVERRIDE;
  virtual void SendManagedMemoryStats() OVERRIDE;
  virtual bool IsInsideDraw() OVERRIDE;
  virtual void RenewTreePriority() OVERRIDE {}
  virtual void RequestScrollbarAnimationOnImplThread(base::TimeDelta delay)
      OVERRIDE {}

  // Called by the legacy path where RenderWidget does the scheduling.
  void CompositeImmediately(base::TimeTicks frame_begin_time);

 private:
  explicit SingleThreadProxy(LayerTreeHost* layer_tree_host);

  bool CommitAndComposite(base::TimeTicks frame_begin_time);
  void DoCommit(scoped_ptr<ResourceUpdateQueue> queue);
  bool DoComposite(
      scoped_refptr<cc::ContextProvider> offscreen_context_provider,
      base::TimeTicks frame_begin_time);
  void DidSwapFrame();

  // Accessed on main thread only.
  LayerTreeHost* layer_tree_host_;
  bool output_surface_lost_;
  bool created_offscreen_context_provider_;

  // Holds on to the context between initializeContext() and
  // InitializeRenderer() calls. Shouldn't be used for anything else.
  scoped_ptr<OutputSurface> output_surface_before_initialization_;

  // Used on the Thread, but checked on main thread during
  // initialization/shutdown.
  scoped_ptr<LayerTreeHostImpl> layer_tree_host_impl_;
  bool renderer_initialized_;
  RendererCapabilities renderer_capabilities_for_main_thread_;

  bool next_frame_is_newly_committed_frame_;

  bool inside_draw_;

  base::TimeDelta total_commit_time_;
  size_t total_commit_count_;

  DISALLOW_COPY_AND_ASSIGN(SingleThreadProxy);
};

// For use in the single-threaded case. In debug builds, it pretends that the
// code is running on the impl thread to satisfy assertion checks.
class DebugScopedSetImplThread {
 public:
  explicit DebugScopedSetImplThread(Proxy* proxy) : proxy_(proxy) {
#ifndef NDEBUG
    previous_value_ = proxy_->impl_thread_is_overridden_;
    proxy_->SetCurrentThreadIsImplThread(true);
#endif
  }
  ~DebugScopedSetImplThread() {
#ifndef NDEBUG
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
#ifndef NDEBUG
    previous_value_ = proxy_->impl_thread_is_overridden_;
    proxy_->SetCurrentThreadIsImplThread(false);
#endif
  }
  ~DebugScopedSetMainThread() {
#ifndef NDEBUG
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
