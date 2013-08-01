// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_H_
#define CC_TREES_LAYER_TREE_HOST_H_

#include <limits>
#include <list>
#include <vector>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/animation/animation_events.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/input/input_handler.h"
#include "cc/input/scrollbar.h"
#include "cc/input/top_controls_state.h"
#include "cc/layers/layer_lists.h"
#include "cc/output/output_surface.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/scheduler/rate_limiter.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/proxy.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/latency_info.h"
#include "ui/gfx/rect.h"

namespace WebKit { class WebGraphicsContext3D; }

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
  bool using_set_visibility;
  bool using_egl_image;
  bool allow_partial_texture_updates;
  bool using_offscreen_context3d;
  int max_texture_size;
  bool avoid_pow2_textures;
  bool using_map_image;
  bool using_shared_memory_resources;
};

struct CC_EXPORT UIResourceRequest {
  enum UIResourceRequestType {
    UIResourceCreate,
    UIResourceDelete,
    UIResourceInvalidRequest
  };

  UIResourceRequest();
  ~UIResourceRequest();
  UIResourceRequestType type;
  UIResourceId id;
  scoped_refptr<UIResourceBitmap> bitmap;
};

class CC_EXPORT LayerTreeHost : NON_EXPORTED_BASE(public RateLimiterClient) {
 public:
  static scoped_ptr<LayerTreeHost> Create(
      LayerTreeHostClient* client,
      const LayerTreeSettings& settings,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);
  virtual ~LayerTreeHost();

  void SetLayerTreeHostClientReady();

  // Returns true if any LayerTreeHost is alive.
  static bool AnyLayerTreeHostInstanceExists();

  void set_needs_filter_context() { needs_filter_context_ = true; }
  bool needs_offscreen_context() const {
    return needs_filter_context_;
  }

  // LayerTreeHost interface to Proxy.
  void WillBeginFrame() { client_->WillBeginFrame(); }
  void DidBeginFrame();
  void UpdateClientAnimations(base::TimeTicks monotonic_frame_begin_time);
  void AnimateLayers(base::TimeTicks monotonic_frame_begin_time);
  void DidStopFlinging();
  void Layout();
  void BeginCommitOnImplThread(LayerTreeHostImpl* host_impl);
  void FinishCommitOnImplThread(LayerTreeHostImpl* host_impl);
  void WillCommit();
  void CommitComplete();
  scoped_ptr<OutputSurface> CreateOutputSurface();
  virtual scoped_ptr<LayerTreeHostImpl> CreateLayerTreeHostImpl(
      LayerTreeHostImplClient* client);
  void DidLoseOutputSurface();
  bool output_surface_lost() const { return output_surface_lost_; }
  enum CreateResult {
    CreateSucceeded,
    CreateFailedButTryAgain,
    CreateFailedAndGaveUp,
  };
  CreateResult OnCreateAndInitializeOutputSurfaceAttempted(bool success);
  void DidCommitAndDrawFrame() { client_->DidCommitAndDrawFrame(); }
  void DidCompleteSwapBuffers() { client_->DidCompleteSwapBuffers(); }
  void DeleteContentsTexturesOnImplThread(ResourceProvider* resource_provider);
  virtual void AcquireLayerTextures();
  // Returns false if we should abort this frame due to initialization failure.
  bool InitializeOutputSurfaceIfNeeded();
  bool UpdateLayers(ResourceUpdateQueue* queue,
                    size_t contents_memory_limit_bytes);

  LayerTreeHostClient* client() { return client_; }
  const base::WeakPtr<InputHandler>& GetInputHandler() {
    return input_handler_weak_ptr_;
  }

  void NotifyInputThrottledUntilCommit();

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

  int source_frame_number() const { return source_frame_number_; }

  void SetNeedsDisplayOnAllLayers();

  void CollectRenderingStats(RenderingStats* stats) const;

  RenderingStatsInstrumentation* rendering_stats_instrumentation() const {
    return rendering_stats_instrumentation_.get();
  }

  const RendererCapabilities& GetRendererCapabilities() const;

  void SetNeedsAnimate();
  virtual void SetNeedsUpdateLayers();
  virtual void SetNeedsCommit();
  virtual void SetNeedsFullTreeSync();
  void SetNeedsRedraw();
  void SetNeedsRedrawRect(gfx::Rect damage_rect);
  bool CommitRequested() const;

  void SetAnimationEvents(scoped_ptr<AnimationEventsVector> events,
                          base::Time wall_clock_time);

  void SetRootLayer(scoped_refptr<Layer> root_layer);
  Layer* root_layer() { return root_layer_.get(); }
  const Layer* root_layer() const { return root_layer_.get(); }

  const LayerTreeSettings& settings() const { return settings_; }

  void SetDebugState(const LayerTreeDebugState& debug_state);
  const LayerTreeDebugState& debug_state() const { return debug_state_; }

  void SetViewportSize(gfx::Size device_viewport_size);
  void SetOverdrawBottomHeight(float overdraw_bottom_height);

  gfx::Size device_viewport_size() const { return device_viewport_size_; }
  float overdraw_bottom_height() const { return overdraw_bottom_height_; }

  void ApplyPageScaleDeltaFromImplSide(float page_scale_delta);
  void SetPageScaleFactorAndLimits(float page_scale_factor,
                                   float min_page_scale_factor,
                                   float max_page_scale_factor);
  float page_scale_factor() const { return page_scale_factor_; }

  SkColor background_color() const { return background_color_; }
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
  void SetLatencyInfo(const ui::LatencyInfo& latency_info);

  virtual void StartRateLimiter(WebKit::WebGraphicsContext3D* context3d);
  virtual void StopRateLimiter(WebKit::WebGraphicsContext3D* context3d);

