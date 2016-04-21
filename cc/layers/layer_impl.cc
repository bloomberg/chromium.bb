// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/animation/mutable_properties.h"
#include "cc/base/math_util.h"
#include "cc/base/simple_enclosed_region.h"
#include "cc/debug/debug_colors.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/debug/micro_benchmark_impl.h"
#include "cc/debug/traced_value.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/scroll_state.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_utils.h"
#include "cc/output/copy_output_request.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/proxy.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {
LayerImpl::LayerImpl(LayerTreeImpl* tree_impl, int id)
    : parent_(nullptr),
      scroll_parent_(nullptr),
      clip_parent_(nullptr),
      mask_layer_id_(-1),
      mask_layer_(nullptr),
      replica_layer_id_(-1),
      replica_layer_(nullptr),
      layer_id_(id),
      layer_tree_impl_(tree_impl),
      scroll_clip_layer_id_(Layer::INVALID_ID),
      main_thread_scrolling_reasons_(
          MainThreadScrollingReason::kNotScrollingOnMain),
      user_scrollable_horizontal_(true),
      user_scrollable_vertical_(true),
      double_sided_(true),
      should_flatten_transform_(true),
      should_flatten_transform_from_property_tree_(false),
      layer_property_changed_(false),
      masks_to_bounds_(false),
      contents_opaque_(false),
      is_root_for_isolated_group_(false),
      use_parent_backface_visibility_(false),
      use_local_transform_for_backface_visibility_(false),
      should_check_backface_visibility_(false),
      draws_content_(false),
      hide_layer_and_subtree_(false),
      transform_is_invertible_(true),
      is_container_for_fixed_position_layers_(false),
      is_affected_by_page_scale_(true),
      was_ever_ready_since_last_transform_animation_(true),
      background_color_(0),
      safe_opaque_background_color_(0),
      opacity_(1.0),
      blend_mode_(SkXfermode::kSrcOver_Mode),
      draw_blend_mode_(SkXfermode::kSrcOver_Mode),
      num_descendants_that_draw_content_(0),
      transform_tree_index_(-1),
      effect_tree_index_(-1),
      clip_tree_index_(-1),
      scroll_tree_index_(-1),
      sorting_context_id_(0),
      current_draw_mode_(DRAW_MODE_NONE),
      element_id_(0),
      mutable_properties_(MutableProperty::kNone),
      debug_info_(nullptr),
      force_render_surface_(false),
      scrolls_drawn_descendant_(false),
      layer_or_descendant_has_touch_handler_(false) {
  DCHECK_GT(layer_id_, 0);

  DCHECK(layer_tree_impl_);
  layer_tree_impl_->RegisterLayer(this);
  layer_tree_impl_->AddToElementMap(this);

  SetNeedsPushProperties();
}

LayerImpl::~LayerImpl() {
  DCHECK_EQ(DRAW_MODE_NONE, current_draw_mode_);

  if (!copy_requests_.empty() && layer_tree_impl_->IsActiveTree())
    layer_tree_impl()->RemoveLayerWithCopyOutputRequest(this);
  layer_tree_impl_->UnregisterScrollLayer(this);
  layer_tree_impl_->UnregisterLayer(this);
  layer_tree_impl_->RemoveLayerShouldPushProperties(this);

  layer_tree_impl_->RemoveFromElementMap(this);

  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), "cc::LayerImpl", this);

  if (mask_layer_)
    layer_tree_impl_->RemoveLayer(mask_layer_id_);
  if (replica_layer_)
    layer_tree_impl_->RemoveLayer(replica_layer_id_);
  ClearChildList();
}

void LayerImpl::AddChild(std::unique_ptr<LayerImpl> child) {
  child->SetParent(this);
  DCHECK_EQ(layer_tree_impl(), child->layer_tree_impl());
  children_.push_back(child.get());
  layer_tree_impl_->AddLayer(std::move(child));
}

std::unique_ptr<LayerImpl> LayerImpl::RemoveChildForTesting(LayerImpl* child) {
  auto it = std::find(children_.begin(), children_.end(), child);
  if (it != children_.end())
    children_.erase(it);
  return layer_tree_impl_->RemoveLayer(child->id());
}

void LayerImpl::SetParent(LayerImpl* parent) {
  parent_ = parent;
}

void LayerImpl::ClearChildList() {
  if (children_.empty())
    return;
  for (auto* child : children_)
    layer_tree_impl_->RemoveLayer(child->id());
  children_.clear();
}

void LayerImpl::ClearLinksToOtherLayers() {
  children_.clear();
  mask_layer_ = nullptr;
  replica_layer_ = nullptr;
}

void LayerImpl::SetScrollParent(LayerImpl* parent) {
  if (scroll_parent_ == parent)
    return;

  if (parent)
    DCHECK_EQ(layer_tree_impl()->LayerById(parent->id()), parent);

  scroll_parent_ = parent;
  SetNeedsPushProperties();
}

void LayerImpl::SetDebugInfo(
    std::unique_ptr<base::trace_event::ConvertableToTraceFormat> debug_info) {
  owned_debug_info_ = std::move(debug_info);
  debug_info_ = owned_debug_info_.get();
  SetNeedsPushProperties();
}

void LayerImpl::SetScrollChildren(std::set<LayerImpl*>* children) {
  if (scroll_children_.get() == children)
    return;
  scroll_children_.reset(children);
  SetNeedsPushProperties();
}

void LayerImpl::DistributeScroll(ScrollState* scroll_state) {
  DCHECK(scroll_state);
  if (scroll_state->FullyConsumed())
    return;

  scroll_state->DistributeToScrollChainDescendant();

  // If the scroll doesn't propagate, and we're currently scrolling
  // a layer other than this one, prevent the scroll from
  // propagating to this layer.
  if (!scroll_state->should_propagate() &&
      scroll_state->delta_consumed_for_scroll_sequence() &&
      scroll_state->current_native_scrolling_node()->owner_id != id()) {
    return;
  }

  ApplyScroll(scroll_state);
}

void LayerImpl::ApplyScroll(ScrollState* scroll_state) {
  DCHECK(scroll_state);
  ScrollNode* node = layer_tree_impl()->property_trees()->scroll_tree.Node(
      scroll_tree_index());
  layer_tree_impl()->ApplyScroll(node, scroll_state);
}

void LayerImpl::SetNumDescendantsThatDrawContent(int num_descendants) {
  if (num_descendants_that_draw_content_ == num_descendants)
    return;
  num_descendants_that_draw_content_ = num_descendants;
  SetNeedsPushProperties();
}

void LayerImpl::SetClipParent(LayerImpl* ancestor) {
  if (clip_parent_ == ancestor)
    return;

  clip_parent_ = ancestor;
  SetNeedsPushProperties();
}

void LayerImpl::SetClipChildren(std::set<LayerImpl*>* children) {
  if (clip_children_.get() == children)
    return;
  clip_children_.reset(children);
  SetNeedsPushProperties();
}

void LayerImpl::SetTransformTreeIndex(int index) {
  transform_tree_index_ = index;
  SetNeedsPushProperties();
}

void LayerImpl::SetClipTreeIndex(int index) {
  clip_tree_index_ = index;
  SetNeedsPushProperties();
}

void LayerImpl::SetEffectTreeIndex(int index) {
  effect_tree_index_ = index;
  SetNeedsPushProperties();
}

