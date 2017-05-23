// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_IMPL_H_
#define CC_LAYERS_LAYER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "cc/base/region.h"
#include "cc/base/synced_property.h"
#include "cc/cc_export.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/input/input_handler.h"
#include "cc/layers/draw_properties.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_impl_test_properties.h"
#include "cc/layers/layer_position_constraint.h"
#include "cc/layers/performance_properties.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/resources/resource_provider.h"
#include "cc/tiles/tile_priority.h"
#include "cc/trees/element_id.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/target_property.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/transform.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
class TracedValue;
}
class DictionaryValue;
}

namespace cc {

class AppendQuadsData;
class LayerTreeImpl;
class MicroBenchmarkImpl;
class MutatorHost;
class PrioritizedTile;
class RenderPass;
class ScrollbarLayerImplBase;
class SimpleEnclosedRegion;
class Tile;


enum DrawMode {
  DRAW_MODE_NONE,
  DRAW_MODE_HARDWARE,
  DRAW_MODE_SOFTWARE,
  DRAW_MODE_RESOURCELESS_SOFTWARE
};

enum ViewportLayerType {
  NOT_VIEWPORT_LAYER,
  INNER_VIEWPORT_CONTAINER,
  OUTER_VIEWPORT_CONTAINER,
  INNER_VIEWPORT_SCROLL,
  OUTER_VIEWPORT_SCROLL,
  LAST_VIEWPORT_LAYER_TYPE = OUTER_VIEWPORT_SCROLL,
};

class CC_EXPORT LayerImpl {
 public:
  static std::unique_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return base::WrapUnique(new LayerImpl(tree_impl, id));
  }

  virtual ~LayerImpl();

  int id() const { return layer_id_; }

  // Interactions with attached animations.
  gfx::ScrollOffset ScrollOffsetForAnimation() const;
  bool IsActive() const;

  void set_property_tree_sequence_number(int sequence_number) {}

  void SetTransformTreeIndex(int index);
  int transform_tree_index() const { return transform_tree_index_; }

  void SetClipTreeIndex(int index);
  int clip_tree_index() const { return clip_tree_index_; }

  void SetEffectTreeIndex(int index);
  int effect_tree_index() const { return effect_tree_index_; }
  int render_target_effect_tree_index() const;

  void SetScrollTreeIndex(int index);
  int scroll_tree_index() const { return scroll_tree_index_; }

  void set_offset_to_transform_parent(const gfx::Vector2dF& offset) {
    offset_to_transform_parent_ = offset;
  }
  gfx::Vector2dF offset_to_transform_parent() const {
    return offset_to_transform_parent_;
  }

  void set_should_flatten_transform_from_property_tree(bool should_flatten) {
    should_flatten_transform_from_property_tree_ = should_flatten;
  }
  bool should_flatten_transform_from_property_tree() const {
    return should_flatten_transform_from_property_tree_;
  }

  bool is_clipped() const { return draw_properties_.is_clipped; }

  void UpdatePropertyTreeScrollOffset();

  LayerTreeImpl* layer_tree_impl() const { return layer_tree_impl_; }

  void PopulateSharedQuadState(SharedQuadState* state) const;
  void PopulateScaledSharedQuadState(SharedQuadState* state,
                                     float layer_to_content_scale_x,
                                     float layer_to_content_scale_y) const;
  // WillDraw must be called before AppendQuads. If WillDraw returns false,
  // AppendQuads and DidDraw will not be called. If WillDraw returns true,
  // DidDraw is guaranteed to be called before another WillDraw or before
  // the layer is destroyed. To enforce this, any class that overrides
  // WillDraw/DidDraw must call the base class version only if WillDraw
  // returns true.
  virtual bool WillDraw(DrawMode draw_mode,
                        ResourceProvider* resource_provider);
  virtual void AppendQuads(RenderPass* render_pass,
                           AppendQuadsData* append_quads_data) {}
  virtual void DidDraw(ResourceProvider* resource_provider);

