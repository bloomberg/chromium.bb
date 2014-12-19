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
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/public/browser/android/compositor.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/android/resources/resource_manager_impl.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/base/android/window_android_compositor.h"

class SkBitmap;
struct ANativeWindow;

namespace cc {
class Layer;
class LayerTreeHost;
class SurfaceIdAllocator;
}

namespace content {
class CompositorClient;
class OnscreenDisplayClient;

// -----------------------------------------------------------------------------
// Browser-side compositor that manages a tree of content and UI layers.
// -----------------------------------------------------------------------------
class CONTENT_EXPORT CompositorImpl
    : public Compositor,
      public cc::LayerTreeHostClient,
      public cc::LayerTreeHostSingleThreadClient,
      public ui::WindowAndroidCompositor {
 public:
  CompositorImpl(CompositorClient* client, gfx::NativeWindow root_window);
  virtual ~CompositorImpl();

  static bool IsInitialized();

  void PopulateGpuCapabilities(gpu::Capabilities gpu_capabilities);

 private:
  // Compositor implementation.
  virtual void SetRootLayer(scoped_refptr<cc::Layer> root) override;
  virtual void SetSurface(jobject surface) override;
  virtual void SetVisible(bool visible) override;
  virtual void setDeviceScaleFactor(float factor) override;
  virtual void SetWindowBounds(const gfx::Size& size) override;
  virtual void SetHasTransparentBackground(bool flag) override;
  virtual void SetNeedsComposite() override;
  virtual ui::UIResourceProvider& GetUIResourceProvider() override;
  virtual ui::ResourceManager& GetResourceManager() override;

  // LayerTreeHostClient implementation.
  virtual void WillBeginMainFrame(int frame_id) override {}
  virtual void DidBeginMainFrame() override {}
  virtual void BeginMainFrame(const cc::BeginFrameArgs& args) override {}
  virtual void Layout() override;
  virtual void ApplyViewportDeltas(
      const gfx::Vector2d& inner_delta,
      const gfx::Vector2d& outer_delta,
      const gfx::Vector2dF& elastic_overscroll_delta,
      float page_scale,
      float top_controls_delta) override {}
  virtual void ApplyViewportDeltas(
      const gfx::Vector2d& scroll_delta,
      float page_scale,
      float top_controls_delta) override {}
  virtual void RequestNewOutputSurface() override;
  virtual void DidInitializeOutputSurface() override {}
  virtual void DidFailToInitializeOutputSurface() override;
  virtual void WillCommit() override {}
  virtual void DidCommit() override;
  virtual void DidCommitAndDrawFrame() override {}
  virtual void DidCompleteSwapBuffers() override;

  // LayerTreeHostSingleThreadClient implementation.
  virtual void ScheduleComposite() override;
  virtual void ScheduleAnimation() override;
  virtual void DidPostSwapBuffers() override;
  virtual void DidAbortSwapBuffers() override;

  // WindowAndroidCompositor implementation.
  virtual void AttachLayerForReadback(scoped_refptr<cc::Layer> layer) override;
  virtual void RequestCopyOfOutputOnRootLayer(
      scoped_ptr<cc::CopyOutputRequest> request) override;
  virtual void OnVSync(base::TimeTicks frame_time,
                       base::TimeDelta vsync_period) override;
  virtual void SetNeedsAnimate() override;

  void SetWindowSurface(ANativeWindow* window);

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
           composite_on_vsync_trigger_ != DO_NOT_COMPOSITE ||
           defer_composite_for_gpu_channel_;
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

  // root_layer_ is the persistent internal root layer, while subroot_layer_
  // is the one attached by the compositor client.
  scoped_refptr<cc::Layer> root_layer_;
  scoped_refptr<cc::Layer> subroot_layer_;

  scoped_ptr<cc::LayerTreeHost> host_;
  ui::UIResourceProvider ui_resource_provider_;
  ui::ResourceManagerImpl resource_manager_;

  scoped_ptr<OnscreenDisplayClient> display_client_;
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

  // Whether we are currently deferring a requested Composite operation until
  // the GPU channel is established (it was either lost or not yet fully
  // established the first time we tried to composite).
  bool defer_composite_for_gpu_channel_;

  base::TimeDelta vsync_period_;
  base::TimeTicks last_vsync_;

  base::WeakPtrFactory<CompositorImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CompositorImpl);
};

} // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_IMPL_ANDROID_H_
