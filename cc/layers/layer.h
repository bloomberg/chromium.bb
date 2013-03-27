// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_H_
#define CC_LAYERS_LAYER_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "cc/animation/layer_animation_controller.h"
#include "cc/animation/layer_animation_event_observer.h"
#include "cc/animation/layer_animation_value_observer.h"
#include "cc/base/cc_export.h"
#include "cc/base/region.h"
#include "cc/layers/draw_properties.h"
#include "cc/layers/render_surface.h"
#include "cc/trees/occlusion_tracker.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperations.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/transform.h"

namespace WebKit {
class WebAnimationDelegate;
class WebLayerScrollClient;
}

namespace cc {

class Animation;
struct AnimationEvent;
class LayerAnimationDelegate;
class LayerImpl;
class LayerTreeHost;
class LayerTreeImpl;
class PriorityCalculator;
class ResourceUpdateQueue;
class ScrollbarLayer;
struct AnimationEvent;
class RenderingStatsInstrumentation;

// Base class for composited layers. Special layer types are derived from
// this class.
class CC_EXPORT Layer : public base::RefCounted<Layer>,
                        public LayerAnimationValueObserver {
 public:
  typedef std::vector<scoped_refptr<Layer> > LayerList;
  enum LayerIdLabels {
    PINCH_ZOOM_ROOT_SCROLL_LAYER_ID = -2,
    INVALID_ID = -1,
  };

  static scoped_refptr<Layer> Create();

  int id() const { return layer_id_; }

  Layer* RootLayer();
  Layer* parent() { return parent_; }
  const Layer* parent() const { return parent_; }
  void AddChild(scoped_refptr<Layer> child);
  void InsertChild(scoped_refptr<Layer> child, size_t index);
  void ReplaceChild(Layer* reference, scoped_refptr<Layer> new_layer);
  void RemoveFromParent();
  void RemoveAllChildren();
  void SetChildren(const LayerList& children);

  const LayerList& children() const { return children_; }
  Layer* child_at(size_t index) { return children_[index].get(); }

  void SetAnchorPoint(gfx::PointF anchor_point);
  gfx::PointF anchor_point() const { return anchor_point_; }

  void SetAnchorPointZ(float anchor_point_z);
  float anchor_point_z() const { return anchor_point_z_; }

  virtual void SetBackgroundColor(SkColor background_color);
  SkColor background_color() const { return background_color_; }

  // A layer's bounds are in logical, non-page-scaled pixels (however, the
  // root layer's bounds are in physical pixels).
  void SetBounds(gfx::Size bounds);
  gfx::Size bounds() const { return bounds_; }

  void SetMasksToBounds(bool masks_to_bounds);
  bool masks_to_bounds() const { return masks_to_bounds_; }

  void SetMaskLayer(Layer* mask_layer);
  Layer* mask_layer() { return mask_layer_.get(); }
  const Layer* mask_layer() const { return mask_layer_.get(); }

  virtual void SetNeedsDisplayRect(const gfx::RectF& dirty_rect);
  void SetNeedsDisplay() { SetNeedsDisplayRect(gfx::RectF(bounds())); }

  void SetOpacity(float opacity);
  float opacity() const { return opacity_; }
  bool OpacityIsAnimating() const;
  virtual bool OpacityCanAnimateOnImplThread() const;

  void SetFilters(const WebKit::WebFilterOperations& filters);
  const WebKit::WebFilterOperations& filters() const { return filters_; }

  void SetFilter(const skia::RefPtr<SkImageFilter>& filter);
  skia::RefPtr<SkImageFilter> filter() const { return filter_; }

  // Background filters are filters applied to what is behind this layer, when
  // they are viewed through non-opaque regions in this layer. They are used
  // through the WebLayer interface, and are not exposed to HTML.
  void SetBackgroundFilters(const WebKit::WebFilterOperations& filters);
  const WebKit::WebFilterOperations& background_filters() const {
    return background_filters_;
  }

  virtual void SetContentsOpaque(bool opaque);
  bool contents_opaque() const { return contents_opaque_; }

  void SetPosition(gfx::PointF position);
  gfx::PointF position() const { return position_; }

  void SetIsContainerForFixedPositionLayers(bool container);
  bool is_container_for_fixed_position_layers() const {
    return is_container_for_fixed_position_layers_;
  }

  void SetFixedToContainerLayer(bool fixed_to_container_layer);
  bool fixed_to_container_layer() const { return fixed_to_container_layer_; }

  void SetSublayerTransform(const gfx::Transform& sublayer_transform);
  const gfx::Transform& sublayer_transform() const {
    return sublayer_transform_;
  }

  void SetTransform(const gfx::Transform& transform);
  const gfx::Transform& transform() const { return transform_; }
  bool TransformIsAnimating() const;

  DrawProperties<Layer, RenderSurface>& draw_properties() {
    return draw_properties_;
  }
  const DrawProperties<Layer, RenderSurface>& draw_properties() const {
    return draw_properties_;
  }

  // The following are shortcut accessors to get various information from
  // draw_properties_
  const gfx::Transform& draw_transform() const {
    return draw_properties_.target_space_transform;
  }
  const gfx::Transform& screen_space_transform() const {
    return draw_properties_.screen_space_transform;
  }
  float draw_opacity() const { return draw_properties_.opacity; }
  bool draw_opacity_is_animating() const {
    return draw_properties_.opacity_is_animating;
  }
  bool draw_transform_is_animating() const {
    return draw_properties_.target_space_transform_is_animating;
  }
  bool screen_space_transform_is_animating() const {
    return draw_properties_.screen_space_transform_is_animating;
  }
  bool screen_space_opacity_is_animating() const {
    return draw_properties_.screen_space_opacity_is_animating;
  }
  bool can_use_lcd_text() const { return draw_properties_.can_use_lcd_text; }
  bool is_clipped() const { return draw_properties_.is_clipped; }
  gfx::Rect clip_rect() const { return draw_properties_.clip_rect; }
  gfx::Rect drawable_content_rect() const {
    return draw_properties_.drawable_content_rect;
  }
  gfx::Rect visible_content_rect() const {
    return draw_properties_.visible_content_rect;
  }
  Layer* render_target() {
    DCHECK(!draw_properties_.render_target ||
           draw_properties_.render_target->render_surface());
    return draw_properties_.render_target;
  }
  const Layer* render_target() const {
    DCHECK(!draw_properties_.render_target ||
           draw_properties_.render_target->render_surface());
    return draw_properties_.render_target;
  }
  RenderSurface* render_surface() const {
    return draw_properties_.render_surface.get();
  }

  void SetScrollOffset(gfx::Vector2d scroll_offset);
  gfx::Vector2d scroll_offset() const { return scroll_offset_; }

  void SetMaxScrollOffset(gfx::Vector2d max_scroll_offset);
  gfx::Vector2d max_scroll_offset() const { return max_scroll_offset_; }

  void SetScrollable(bool scrollable);
  bool scrollable() const { return scrollable_; }

  void SetShouldScrollOnMainThread(bool should_scroll_on_main_thread);
  bool should_scroll_on_main_thread() const {
    return should_scroll_on_main_thread_;
  }

  void SetHaveWheelEventHandlers(bool have_wheel_event_handlers);
  bool have_wheel_event_handlers() const { return have_wheel_event_handlers_; }

  void SetNonFastScrollableRegion(const Region& non_fast_scrollable_region);
  const Region& non_fast_scrollable_region() const {
    return non_fast_scrollable_region_;
  }

  void SetTouchEventHandlerRegion(const Region& touch_event_handler_region);
  const Region& touch_event_handler_region() const {
    return touch_event_handler_region_;
  }

  void set_layer_scroll_client(WebKit::WebLayerScrollClient* client) {
    layer_scroll_client_ = client;
  }

  void SetDrawCheckerboardForMissingTiles(bool checkerboard);
  bool DrawCheckerboardForMissingTiles() const {
    return draw_checkerboard_for_missing_tiles_;
  }

  void SetForceRenderSurface(bool force_render_surface);
  bool force_render_surface() const { return force_render_surface_; }

  gfx::Vector2d scroll_delta() const { return gfx::Vector2d(); }

  void SetImplTransform(const gfx::Transform& transform);
  const gfx::Transform& impl_transform() const { return impl_transform_; }

  void SetDoubleSided(bool double_sided);
  bool double_sided() const { return double_sided_; }

  void SetPreserves3d(bool preserves_3d) { preserves_3d_ = preserves_3d; }
  bool preserves_3d() const { return preserves_3d_; }

  void set_use_parent_backface_visibility(bool use) {
    use_parent_backface_visibility_ = use;
  }
  bool use_parent_backface_visibility() const {
    return use_parent_backface_visibility_;
  }

  virtual void SetLayerTreeHost(LayerTreeHost* host);

  bool HasDelegatedContent() const { return false; }
  bool HasContributingDelegatedRenderPasses() const { return false; }

  void SetIsDrawable(bool is_drawable);

  void SetReplicaLayer(Layer* layer);
  Layer* replica_layer() { return replica_layer_.get(); }
  const Layer* replica_layer() const { return replica_layer_.get(); }

  bool has_mask() const { return !!mask_layer_; }
  bool has_replica() const { return !!replica_layer_; }
  bool replica_has_mask() const {
    return replica_layer_ && (mask_layer_ || replica_layer_->mask_layer_);
  }

  // These methods typically need to be overwritten by derived classes.
  virtual bool DrawsContent() const;
  virtual void Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker* occlusion) {}
  virtual bool NeedMoreUpdates();
  virtual void SetIsMask(bool is_mask) {}