void LayerImpl::SetScrollTreeIndex(int index) {
  scroll_tree_index_ = index;
  SetNeedsPushProperties();
}

void LayerImpl::PassCopyRequests(
    std::vector<std::unique_ptr<CopyOutputRequest>>* requests) {
  // In the case that a layer still has a copy request, this means that there's
  // a commit to the active tree without a draw.  This only happens in some
  // edge cases during lost context or visibility changes, so don't try to
  // handle preserving these output requests (and their surface).
  if (!copy_requests_.empty()) {
    layer_tree_impl()->RemoveLayerWithCopyOutputRequest(this);
    // Destroying these will abort them.
    copy_requests_.clear();
  }

  if (requests->empty())
    return;

  bool was_empty = copy_requests_.empty();
  for (auto& request : *requests)
    copy_requests_.push_back(std::move(request));
  requests->clear();

  if (was_empty && layer_tree_impl()->IsActiveTree())
    layer_tree_impl()->AddLayerWithCopyOutputRequest(this);
}

void LayerImpl::TakeCopyRequestsAndTransformToTarget(
    std::vector<std::unique_ptr<CopyOutputRequest>>* requests) {
  DCHECK(!copy_requests_.empty());
  DCHECK(layer_tree_impl()->IsActiveTree());
  DCHECK(has_render_surface());
  DCHECK_EQ(render_target(), render_surface());

  size_t first_inserted_request = requests->size();
  for (auto& request : copy_requests_)
    requests->push_back(std::move(request));
  copy_requests_.clear();

  for (size_t i = first_inserted_request; i < requests->size(); ++i) {
    CopyOutputRequest* request = (*requests)[i].get();
    if (!request->has_area())
      continue;

    gfx::Rect request_in_layer_space = request->area();
    request_in_layer_space.Intersect(gfx::Rect(bounds()));
    request->set_area(MathUtil::MapEnclosingClippedRect(
        DrawTransform(), request_in_layer_space));
  }

  layer_tree_impl()->RemoveLayerWithCopyOutputRequest(this);
  layer_tree_impl()->set_needs_update_draw_properties();
}

bool LayerImpl::InsideCopyRequest() const {
  EffectTree& effect_tree = layer_tree_impl()->property_trees()->effect_tree;
  EffectNode* node = effect_tree.Node(effect_tree_index_);

  for (; node; node = effect_tree.parent(node)) {
    if (node->data.has_copy_request)
      return true;
  }
  return false;
}

void LayerImpl::ClearRenderSurfaceLayerList() {
  if (render_surface_)
    render_surface_->ClearLayerLists();
}

void LayerImpl::PopulateSharedQuadState(SharedQuadState* state) const {
  state->SetAll(draw_properties_.target_space_transform, bounds(),
                draw_properties_.visible_layer_rect, draw_properties_.clip_rect,
                draw_properties_.is_clipped, draw_properties_.opacity,
                draw_blend_mode_, sorting_context_id_);
}

void LayerImpl::PopulateScaledSharedQuadState(SharedQuadState* state,
                                              float scale) const {
  gfx::Transform scaled_draw_transform =
      draw_properties_.target_space_transform;
  scaled_draw_transform.Scale(SK_MScalar1 / scale, SK_MScalar1 / scale);
  gfx::Size scaled_bounds = gfx::ScaleToCeiledSize(bounds(), scale);
  gfx::Rect scaled_visible_layer_rect =
      gfx::ScaleToEnclosingRect(visible_layer_rect(), scale);
  scaled_visible_layer_rect.Intersect(gfx::Rect(scaled_bounds));

  state->SetAll(scaled_draw_transform, scaled_bounds, scaled_visible_layer_rect,
                draw_properties().clip_rect, draw_properties().is_clipped,
                draw_properties().opacity, draw_blend_mode_,
                sorting_context_id_);
}

bool LayerImpl::WillDraw(DrawMode draw_mode,
                         ResourceProvider* resource_provider) {
  // WillDraw/DidDraw must be matched.
  DCHECK_NE(DRAW_MODE_NONE, draw_mode);
  DCHECK_EQ(DRAW_MODE_NONE, current_draw_mode_);
  current_draw_mode_ = draw_mode;
  return true;
}

void LayerImpl::DidDraw(ResourceProvider* resource_provider) {
  DCHECK_NE(DRAW_MODE_NONE, current_draw_mode_);
  current_draw_mode_ = DRAW_MODE_NONE;
}

bool LayerImpl::ShowDebugBorders() const {
  return layer_tree_impl()->debug_state().show_debug_borders;
}

void LayerImpl::GetDebugBorderProperties(SkColor* color, float* width) const {
  if (draws_content_) {
    *color = DebugColors::ContentLayerBorderColor();
    *width = DebugColors::ContentLayerBorderWidth(layer_tree_impl());
    return;
  }

  if (masks_to_bounds_) {
    *color = DebugColors::MaskingLayerBorderColor();
    *width = DebugColors::MaskingLayerBorderWidth(layer_tree_impl());
    return;
  }

  *color = DebugColors::ContainerLayerBorderColor();
  *width = DebugColors::ContainerLayerBorderWidth(layer_tree_impl());
}

void LayerImpl::AppendDebugBorderQuad(
    RenderPass* render_pass,
    const gfx::Size& bounds,
    const SharedQuadState* shared_quad_state,
    AppendQuadsData* append_quads_data) const {
  SkColor color;
  float width;
  GetDebugBorderProperties(&color, &width);
  AppendDebugBorderQuad(render_pass, bounds, shared_quad_state,
                        append_quads_data, color, width);
}

void LayerImpl::AppendDebugBorderQuad(RenderPass* render_pass,
                                      const gfx::Size& bounds,
                                      const SharedQuadState* shared_quad_state,
                                      AppendQuadsData* append_quads_data,
                                      SkColor color,
                                      float width) const {
  if (!ShowDebugBorders())
    return;

  gfx::Rect quad_rect(bounds);
  gfx::Rect visible_quad_rect(quad_rect);
  DebugBorderDrawQuad* debug_border_quad =
      render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  debug_border_quad->SetNew(
      shared_quad_state, quad_rect, visible_quad_rect, color, width);
  if (contents_opaque()) {
    // When opaque, draw a second inner border that is thicker than the outer
    // border, but more transparent.
    static const float kFillOpacity = 0.3f;
    SkColor fill_color = SkColorSetA(
        color, static_cast<uint8_t>(SkColorGetA(color) * kFillOpacity));
    float fill_width = width * 3;
    gfx::Rect fill_rect = quad_rect;
    fill_rect.Inset(fill_width / 2.f, fill_width / 2.f);
    if (fill_rect.IsEmpty())
      return;
    gfx::Rect visible_fill_rect =
        gfx::IntersectRects(visible_quad_rect, fill_rect);
    DebugBorderDrawQuad* fill_quad =
        render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
    fill_quad->SetNew(shared_quad_state, fill_rect, visible_fill_rect,
                      fill_color, fill_width);
  }
}

void LayerImpl::GetContentsResourceId(ResourceId* resource_id,
                                      gfx::Size* resource_size) const {
  NOTREACHED();
  *resource_id = 0;
}

