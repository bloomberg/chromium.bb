// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_H_
#define CC_TREES_LAYER_TREE_HOST_H_

#include <limits>
#include <vector>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "cc/animation/animation_events.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/output/output_surface.h"
#include "cc/scheduler/rate_limiter.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/proxy.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/rect.h"

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template <>
struct hash<WebKit::WebGraphicsContext3D*> {
  size_t operator()(WebKit::WebGraphicsContext3D* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif  // COMPILER

namespace cc {

class AnimationRegistrar;
class HeadsUpDisplayLayer;
class Layer;
class LayerTreeHostImpl;
class LayerTreeHostImplClient;
class PrioritizedResourceManager;
class PrioritizedResource;
class Region;
class RenderingStatsInstrumentation;
class ResourceProvider;
class ResourceUpdateQueue;
class ScrollbarLayer;
class TopControlsManager;
struct RenderingStats;
struct ScrollAndScaleSet;

// Provides information on an Impl's rendering capabilities back to the
// LayerTreeHost.
struct CC_EXPORT RendererCapabilities {
  RendererCapabilities();
  ~RendererCapabilities();

  unsigned best_texture_format;
  bool using_partial_swap;
  bool using_accelerated_painting;
  bool using_set_visibility;
  bool using_swap_complete_callback;
  bool using_gpu_memory_manager;
  bool using_egl_image;
  bool allow_partial_texture_updates;
  bool using_offscreen_context3d;
  int max_texture_size;
  bool avoid_pow2_textures;
};

class CC_EXPORT LayerTreeHost : NON_EXPORTED_BASE(public RateLimiterClient) {
 public:
  static scoped_ptr<LayerTreeHost> Create(LayerTreeHostClient* client,
                                          const LayerTreeSettings& settings,
                                          scoped_ptr<Thread> impl_thread);
  virtual ~LayerTreeHost();

  void SetSurfaceReady();

  // Returns true if any LayerTreeHost is alive.
  static bool AnyLayerTreeHostInstanceExists();

  void set_needs_filter_context() { needs_filter_context_ = true; }
  bool needs_offscreen_context() const {
    return needs_filter_context_ || settings_.accelerate_painting;
  }

  // LayerTreeHost interface to Proxy.
  void WillBeginFrame() { client_->WillBeginFrame(); }
  void DidBeginFrame();
  void UpdateAnimations(base::TimeTicks monotonic_frame_begin_time);
  void DidStopFlinging();
  void Layout();
  void BeginCommitOnImplThread(LayerTreeHostImpl* host_impl);
  void FinishCommitOnImplThread(LayerTreeHostImpl* host_impl);
  void SetPinchZoomScrollbarsBoundsAndPosition();
  void CreateAndAddPinchZoomScrollbars();
  void WillCommit();
  void CommitComplete();
  scoped_ptr<OutputSurface> CreateOutputSurface();
  scoped_ptr<InputHandler> CreateInputHandler();
  virtual scoped_ptr<LayerTreeHostImpl> CreateLayerTreeHostImpl(
      LayerTreeHostImplClient* client);
  void DidLoseOutputSurface();
  enum RecreateResult {
    RecreateSucceeded,
    RecreateFailedButTryAgain,
    RecreateFailedAndGaveUp,
  };
  RecreateResult RecreateOutputSurface();
  void DidCommitAndDrawFrame() { client_->DidCommitAndDrawFrame(); }
  void DidCompleteSwapBuffers() { client_->DidCompleteSwapBuffers(); }
  void DeleteContentsTexturesOnImplThread(ResourceProvider* resource_provider);
  virtual void AcquireLayerTextures();
  // Returns false if we should abort this frame due to initialization failure.
  bool InitializeRendererIfNeeded();
  void UpdateLayers(ResourceUpdateQueue* queue,
                    size_t contents_memory_limit_bytes);

  LayerTreeHostClient* client() { return client_; }

  void Composite(base::TimeTicks frame_begin_time);

  // Only used when compositing on the main thread.
  void ScheduleComposite();

  // Composites and attempts to read back the result into the provided
  // buffer. If it wasn't possible, e.g. due to context lost, will return
  // false.
  bool CompositeAndReadback(void* pixels, gfx::Rect rect_in_device_viewport);

  void FinishAllRendering();

  void SetDeferCommits(bool defer_commits);

  // Test only hook
  virtual void DidDeferCommit();

  int commit_number() const { return commit_number_; }

  void CollectRenderingStats(RenderingStats* stats) const;

  RenderingStatsInstrumentation* rendering_stats_instrumentation() const {
    return rendering_stats_instrumentation_.get();
  }

  const RendererCapabilities& GetRendererCapabilities() const;

  void SetNeedsAnimate();
  virtual void SetNeedsCommit();
  virtual void SetNeedsFullTreeSync();
  void SetNeedsRedraw();
  bool CommitRequested() const;

  void SetAnimationEvents(scoped_ptr<AnimationEventsVector> events,
                          base::Time wall_clock_time);

  void SetRootLayer(scoped_refptr<Layer> root_layer);
  Layer* root_layer() { return root_layer_.get(); }
  const Layer* root_layer() const { return root_layer_.get(); }
  const Layer* RootScrollLayer() const;

  const LayerTreeSettings& settings() const { return settings_; }

  void SetDebugState(const LayerTreeDebugState& debug_state);
  const LayerTreeDebugState& debug_state() const { return debug_state_; }

  void SetViewportSize(gfx::Size layout_viewport_size,
                       gfx::Size device_viewport_size);
  void SetOverdrawBottomHeight(float overdraw_bottom_height);

  gfx::Size layout_viewport_size() const { return layout_viewport_size_; }
  gfx::Size device_viewport_size() const { return device_viewport_size_; }
  float overdraw_bottom_height() const { return overdraw_bottom_height_; }

  void SetPageScaleFactorAndLimits(float page_scale_factor,
                                   float min_page_scale_factor,
                                   float max_page_scale_factor);

  void set_background_color(SkColor color) { background_color_ = color; }

  void set_has_transparent_background(bool transparent) {
    has_transparent_background_ = transparent;
  }

  PrioritizedResourceManager* contents_texture_manager() const {
    return contents_texture_manager_.get();
  }

  void SetVisible(bool visible);
  bool visible() const { return visible_; }

  void StartPageScaleAnimation(gfx::Vector2d target_offset,
                               bool use_anchor,
                               float scale,
                               base::TimeDelta duration);

  void ApplyScrollAndScale(const ScrollAndScaleSet& info);

  void SetImplTransform(const gfx::Transform& transform);

  void StartRateLimiter(WebKit::WebGraphicsContext3D* context3d);
  void StopRateLimiter(WebKit::WebGraphicsContext3D* context3d);

  // RateLimiterClient implementation.
  virtual void RateLimit() OVERRIDE;

  bool buffered_updates() const {
    return settings_.max_partial_texture_updates !=
        std::numeric_limits<size_t>::max();
  }
  bool RequestPartialTextureUpdate();

  void SetDeviceScaleFactor(float device_scale_factor);
  float device_scale_factor() const { return device_scale_factor_; }

  void EnableHidingTopControls(bool enable);

  HeadsUpDisplayLayer* hud_layer() const { return hud_layer_.get(); }

  Proxy* proxy() const { return proxy_.get(); }

  AnimationRegistrar* animation_registrar() const {
    return animation_registrar_.get();
  }

  skia::RefPtr<SkPicture> CapturePicture();

  bool BlocksPendingCommit() const;

  // Obtains a thorough dump of the LayerTreeHost as a value.
  scoped_ptr<base::Value> AsValue() const;

 protected:
  LayerTreeHost(LayerTreeHostClient* client, const LayerTreeSettings& settings);
  bool Initialize(scoped_ptr<Thread> impl_thread);
  bool InitializeForTesting(scoped_ptr<Proxy> proxy_for_testing);

 private:
  typedef std::vector<scoped_refptr<Layer> > LayerList;

  bool InitializeProxy(scoped_ptr<Proxy> proxy);
  void InitializeRenderer();

  bool PaintLayerContents(const LayerList& render_surface_layer_list,
                          ResourceUpdateQueue* quue);
  bool PaintMasksForRenderSurface(Layer* render_surface_layer,
                                  ResourceUpdateQueue* queue);

  void UpdateLayers(Layer* root_layer, ResourceUpdateQueue* queue);
  void UpdateHudLayer();
  void TriggerPrepaint();

  void PrioritizeTextures(const LayerList& render_surface_layer_list,
                          OverdrawMetrics* metrics);
  void SetPrioritiesForSurfaces(size_t surface_memory_bytes);
  void SetPrioritiesForLayers(const LayerList& update_list);
  size_t CalculateMemoryForRenderSurfaces(const LayerList& update_list);

  void AnimateLayers(base::TimeTicks monotonic_time);
  bool AnimateLayersRecursive(Layer* current, base::TimeTicks time);
  void SetAnimationEventsRecursive(const AnimationEventsVector& events,
                                   Layer* layer,
                                   base::Time wall_clock_time);

  bool animating_;
  bool needs_full_tree_sync_;
  bool needs_filter_context_;

  base::CancelableClosure prepaint_callback_;

  LayerTreeHostClient* client_;
  scoped_ptr<Proxy> proxy_;

  int commit_number_;
  scoped_ptr<RenderingStatsInstrumentation> rendering_stats_instrumentation_;

  bool renderer_initialized_;
  bool output_surface_lost_;
  int num_failed_recreate_attempts_;

  scoped_refptr<Layer> root_layer_;
  scoped_refptr<HeadsUpDisplayLayer> hud_layer_;
  scoped_refptr<ScrollbarLayer> pinch_zoom_scrollbar_horizontal_;
  scoped_refptr<ScrollbarLayer> pinch_zoom_scrollbar_vertical_;

  scoped_ptr<PrioritizedResourceManager> contents_texture_manager_;
  scoped_ptr<PrioritizedResource> surface_memory_placeholder_;

  base::WeakPtr<TopControlsManager> top_controls_manager_weak_ptr_;

  LayerTreeSettings settings_;
  LayerTreeDebugState debug_state_;

  gfx::Size layout_viewport_size_;
  gfx::Size device_viewport_size_;
  float overdraw_bottom_height_;
  float device_scale_factor_;

  bool visible_;

  typedef base::hash_map<WebKit::WebGraphicsContext3D*,
                         scoped_refptr<RateLimiter> > RateLimiterMap;
  RateLimiterMap rate_limiters_;

  float page_scale_factor_;
  float min_page_scale_factor_, max_page_scale_factor_;
  gfx::Transform impl_transform_;
  bool trigger_idle_updates_;

  SkColor background_color_;
  bool has_transparent_background_;

  typedef ScopedPtrVector<PrioritizedResource> TextureList;
  size_t partial_texture_update_requests_;

  scoped_ptr<AnimationRegistrar> animation_registrar_;

  struct PendingPageScaleAnimation {
    gfx::Vector2d target_offset;
    bool use_anchor;
    float scale;
    base::TimeDelta duration;
  };
  scoped_ptr<PendingPageScaleAnimation> pending_page_scale_animation_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeHost);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_H_