  void SetDebugName(const std::string& debug_name);

  virtual void PushPropertiesTo(LayerImpl* layer);

  void ClearRenderSurface() { draw_properties_.render_surface.reset(); }
  void CreateRenderSurface();

  // The contents scale converts from logical, non-page-scaled pixels to target
  // pixels. The contents scale is 1 for the root layer as it is already in
  // physical pixels. By default contents scale is forced to be 1 except for
  // subclasses of ContentsScalingLayer.
  float contents_scale_x() const { return draw_properties_.contents_scale_x; }
  float contents_scale_y() const { return draw_properties_.contents_scale_y; }
  gfx::Size content_bounds() const { return draw_properties_.content_bounds; }

  virtual void CalculateContentsScale(float ideal_contents_scale,
                                      bool animating_transform_to_screen,
                                      float* contents_scale_x,
                                      float* contents_scale_y,
                                      gfx::Size* content_bounds);

  // The scale at which contents should be rastered, to match the scale at
  // which they will drawn to the screen. This scale is a component of the
  // contentsScale() but does not include page/device scale factors.
  void SetRasterScale(float scale);
  float raster_scale() const { return raster_scale_; }

  // When true, the RasterScale() will be set by the compositor. If false, it
  // will use whatever value is given to it by the embedder.
  bool automatically_compute_raster_scale() {
    return automatically_compute_raster_scale_;
  }
  void SetAutomaticallyComputeRasterScale(bool automatic);