gfx::Vector2dF LayerImpl::ScrollBy(const gfx::Vector2dF& scroll) {
  ScrollTree& scroll_tree = layer_tree_impl()->property_trees()->scroll_tree;
  ScrollNode* scroll_node = scroll_tree.Node(scroll_tree_index());
  return scroll_tree.ScrollBy(scroll_node, scroll, layer_tree_impl());
}

void LayerImpl::SetScrollClipLayer(int scroll_clip_layer_id) {
  if (scroll_clip_layer_id_ == scroll_clip_layer_id)
    return;

  layer_tree_impl()->UnregisterScrollLayer(this);
  scroll_clip_layer_id_ = scroll_clip_layer_id;
  layer_tree_impl()->RegisterScrollLayer(this);
}

LayerImpl* LayerImpl::scroll_clip_layer() const {
  return layer_tree_impl()->LayerById(scroll_clip_layer_id_);
}

bool LayerImpl::scrollable() const {
  return scroll_clip_layer_id_ != Layer::INVALID_ID;
}

void LayerImpl::set_user_scrollable_horizontal(bool scrollable) {
  user_scrollable_horizontal_ = scrollable;
}

void LayerImpl::set_user_scrollable_vertical(bool scrollable) {
  user_scrollable_vertical_ = scrollable;
}

bool LayerImpl::user_scrollable(ScrollbarOrientation orientation) const {
  return (orientation == HORIZONTAL) ? user_scrollable_horizontal_
                                     : user_scrollable_vertical_;
}

std::unique_ptr<LayerImpl> LayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return LayerImpl::Create(tree_impl, layer_id_);
}

void LayerImpl::set_main_thread_scrolling_reasons(
    uint32_t main_thread_scrolling_reasons) {
  if (main_thread_scrolling_reasons_ == main_thread_scrolling_reasons)
    return;

  if (main_thread_scrolling_reasons &
          MainThreadScrollingReason::kHasNonLayerViewportConstrainedObjects &&
      layer_tree_impl()) {
    if (layer_tree_impl()->ScrollOffsetIsAnimatingOnImplOnly(this)) {
      const bool needs_completion = true;
      layer_tree_impl()->ScrollAnimationAbort(needs_completion);
    }
  }

  main_thread_scrolling_reasons_ = main_thread_scrolling_reasons;
}

void LayerImpl::PushPropertiesTo(LayerImpl* layer) {
  layer->SetTransformOrigin(transform_origin_);
  layer->SetBackgroundColor(background_color_);
  layer->SetSafeOpaqueBackgroundColor(safe_opaque_background_color_);
  layer->SetBounds(bounds_);
  layer->SetDoubleSided(double_sided_);
  layer->SetDrawsContent(DrawsContent());
  layer->SetHideLayerAndSubtree(hide_layer_and_subtree_);
  // If whether layer has render surface changes, we need to update draw
  // properties.
  // TODO(weiliangc): Should be safely removed after impl side is able to
  // update render surfaces without rebuilding property trees.
  if (layer->has_render_surface() != has_render_surface())
    layer->layer_tree_impl()->set_needs_update_draw_properties();
  layer->SetHasRenderSurface(!!render_surface());
  layer->SetForceRenderSurface(force_render_surface_);
  layer->SetFilters(filters());
  layer->SetBackgroundFilters(background_filters());
  layer->SetMasksToBounds(masks_to_bounds_);
  layer->set_main_thread_scrolling_reasons(main_thread_scrolling_reasons_);
  layer->SetNonFastScrollableRegion(non_fast_scrollable_region_);
  layer->SetTouchEventHandlerRegion(touch_event_handler_region_);
  layer->SetContentsOpaque(contents_opaque_);
  layer->SetOpacity(opacity_);
  layer->SetBlendMode(blend_mode_);
  layer->SetIsRootForIsolatedGroup(is_root_for_isolated_group_);
  layer->SetPosition(position_);
  layer->SetIsContainerForFixedPositionLayers(
      is_container_for_fixed_position_layers_);
  layer->SetPositionConstraint(position_constraint_);
  layer->SetShouldFlattenTransform(should_flatten_transform_);
  layer->set_should_flatten_transform_from_property_tree(
      should_flatten_transform_from_property_tree_);
  layer->set_draw_blend_mode(draw_blend_mode_);
  layer->SetUseParentBackfaceVisibility(use_parent_backface_visibility_);
  layer->SetUseLocalTransformForBackfaceVisibility(
      use_local_transform_for_backface_visibility_);
  layer->SetShouldCheckBackfaceVisibility(should_check_backface_visibility_);
  layer->SetTransformAndInvertibility(transform_, transform_is_invertible_);
  if (layer_property_changed_)
    layer->NoteLayerPropertyChanged();

  layer->SetScrollClipLayer(scroll_clip_layer_id_);
  layer->SetElementId(element_id_);
  layer->SetMutableProperties(mutable_properties_);
  layer->set_user_scrollable_horizontal(user_scrollable_horizontal_);
  layer->set_user_scrollable_vertical(user_scrollable_vertical_);

  layer->Set3dSortingContextId(sorting_context_id_);
  layer->SetNumDescendantsThatDrawContent(num_descendants_that_draw_content_);

  layer->SetTransformTreeIndex(transform_tree_index_);
  layer->SetClipTreeIndex(clip_tree_index_);
  layer->SetEffectTreeIndex(effect_tree_index_);
  layer->SetScrollTreeIndex(scroll_tree_index_);
  layer->set_offset_to_transform_parent(offset_to_transform_parent_);

  LayerImpl* scroll_parent = nullptr;
  if (scroll_parent_) {
    scroll_parent = layer->layer_tree_impl()->LayerById(scroll_parent_->id());
    DCHECK(scroll_parent);
  }

  layer->SetScrollParent(scroll_parent);
  if (scroll_children_) {
    std::set<LayerImpl*>* scroll_children = new std::set<LayerImpl*>;
    for (std::set<LayerImpl*>::iterator it = scroll_children_->begin();
         it != scroll_children_->end();
         ++it) {
      DCHECK_EQ((*it)->scroll_parent(), this);
      LayerImpl* scroll_child =
          layer->layer_tree_impl()->LayerById((*it)->id());
      DCHECK(scroll_child);
      scroll_children->insert(scroll_child);
    }
    layer->SetScrollChildren(scroll_children);
  } else {
    layer->SetScrollChildren(nullptr);
  }

  LayerImpl* clip_parent = nullptr;
  if (clip_parent_) {
    clip_parent = layer->layer_tree_impl()->LayerById(
        clip_parent_->id());
    DCHECK(clip_parent);
  }

  layer->SetClipParent(clip_parent);
  if (clip_children_) {
    std::set<LayerImpl*>* clip_children = new std::set<LayerImpl*>;
    for (std::set<LayerImpl*>::iterator it = clip_children_->begin();
        it != clip_children_->end(); ++it)
      clip_children->insert(layer->layer_tree_impl()->LayerById((*it)->id()));
    layer->SetClipChildren(clip_children);
  } else {
    layer->SetClipChildren(nullptr);
  }

  layer->PassCopyRequests(&copy_requests_);

  // If the main thread commits multiple times before the impl thread actually
  // draws, then damage tracking will become incorrect if we simply clobber the
  // update_rect here. The LayerImpl's update_rect needs to accumulate (i.e.
  // union) any update changes that have occurred on the main thread.
  update_rect_.Union(layer->update_rect());
  layer->SetUpdateRect(update_rect_);

  if (owned_debug_info_)
    layer->SetDebugInfo(std::move(owned_debug_info_));

  // Reset any state that should be cleared for the next update.
  layer_property_changed_ = false;
  update_rect_ = gfx::Rect();
  layer_tree_impl()->RemoveLayerShouldPushProperties(this);
}

