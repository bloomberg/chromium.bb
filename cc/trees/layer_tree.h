// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_H_
#define CC_TREES_LAYER_TREE_H_

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"
#include "cc/input/event_listener_properties.h"
#include "cc/input/layer_selection_bound.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_list_iterator.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/property_tree.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace cc {
class HeadsUpDisplayLayer;
class Layer;
class LayerTreeHost;
class LayerTreeImpl;
class LayerTreeSettings;
class MutatorHost;
struct PendingPageScaleAnimation;

class CC_EXPORT LayerTree : public MutatorHostClient {
 public:
  using LayerSet = std::unordered_set<Layer*>;
  using LayerIdMap = std::unordered_map<int, Layer*>;

  LayerTree(MutatorHost* mutator_host, LayerTreeHost* layer_tree_host);
  virtual ~LayerTree();

  void SetRootLayer(scoped_refptr<Layer> root_layer);
  Layer* root_layer() { return root_layer_.get(); }
  const Layer* root_layer() const { return root_layer_.get(); }

  void RegisterViewportLayers(scoped_refptr<Layer> overscroll_elasticity_layer,
                              scoped_refptr<Layer> page_scale_layer,
                              scoped_refptr<Layer> inner_viewport_scroll_layer,
                              scoped_refptr<Layer> outer_viewport_scroll_layer);

  Layer* overscroll_elasticity_layer() const {
    return overscroll_elasticity_layer_.get();
  }
  Layer* page_scale_layer() const { return page_scale_layer_.get(); }
  Layer* inner_viewport_scroll_layer() const {
    return inner_viewport_scroll_layer_.get();
  }
  Layer* outer_viewport_scroll_layer() const {
    return outer_viewport_scroll_layer_.get();
  }

  void RegisterSelection(const LayerSelection& selection);
  const LayerSelection& selection() const { return selection_; }

  void SetHaveScrollEventHandlers(bool have_event_handlers);
  bool have_scroll_event_handlers() const {
    return have_scroll_event_handlers_;
  }

  void SetEventListenerProperties(EventListenerClass event_class,
                                  EventListenerProperties event_properties);
  EventListenerProperties event_listener_properties(
      EventListenerClass event_class) const {
    return event_listener_properties_[static_cast<size_t>(event_class)];
  }

  void SetViewportSize(const gfx::Size& device_viewport_size);
  gfx::Size device_viewport_size() const { return device_viewport_size_; }

  void SetBrowserControlsHeight(float height, bool shrink);
  void SetBrowserControlsShownRatio(float ratio);
  void SetBottomControlsHeight(float height);

  void SetPageScaleFactorAndLimits(float page_scale_factor,
                                   float min_page_scale_factor,
                                   float max_page_scale_factor);
  float page_scale_factor() const { return page_scale_factor_; }
  float min_page_scale_factor() const { return min_page_scale_factor_; }
  float max_page_scale_factor() const { return max_page_scale_factor_; }

  void set_background_color(SkColor color) { background_color_ = color; }
  SkColor background_color() const { return background_color_; }

  void set_has_transparent_background(bool transparent) {
    has_transparent_background_ = transparent;
  }
  bool has_transparent_background() const {
    return has_transparent_background_;
  }

