// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree.h"

#include "base/auto_reset.h"
#include "base/time/time.h"
#include "cc/input/page_scale_animation.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/property_tree_builder.h"

namespace cc {

LayerTree::LayerTree(MutatorHost* mutator_host, LayerTreeHost* layer_tree_host)
    : event_listener_properties_(),
      mutator_host_(mutator_host),
      layer_tree_host_(layer_tree_host) {
  DCHECK(mutator_host_);
  DCHECK(layer_tree_host_);
  mutator_host_->SetMutatorHostClient(this);
}

LayerTree::~LayerTree() {
  mutator_host_->SetMutatorHostClient(nullptr);

  // We must clear any pointers into the layer tree prior to destroying it.
  RegisterViewportLayers(nullptr, nullptr, nullptr, nullptr);

  if (root_layer_) {
    root_layer_->SetLayerTreeHost(nullptr);

    // The root layer must be destroyed before the layer tree. We've made a
    // contract with our animation controllers that the animation_host will
    // outlive them, and we must make good.
    root_layer_ = nullptr;
  }
}

void LayerTree::SetRootLayer(scoped_refptr<Layer> root_layer) {
  if (root_layer_.get() == root_layer.get())
    return;

  if (root_layer_.get())
    root_layer_->SetLayerTreeHost(nullptr);
  root_layer_ = root_layer;
  if (root_layer_.get()) {
    DCHECK(!root_layer_->parent());
    root_layer_->SetLayerTreeHost(layer_tree_host_);
  }

  if (hud_layer_.get())
    hud_layer_->RemoveFromParent();

  // Reset gpu rasterization tracking.
  // This flag is sticky until a new tree comes along.
  layer_tree_host_->ResetGpuRasterizationTracking();

  SetNeedsFullTreeSync();
}

void LayerTree::RegisterViewportLayers(
    scoped_refptr<Layer> overscroll_elasticity_layer,
    scoped_refptr<Layer> page_scale_layer,
    scoped_refptr<Layer> inner_viewport_scroll_layer,
    scoped_refptr<Layer> outer_viewport_scroll_layer) {
  DCHECK(!inner_viewport_scroll_layer ||
         inner_viewport_scroll_layer != outer_viewport_scroll_layer);
  overscroll_elasticity_layer_ = overscroll_elasticity_layer;
  page_scale_layer_ = page_scale_layer;
  inner_viewport_scroll_layer_ = inner_viewport_scroll_layer;
  outer_viewport_scroll_layer_ = outer_viewport_scroll_layer;
}

void LayerTree::RegisterSelection(const LayerSelection& selection) {
  if (selection_ == selection)
    return;

  selection_ = selection;
  SetNeedsCommit();
}

void LayerTree::SetHaveScrollEventHandlers(bool have_event_handlers) {
  if (have_scroll_event_handlers_ == have_event_handlers)
    return;

  have_scroll_event_handlers_ = have_event_handlers;
  SetNeedsCommit();
}

void LayerTree::SetEventListenerProperties(EventListenerClass event_class,
                                           EventListenerProperties properties) {
  const size_t index = static_cast<size_t>(event_class);
  if (event_listener_properties_[index] == properties)
    return;

  event_listener_properties_[index] = properties;
  SetNeedsCommit();
}

void LayerTree::SetViewportSize(const gfx::Size& device_viewport_size) {
  if (device_viewport_size_ == device_viewport_size)
    return;

  device_viewport_size_ = device_viewport_size;

  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void LayerTree::SetBrowserControlsHeight(float height, bool shrink) {
  if (top_controls_height_ == height &&
      browser_controls_shrink_blink_size_ == shrink)
    return;

  top_controls_height_ = height;
  browser_controls_shrink_blink_size_ = shrink;
  SetNeedsCommit();
}

void LayerTree::SetBrowserControlsShownRatio(float ratio) {
  if (top_controls_shown_ratio_ == ratio)
    return;

  top_controls_shown_ratio_ = ratio;
  SetNeedsCommit();
}

void LayerTree::SetBottomControlsHeight(float height) {
  if (bottom_controls_height_ == height)
    return;

  bottom_controls_height_ = height;
  SetNeedsCommit();
}

void LayerTree::SetPageScaleFactorAndLimits(float page_scale_factor,
                                            float min_page_scale_factor,
                                            float max_page_scale_factor) {
  if (page_scale_factor_ == page_scale_factor &&
      min_page_scale_factor_ == min_page_scale_factor &&
      max_page_scale_factor_ == max_page_scale_factor)
    return;

  page_scale_factor_ = page_scale_factor;
  min_page_scale_factor_ = min_page_scale_factor;
  max_page_scale_factor_ = max_page_scale_factor;
  SetPropertyTreesNeedRebuild();
  SetNeedsCommit();
}

void LayerTree::StartPageScaleAnimation(const gfx::Vector2d& target_offset,
                                        bool use_anchor,
                                        float scale,
                                        base::TimeDelta duration) {
  pending_page_scale_animation_.reset(new PendingPageScaleAnimation(
      target_offset, use_anchor, scale, duration));

  SetNeedsCommit();
}

bool LayerTree::HasPendingPageScaleAnimation() const {
  return !!pending_page_scale_animation_.get();
}

void LayerTree::SetDeviceScaleFactor(float device_scale_factor) {
  if (device_scale_factor_ == device_scale_factor)
    return;
  device_scale_factor_ = device_scale_factor;

  property_trees_.needs_rebuild = true;
  SetNeedsCommit();
}

void LayerTree::SetPaintedDeviceScaleFactor(float painted_device_scale_factor) {
  if (painted_device_scale_factor_ == painted_device_scale_factor)
    return;
  painted_device_scale_factor_ = painted_device_scale_factor;

  SetNeedsCommit();
}

void LayerTree::SetDeviceColorSpace(const gfx::ColorSpace& device_color_space) {
  if (device_color_space_ == device_color_space)
    return;
  device_color_space_ = device_color_space;
  LayerTreeHostCommon::CallFunctionForEveryLayer(
      this, [](Layer* layer) { layer->SetNeedsDisplay(); });
}

void LayerTree::RegisterLayer(Layer* layer) {
  DCHECK(!LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  layer_id_map_[layer->id()] = layer;
  if (layer->element_id()) {
    mutator_host_->RegisterElement(layer->element_id(),
                                   ElementListType::ACTIVE);
  }
}

void LayerTree::UnregisterLayer(Layer* layer) {
  DCHECK(LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  if (layer->element_id()) {
    mutator_host_->UnregisterElement(layer->element_id(),
                                     ElementListType::ACTIVE);
  }
  RemoveLayerShouldPushProperties(layer);
  layer_id_map_.erase(layer->id());
}

Layer* LayerTree::LayerById(int id) const {
  LayerIdMap::const_iterator iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : nullptr;
}

size_t LayerTree::NumLayers() const {
  return layer_id_map_.size();
}

bool LayerTree::UpdateLayers(const LayerList& update_layer_list,
                             bool* content_is_suitable_for_gpu) {
  base::AutoReset<bool> painting(&in_paint_layer_contents_, true);
  bool did_paint_content = false;
  for (const auto& layer : update_layer_list) {
    did_paint_content |= layer->Update();
    *content_is_suitable_for_gpu &= layer->IsSuitableForGpuRasterization();
  }
  return did_paint_content;
}

void LayerTree::AddLayerShouldPushProperties(Layer* layer) {
  layers_that_should_push_properties_.insert(layer);
}

void LayerTree::RemoveLayerShouldPushProperties(Layer* layer) {
  layers_that_should_push_properties_.erase(layer);
}

std::unordered_set<Layer*>& LayerTree::LayersThatShouldPushProperties() {
  return layers_that_should_push_properties_;
}

bool LayerTree::LayerNeedsPushPropertiesForTesting(Layer* layer) const {
  return layers_that_should_push_properties_.find(layer) !=
         layers_that_should_push_properties_.end();
}

void LayerTree::SetNeedsMetaInfoRecomputation(bool needs_recomputation) {
  needs_meta_info_recomputation_ = needs_recomputation;
}

void LayerTree::SetPageScaleFromImplSide(float page_scale) {
  DCHECK(layer_tree_host_->CommitRequested());
  page_scale_factor_ = page_scale;
  SetPropertyTreesNeedRebuild();
}

void LayerTree::SetElasticOverscrollFromImplSide(
    gfx::Vector2dF elastic_overscroll) {
  DCHECK(layer_tree_host_->CommitRequested());
  elastic_overscroll_ = elastic_overscroll;
}

void LayerTree::UpdateHudLayer(bool show_hud_info) {
  if (show_hud_info) {
    if (!hud_layer_.get()) {
      hud_layer_ = HeadsUpDisplayLayer::Create();
    }

    if (root_layer_.get() && !hud_layer_->parent())
      root_layer_->AddChild(hud_layer_);
  } else if (hud_layer_.get()) {
    hud_layer_->RemoveFromParent();
    hud_layer_ = nullptr;
  }
}

void LayerTree::SetNeedsFullTreeSync() {
  needs_full_tree_sync_ = true;
  needs_meta_info_recomputation_ = true;

  property_trees_.needs_rebuild = true;
  SetNeedsCommit();
}

void LayerTree::SetNeedsCommit() {
  layer_tree_host_->SetNeedsCommit();
}

const LayerTreeSettings& LayerTree::GetSettings() const {
  return layer_tree_host_->GetSettings();
}

void LayerTree::SetPropertyTreesNeedRebuild() {
  property_trees_.needs_rebuild = true;
  layer_tree_host_->SetNeedsUpdateLayers();
}

void LayerTree::PushPropertiesTo(LayerTreeImpl* tree_impl) {
  tree_impl->set_needs_full_tree_sync(needs_full_tree_sync_);
  needs_full_tree_sync_ = false;

  if (hud_layer_.get()) {
    LayerImpl* hud_impl = tree_impl->LayerById(hud_layer_->id());
    tree_impl->set_hud_layer(static_cast<HeadsUpDisplayLayerImpl*>(hud_impl));
  } else {
    tree_impl->set_hud_layer(nullptr);
  }

  tree_impl->set_background_color(background_color_);
  tree_impl->set_has_transparent_background(has_transparent_background_);
  tree_impl->set_have_scroll_event_handlers(have_scroll_event_handlers_);
  tree_impl->set_event_listener_properties(
      EventListenerClass::kTouchStartOrMove,
      event_listener_properties(EventListenerClass::kTouchStartOrMove));
  tree_impl->set_event_listener_properties(
      EventListenerClass::kMouseWheel,
      event_listener_properties(EventListenerClass::kMouseWheel));
  tree_impl->set_event_listener_properties(
      EventListenerClass::kTouchEndOrCancel,
      event_listener_properties(EventListenerClass::kTouchEndOrCancel));

  if (page_scale_layer_ && inner_viewport_scroll_layer_) {
    tree_impl->SetViewportLayersFromIds(
        overscroll_elasticity_layer_ ? overscroll_elasticity_layer_->id()
                                     : Layer::INVALID_ID,
        page_scale_layer_->id(), inner_viewport_scroll_layer_->id(),
        outer_viewport_scroll_layer_ ? outer_viewport_scroll_layer_->id()
                                     : Layer::INVALID_ID);
    DCHECK(inner_viewport_scroll_layer_->IsContainerForFixedPositionLayers());
  } else {
    tree_impl->ClearViewportLayers();
  }

  tree_impl->RegisterSelection(selection_);

  bool property_trees_changed_on_active_tree =
      tree_impl->IsActiveTree() && tree_impl->property_trees()->changed;
  // Property trees may store damage status. We preserve the sync tree damage
  // status by pushing the damage status from sync tree property trees to main
  // thread property trees or by moving it onto the layers.
  if (root_layer_ && property_trees_changed_on_active_tree) {
    if (property_trees_.sequence_number ==
        tree_impl->property_trees()->sequence_number)
      tree_impl->property_trees()->PushChangeTrackingTo(&property_trees_);
    else
      tree_impl->MoveChangeTrackingToLayers();
  }
  // Setting property trees must happen before pushing the page scale.
  tree_impl->SetPropertyTrees(&property_trees_);

  tree_impl->PushPageScaleFromMainThread(
      page_scale_factor_, min_page_scale_factor_, max_page_scale_factor_);

  tree_impl->set_browser_controls_shrink_blink_size(
      browser_controls_shrink_blink_size_);
  tree_impl->set_top_controls_height(top_controls_height_);
  tree_impl->set_bottom_controls_height(bottom_controls_height_);
  tree_impl->PushBrowserControlsFromMainThread(top_controls_shown_ratio_);
  tree_impl->elastic_overscroll()->PushFromMainThread(elastic_overscroll_);
  if (tree_impl->IsActiveTree())
    tree_impl->elastic_overscroll()->PushPendingToActive();

  tree_impl->set_painted_device_scale_factor(painted_device_scale_factor_);

  tree_impl->SetDeviceColorSpace(device_color_space_);

  if (pending_page_scale_animation_) {
    tree_impl->SetPendingPageScaleAnimation(
        std::move(pending_page_scale_animation_));
  }

  DCHECK(!tree_impl->ViewportSizeInvalid());

  tree_impl->set_has_ever_been_drawn(false);
}

Layer* LayerTree::LayerByElementId(ElementId element_id) const {
  ElementLayersMap::const_iterator iter = element_layers_map_.find(element_id);
  return iter != element_layers_map_.end() ? iter->second : nullptr;
}

void LayerTree::RegisterElement(ElementId element_id,
                                ElementListType list_type,
                                Layer* layer) {
  if (layer->element_id()) {
    element_layers_map_[layer->element_id()] = layer;
  }

  mutator_host_->RegisterElement(element_id, list_type);
}

void LayerTree::UnregisterElement(ElementId element_id,
                                  ElementListType list_type,
                                  Layer* layer) {
  mutator_host_->UnregisterElement(element_id, list_type);

  if (layer->element_id()) {
    element_layers_map_.erase(layer->element_id());
  }
}

static void SetElementIdForTesting(Layer* layer) {
  layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));
}

void LayerTree::SetElementIdsForTesting() {
  LayerTreeHostCommon::CallFunctionForEveryLayer(this, SetElementIdForTesting);
}

void LayerTree::BuildPropertyTreesForTesting() {
  PropertyTreeBuilder::PreCalculateMetaInformation(root_layer());
  gfx::Transform identity_transform;
  PropertyTreeBuilder::BuildPropertyTrees(
      root_layer(), page_scale_layer(), inner_viewport_scroll_layer(),
      outer_viewport_scroll_layer(), overscroll_elasticity_layer(),
      elastic_overscroll(), page_scale_factor(), device_scale_factor(),
      gfx::Rect(device_viewport_size()), identity_transform, property_trees());
}

bool LayerTree::IsElementInList(ElementId element_id,
                                ElementListType list_type) const {
  return list_type == ElementListType::ACTIVE && LayerByElementId(element_id);
}

void LayerTree::SetMutatorsNeedCommit() {
  layer_tree_host_->SetNeedsCommit();
}

void LayerTree::SetMutatorsNeedRebuildPropertyTrees() {
  property_trees_.needs_rebuild = true;
}

void LayerTree::SetElementFilterMutated(ElementId element_id,
                                        ElementListType list_type,
                                        const FilterOperations& filters) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnFilterAnimated(filters);
}

void LayerTree::SetElementOpacityMutated(ElementId element_id,
                                         ElementListType list_type,
                                         float opacity) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnOpacityAnimated(opacity);
}