bool LayerImpl::IsAffectedByPageScale() const {
  TransformTree& transform_tree =
      layer_tree_impl()->property_trees()->transform_tree;
  return transform_tree.Node(transform_tree_index())
      ->data.in_subtree_of_page_scale_layer;
}

gfx::Vector2dF LayerImpl::FixedContainerSizeDelta() const {
  LayerImpl* scroll_clip_layer =
      layer_tree_impl()->LayerById(scroll_clip_layer_id_);
  if (!scroll_clip_layer)
    return gfx::Vector2dF();

  return scroll_clip_layer->bounds_delta();
}

base::DictionaryValue* LayerImpl::LayerTreeAsJson() const {
  base::DictionaryValue* result = new base::DictionaryValue;
  result->SetInteger("LayerId", id());
  result->SetString("LayerType", LayerTypeAsString());

  base::ListValue* list = new base::ListValue;
  list->AppendInteger(bounds().width());
  list->AppendInteger(bounds().height());
  result->Set("Bounds", list);

  list = new base::ListValue;
  list->AppendDouble(position_.x());
  list->AppendDouble(position_.y());
  result->Set("Position", list);

  const gfx::Transform& gfx_transform = DrawTransform();
  double transform[16];
  gfx_transform.matrix().asColMajord(transform);
  list = new base::ListValue;
  for (int i = 0; i < 16; ++i)
    list->AppendDouble(transform[i]);
  result->Set("DrawTransform", list);

  result->SetBoolean("DrawsContent", draws_content_);
  result->SetBoolean("Is3dSorted", Is3dSorted());
  result->SetDouble("OPACITY", opacity());
  result->SetBoolean("ContentsOpaque", contents_opaque_);

  if (scrollable())
    result->SetBoolean("Scrollable", true);

  if (!touch_event_handler_region_.IsEmpty()) {
    std::unique_ptr<base::Value> region = touch_event_handler_region_.AsValue();
    result->Set("TouchRegion", region.release());
  }

  list = new base::ListValue;
  for (size_t i = 0; i < children_.size(); ++i)
    list->Append(children_[i]->LayerTreeAsJson());
  result->Set("Children", list);

  return result;
}

bool LayerImpl::LayerPropertyChanged() const {
  if (layer_property_changed_ ||
      (layer_tree_impl()->property_trees() &&
       layer_tree_impl()->property_trees()->full_tree_damaged))
    return true;
  if (transform_tree_index() == -1)
    return false;
  TransformNode* transform_node =
      layer_tree_impl()->property_trees()->transform_tree.Node(
          transform_tree_index());
  if (transform_node && transform_node->data.transform_changed)
    return true;
  if (effect_tree_index() == -1)
    return false;
  EffectNode* effect_node =
      layer_tree_impl()->property_trees()->effect_tree.Node(
          effect_tree_index());
  if (effect_node && effect_node->data.effect_changed)
    return true;
  return false;
}

void LayerImpl::NoteLayerPropertyChanged() {
  layer_property_changed_ = true;
  layer_tree_impl()->set_needs_update_draw_properties();
  SetNeedsPushProperties();
}

void LayerImpl::PushLayerPropertyChangedForSubtreeInternal() {
  if (LayerPropertyChanged())
    NoteLayerPropertyChanged();
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->PushLayerPropertyChangedForSubtreeInternal();
}

void LayerImpl::PushLayerPropertyChangedForSubtree() {
  // We need to update property trees first as layer property can change
  // when its corresponsing property tree node changes.
  PropertyTrees* property_trees = layer_tree_impl()->property_trees();
  EffectTree& effect_tree = property_trees->effect_tree;
  TransformTree& transform_tree = property_trees->transform_tree;
  for (int i = 1; i < static_cast<int>(effect_tree.size()); ++i) {
    EffectNode* node = effect_tree.Node(i);
    EffectNode* parent_node = effect_tree.parent(node);
    effect_tree.UpdateEffectChanged(node, parent_node);
  }
  for (int i = 1; i < static_cast<int>(transform_tree.size()); ++i) {
    TransformNode* node = transform_tree.Node(i);
    TransformNode* parent_node = transform_tree.parent(node);
    TransformNode* source_node = transform_tree.Node(node->data.source_node_id);
    transform_tree.UpdateTransformChanged(node, parent_node, source_node);
  }
  PushLayerPropertyChangedForSubtreeInternal();
}

void LayerImpl::ValidateQuadResourcesInternal(DrawQuad* quad) const {
#if DCHECK_IS_ON()
  const ResourceProvider* resource_provider =
      layer_tree_impl_->resource_provider();
  for (ResourceId resource_id : quad->resources)
    resource_provider->ValidateResource(resource_id);
#endif
}

const char* LayerImpl::LayerTypeAsString() const {
  return "cc::LayerImpl";
}

void LayerImpl::ResetChangeTracking() {
  layer_property_changed_ = false;

  update_rect_.SetRect(0, 0, 0, 0);
  damage_rect_.SetRect(0, 0, 0, 0);

  if (render_surface_)
    render_surface_->ResetPropertyChangedFlag();

  if (mask_layer_)
    mask_layer_->ResetChangeTracking();

  if (replica_layer_) {
    // This also resets the replica mask, if it exists.
    replica_layer_->ResetChangeTracking();
  }
}

int LayerImpl::num_copy_requests_in_target_subtree() {
  return layer_tree_impl()
      ->property_trees()
      ->effect_tree.Node(effect_tree_index())
      ->data.num_copy_requests_in_subtree;
}

void LayerImpl::UpdatePropertyTreeTransform() {
  if (transform_tree_index_ != -1) {
    TransformTree& transform_tree =
        layer_tree_impl()->property_trees()->transform_tree;
    TransformNode* node = transform_tree.Node(transform_tree_index_);
    // A LayerImpl's own current state is insufficient for determining whether
    // it owns a TransformNode, since this depends on the state of the
    // corresponding Layer at the time of the last commit. For example, a
    // transform animation might have been in progress at the time the last
    // commit started, but might have finished since then on the compositor
    // thread.
    if (node->owner_id != id())
      return;
    if (node->data.local != transform_) {
      node->data.local = transform_;
      node->data.needs_local_transform_update = true;
      node->data.transform_changed = true;
      layer_tree_impl()->property_trees()->changed = true;
      transform_tree.set_needs_update(true);
      // TODO(ajuma): The current criteria for creating clip nodes means that
      // property trees may need to be rebuilt when the new transform isn't
      // axis-aligned wrt the old transform (see Layer::SetTransform). Since
      // rebuilding property trees every frame of a transform animation is
      // something we should try to avoid, change property tree-building so that
      // it doesn't depend on axis aliginment.
    }
  }
}

