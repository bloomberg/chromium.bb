// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_IMPL_H_
#define CC_TREES_LAYER_TREE_IMPL_H_

#include "base/hash_tables.h"
#include "base/values.h"
#include "cc/layers/layer_impl.h"

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template<>
struct hash<cc::LayerImpl*> {
  size_t operator()(cc::LayerImpl* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
} // namespace BASE_HASH_NAMESPACE
#endif // COMPILER

namespace cc {

class DebugRectHistory;
class FrameRateCounter;
class HeadsUpDisplayLayerImpl;
class LayerTreeDebugState;
class LayerTreeHostImpl;
class LayerTreeImpl;
class LayerTreeSettings;
class MemoryHistory;
class OutputSurface;
class PaintTimeCounter;
class Proxy;
class ResourceProvider;
class ScrollbarLayerImpl;
class TileManager;
struct RendererCapabilities;

class CC_EXPORT LayerTreeImpl {
 public:
  typedef std::vector<LayerImpl*> LayerList;

  static scoped_ptr<LayerTreeImpl> create(LayerTreeHostImpl* layer_tree_host_impl)
  {
    return make_scoped_ptr(new LayerTreeImpl(layer_tree_host_impl));
  }
  virtual ~LayerTreeImpl();

  // Methods called by the layer tree that pass-through or access LTHI.
  // ---------------------------------------------------------------------------
  const LayerTreeSettings& settings() const;
  const RendererCapabilities& rendererCapabilities() const;
  OutputSurface* output_surface() const;
  ResourceProvider* resource_provider() const;
  TileManager* tile_manager() const;
  FrameRateCounter* frame_rate_counter() const;
  PaintTimeCounter* paint_time_counter() const;
  MemoryHistory* memory_history() const;
  bool IsActiveTree() const;
  bool IsPendingTree() const;
  bool IsRecycleTree() const;
  LayerImpl* FindActiveTreeLayerById(int id);
  LayerImpl* FindPendingTreeLayerById(int id);
  int MaxTextureSize() const;
  bool PinchGestureActive() const;
  base::TimeTicks CurrentFrameTime() const;
  void SetNeedsCommit();

  // Tree specific methods exposed to layer-impl tree.
  // ---------------------------------------------------------------------------
  void SetNeedsRedraw();

  // TODO(nduca): These are implemented in cc files temporarily, but will become
  // trivial accessors in a followup patch.
  const LayerTreeDebugState& debug_state() const;
  float device_scale_factor() const;
  gfx::Size device_viewport_size() const;
  gfx::Size layout_viewport_size() const;
  std::string layer_tree_as_text() const;
  DebugRectHistory* debug_rect_history() const;
  scoped_ptr<base::Value> AsValue() const;

  // Other public methods
  // ---------------------------------------------------------------------------
  LayerImpl* root_layer() const { return root_layer_.get(); }
  void SetRootLayer(scoped_ptr<LayerImpl>);
  scoped_ptr<LayerImpl> DetachLayerTree();

  void PushPropertiesTo(LayerTreeImpl*);

  int source_frame_number() const { return source_frame_number_; }
  void set_source_frame_number(int frame_number) {
    source_frame_number_ = frame_number;
  }

  HeadsUpDisplayLayerImpl* hud_layer() { return hud_layer_; }
  void set_hud_layer(HeadsUpDisplayLayerImpl* layer_impl) {
    hud_layer_ = layer_impl;
  }

  LayerImpl* RootScrollLayer() const;
  LayerImpl* RootClipLayer() const;
  LayerImpl* CurrentlyScrollingLayer() const;
  void SetCurrentlyScrollingLayer(LayerImpl* layer);
  void ClearCurrentlyScrollingLayer();

  void FindRootScrollLayer();
  void UpdateMaxScrollOffset();

  SkColor background_color() const { return background_color_; }
  void set_background_color(SkColor color) { background_color_ = color; }

  bool has_transparent_background() const {
    return has_transparent_background_;
  }
  void set_has_transparent_background(bool transparent) {
    has_transparent_background_ = transparent;
  }

  enum UpdateDrawPropertiesReason {
    UPDATE_PENDING_TREE,
    UPDATE_ACTIVE_TREE,
    UPDATE_ACTIVE_TREE_FOR_DRAW
  };

  gfx::Transform ImplTransform() const;

