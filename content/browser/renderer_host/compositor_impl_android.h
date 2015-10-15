// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/public/browser/android/compositor.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/android/window_android_compositor.h"

class SkBitmap;
struct ANativeWindow;

namespace cc {
class Layer;
class LayerTreeHost;
class OnscreenDisplayClient;
class SurfaceIdAllocator;
class SurfaceManager;
}

namespace content {
class CompositorClient;

// -----------------------------------------------------------------------------
// Browser-side compositor that manages a tree of content and UI layers.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT CompositorImpl
    : public Compositor,
      public cc::LayerTreeHostClient,
      public cc::LayerTreeHostSingleThreadClient,
      public ui::UIResourceProvider,
      public ui::WindowAndroidCompositor {
 public:
  class VSyncObserver {
   public:
    virtual void OnUpdateVSyncParameters(base::TimeTicks timebase,
                                         base::TimeDelta interval) = 0;
  };

  CompositorImpl(CompositorClient* client, gfx::NativeWindow root_window);
  ~CompositorImpl() override;

  static bool IsInitialized();

  static cc::SurfaceManager* GetSurfaceManager();
  static scoped_ptr<cc::SurfaceIdAllocator> CreateSurfaceIdAllocator();

  void PopulateGpuCapabilities(gpu::Capabilities gpu_capabilities);

  void AddObserver(VSyncObserver* observer);
  void RemoveObserver(VSyncObserver* observer);

  // ui::ResourceProvider implementation.
  cc::UIResourceId CreateUIResource(cc::UIResourceClient* client) override;
  void DeleteUIResource(cc::UIResourceId resource_id) override;
  bool SupportsETC1NonPowerOfTwo() const override;

 private:
  // Compositor implementation.
  void SetRootLayer(scoped_refptr<cc::Layer> root) override;
  void SetSurface(jobject surface) override;
  void setDeviceScaleFactor(float factor) override;
  void SetWindowBounds(const gfx::Size& size) override;
  void SetHasTransparentBackground(bool flag) override;
  void SetNeedsComposite() override;
  ui::UIResourceProvider& GetUIResourceProvider() override;
  ui::ResourceManager& GetResourceManager() override;

  // LayerTreeHostClient implementation.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override {}
  void BeginMainFrame(const cc::BeginFrameArgs& args) override {}
  void BeginMainFrameNotExpectedSoon() override {}
  void Layout() override;
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override {}
  void RequestNewOutputSurface() override;
  void DidInitializeOutputSurface() override;
  void DidFailToInitializeOutputSurface() override;
  void WillCommit() override {}
  void DidCommit() override;
  void DidCommitAndDrawFrame() override {}
  void DidCompleteSwapBuffers() override;
  void DidCompletePageScaleAnimation() override {}
  void RecordFrameTimingEvents(
      scoped_ptr<cc::FrameTimingTracker::CompositeTimingSet> composite_events,
      scoped_ptr<cc::FrameTimingTracker::MainFrameTimingSet> main_frame_events)
      override {}

  // LayerTreeHostSingleThreadClient implementation.
  void ScheduleComposite() override;
  void ScheduleAnimation() override;
  void DidPostSwapBuffers() override;
  void DidAbortSwapBuffers() override;

  // WindowAndroidCompositor implementation.
  void AttachLayerForReadback(scoped_refptr<cc::Layer> layer) override;
  void RequestCopyOfOutputOnRootLayer(
      scoped_ptr<cc::CopyOutputRequest> request) override;
  void OnVSync(base::TimeTicks frame_time,
               base::TimeDelta vsync_period) override;
  void SetNeedsAnimate() override;

  void SetVisible(bool visible);

  enum CompositingTrigger {
    DO_NOT_COMPOSITE,
    COMPOSITE_IMMEDIATELY,
    COMPOSITE_EVENTUALLY,
  };
  void PostComposite(CompositingTrigger trigger);
  void Composite(CompositingTrigger trigger);
  void CreateOutputSurface();

  bool WillCompositeThisFrame() const {
    return current_composite_task_ &&
           !current_composite_task_->callback().is_null();
  }
  bool DidCompositeThisFrame() const {
    return current_composite_task_ &&
           current_composite_task_->callback().is_null();
  }
  bool WillComposite() const {
    return WillCompositeThisFrame() ||
           composite_on_vsync_trigger_ != DO_NOT_COMPOSITE;
  }
  void CancelComposite() {
    DCHECK(WillComposite());
    if (WillCompositeThisFrame())
      current_composite_task_->Cancel();
    current_composite_task_.reset();
    composite_on_vsync_trigger_ = DO_NOT_COMPOSITE;
    will_composite_immediately_ = false;
  }
  void CreateLayerTreeHost();

  void OnGpuChannelEstablished();
  void OnGpuChannelTimeout();

  // root_layer_ is the persistent internal root layer, while subroot_layer_
  // is the one attached by the compositor client.
  scoped_refptr<cc::Layer> root_layer_;
  scoped_refptr<cc::Layer> subroot_layer_;

  scoped_ptr<cc::LayerTreeHost> host_;
  ui::ResourceManagerImpl resource_manager_;

  scoped_ptr<cc::OnscreenDisplayClient> display_client_;
  scoped_ptr<cc::SurfaceIdAllocator> surface_id_allocator_;

  gfx::Size size_;
  bool has_transparent_background_;
  float device_scale_factor_;

  ANativeWindow* window_;
  int surface_id_;

  CompositorClient* client_;

  gfx::NativeWindow root_window_;

  // Used locally to track whether a call to LTH::Composite() did result in
  // a posted SwapBuffers().
  bool did_post_swapbuffers_;

  // Used locally to inhibit ScheduleComposite() during Layout().
  bool ignore_schedule_composite_;

  // Whether we need to composite in general because of any invalidation or
  // explicit request.
  bool needs_composite_;

  // Whether we need to update animations on the next composite.
  bool needs_animate_;

  // Whether we posted a task and are about to composite.
  bool will_composite_immediately_;

  // How we should schedule Composite during the next vsync.
  CompositingTrigger composite_on_vsync_trigger_;

  // The Composite operation scheduled for the current vsync interval.
  scoped_ptr<base::CancelableClosure> current_composite_task_;

  // The number of SwapBuffer calls that have not returned and ACK'd from
  // the GPU thread.
  unsigned int pending_swapbuffers_;

  size_t num_successive_context_creation_failures_;

  base::TimeDelta vsync_period_;
  base::TimeTicks last_vsync_;

  base::OneShotTimer establish_gpu_channel_timeout_;

  // Whether there is an OutputSurface request pending from the current
  // |host_|. Becomes |true| if RequestNewOutputSurface is called, and |false|
  // if |host_| is deleted or we succeed in creating *and* initializing an
  // OutputSurface (which is essentially the contract with cc).
  bool output_surface_request_pending_;

  gpu::Capabilities gpu_capabilities_;

  base::ObserverList<VSyncObserver, true> observer_list_;

  base::WeakPtrFactory<CompositorImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositorImpl);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_