  // Verify that the resource ids in the quad are valid.
  void ValidateQuadResources(DrawQuad* quad) const {
#if DCHECK_IS_ON()
    ValidateQuadResourcesInternal(quad);
#endif
  }

  virtual void GetContentsResourceId(ResourceId* resource_id,
                                     gfx::Size* resource_size,
                                     gfx::SizeF* resource_uv_size) const;

  virtual void NotifyTileStateChanged(const Tile* tile) {}

  virtual ScrollbarLayerImplBase* ToScrollbarLayer();

  // Returns true if this layer has content to draw.
  void SetDrawsContent(bool draws_content);
  bool DrawsContent() const { return draws_content_; }

  LayerImplTestProperties* test_properties() {
    if (!test_properties_)
      test_properties_.reset(new LayerImplTestProperties(this));
    return test_properties_.get();
  }

  void SetBackgroundColor(SkColor background_color);
  SkColor background_color() const { return background_color_; }
  void SetSafeOpaqueBackgroundColor(SkColor background_color);
  // If contents_opaque(), return an opaque color else return a
  // non-opaque color.  Tries to return background_color(), if possible.
  SkColor SafeOpaqueBackgroundColor() const;

  bool HasPotentiallyRunningFilterAnimation() const;

  void SetMasksToBounds(bool masks_to_bounds);
  bool masks_to_bounds() const { return masks_to_bounds_; }

  void SetContentsOpaque(bool opaque);
  bool contents_opaque() const { return contents_opaque_; }

  float Opacity() const;
  const gfx::Transform& Transform() const;

  // Stable identifier for clients. See comment in cc/trees/element_id.h.
  void SetElementId(ElementId element_id);
  ElementId element_id() const { return element_id_; }

  void SetMutableProperties(uint32_t properties);
  uint32_t mutable_properties() const { return mutable_properties_; }

  void SetPosition(const gfx::PointF& position);
  gfx::PointF position() const { return position_; }

  bool IsAffectedByPageScale() const;

  bool Is3dSorted() const { return GetSortingContextId() != 0; }

  void SetUseParentBackfaceVisibility(bool use) {
    use_parent_backface_visibility_ = use;
  }
  bool use_parent_backface_visibility() const {
    return use_parent_backface_visibility_;
  }

  void SetUseLocalTransformForBackfaceVisibility(bool use_local) {
    use_local_transform_for_backface_visibility_ = use_local;
  }
  bool use_local_transform_for_backface_visibility() const {
    return use_local_transform_for_backface_visibility_;
  }

  void SetShouldCheckBackfaceVisibility(bool should_check_backface_visibility) {
    should_check_backface_visibility_ = should_check_backface_visibility;
  }
  bool should_check_backface_visibility() const {
    return should_check_backface_visibility_;
  }

  bool ShowDebugBorders(DebugBorderType type) const;

  // The render surface which this layer draws into. This can be either owned by
  // the same layer or an ancestor of this layer.
  RenderSurfaceImpl* render_target();
  const RenderSurfaceImpl* render_target() const;

  DrawProperties& draw_properties() { return draw_properties_; }
  const DrawProperties& draw_properties() const { return draw_properties_; }

  gfx::Transform DrawTransform() const;
  gfx::Transform ScreenSpaceTransform() const;
  PerformanceProperties<LayerImpl>& performance_properties() {
    return performance_properties_;
  }

  bool CanUseLCDText() const;

  // Setter for draw_properties_.
  void set_visible_layer_rect(const gfx::Rect& visible_rect) {
    draw_properties_.visible_layer_rect = visible_rect;
  }
  void set_clip_rect(const gfx::Rect& clip_rect) {
    draw_properties_.clip_rect = clip_rect;
  }