  void SetPageScaleFactorAndLimits(float page_scale_factor,
      float min_page_scale_factor, float max_page_scale_factor);
  void SetPageScaleDelta(float delta);
  float total_page_scale_factor() const {
    return page_scale_factor_ * page_scale_delta_;
  }
  float page_scale_factor() const { return page_scale_factor_; }
  float min_page_scale_factor() const { return min_page_scale_factor_; }
  float max_page_scale_factor() const { return max_page_scale_factor_; }
  float page_scale_delta() const  { return page_scale_delta_; }
  void set_sent_page_scale_delta(float delta) {
    sent_page_scale_delta_ = delta;
  }
  float sent_page_scale_delta() const { return sent_page_scale_delta_; }

  // Updates draw properties and render surface layer list
  void UpdateDrawProperties(UpdateDrawPropertiesReason reason);
  void set_needs_update_draw_properties() {
    needs_update_draw_properties_ = true;
  }
  bool needs_update_draw_properties() const {
    return needs_update_draw_properties_;
  }

  void set_needs_full_tree_sync(bool needs) { needs_full_tree_sync_ = needs; }
  bool needs_full_tree_sync() const { return needs_full_tree_sync_; }

  void ClearRenderSurfaces();

  bool AreVisibleResourcesReady() const;

  const LayerList& RenderSurfaceLayerList() const;

  // These return the size of the root scrollable area and the size of
  // the user-visible scrolling viewport, in CSS layout coordinates.
  gfx::Size ScrollableSize() const;
  gfx::SizeF ScrollableViewportSize() const;

  LayerImpl* LayerById(int id);

  // These should be called by LayerImpl's ctor/dtor.
  void RegisterLayer(LayerImpl* layer);
  void UnregisterLayer(LayerImpl* layer);

  AnimationRegistrar* animationRegistrar() const;

  void PushPersistedState(LayerTreeImpl* pendingTree);

  void DidBecomeActive();

  bool ContentsTexturesPurged() const;
  void SetContentsTexturesPurged();
  void ResetContentsTexturesPurged();

  // Set on the active tree when the viewport size recently changed
  // and the active tree's size is now out of date.
  bool ViewportSizeInvalid() const;
  void SetViewportSizeInvalid();
  void ResetViewportSizeInvalid();

  // Useful for debug assertions, probably shouldn't be used for anything else.
  Proxy* proxy() const;

  void SetPinchZoomHorizontalLayerId(int layer_id);
  void SetPinchZoomVerticalLayerId(int layer_id);

  void DidBeginScroll();
  void DidUpdateScroll();
  void DidEndScroll();

 protected:
  LayerTreeImpl(LayerTreeHostImpl* layer_tree_host_impl);

  void UpdateSolidColorScrollbars();

  // Hide existence of pinch-zoom scrollbars.
  void UpdatePinchZoomScrollbars();
  void FadeInPinchZoomScrollbars();
  void FadeOutPinchZoomScrollbars();
  ScrollbarLayerImpl* PinchZoomScrollbarHorizontal();
  ScrollbarLayerImpl* PinchZoomScrollbarVertical();
  bool HasPinchZoomScrollbars() const;

  LayerTreeHostImpl* layer_tree_host_impl_;
  int source_frame_number_;
  scoped_ptr<LayerImpl> root_layer_;
  HeadsUpDisplayLayerImpl* hud_layer_;
  LayerImpl* root_scroll_layer_;
  LayerImpl* currently_scrolling_layer_;
  SkColor background_color_;
  bool has_transparent_background_;

  int pinch_zoom_scrollbar_horizontal_layer_id_;
  int pinch_zoom_scrollbar_vertical_layer_id_;

  float page_scale_factor_;
  float page_scale_delta_;
  float sent_page_scale_delta_;
  float min_page_scale_factor_;
  float max_page_scale_factor_;

  typedef base::hash_map<int, LayerImpl*> LayerIdMap;
  LayerIdMap layer_id_map_;

  // Persisted state for non-impl-side-painting.
  int scrolling_layer_id_from_previous_tree_;

  // List of visible layers for the most recently prepared frame. Used for
  // rendering and input event hit testing.
  LayerList render_surface_layer_list_;

  bool contents_textures_purged_;
  bool viewport_size_invalid_;
  bool needs_update_draw_properties_;

  // In impl-side painting mode, this is true when the tree may contain
  // structural differences relative to the active tree.
  bool needs_full_tree_sync_;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeImpl);
};

}

#endif  // CC_TREES_LAYER_TREE_IMPL_H_