void LayerImpl::UpdatePropertyTreeTransformIsAnimated(bool is_animated) {
  if (transform_tree_index_ != -1) {
    TransformTree& transform_tree =
        layer_tree_impl()->property_trees()->transform_tree;
    TransformNode* node = transform_tree.Node(transform_tree_index_);
    // A LayerImpl's own current state is insufficient for determining whether
    // it owns a TransformNode, since this depends on the state of the
    // corresponding Layer at the time of the last commit. For example, if
    // |is_animated| is false, this might mean a transform animation just ticked
    // past its finish point (so the LayerImpl still owns a TransformNode) or it
    // might mean that a transform animation was removed during commit or
    // activation (and, in that case, the LayerImpl will no longer own a
    // TransformNode, unless it has non-animation-related reasons for owning a
    // node).
    if (node->owner_id != id())
      return;
    if (node->data.is_animated != is_animated) {
      node->data.is_animated = is_animated;
      if (is_animated) {
        float maximum_target_scale = 0.f;
        node->data.local_maximum_animation_target_scale =
            MaximumTargetScale(&maximum_target_scale) ? maximum_target_scale
                                                      : 0.f;

        float animation_start_scale = 0.f;
        node->data.local_starting_animation_scale =
            AnimationStartScale(&animation_start_scale) ? animation_start_scale
                                                        : 0.f;

        node->data.has_only_translation_animations =
            HasOnlyTranslationTransforms();
      } else {
        node->data.local_maximum_animation_target_scale = 0.f;
        node->data.local_starting_animation_scale = 0.f;
        node->data.has_only_translation_animations = true;
      }

      transform_tree.set_needs_update(true);
      layer_tree_impl()->set_needs_update_draw_properties();
    }
  }
}

void LayerImpl::UpdatePropertyTreeOpacity() {
  if (effect_tree_index_ != -1) {
    EffectTree& effect_tree = layer_tree_impl()->property_trees()->effect_tree;
    if (effect_tree_index_ >= static_cast<int>(effect_tree.size()))
      return;
    EffectNode* node = effect_tree.Node(effect_tree_index_);
    // A LayerImpl's own current state is insufficient for determining whether
    // it owns an OpacityNode, since this depends on the state of the
    // corresponding Layer at the time of the last commit. For example, an
    // opacity animation might have been in progress at the time the last commit
    // started, but might have finished since then on the compositor thread.
    float effective_opacity = EffectiveOpacity();
    if (node->owner_id != id() || node->data.opacity == effective_opacity)
      return;
    node->data.opacity = effective_opacity;
    node->data.effect_changed = true;
    layer_tree_impl()->property_trees()->changed = true;
    effect_tree.set_needs_update(true);
  }
}

void LayerImpl::UpdatePropertyTreeForScrollingAndAnimationIfNeeded() {
  if (scrollable())
    UpdatePropertyTreeScrollOffset();

  if (HasAnyAnimationTargetingProperty(TargetProperty::OPACITY))
    UpdatePropertyTreeOpacity();

  if (HasAnyAnimationTargetingProperty(TargetProperty::TRANSFORM)) {
    UpdatePropertyTreeTransform();
    UpdatePropertyTreeTransformIsAnimated(
        HasPotentiallyRunningTransformAnimation());
  }
}

gfx::ScrollOffset LayerImpl::ScrollOffsetForAnimation() const {
  return CurrentScrollOffset();
}

void LayerImpl::OnFilterAnimated(const FilterOperations& filters) {
  if (filters_ != filters) {
    SetFilters(filters);
    SetNeedsPushProperties();
    layer_tree_impl()->set_needs_update_draw_properties();
    EffectTree& effect_tree = layer_tree_impl()->property_trees()->effect_tree;
    EffectNode* node = effect_tree.Node(effect_tree_index_);
    DCHECK_EQ(node->owner_id, id());
    node->data.effect_changed = true;
    layer_tree_impl()->property_trees()->changed = true;
    effect_tree.set_needs_update(true);
  }
}

void LayerImpl::OnOpacityAnimated(float opacity) {
  SetOpacity(opacity);
  // When hide_layer_and_subtree is true, the effective opacity is zero and we
  // need not update the opacity on property trees.
  if (!hide_layer_and_subtree_) {
    UpdatePropertyTreeOpacity();
    SetNeedsPushProperties();
    layer_tree_impl()->set_needs_update_draw_properties();
  }
}

void LayerImpl::OnTransformAnimated(const gfx::Transform& transform) {
  gfx::Transform old_transform = transform_;
  SetTransform(transform);
  UpdatePropertyTreeTransform();
  was_ever_ready_since_last_transform_animation_ = false;
  if (old_transform != transform) {
    SetNeedsPushProperties();
    layer_tree_impl()->set_needs_update_draw_properties();
  }
}

void LayerImpl::OnScrollOffsetAnimated(const gfx::ScrollOffset& scroll_offset) {
  // Only layers in the active tree should need to do anything here, since
  // layers in the pending tree will find out about these changes as a
  // result of the shared SyncedProperty.
  if (!IsActive())
    return;

  SetCurrentScrollOffset(scroll_offset);

  layer_tree_impl_->DidAnimateScrollOffset();
}

void LayerImpl::OnTransformIsPotentiallyAnimatingChanged(bool is_animating) {
  UpdatePropertyTreeTransformIsAnimated(is_animating);
  was_ever_ready_since_last_transform_animation_ = false;
}

bool LayerImpl::IsActive() const {
  return layer_tree_impl_->IsActiveTree();
}

gfx::Size LayerImpl::bounds() const {
  gfx::Vector2d delta = gfx::ToCeiledVector2d(bounds_delta_);
  return gfx::Size(bounds_.width() + delta.x(),
                   bounds_.height() + delta.y());
}

gfx::SizeF LayerImpl::BoundsForScrolling() const {
  return gfx::SizeF(bounds_.width() + bounds_delta_.x(),
                    bounds_.height() + bounds_delta_.y());
}

void LayerImpl::SetBounds(const gfx::Size& bounds) {
  if (bounds_ == bounds)
    return;

  bounds_ = bounds;

  layer_tree_impl()->DidUpdateScrollState(id());

  NoteLayerPropertyChanged();
}

void LayerImpl::SetBoundsDelta(const gfx::Vector2dF& bounds_delta) {
  DCHECK(IsActive());
  if (bounds_delta_ == bounds_delta)
    return;

  bounds_delta_ = bounds_delta;

  PropertyTrees* property_trees = layer_tree_impl()->property_trees();
  if (this == layer_tree_impl()->InnerViewportContainerLayer())
    property_trees->SetInnerViewportContainerBoundsDelta(bounds_delta);
  else if (this == layer_tree_impl()->OuterViewportContainerLayer())
    property_trees->SetOuterViewportContainerBoundsDelta(bounds_delta);
  else if (this == layer_tree_impl()->InnerViewportScrollLayer())
    property_trees->SetInnerViewportScrollBoundsDelta(bounds_delta);

  layer_tree_impl()->DidUpdateScrollState(id());

  if (masks_to_bounds()) {
    // If layer is clipping, then update the clip node using the new bounds.
    ClipNode* clip_node = property_trees->clip_tree.Node(clip_tree_index());
    if (clip_node) {
      DCHECK(id() == clip_node->owner_id);
      clip_node->data.clip = gfx::RectF(
          gfx::PointF() + offset_to_transform_parent(), gfx::SizeF(bounds()));
      property_trees->clip_tree.set_needs_update(true);
    }
    property_trees->full_tree_damaged = true;
    layer_tree_impl()->set_needs_update_draw_properties();
  } else {
    NoteLayerPropertyChanged();
  }
}