  // RateLimiterClient implementation.
  virtual void RateLimit() OVERRIDE;

  bool buffered_updates() const {
    return settings_.max_partial_texture_updates !=
        std::numeric_limits<size_t>::max();
  }
  bool RequestPartialTextureUpdate();

  void SetDeviceScaleFactor(float device_scale_factor);
  float device_scale_factor() const { return device_scale_factor_; }

  void UpdateTopControlsState(TopControlsState constraints,
                              TopControlsState current,
                              bool animate);

  HeadsUpDisplayLayer* hud_layer() const { return hud_layer_.get(); }

  Proxy* proxy() const { return proxy_.get(); }

  AnimationRegistrar* animation_registrar() const {
    return animation_registrar_.get();
  }

  bool BlocksPendingCommit() const;

  // Obtains a thorough dump of the LayerTreeHost as a value.
  scoped_ptr<base::Value> AsValue() const;

  bool in_paint_layer_contents() const { return in_paint_layer_contents_; }

  // CreateUIResource creates a resource given a bitmap.  The bitmap is
  // generated via an interface function, which is called when initializing the
  // resource and when the resource has been lost (due to lost context).  The
  // parameter of the interface is a single boolean, which indicates whether the
  // resource has been lost or not.  CreateUIResource returns an Id of the
  // resource, which is always positive.
  virtual UIResourceId CreateUIResource(UIResourceClient* client);
  // Deletes a UI resource.  May safely be called more than once.
  virtual void DeleteUIResource(UIResourceId id);

  bool UsingSharedMemoryResources();
  int id() const { return tree_id_; }

 protected:
  LayerTreeHost(LayerTreeHostClient* client, const LayerTreeSettings& settings);
  bool Initialize(scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);
  bool InitializeForTesting(scoped_ptr<Proxy> proxy_for_testing);

 private:
  bool InitializeProxy(scoped_ptr<Proxy> proxy);

  void PaintLayerContents(
      const RenderSurfaceLayerList& render_surface_layer_list,
      ResourceUpdateQueue* queue,
      bool* did_paint_content,
      bool* need_more_updates);
  void PaintMasksForRenderSurface(Layer* render_surface_layer,
                                  ResourceUpdateQueue* queue,
                                  bool* did_paint_content,
                                  bool* need_more_updates);
  bool UpdateLayers(Layer* root_layer, ResourceUpdateQueue* queue);
  void UpdateHudLayer();
  void TriggerPrepaint();

  void ReduceMemoryUsage();

  void PrioritizeTextures(
      const RenderSurfaceLayerList& render_surface_layer_list,
      OverdrawMetrics* metrics);
  void SetPrioritiesForSurfaces(size_t surface_memory_bytes);
  void SetPrioritiesForLayers(const RenderSurfaceLayerList& update_list);
  size_t CalculateMemoryForRenderSurfaces(
      const RenderSurfaceLayerList& update_list);

  bool AnimateLayersRecursive(Layer* current, base::TimeTicks time);

  void UIResourceLost(UIResourceId id);

  void DidLoseUIResources();

  typedef base::hash_map<UIResourceId, UIResourceClient*> UIResourceClientMap;
  UIResourceClientMap ui_resource_client_map_;
  int next_ui_resource_id_;

  typedef std::list<UIResourceRequest> UIResourceRequestQueue;
  UIResourceRequestQueue ui_resource_request_queue_;

  void CalculateLCDTextMetricsCallback(Layer* layer);

  bool animating_;
  bool needs_full_tree_sync_;
  bool needs_filter_context_;

  base::CancelableClosure prepaint_callback_;

  LayerTreeHostClient* client_;
  scoped_ptr<Proxy> proxy_;

  int source_frame_number_;
  scoped_ptr<RenderingStatsInstrumentation> rendering_stats_instrumentation_;

  bool output_surface_can_be_initialized_;
  bool output_surface_lost_;
  int num_failed_recreate_attempts_;

  scoped_refptr<Layer> root_layer_;
  scoped_refptr<HeadsUpDisplayLayer> hud_layer_;

  scoped_ptr<PrioritizedResourceManager> contents_texture_manager_;
  scoped_ptr<PrioritizedResource> surface_memory_placeholder_;

  base::WeakPtr<InputHandler> input_handler_weak_ptr_;
  base::WeakPtr<TopControlsManager> top_controls_manager_weak_ptr_;

  LayerTreeSettings settings_;
  LayerTreeDebugState debug_state_;

  gfx::Size device_viewport_size_;
  float overdraw_bottom_height_;
  float device_scale_factor_;

  bool visible_;

  typedef base::hash_map<WebKit::WebGraphicsContext3D*,
                         scoped_refptr<RateLimiter> > RateLimiterMap;
  RateLimiterMap rate_limiters_;

  float page_scale_factor_;
  float min_page_scale_factor_;
  float max_page_scale_factor_;
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

  bool in_paint_layer_contents_;

  ui::LatencyInfo latency_info_;

  static const int kTotalFramesToUseForLCDTextMetrics = 50;
  int total_frames_used_for_lcd_text_metrics_;

  struct LCDTextMetrics {
    LCDTextMetrics()
        : total_num_cc_layers(0),
          total_num_cc_layers_can_use_lcd_text(0),
          total_num_cc_layers_will_use_lcd_text(0) {}

    int64 total_num_cc_layers;
    int64 total_num_cc_layers_can_use_lcd_text;
    int64 total_num_cc_layers_will_use_lcd_text;
  };
  LCDTextMetrics lcd_text_metrics_;
  int tree_id_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeHost);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_H_