  void ForceAutomaticRasterScaleToBeRecomputed();

  // When true, the layer's contents are not scaled by the current page scale
  // factor. SetBoundsContainPageScale() recursively sets the value on all
  // child layers.
  void SetBoundsContainPageScale(bool bounds_contain_page_scale);
  bool bounds_contain_page_scale() const { return bounds_contain_page_scale_; }

  LayerTreeHost* layer_tree_host() const { return layer_tree_host_; }

  // Set the priority of all desired textures in this layer.
  virtual void SetTexturePriorities(const PriorityCalculator& priority_calc) {}

  bool AddAnimation(scoped_ptr<Animation> animation);
  void PauseAnimation(int animation_id, double time_offset);
  void RemoveAnimation(int animation_id);

  void SuspendAnimations(double monotonic_time);
  void ResumeAnimations(double monotonic_time);

  LayerAnimationController* layer_animation_controller() {
    return layer_animation_controller_.get();
  }
  void SetLayerAnimationController(
      scoped_refptr<LayerAnimationController> controller);
  scoped_refptr<LayerAnimationController> ReleaseLayerAnimationController();

  void set_layer_animation_delegate(WebKit::WebAnimationDelegate* delegate) {
    layer_animation_delegate_ = delegate;
  }

  bool HasActiveAnimation() const;

  virtual void NotifyAnimationStarted(const AnimationEvent& event,
                                      double wall_clock_time);
  virtual void NotifyAnimationFinished(double wall_clock_time);
  virtual void NotifyAnimationPropertyUpdate(const AnimationEvent& event);

  void AddLayerAnimationEventObserver(
      LayerAnimationEventObserver* animation_observer);
  void RemoveLayerAnimationEventObserver(
      LayerAnimationEventObserver* animation_observer);

  virtual Region VisibleContentOpaqueRegion() const;

  virtual ScrollbarLayer* ToScrollbarLayer();

  gfx::Rect LayerRectToContentRect(const gfx::RectF& layer_rect) const;