void LayerTree::SetElementTransformMutated(ElementId element_id,
                                           ElementListType list_type,
                                           const gfx::Transform& transform) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnTransformAnimated(transform);
}

void LayerTree::SetElementScrollOffsetMutated(
    ElementId element_id,
    ElementListType list_type,
    const gfx::ScrollOffset& scroll_offset) {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  layer->OnScrollOffsetAnimated(scroll_offset);
}

void LayerTree::ElementIsAnimatingChanged(ElementId element_id,
                                          ElementListType list_type,
                                          const PropertyAnimationState& mask,
                                          const PropertyAnimationState& state) {
  Layer* layer = LayerByElementId(element_id);
  if (layer)
    layer->OnIsAnimatingChanged(mask, state);
}

gfx::ScrollOffset LayerTree::GetScrollOffsetForAnimation(
    ElementId element_id) const {
  Layer* layer = LayerByElementId(element_id);
  DCHECK(layer);
  return layer->ScrollOffsetForAnimation();
}

LayerListIterator<Layer> LayerTree::begin() const {
  return LayerListIterator<Layer>(root_layer_.get());
}

LayerListIterator<Layer> LayerTree::end() const {
  return LayerListIterator<Layer>(nullptr);
}

LayerListReverseIterator<Layer> LayerTree::rbegin() {
  return LayerListReverseIterator<Layer>(root_layer_.get());
}

LayerListReverseIterator<Layer> LayerTree::rend() {
  return LayerListReverseIterator<Layer>(nullptr);
}

void LayerTree::SetNeedsDisplayOnAllLayers() {
  for (auto* layer : *this)
    layer->SetNeedsDisplay();
}

}  // namespace cc