  void StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                               bool use_anchor,
                               float scale,
                               base::TimeDelta duration);
  bool HasPendingPageScaleAnimation() const;

  void SetDeviceScaleFactor(float device_scale_factor);
  float device_scale_factor() const { return device_scale_factor_; }

  void SetPaintedDeviceScaleFactor(float painted_device_scale_factor);
  float painted_device_scale_factor() const {
    return painted_device_scale_factor_;
  }

  void SetDeviceColorSpace(const gfx::ColorSpace& device_color_space);
  const gfx::ColorSpace& device_color_space() const {
    return device_color_space_;
  }

  // Used externally by blink for setting the PropertyTrees when
  // |settings_.use_layer_lists| is true. This is a SPV2 setting.
  PropertyTrees* property_trees() { return &property_trees_; }

  void SetNeedsDisplayOnAllLayers();

  void SetNeedsCommit();

  const LayerTreeSettings& GetSettings() const;

  // Methods which should only be used internally in cc ------------------
  void RegisterLayer(Layer* layer);
  void UnregisterLayer(Layer* layer);
  Layer* LayerById(int id) const;

  size_t NumLayers() const;

  bool UpdateLayers(const LayerList& update_layer_list,
                    bool* content_is_suitable_for_gpu);
  bool in_paint_layer_contents() const { return in_paint_layer_contents_; }

  void AddLayerShouldPushProperties(Layer* layer);
  void RemoveLayerShouldPushProperties(Layer* layer);
  std::unordered_set<Layer*>& LayersThatShouldPushProperties();
  bool LayerNeedsPushPropertiesForTesting(Layer* layer) const;

  virtual void SetNeedsMetaInfoRecomputation(
      bool needs_meta_info_recomputation);
  bool needs_meta_info_recomputation() const {
    return needs_meta_info_recomputation_;
  }

  void SetPageScaleFromImplSide(float page_scale);
  void SetElasticOverscrollFromImplSide(gfx::Vector2dF elastic_overscroll);
  gfx::Vector2dF elastic_overscroll() const { return elastic_overscroll_; }

  void UpdateHudLayer(bool show_hud_info);
  HeadsUpDisplayLayer* hud_layer() const { return hud_layer_.get(); }

  virtual void SetNeedsFullTreeSync();
  bool needs_full_tree_sync() const { return needs_full_tree_sync_; }

  void SetPropertyTreesNeedRebuild();

  void PushPropertiesTo(LayerTreeImpl* tree_impl);

  MutatorHost* mutator_host() const { return mutator_host_; }

  Layer* LayerByElementId(ElementId element_id) const;
  void RegisterElement(ElementId element_id,
                       ElementListType list_type,
                       Layer* layer);
  void UnregisterElement(ElementId element_id,
                         ElementListType list_type,
                         Layer* layer);
  void SetElementIdsForTesting();

  void BuildPropertyTreesForTesting();

  // Layer iterators.
  LayerListIterator<Layer> begin() const;
  LayerListIterator<Layer> end() const;
  LayerListReverseIterator<Layer> rbegin();
  LayerListReverseIterator<Layer> rend();
  // ---------------------------------------------------------------------

 private:
  // MutatorHostClient implementation.
  bool IsElementInList(ElementId element_id,
                       ElementListType list_type) const override;
  void SetMutatorsNeedCommit() override;
  void SetMutatorsNeedRebuildPropertyTrees() override;
  void SetElementFilterMutated(ElementId element_id,
                               ElementListType list_type,
                               const FilterOperations& filters) override;
  void SetElementOpacityMutated(ElementId element_id,
                                ElementListType list_type,
                                float opacity) override;
  void SetElementTransformMutated(ElementId element_id,
                                  ElementListType list_type,
                                  const gfx::Transform& transform) override;
  void SetElementScrollOffsetMutated(
      ElementId element_id,
      ElementListType list_type,
      const gfx::ScrollOffset& scroll_offset) override;

  void ElementIsAnimatingChanged(ElementId element_id,
                                 ElementListType list_type,
                                 const PropertyAnimationState& mask,
                                 const PropertyAnimationState& state) override;

  void ScrollOffsetAnimationFinished() override {}
  gfx::ScrollOffset GetScrollOffsetForAnimation(
      ElementId element_id) const override;

  scoped_refptr<Layer> root_layer_;

  scoped_refptr<Layer> overscroll_elasticity_layer_;
  scoped_refptr<Layer> page_scale_layer_;
  scoped_refptr<Layer> inner_viewport_scroll_layer_;
  scoped_refptr<Layer> outer_viewport_scroll_layer_;

  float top_controls_height_ = 0.f;
  float top_controls_shown_ratio_ = 0.f;
  bool browser_controls_shrink_blink_size_ = false;

  float bottom_controls_height_ = 0.f;

  float device_scale_factor_ = 1.f;
  float painted_device_scale_factor_ = 1.f;
  float page_scale_factor_ = 1.f;
  float min_page_scale_factor_ = 1.f;
  float max_page_scale_factor_ = 1.f;
  gfx::ColorSpace device_color_space_;

  SkColor background_color_ = SK_ColorWHITE;
  bool has_transparent_background_ = false;

  LayerSelection selection_;

  gfx::Size device_viewport_size_;

  bool have_scroll_event_handlers_ = false;
  EventListenerProperties event_listener_properties_[static_cast<size_t>(
      EventListenerClass::kNumClasses)];

  std::unique_ptr<PendingPageScaleAnimation> pending_page_scale_animation_;

  PropertyTrees property_trees_;

  bool needs_full_tree_sync_ = true;
  bool needs_meta_info_recomputation_ = true;

  gfx::Vector2dF elastic_overscroll_;

  scoped_refptr<HeadsUpDisplayLayer> hud_layer_;

  // Set of layers that need to push properties.
  LayerSet layers_that_should_push_properties_;

  // Layer id to Layer map.
  LayerIdMap layer_id_map_;

  using ElementLayersMap = std::unordered_map<ElementId, Layer*, ElementIdHash>;
  ElementLayersMap element_layers_map_;

  bool in_paint_layer_contents_ = false;

  MutatorHost* mutator_host_;
  LayerTreeHost* layer_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(LayerTree);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_H_