  // In impl-side painting, this returns true if this layer type is not
  // compatible with the main thread running freely, such as a double-buffered
  // canvas that doesn't want to be triple-buffered across all three trees.
  virtual bool BlocksPendingCommit() const;
  // Returns true if anything in this tree blocksPendingCommit.
  bool BlocksPendingCommitRecursive() const;

  virtual bool CanClipSelf() const;

  // Constructs a LayerImpl of the correct runtime type for this Layer type.
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl);

  bool NeedsDisplayForTesting() const { return needs_display_; }
  void ResetNeedsDisplayForTesting() { needs_display_ = false; }

  RenderingStatsInstrumentation* rendering_stats_instrumentation() const;

 protected:
  friend class LayerImpl;
  friend class TreeSynchronizer;
  virtual ~Layer();

  Layer();

  void SetNeedsCommit();
  void SetNeedsFullTreeSync();

  // This flag is set when layer need repainting/updating.
  bool needs_display_;

  // Tracks whether this layer may have changed stacking order with its
  // siblings.
  bool stacking_order_changed_;

  // The update rect is the region of the compositor resource that was
  // actually updated by the compositor. For layers that may do updating
  // outside the compositor's control (i.e. plugin layers), this information
  // is not available and the update rect will remain empty.
  // Note this rect is in layer space (not content space).
  gfx::RectF update_rect_;

  scoped_refptr<Layer> mask_layer_;

  int layer_id_;

  // When true, the layer is about to perform an update. Any commit requests
  // will be handled implcitly after the update completes.
  bool ignore_set_needs_commit_;

 private:
  friend class base::RefCounted<Layer>;

  void SetParent(Layer* layer);
  bool HasAncestor(Layer* ancestor) const;
  bool DescendantIsFixedToContainerLayer() const;

  // Returns the index of the child or -1 if not found.
  int IndexOfChild(const Layer* reference);

  // This should only be called from RemoveFromParent().
  void RemoveChildOrDependent(Layer* child);

  // LayerAnimationValueObserver implementation.
  virtual void OnOpacityAnimated(float opacity) OVERRIDE;
  virtual void OnTransformAnimated(const gfx::Transform& transform) OVERRIDE;
  virtual bool IsActive() const OVERRIDE;

  LayerList children_;
  Layer* parent_;

  // Layer instances have a weak pointer to their LayerTreeHost.
  // This pointer value is nil when a Layer is not in a tree and is
  // updated via SetLayerTreeHost() if a layer moves between trees.
  LayerTreeHost* layer_tree_host_;

  ObserverList<LayerAnimationEventObserver> layer_animation_observers_;

  scoped_refptr<LayerAnimationController> layer_animation_controller_;

  // Layer properties.
  gfx::Size bounds_;

  gfx::Vector2d scroll_offset_;
  gfx::Vector2d max_scroll_offset_;
  bool scrollable_;
  bool should_scroll_on_main_thread_;
  bool have_wheel_event_handlers_;
  Region non_fast_scrollable_region_;
  Region touch_event_handler_region_;
  gfx::PointF position_;
  gfx::PointF anchor_point_;
  SkColor background_color_;
  std::string debug_name_;
  float opacity_;
  skia::RefPtr<SkImageFilter> filter_;
  WebKit::WebFilterOperations filters_;
  WebKit::WebFilterOperations background_filters_;
  float anchor_point_z_;
  bool is_container_for_fixed_position_layers_;
  bool fixed_to_container_layer_;
  bool is_drawable_;
  bool masks_to_bounds_;
  bool contents_opaque_;
  bool double_sided_;
  bool preserves_3d_;
  bool use_parent_backface_visibility_;
  bool draw_checkerboard_for_missing_tiles_;
  bool force_render_surface_;

  gfx::Transform transform_;
  gfx::Transform sublayer_transform_;

  // Replica layer used for reflections.
  scoped_refptr<Layer> replica_layer_;

  // Transient properties.
  float raster_scale_;
  bool automatically_compute_raster_scale_;
  bool bounds_contain_page_scale_;

  gfx::Transform impl_transform_;

  WebKit::WebAnimationDelegate* layer_animation_delegate_;
  WebKit::WebLayerScrollClient* layer_scroll_client_;

  DrawProperties<Layer, RenderSurface> draw_properties_;

  DISALLOW_COPY_AND_ASSIGN(Layer);
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_H_