  // The following are shortcut accessors to get various information from
  // draw_properties_
  float draw_opacity() const { return draw_properties_.opacity; }
  bool screen_space_transform_is_animating() const {
    return draw_properties_.screen_space_transform_is_animating;
  }
  gfx::Rect clip_rect() const { return draw_properties_.clip_rect; }
  gfx::Rect drawable_content_rect() const {
    return draw_properties_.drawable_content_rect;
  }
  gfx::Rect visible_layer_rect() const {
    return draw_properties_.visible_layer_rect;
  }

  // The client should be responsible for setting bounds, content bounds and
  // contents scale to appropriate values. LayerImpl doesn't calculate any of
  // them from the other values.

  void SetBounds(const gfx::Size& bounds);
  gfx::Size bounds() const;
  // Like bounds() but doesn't snap to int. Lossy on giant pages (e.g. millions
  // of pixels) due to use of single precision float.
  gfx::SizeF BoundsForScrolling() const;

  // Viewport bounds delta are only used for viewport layers and account for
  // changes in the viewport layers from browser controls and page scale
  // factors. These deltas are only set on the active tree.
  void SetViewportBoundsDelta(const gfx::Vector2dF& bounds_delta);
  gfx::Vector2dF ViewportBoundsDelta() const;

  void SetViewportLayerType(ViewportLayerType type) {
    // Once set as a viewport layer type, the viewport type should not change.
    DCHECK(viewport_layer_type() == NOT_VIEWPORT_LAYER ||
           viewport_layer_type() == type);
    viewport_layer_type_ = type;
  }
  ViewportLayerType viewport_layer_type() const {
    return static_cast<ViewportLayerType>(viewport_layer_type_);
  }

  void SetCurrentScrollOffset(const gfx::ScrollOffset& scroll_offset);
  gfx::ScrollOffset CurrentScrollOffset() const;

  gfx::ScrollOffset MaxScrollOffset() const;
  gfx::ScrollOffset ClampScrollOffsetToLimits(gfx::ScrollOffset offset) const;
  gfx::Vector2dF ClampScrollToMaxScrollOffset();

  // Returns the delta of the scroll that was outside of the bounds of the
  // initial scroll
  gfx::Vector2dF ScrollBy(const gfx::Vector2dF& scroll);

  void SetScrollClipLayer(int scroll_clip_layer_id);
  int scroll_clip_layer_id() const { return scroll_clip_layer_id_; }
  LayerImpl* scroll_clip_layer() const;
  bool scrollable() const;

  void set_user_scrollable_horizontal(bool scrollable);
  bool user_scrollable_horizontal() const {
    return user_scrollable_horizontal_;
  }
  void set_user_scrollable_vertical(bool scrollable);
  bool user_scrollable_vertical() const { return user_scrollable_vertical_; }

  bool user_scrollable(ScrollbarOrientation orientation) const;

  void set_main_thread_scrolling_reasons(
      uint32_t main_thread_scrolling_reasons) {
    main_thread_scrolling_reasons_ = main_thread_scrolling_reasons;
  }
  uint32_t main_thread_scrolling_reasons() const {
    return main_thread_scrolling_reasons_;
  }

  void SetNonFastScrollableRegion(const Region& region) {
    non_fast_scrollable_region_ = region;
  }
  const Region& non_fast_scrollable_region() const {
    return non_fast_scrollable_region_;
  }

  void SetTouchEventHandlerRegion(const Region& region) {
    touch_event_handler_region_ = region;
  }
  const Region& touch_event_handler_region() const {
    return touch_event_handler_region_;
  }

  bool HasPotentiallyRunningTransformAnimation() const;

  bool HasFilterAnimationThatInflatesBounds() const;
  bool HasTransformAnimationThatInflatesBounds() const;
  bool HasAnimationThatInflatesBounds() const;

  bool FilterAnimationBoundsForBox(const gfx::BoxF& box,
                                   gfx::BoxF* bounds) const;
  bool TransformAnimationBoundsForBox(const gfx::BoxF& box,
                                      gfx::BoxF* bounds) const;