void LayerImpl::SetMaskLayer(std::unique_ptr<LayerImpl> mask_layer) {
  int new_layer_id = mask_layer ? mask_layer->id() : -1;

  if (mask_layer) {
    DCHECK_EQ(layer_tree_impl(), mask_layer->layer_tree_impl());
    DCHECK_NE(new_layer_id, mask_layer_id_);
  } else if (new_layer_id == mask_layer_id_) {
    return;
  }

  if (mask_layer_)
    layer_tree_impl_->RemoveLayer(mask_layer_->id());
  mask_layer_ = mask_layer.get();
  if (mask_layer_)
    layer_tree_impl_->AddLayer(std::move(mask_layer));

  mask_layer_id_ = new_layer_id;
}

std::unique_ptr<LayerImpl> LayerImpl::TakeMaskLayer() {
  mask_layer_id_ = -1;
  std::unique_ptr<LayerImpl> ret;
  if (mask_layer_)
    ret = layer_tree_impl_->RemoveLayer(mask_layer_->id());
  mask_layer_ = nullptr;
  return ret;
}

void LayerImpl::SetReplicaLayer(std::unique_ptr<LayerImpl> replica_layer) {
  int new_layer_id = replica_layer ? replica_layer->id() : -1;

  if (replica_layer) {
    DCHECK_EQ(layer_tree_impl(), replica_layer->layer_tree_impl());
    DCHECK_NE(new_layer_id, replica_layer_id_);
  } else if (new_layer_id == replica_layer_id_) {
    return;
  }

  if (replica_layer_)
    layer_tree_impl_->RemoveLayer(replica_layer_->id());
  replica_layer_ = replica_layer.get();
  if (replica_layer_)
    layer_tree_impl_->AddLayer(std::move(replica_layer));

  replica_layer_id_ = new_layer_id;
}

std::unique_ptr<LayerImpl> LayerImpl::TakeReplicaLayer() {
  replica_layer_id_ = -1;
  std::unique_ptr<LayerImpl> ret;
  if (replica_layer_)
    ret = layer_tree_impl_->RemoveLayer(replica_layer_->id());
  replica_layer_ = nullptr;
  return ret;
}

ScrollbarLayerImplBase* LayerImpl::ToScrollbarLayer() {
  return nullptr;
}

