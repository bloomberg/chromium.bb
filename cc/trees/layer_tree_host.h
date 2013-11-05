// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_H_
#define CC_TREES_LAYER_TREE_HOST_H_

#include <limits>
#include <list>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/animation/animation_events.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/debug/micro_benchmark.h"
#include "cc/debug/micro_benchmark_controller.h"
#include "cc/input/input_handler.h"
#include "cc/input/scrollbar.h"
#include "cc/input/top_controls_state.h"
#include "cc/layers/layer_lists.h"
#include "cc/output/output_surface.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/occlusion_tracker.h"
#include "cc/trees/proxy.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/rect.h"

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
class SharedBitmapManager;
class TopControlsManager;
struct RenderingStats;
struct ScrollAndScaleSet;

// Provides information on an Impl's rendering capabilities back to the
// LayerTreeHost.
struct CC_EXPORT RendererCapabilities {
  RendererCapabilities();
  ~RendererCapabilities();

  ResourceFormat best_texture_format;
  bool using_partial_swap;
  bool using_set_visibility;
  bool using_egl_image;
  bool allow_partial_texture_updates;
  bool using_offscreen_context3d;
  int max_texture_size;
  bool avoid_pow2_textures;
  bool using_map_image;
  bool using_shared_memory_resources;
  bool using_discard_framebuffer;
};

class CC_EXPORT UIResourceRequest {
 public:
  enum UIResourceRequestType {
    UIResourceCreate,
    UIResourceDelete,
    UIResourceInvalidRequest
  };

  UIResourceRequest(UIResourceRequestType type, UIResourceId id);
  UIResourceRequest(UIResourceRequestType type,
                    UIResourceId id,
                    const UIResourceBitmap& bitmap);
  UIResourceRequest(const UIResourceRequest& request);

  ~UIResourceRequest();

  UIResourceRequestType GetType() const { return type_; }
  UIResourceId GetId() const { return id_; }
  UIResourceBitmap GetBitmap() const {
    DCHECK(bitmap_);
    return *bitmap_.get();
  }

  UIResourceRequest& operator=(const UIResourceRequest& request);

 private:
  UIResourceRequestType type_;
  UIResourceId id_;
  scoped_ptr<UIResourceBitmap> bitmap_;
};

class CC_EXPORT LayerTreeHost {
 public:
  // The SharedBitmapManager will be used on the compositor thread.
  static scoped_ptr<LayerTreeHost> Create(
      LayerTreeHostClient* client,
      SharedBitmapManager* manager,
      const LayerTreeSettings& settings,
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);
  virtual ~LayerTreeHost();

  void SetLayerTreeHostClientReady();

  void set_needs_filter_context() { needs_filter_context_ = true; }
  bool needs_offscreen_context() const {
    return needs_filter_context_;
  }

  // LayerTreeHost interface to Proxy.
  void WillBeginMainFrame() { client_->WillBeginMainFrame(); }
  void DidBeginMainFrame();
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
  bool UpdateLayers(ResourceUpdateQueue* queue);

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
  bool BeginMainFrameRequested() const;

  void SetNextCommitWaitsForActivation();

  void SetNextCommitForcesRedraw();

  void SetAnimationEvents(scoped_ptr<AnimationEventsVector> events,
                          base::Time wall_clock_time);

  void SetRootLayer(scoped_refptr<Layer> root_layer);
  Layer* root_layer() { return root_layer_.get(); }
  const Layer* root_layer() const { return root_layer_.get(); }
  const Layer* page_scale_layer() const { return page_scale_layer_.get(); }
  void RegisterViewportLayers(
      scoped_refptr<Layer> page_scale_layer,
      scoped_refptr<Layer> inner_viewport_scroll_layer,
      scoped_refptr<Layer> outer_viewport_scroll_layer);

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

  void SetOverhangBitmap(const SkBitmap& bitmap);

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

  // Virtual for tests.
  virtual void StartRateLimiter();
  virtual void StopRateLimiter();

  void RateLimit();

  bool AlwaysUsePartialTextureUpdates();
  size_t MaxPartialTextureUpdates() const;
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
  // Put the recreation of all UI resources into the resource queue after they
  // were evicted on the impl thread.
  void RecreateUIResources();

  virtual gfx::Size GetUIResourceSize(UIResourceId id) const;

  bool UsingSharedMemoryResources();
  int id() const { return tree_id_; }

  bool ScheduleMicroBenchmark(const std::string& benchmark_name,
                              scoped_ptr<base::Value> value,
                              const MicroBenchmark::DoneCallback& callback);

 protected:
  LayerTreeHost(LayerTreeHostClient* client,
                SharedBitmapManager* manager,
                const LayerTreeSettings& settings);
  bool Initialize(scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner);
  bool InitializeForTesting(scoped_ptr<Proxy> proxy_for_testing);
  void SetOutputSurfaceLostForTesting(bool is_lost) {
    output_surface_lost_ = is_lost;
  }

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

  struct UIResourceClientData {
    UIResourceClient* client;
    gfx::Size size;
  };

  typedef base::hash_map<UIResourceId, UIResourceClientData>
      UIResourceClientMap;
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
  MicroBenchmarkController micro_benchmark_controller_;


  bool output_surface_can_be_initialized_;
  bool output_surface_lost_;
  int num_failed_recreate_attempts_;

  scoped_refptr<Layer> root_layer_;
  scoped_refptr<HeadsUpDisplayLayer> hud_layer_;

  scoped_ptr<PrioritizedResourceManager> contents_texture_manager_;
  scoped_ptr<PrioritizedResource> surface_memory_placeholder_;

  base::WeakPtr<InputHandler> input_handler_weak_ptr_;
  base::WeakPtr<TopControlsManager> top_controls_manager_weak_ptr_;

  const LayerTreeSettings settings_;
  LayerTreeDebugState debug_state_;

  gfx::Size device_viewport_size_;
  float overdraw_bottom_height_;
  float device_scale_factor_;

  bool visible_;

  base::OneShotTimer<LayerTreeHost> rate_limit_timer_;

  float page_scale_factor_;
  float min_page_scale_factor_;
  float max_page_scale_factor_;
  gfx::Transform impl_transform_;
  bool trigger_idle_updates_;

  SkColor background_color_;
  bool has_transparent_background_;

  // If set, this texture is used to fill in the parts of the screen not
  // covered by layers.
  scoped_ptr<ScopedUIResource> overhang_ui_resource_;

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
  bool next_commit_forces_redraw_;

  scoped_refptr<Layer> page_scale_layer_;
  scoped_refptr<Layer> inner_viewport_scroll_layer_;
  scoped_refptr<Layer> outer_viewport_scroll_layer_;

  SharedBitmapManager* shared_bitmap_manager_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeHost);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_H_