  // Note this rect is in layer space (not content space).
  void SetUpdateRect(const gfx::Rect& update_rect);
  const gfx::Rect& update_rect() const { return update_rect_; }

  void AddDamageRect(const gfx::Rect& damage_rect);
  const gfx::Rect& damage_rect() const { return damage_rect_; }

  virtual std::unique_ptr<base::DictionaryValue> LayerTreeAsJson();

  bool LayerPropertyChanged() const;

  void ResetChangeTracking();

  virtual SimpleEnclosedRegion VisibleOpaqueRegion() const;

  virtual void DidBecomeActive() {}

  virtual void DidBeginTracing();

  // Release resources held by this layer. Called when the output surface
  // that rendered this layer was lost.
  virtual void ReleaseResources();

  // Release tile resources held by this layer. Called when a rendering mode
  // switch has occured and tiles are no longer valid.
  virtual void ReleaseTileResources();

  // Recreate tile resources held by this layer after they were released by a
  // ReleaseTileResources call.
  virtual void RecreateTileResources();

  virtual std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl);
  virtual bool IsSnapped();
  virtual void PushPropertiesTo(LayerImpl* layer);

  virtual void GetAllPrioritizedTilesForTracing(
      std::vector<PrioritizedTile>* prioritized_tiles) const;
  virtual void AsValueInto(base::trace_event::TracedValue* dict) const;

  virtual size_t GPUMemoryUsageInBytes() const;

  void SetNeedsPushProperties();

  virtual void RunMicroBenchmark(MicroBenchmarkImpl* benchmark);

  void SetDebugInfo(
      std::unique_ptr<base::trace_event::ConvertableToTraceFormat> debug_info);

  void set_contributes_to_drawn_render_surface(bool is_member) {
    contributes_to_drawn_render_surface_ = is_member;
  }

  bool contributes_to_drawn_render_surface() const {
    return contributes_to_drawn_render_surface_;
  }

  bool IsDrawnScrollbar() {
    return ToScrollbarLayer() && contributes_to_drawn_render_surface_;
  }

  void set_may_contain_video(bool yes) { may_contain_video_ = yes; }
  bool may_contain_video() const { return may_contain_video_; }

  // Layers that share a sorting context id will be sorted together in 3d
  // space.  0 is a special value that means this layer will not be sorted and
  // will be drawn in paint order.
  int GetSortingContextId() const;

  // Get the correct invalidation region instead of conservative Rect
  // for layers that provide it.
  virtual Region GetInvalidationRegionForDebugging();

  virtual gfx::Rect GetEnclosingRectInTargetSpace() const;

  bool has_copy_requests_in_target_subtree();

  void UpdatePropertyTreeForScrollingAndAnimationIfNeeded();

  float GetIdealContentsScale() const;

  void NoteLayerPropertyChanged();

  void SetHasWillChangeTransformHint(bool has_will_change);
  bool has_will_change_transform_hint() const {
    return has_will_change_transform_hint_;
  }

  MutatorHost* GetMutatorHost() const;

  ElementListType GetElementTypeForAnimation() const;

  void set_needs_show_scrollbars(bool yes) { needs_show_scrollbars_ = yes; }
  bool needs_show_scrollbars() { return needs_show_scrollbars_; }

  void set_raster_even_if_not_in_rsll(bool yes) {
    raster_even_if_not_in_rsll_ = yes;
  }
  bool raster_even_if_not_in_rsll() const {
    return raster_even_if_not_in_rsll_;
  }

 protected:
  LayerImpl(LayerTreeImpl* layer_impl,
            int id,
            scoped_refptr<SyncedScrollOffset> scroll_offset);
  LayerImpl(LayerTreeImpl* layer_impl, int id);

  // Get the color and size of the layer's debug border.
  virtual void GetDebugBorderProperties(SkColor* color, float* width) const;

  void AppendDebugBorderQuad(RenderPass* render_pass,
                             const gfx::Size& bounds,
                             const SharedQuadState* shared_quad_state,
                             AppendQuadsData* append_quads_data) const;
  void AppendDebugBorderQuad(RenderPass* render_pass,
                             const gfx::Size& bounds,
                             const SharedQuadState* shared_quad_state,
                             AppendQuadsData* append_quads_data,
                             SkColor color,
                             float width) const;

  gfx::Rect GetScaledEnclosingRectInTargetSpace(float scale) const;

 private:
  // This includes all animations, even those that are finished but haven't yet
  // been deleted.
  bool HasAnyAnimationTargetingProperty(TargetProperty::Type property) const;

  void ValidateQuadResourcesInternal(DrawQuad* quad) const;

  virtual const char* LayerTypeAsString() const;

  int layer_id_;
  LayerTreeImpl* layer_tree_impl_;

  std::unique_ptr<LayerImplTestProperties> test_properties_;

  // Properties synchronized from the associated Layer.
  gfx::Size bounds_;
  int scroll_clip_layer_id_;

  gfx::Vector2dF offset_to_transform_parent_;
  uint32_t main_thread_scrolling_reasons_;

  bool user_scrollable_horizontal_ : 1;
  bool user_scrollable_vertical_ : 1;
  bool should_flatten_transform_from_property_tree_ : 1;

  // Tracks if drawing-related properties have changed since last redraw.
  bool layer_property_changed_ : 1;
  bool may_contain_video_ : 1;

  bool masks_to_bounds_ : 1;
  bool contents_opaque_ : 1;
  bool use_parent_backface_visibility_ : 1;
  bool use_local_transform_for_backface_visibility_ : 1;
  bool should_check_backface_visibility_ : 1;
  bool draws_content_ : 1;
  bool contributes_to_drawn_render_surface_ : 1;

  static_assert(LAST_VIEWPORT_LAYER_TYPE < (1u << 3),
                "enough bits for ViewportLayerType (viewport_layer_type_)");
  uint8_t viewport_layer_type_ : 3;  // ViewportLayerType

  Region non_fast_scrollable_region_;
  Region touch_event_handler_region_;
  SkColor background_color_;
  SkColor safe_opaque_background_color_;

  gfx::PointF position_;

  gfx::Rect clip_rect_in_target_space_;
  int transform_tree_index_;
  int effect_tree_index_;
  int clip_tree_index_;
  int scroll_tree_index_;

 protected:
  friend class TreeSynchronizer;

  DrawMode current_draw_mode_;

 private:
  PropertyTrees* GetPropertyTrees() const;
  ClipTree& GetClipTree() const;
  EffectTree& GetEffectTree() const;
  ScrollTree& GetScrollTree() const;
  TransformTree& GetTransformTree() const;

  ElementId element_id_;
  uint32_t mutable_properties_;
  // Rect indicating what was repainted/updated during update.
  // Note that plugin layers bypass this and leave it empty.
  // This is in the layer's space.
  gfx::Rect update_rect_;

  // Denotes an area that is damaged and needs redraw. This is in the layer's
  // space.
  gfx::Rect damage_rect_;

  // Group of properties that need to be computed based on the layer tree
  // hierarchy before layers can be drawn.
  DrawProperties draw_properties_;
  PerformanceProperties<LayerImpl> performance_properties_;

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
      owned_debug_info_;
  base::trace_event::ConvertableToTraceFormat* debug_info_;

  bool has_will_change_transform_hint_ : 1;
  bool needs_push_properties_ : 1;
  bool scrollbars_hidden_ : 1;

  // The needs_show_scrollbars_ bit tracks a pending request from Blink to show
  // the overlay scrollbars. It's set on the scroll layer (not the scrollbar
  // layers) and consumed by LayerTreeImpl::PushPropertiesTo during activation.
  bool needs_show_scrollbars_ : 1;

  bool raster_even_if_not_in_rsll_ : 1;

  DISALLOW_COPY_AND_ASSIGN(LayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_IMPL_H_