void LayerImpl::SetDrawsContent(bool draws_content) {
  if (draws_content_ == draws_content)
    return;

  draws_content_ = draws_content;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetHideLayerAndSubtree(bool hide) {
  if (hide_layer_and_subtree_ == hide)
    return;

  hide_layer_and_subtree_ = hide;
}

void LayerImpl::SetTransformOrigin(const gfx::Point3F& transform_origin) {
  if (transform_origin_ == transform_origin)
    return;
  transform_origin_ = transform_origin;
}

void LayerImpl::SetBackgroundColor(SkColor background_color) {
  if (background_color_ == background_color)
    return;

  background_color_ = background_color;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetSafeOpaqueBackgroundColor(SkColor background_color) {
  if (background_color == safe_opaque_background_color_)
    return;

  safe_opaque_background_color_ = background_color;
  SetNeedsPushProperties();
}

SkColor LayerImpl::SafeOpaqueBackgroundColor() const {
  if (contents_opaque())
    return safe_opaque_background_color_;
  SkColor color = background_color();
  if (SkColorGetA(color) == 255)
    color = SK_ColorTRANSPARENT;
  return color;
}

void LayerImpl::SetFilters(const FilterOperations& filters) {
  if (filters_ == filters)
    return;

  filters_ = filters;
}

bool LayerImpl::FilterIsAnimating() const {
  return layer_tree_impl_->IsAnimatingFilterProperty(this);
}

bool LayerImpl::HasPotentiallyRunningFilterAnimation() const {
  return layer_tree_impl_->HasPotentiallyRunningFilterAnimation(this);
}

bool LayerImpl::FilterIsAnimatingOnImplOnly() const {
  return layer_tree_impl_->FilterIsAnimatingOnImplOnly(this);
}

void LayerImpl::SetBackgroundFilters(
    const FilterOperations& filters) {
  if (background_filters_ == filters)
    return;

  background_filters_ = filters;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetMasksToBounds(bool masks_to_bounds) {
  if (masks_to_bounds_ == masks_to_bounds)
    return;

  masks_to_bounds_ = masks_to_bounds;
}

void LayerImpl::SetContentsOpaque(bool opaque) {
  if (contents_opaque_ == opaque)
    return;

  contents_opaque_ = opaque;
}

void LayerImpl::SetOpacity(float opacity) {
  if (opacity_ == opacity)
    return;

  opacity_ = opacity;
}

float LayerImpl::EffectiveOpacity() const {
  return hide_layer_and_subtree_ ? 0.f : opacity_;
}

bool LayerImpl::OpacityIsAnimating() const {
  return layer_tree_impl_->IsAnimatingOpacityProperty(this);
}

bool LayerImpl::HasPotentiallyRunningOpacityAnimation() const {
  return layer_tree_impl_->HasPotentiallyRunningOpacityAnimation(this);
}

bool LayerImpl::OpacityIsAnimatingOnImplOnly() const {
  return layer_tree_impl_->OpacityIsAnimatingOnImplOnly(this);
}

void LayerImpl::SetElementId(uint64_t element_id) {
  if (element_id == element_id_)
    return;

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "LayerImpl::SetElementId", "id", element_id);

  layer_tree_impl_->RemoveFromElementMap(this);
  element_id_ = element_id;
  layer_tree_impl_->AddToElementMap(this);
  SetNeedsPushProperties();
}

void LayerImpl::SetMutableProperties(uint32_t properties) {
  if (mutable_properties_ == properties)
    return;

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "LayerImpl::SetMutableProperties", "properties", properties);

  mutable_properties_ = properties;
  // If this layer is already in the element map, update its properties.
  layer_tree_impl_->AddToElementMap(this);
  SetNeedsPushProperties();
}

void LayerImpl::SetBlendMode(SkXfermode::Mode blend_mode) {
  if (blend_mode_ == blend_mode)
    return;

  blend_mode_ = blend_mode;
}

void LayerImpl::SetIsRootForIsolatedGroup(bool root) {
  if (is_root_for_isolated_group_ == root)
    return;

  is_root_for_isolated_group_ = root;
  SetNeedsPushProperties();
}

void LayerImpl::SetPosition(const gfx::PointF& position) {
  if (position_ == position)
    return;

  position_ = position;
}

void LayerImpl::SetShouldFlattenTransform(bool flatten) {
  if (should_flatten_transform_ == flatten)
    return;

  should_flatten_transform_ = flatten;
}

void LayerImpl::Set3dSortingContextId(int id) {
  if (id == sorting_context_id_)
    return;
  sorting_context_id_ = id;
}

void LayerImpl::SetTransform(const gfx::Transform& transform) {
  if (transform_ == transform)
    return;

  transform_ = transform;
  transform_is_invertible_ = transform_.IsInvertible();
}

void LayerImpl::SetTransformAndInvertibility(const gfx::Transform& transform,
                                             bool transform_is_invertible) {
  if (transform_ == transform) {
    DCHECK(transform_is_invertible_ == transform_is_invertible)
        << "Can't change invertibility if transform is unchanged";
    return;
  }
  transform_ = transform;
  transform_is_invertible_ = transform_is_invertible;
}

bool LayerImpl::TransformIsAnimating() const {
  return layer_tree_impl_->IsAnimatingTransformProperty(this);
}

bool LayerImpl::HasPotentiallyRunningTransformAnimation() const {
  return layer_tree_impl_->HasPotentiallyRunningTransformAnimation(this);
}

bool LayerImpl::TransformIsAnimatingOnImplOnly() const {
  return layer_tree_impl_->TransformIsAnimatingOnImplOnly(this);
}

bool LayerImpl::HasOnlyTranslationTransforms() const {
  return layer_tree_impl_->HasOnlyTranslationTransforms(this);
}

bool LayerImpl::AnimationsPreserveAxisAlignment() const {
  return layer_tree_impl_->AnimationsPreserveAxisAlignment(this);
}

bool LayerImpl::MaximumTargetScale(float* max_scale) const {
  return layer_tree_impl_->MaximumTargetScale(this, max_scale);
}

bool LayerImpl::AnimationStartScale(float* start_scale) const {
  return layer_tree_impl_->AnimationStartScale(this, start_scale);
}

bool LayerImpl::HasAnyAnimationTargetingProperty(
    TargetProperty::Type property) const {
  return layer_tree_impl_->HasAnyAnimationTargetingProperty(this, property);
}

bool LayerImpl::HasFilterAnimationThatInflatesBounds() const {
  return layer_tree_impl_->HasFilterAnimationThatInflatesBounds(this);
}

bool LayerImpl::HasTransformAnimationThatInflatesBounds() const {
  return layer_tree_impl_->HasTransformAnimationThatInflatesBounds(this);
}

bool LayerImpl::HasAnimationThatInflatesBounds() const {
  return layer_tree_impl_->HasAnimationThatInflatesBounds(this);
}

bool LayerImpl::FilterAnimationBoundsForBox(const gfx::BoxF& box,
                                            gfx::BoxF* bounds) const {
  return layer_tree_impl_->FilterAnimationBoundsForBox(this, box, bounds);
}

bool LayerImpl::TransformAnimationBoundsForBox(const gfx::BoxF& box,
                                               gfx::BoxF* bounds) const {
  return layer_tree_impl_->TransformAnimationBoundsForBox(this, box, bounds);
}

void LayerImpl::SetUpdateRect(const gfx::Rect& update_rect) {
  update_rect_ = update_rect;
  SetNeedsPushProperties();
}

void LayerImpl::AddDamageRect(const gfx::Rect& damage_rect) {
  damage_rect_.Union(damage_rect);
}

void LayerImpl::SetCurrentScrollOffset(const gfx::ScrollOffset& scroll_offset) {
  DCHECK(IsActive());
  if (layer_tree_impl()->property_trees()->scroll_tree.SetScrollOffset(
          id(), scroll_offset))
    layer_tree_impl()->DidUpdateScrollOffset(id(), transform_tree_index());
}

gfx::ScrollOffset LayerImpl::CurrentScrollOffset() const {
  return layer_tree_impl()->property_trees()->scroll_tree.current_scroll_offset(
      id());
}

void LayerImpl::UpdatePropertyTreeScrollOffset() {
  // TODO(enne): in the future, scrolling should update the scroll tree
  // directly instead of going through layers.
  if (transform_tree_index_ != -1) {
    TransformTree& transform_tree =
        layer_tree_impl()->property_trees()->transform_tree;
    TransformNode* node = transform_tree.Node(transform_tree_index_);
    gfx::ScrollOffset current_offset = CurrentScrollOffset();
    if (node->data.scroll_offset != current_offset) {
      node->data.scroll_offset = current_offset;
      node->data.needs_local_transform_update = true;
      transform_tree.set_needs_update(true);
    }
  }
}

void LayerImpl::SetDoubleSided(bool double_sided) {
  if (double_sided_ == double_sided)
    return;

  double_sided_ = double_sided;
}

SimpleEnclosedRegion LayerImpl::VisibleOpaqueRegion() const {
  if (contents_opaque())
    return SimpleEnclosedRegion(visible_layer_rect());
  return SimpleEnclosedRegion();
}

void LayerImpl::DidBeginTracing() {}

void LayerImpl::ReleaseResources() {}

void LayerImpl::RecreateResources() {
}

gfx::ScrollOffset LayerImpl::MaxScrollOffset() const {
  return layer_tree_impl()->property_trees()->scroll_tree.MaxScrollOffset(
      scroll_tree_index());
}

gfx::ScrollOffset LayerImpl::ClampScrollOffsetToLimits(
    gfx::ScrollOffset offset) const {
  offset.SetToMin(MaxScrollOffset());
  offset.SetToMax(gfx::ScrollOffset());
  return offset;
}

gfx::Vector2dF LayerImpl::ClampScrollToMaxScrollOffset() {
  gfx::ScrollOffset old_offset = CurrentScrollOffset();
  gfx::ScrollOffset clamped_offset = ClampScrollOffsetToLimits(old_offset);
  gfx::Vector2dF delta = clamped_offset.DeltaFrom(old_offset);
  if (!delta.IsZero())
    ScrollBy(delta);
  return delta;
}

void LayerImpl::SetNeedsPushProperties() {
  if (layer_tree_impl_)
    layer_tree_impl()->AddLayerShouldPushProperties(this);
}

void LayerImpl::GetAllPrioritizedTilesForTracing(
    std::vector<PrioritizedTile>* prioritized_tiles) const {
}

void LayerImpl::AsValueInto(base::trace_event::TracedValue* state) const {
  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"),
      state,
      "cc::LayerImpl",
      LayerTypeAsString(),
      this);
  state->SetInteger("layer_id", id());
  MathUtil::AddToTracedValue("bounds", bounds_, state);

  state->SetDouble("opacity", opacity());

  MathUtil::AddToTracedValue("position", position_, state);

  state->SetInteger("draws_content", DrawsContent());
  state->SetInteger("gpu_memory_usage",
                    base::saturated_cast<int>(GPUMemoryUsageInBytes()));

  if (mutable_properties_ != MutableProperty::kNone) {
    state->SetInteger("element_id", base::saturated_cast<int>(element_id_));
    state->SetInteger("mutable_properties", mutable_properties_);
  }

  MathUtil::AddToTracedValue("scroll_offset", CurrentScrollOffset(), state);

  MathUtil::AddToTracedValue("transform_origin", transform_origin_, state);

  if (!transform().IsIdentity())
    MathUtil::AddToTracedValue("transform", transform(), state);

  state->SetBoolean("should_flatten", should_flatten_transform_);

  bool clipped;
  gfx::QuadF layer_quad =
      MathUtil::MapQuad(ScreenSpaceTransform(),
                        gfx::QuadF(gfx::RectF(gfx::Rect(bounds()))), &clipped);
  MathUtil::AddToTracedValue("layer_quad", layer_quad, state);
  if (!touch_event_handler_region_.IsEmpty()) {
    state->BeginArray("touch_event_handler_region");
    touch_event_handler_region_.AsValueInto(state);
    state->EndArray();
  }
  if (!non_fast_scrollable_region_.IsEmpty()) {
    state->BeginArray("non_fast_scrollable_region");
    non_fast_scrollable_region_.AsValueInto(state);
    state->EndArray();
  }
  state->BeginArray("children");
  for (size_t i = 0; i < children_.size(); ++i) {
    state->BeginDictionary();
    children_[i]->AsValueInto(state);
    state->EndDictionary();
  }
  state->EndArray();
  if (mask_layer_) {
    state->BeginDictionary("mask_layer");
    mask_layer_->AsValueInto(state);
    state->EndDictionary();
  }
  if (replica_layer_) {
    state->BeginDictionary("replica_layer");
    replica_layer_->AsValueInto(state);
    state->EndDictionary();
  }

  if (scroll_parent_)
    state->SetInteger("scroll_parent", scroll_parent_->id());

  if (clip_parent_)
    state->SetInteger("clip_parent", clip_parent_->id());

  state->SetBoolean("can_use_lcd_text", can_use_lcd_text());
  state->SetBoolean("contents_opaque", contents_opaque());

  state->SetBoolean("has_animation_bounds",
                    layer_tree_impl_->HasAnimationThatInflatesBounds(this));

  gfx::BoxF box;
  if (LayerUtils::GetAnimationBounds(*this, &box))
    MathUtil::AddToTracedValue("animation_bounds", box, state);

  if (debug_info_) {
    std::string str;
    debug_info_->AppendAsTraceFormat(&str);
    base::JSONReader json_reader;
    std::unique_ptr<base::Value> debug_info_value(json_reader.ReadToValue(str));

    if (debug_info_value->IsType(base::Value::TYPE_DICTIONARY)) {
      base::DictionaryValue* dictionary_value = nullptr;
      bool converted_to_dictionary =
          debug_info_value->GetAsDictionary(&dictionary_value);
      DCHECK(converted_to_dictionary);
      for (base::DictionaryValue::Iterator it(*dictionary_value); !it.IsAtEnd();
           it.Advance()) {
        state->SetValue(it.key().data(), it.value().CreateDeepCopy());
      }
    } else {
      NOTREACHED();
    }
  }
}

bool LayerImpl::IsDrawnRenderSurfaceLayerListMember() const {
  return draw_properties_.last_drawn_render_surface_layer_list_id ==
         layer_tree_impl_->current_render_surface_list_id();
}

size_t LayerImpl::GPUMemoryUsageInBytes() const { return 0; }

void LayerImpl::RunMicroBenchmark(MicroBenchmarkImpl* benchmark) {
  benchmark->RunOnLayer(this);
}

int LayerImpl::NumDescendantsThatDrawContent() const {
  return num_descendants_that_draw_content_;
}

void LayerImpl::SetHasRenderSurface(bool should_have_render_surface) {
  if (!!render_surface() == should_have_render_surface)
    return;

  SetNeedsPushProperties();
  if (should_have_render_surface) {
    render_surface_ = base::WrapUnique(new RenderSurfaceImpl(this));
    return;
  }
  render_surface_.reset();
}

gfx::Transform LayerImpl::DrawTransform() const {
  // Only drawn layers have up-to-date draw properties.
  if (!IsDrawnRenderSurfaceLayerListMember()) {
    if (layer_tree_impl()->property_trees()->non_root_surfaces_enabled) {
      return draw_property_utils::DrawTransform(
          this, layer_tree_impl()->property_trees()->transform_tree);
    } else {
      return draw_property_utils::ScreenSpaceTransform(
          this, layer_tree_impl()->property_trees()->transform_tree);
    }
  }

  return draw_properties().target_space_transform;
}

gfx::Transform LayerImpl::ScreenSpaceTransform() const {
  // Only drawn layers have up-to-date draw properties.
  if (!IsDrawnRenderSurfaceLayerListMember()) {
    return draw_property_utils::ScreenSpaceTransform(
        this, layer_tree_impl()->property_trees()->transform_tree);
  }

  return draw_properties().screen_space_transform;
}

void LayerImpl::SetForceRenderSurface(bool force_render_surface) {
  if (force_render_surface == force_render_surface_)
    return;

  force_render_surface_ = force_render_surface;
  NoteLayerPropertyChanged();
}

Region LayerImpl::GetInvalidationRegionForDebugging() {
  return Region(update_rect_);
}

gfx::Rect LayerImpl::GetEnclosingRectInTargetSpace() const {
  return MathUtil::MapEnclosingClippedRect(DrawTransform(),
                                           gfx::Rect(bounds()));
}

gfx::Rect LayerImpl::GetScaledEnclosingRectInTargetSpace(float scale) const {
  gfx::Transform scaled_draw_transform = DrawTransform();
  scaled_draw_transform.Scale(SK_MScalar1 / scale, SK_MScalar1 / scale);
  gfx::Size scaled_bounds = gfx::ScaleToCeiledSize(bounds(), scale);
  return MathUtil::MapEnclosingClippedRect(scaled_draw_transform,
                                           gfx::Rect(scaled_bounds));
}

RenderSurfaceImpl* LayerImpl::render_target() {
  EffectTree& effect_tree = layer_tree_impl_->property_trees()->effect_tree;
  EffectNode* node = effect_tree.Node(effect_tree_index_);

  if (node->data.render_surface)
    return node->data.render_surface;
  else
    return effect_tree.Node(node->data.target_id)->data.render_surface;
}

const RenderSurfaceImpl* LayerImpl::render_target() const {
  const EffectTree& effect_tree =
      layer_tree_impl_->property_trees()->effect_tree;
  const EffectNode* node = effect_tree.Node(effect_tree_index_);

  if (node->data.render_surface)
    return node->data.render_surface;
  else
    return effect_tree.Node(node->data.target_id)->data.render_surface;
}

bool LayerImpl::IsHidden() const {
  EffectTree& effect_tree = layer_tree_impl_->property_trees()->effect_tree;
  EffectNode* node = effect_tree.Node(effect_tree_index_);
  return node->data.screen_space_opacity == 0.f;
}

bool LayerImpl::InsideReplica() const {
  // There are very few render targets so this should be cheap to do for each
  // layer instead of something more complicated.
  EffectTree& effect_tree = layer_tree_impl_->property_trees()->effect_tree;
  EffectNode* node = effect_tree.Node(effect_tree_index_);

  while (node->id > 0) {
    LayerImpl* target_layer = layer_tree_impl()->LayerById(node->owner_id);
    DCHECK(target_layer);
    if (target_layer->has_replica())
      return true;
    node = effect_tree.Node(node->data.target_id);
  }

  return false;
}

float LayerImpl::GetIdealContentsScale() const {
  float page_scale = IsAffectedByPageScale()
                         ? layer_tree_impl()->current_page_scale_factor()
                         : 1.f;
  float device_scale = layer_tree_impl()->device_scale_factor();

  float default_scale = page_scale * device_scale;
  if (!layer_tree_impl()
           ->settings()
           .layer_transforms_should_scale_layer_contents) {
    return default_scale;
  }

  gfx::Vector2dF transform_scales = MathUtil::ComputeTransform2dScaleComponents(
      DrawTransform(), default_scale);
  return std::max(transform_scales.x(), transform_scales.y());
}

}  // namespace cc
