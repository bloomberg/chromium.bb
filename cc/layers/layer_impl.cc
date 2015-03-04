// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_impl.h"

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/animation/animation_registrar.h"
#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/base/math_util.h"
#include "cc/base/simple_enclosed_region.h"
#include "cc/debug/debug_colors.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/debug/micro_benchmark_impl.h"
#include "cc/debug/traced_value.h"
#include "cc/input/layer_scroll_offset_delegate.h"
#include "cc/layers/layer_utils.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/output/copy_output_request.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass.h"
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
LayerImpl::LayerImpl(LayerTreeImpl* layer_impl, int id)
    : LayerImpl(layer_impl, id, new LayerImpl::SyncedScrollOffset) {
}

LayerImpl::LayerImpl(LayerTreeImpl* tree_impl,
                     int id,
                     scoped_refptr<SyncedScrollOffset> scroll_offset)
    : parent_(nullptr),
      scroll_parent_(nullptr),
      clip_parent_(nullptr),
      mask_layer_id_(-1),
      replica_layer_id_(-1),
      layer_id_(id),
      layer_tree_impl_(tree_impl),
      scroll_offset_(scroll_offset),
      scroll_offset_delegate_(nullptr),
      scroll_clip_layer_(nullptr),
      should_scroll_on_main_thread_(false),
      have_wheel_event_handlers_(false),
      have_scroll_event_handlers_(false),
      scroll_blocks_on_(SCROLL_BLOCKS_ON_NONE),
      user_scrollable_horizontal_(true),
      user_scrollable_vertical_(true),
      stacking_order_changed_(false),
      double_sided_(true),
      should_flatten_transform_(true),
      layer_property_changed_(false),
      masks_to_bounds_(false),
      contents_opaque_(false),
      is_root_for_isolated_group_(false),
      use_parent_backface_visibility_(false),
      draw_checkerboard_for_missing_tiles_(false),
      draws_content_(false),
      hide_layer_and_subtree_(false),
      transform_is_invertible_(true),
      is_container_for_fixed_position_layers_(false),
      background_color_(0),
      opacity_(1.0),
      blend_mode_(SkXfermode::kSrcOver_Mode),
      num_descendants_that_draw_content_(0),
      draw_depth_(0.f),
      needs_push_properties_(false),
      num_dependents_need_push_properties_(0),
      sorting_context_id_(0),
      current_draw_mode_(DRAW_MODE_NONE),
      frame_timing_requests_dirty_(false) {
  DCHECK_GT(layer_id_, 0);
  DCHECK(layer_tree_impl_);
  layer_tree_impl_->RegisterLayer(this);
  AnimationRegistrar* registrar = layer_tree_impl_->GetAnimationRegistrar();
  layer_animation_controller_ =
      registrar->GetAnimationControllerForId(layer_id_);
  layer_animation_controller_->AddValueObserver(this);
  if (IsActive()) {
    layer_animation_controller_->set_value_provider(this);
    layer_animation_controller_->set_layer_animation_delegate(this);
  }
  SetNeedsPushProperties();
}

LayerImpl::~LayerImpl() {
  DCHECK_EQ(DRAW_MODE_NONE, current_draw_mode_);

  layer_animation_controller_->RemoveValueObserver(this);
  layer_animation_controller_->remove_value_provider(this);
  layer_animation_controller_->remove_layer_animation_delegate(this);

  if (!copy_requests_.empty() && layer_tree_impl_->IsActiveTree())
    layer_tree_impl()->RemoveLayerWithCopyOutputRequest(this);
  layer_tree_impl_->UnregisterLayer(this);

  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), "cc::LayerImpl", this);
}

void LayerImpl::AddChild(scoped_ptr<LayerImpl> child) {
  child->SetParent(this);
  DCHECK_EQ(layer_tree_impl(), child->layer_tree_impl());
  children_.push_back(child.Pass());
  layer_tree_impl()->set_needs_update_draw_properties();
}

scoped_ptr<LayerImpl> LayerImpl::RemoveChild(LayerImpl* child) {
  for (OwnedLayerImplList::iterator it = children_.begin();
       it != children_.end();
       ++it) {
    if (*it == child) {
      scoped_ptr<LayerImpl> ret = children_.take(it);
      children_.erase(it);
      layer_tree_impl()->set_needs_update_draw_properties();
      return ret.Pass();
    }
  }
  return nullptr;
}

void LayerImpl::SetParent(LayerImpl* parent) {
  if (parent_should_know_need_push_properties()) {
    if (parent_)
      parent_->RemoveDependentNeedsPushProperties();
    if (parent)
      parent->AddDependentNeedsPushProperties();
  }
  parent_ = parent;
}

void LayerImpl::ClearChildList() {
  if (children_.empty())
    return;

  children_.clear();
  layer_tree_impl()->set_needs_update_draw_properties();
}

bool LayerImpl::HasAncestor(const LayerImpl* ancestor) const {
  if (!ancestor)
    return false;

  for (const LayerImpl* layer = this; layer; layer = layer->parent()) {
    if (layer == ancestor)
      return true;
  }

  return false;
}

void LayerImpl::SetScrollParent(LayerImpl* parent) {
  if (scroll_parent_ == parent)
    return;

  // Having both a scroll parent and a scroll offset delegate is unsupported.
  DCHECK(!scroll_offset_delegate_);

  if (parent)
    DCHECK_EQ(layer_tree_impl()->LayerById(parent->id()), parent);

  scroll_parent_ = parent;
  SetNeedsPushProperties();
}

void LayerImpl::SetDebugInfo(
    scoped_refptr<base::trace_event::ConvertableToTraceFormat> other) {
  debug_info_ = other;
  SetNeedsPushProperties();
}

void LayerImpl::SetScrollChildren(std::set<LayerImpl*>* children) {
  if (scroll_children_.get() == children)
    return;
  scroll_children_.reset(children);
  SetNeedsPushProperties();
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

void LayerImpl::PassCopyRequests(ScopedPtrVector<CopyOutputRequest>* requests) {
  if (requests->empty())
    return;
  DCHECK(render_surface());
  bool was_empty = copy_requests_.empty();
  copy_requests_.insert_and_take(copy_requests_.end(), requests);
  requests->clear();

  if (was_empty && layer_tree_impl()->IsActiveTree())
    layer_tree_impl()->AddLayerWithCopyOutputRequest(this);
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::TakeCopyRequestsAndTransformToTarget(
    ScopedPtrVector<CopyOutputRequest>* requests) {
  DCHECK(!copy_requests_.empty());
  DCHECK(layer_tree_impl()->IsActiveTree());
  DCHECK_EQ(render_target(), this);

  size_t first_inserted_request = requests->size();
  requests->insert_and_take(requests->end(), &copy_requests_);
  copy_requests_.clear();

  for (size_t i = first_inserted_request; i < requests->size(); ++i) {
    CopyOutputRequest* request = requests->at(i);
    if (!request->has_area())
      continue;

    gfx::Rect request_in_layer_space = request->area();
    gfx::Rect request_in_content_space =
        LayerRectToContentRect(request_in_layer_space);
    request->set_area(MathUtil::MapEnclosingClippedRect(
        draw_properties_.target_space_transform, request_in_content_space));
  }

  layer_tree_impl()->RemoveLayerWithCopyOutputRequest(this);
  layer_tree_impl()->set_needs_update_draw_properties();
}

void LayerImpl::ClearRenderSurfaceLayerList() {
  if (render_surface_)
    render_surface_->ClearLayerLists();
}

void LayerImpl::PopulateSharedQuadState(SharedQuadState* state) const {
  state->SetAll(
      draw_properties_.target_space_transform, draw_properties_.content_bounds,
      draw_properties_.visible_content_rect, draw_properties_.clip_rect,
      draw_properties_.is_clipped, draw_properties_.opacity,
      draw_properties_.blend_mode, sorting_context_id_);
}

void LayerImpl::PopulateScaledSharedQuadState(SharedQuadState* state,
                                              float scale) const {
  gfx::Transform scaled_draw_transform =
      draw_properties_.target_space_transform;
  scaled_draw_transform.Scale(SK_MScalar1 / scale, SK_MScalar1 / scale);
  gfx::Size scaled_content_bounds =
      gfx::ToCeiledSize(gfx::ScaleSize(bounds(), scale));
  gfx::Rect scaled_visible_content_rect =
      gfx::ScaleToEnclosingRect(visible_content_rect(), scale);
  scaled_visible_content_rect.Intersect(gfx::Rect(scaled_content_bounds));

  state->SetAll(scaled_draw_transform, scaled_content_bounds,
                scaled_visible_content_rect, draw_properties().clip_rect,
                draw_properties().is_clipped, draw_properties().opacity,
                draw_properties().blend_mode, sorting_context_id_);
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
    const gfx::Size& content_bounds,
    const SharedQuadState* shared_quad_state,
    AppendQuadsData* append_quads_data) const {
  SkColor color;
  float width;
  GetDebugBorderProperties(&color, &width);
  AppendDebugBorderQuad(render_pass,
                        content_bounds,
                        shared_quad_state,
                        append_quads_data,
                        color,
                        width);
}

void LayerImpl::AppendDebugBorderQuad(RenderPass* render_pass,
                                      const gfx::Size& content_bounds,
                                      const SharedQuadState* shared_quad_state,
                                      AppendQuadsData* append_quads_data,
                                      SkColor color,
                                      float width) const {
  if (!ShowDebugBorders())
    return;

  gfx::Rect quad_rect(content_bounds);
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
    gfx::Rect visible_fill_rect =
        gfx::IntersectRects(visible_quad_rect, fill_rect);
    DebugBorderDrawQuad* fill_quad =
        render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
    fill_quad->SetNew(shared_quad_state, fill_rect, visible_fill_rect,
                      fill_color, fill_width);
  }
}

bool LayerImpl::HasDelegatedContent() const {
  return false;
}

bool LayerImpl::HasContributingDelegatedRenderPasses() const {
  return false;
}

RenderPassId LayerImpl::FirstContributingRenderPassId() const {
  return RenderPassId(0, 0);
}

RenderPassId LayerImpl::NextContributingRenderPassId(RenderPassId id) const {
  return RenderPassId(0, 0);
}

void LayerImpl::GetContentsResourceId(ResourceProvider::ResourceId* resource_id,
                                      gfx::Size* resource_size) const {
  NOTREACHED();
  *resource_id = 0;
}

gfx::Vector2dF LayerImpl::ScrollBy(const gfx::Vector2dF& scroll) {
  RefreshFromScrollDelegate();

  gfx::ScrollOffset adjusted_scroll(scroll);
  if (layer_tree_impl()->settings().use_pinch_virtual_viewport) {
    if (!user_scrollable_horizontal_)
      adjusted_scroll.set_x(0);
    if (!user_scrollable_vertical_)
      adjusted_scroll.set_y(0);
  }
  DCHECK(scrollable());
  gfx::ScrollOffset old_offset = CurrentScrollOffset();
  gfx::ScrollOffset new_offset =
      ClampScrollOffsetToLimits(old_offset + adjusted_scroll);
  SetCurrentScrollOffset(new_offset);

  gfx::ScrollOffset unscrolled =
      old_offset + gfx::ScrollOffset(scroll) - new_offset;
  return gfx::Vector2dF(unscrolled.x(), unscrolled.y());
}

void LayerImpl::SetScrollClipLayer(int scroll_clip_layer_id) {
  scroll_clip_layer_ = layer_tree_impl()->LayerById(scroll_clip_layer_id);
}

bool LayerImpl::user_scrollable(ScrollbarOrientation orientation) const {
  return (orientation == HORIZONTAL) ? user_scrollable_horizontal_
                                     : user_scrollable_vertical_;
}

void LayerImpl::ApplySentScrollDeltasFromAbortedCommit() {
  DCHECK(layer_tree_impl()->IsActiveTree());
  scroll_offset_->AbortCommit();
}

InputHandler::ScrollStatus LayerImpl::TryScroll(
    const gfx::PointF& screen_space_point,
    InputHandler::ScrollInputType type,
    ScrollBlocksOn effective_block_mode) const {
  if (should_scroll_on_main_thread()) {
    TRACE_EVENT0("cc", "LayerImpl::TryScroll: Failed ShouldScrollOnMainThread");
    return InputHandler::SCROLL_ON_MAIN_THREAD;
  }

  if (!screen_space_transform().IsInvertible()) {
    TRACE_EVENT0("cc", "LayerImpl::TryScroll: Ignored NonInvertibleTransform");
    return InputHandler::SCROLL_IGNORED;
  }

  if (!non_fast_scrollable_region().IsEmpty()) {
    bool clipped = false;
    gfx::Transform inverse_screen_space_transform(
        gfx::Transform::kSkipInitialization);
    if (!screen_space_transform().GetInverse(&inverse_screen_space_transform)) {
      // TODO(shawnsingh): We shouldn't be applying a projection if screen space
      // transform is uninvertible here. Perhaps we should be returning
      // SCROLL_ON_MAIN_THREAD in this case?
    }

    gfx::PointF hit_test_point_in_content_space =
        MathUtil::ProjectPoint(inverse_screen_space_transform,
                               screen_space_point,
                               &clipped);
    gfx::PointF hit_test_point_in_layer_space =
        gfx::ScalePoint(hit_test_point_in_content_space,
                        1.f / contents_scale_x(),
                        1.f / contents_scale_y());
    if (!clipped &&
        non_fast_scrollable_region().Contains(
            gfx::ToRoundedPoint(hit_test_point_in_layer_space))) {
      TRACE_EVENT0("cc",
                   "LayerImpl::tryScroll: Failed NonFastScrollableRegion");
      return InputHandler::SCROLL_ON_MAIN_THREAD;
    }
  }

  if (have_scroll_event_handlers() &&
      effective_block_mode & SCROLL_BLOCKS_ON_SCROLL_EVENT) {
    TRACE_EVENT0("cc", "LayerImpl::tryScroll: Failed ScrollEventHandlers");
    return InputHandler::SCROLL_ON_MAIN_THREAD;
  }

  if (type == InputHandler::WHEEL && have_wheel_event_handlers() &&
      effective_block_mode & SCROLL_BLOCKS_ON_WHEEL_EVENT) {
    TRACE_EVENT0("cc", "LayerImpl::tryScroll: Failed WheelEventHandlers");
    return InputHandler::SCROLL_ON_MAIN_THREAD;
  }

  if (!scrollable()) {
    TRACE_EVENT0("cc", "LayerImpl::tryScroll: Ignored not scrollable");
    return InputHandler::SCROLL_IGNORED;
  }

  gfx::ScrollOffset max_scroll_offset = MaxScrollOffset();
  if (max_scroll_offset.x() <= 0 && max_scroll_offset.y() <= 0) {
    TRACE_EVENT0("cc",
                 "LayerImpl::tryScroll: Ignored. Technically scrollable,"
                 " but has no affordance in either direction.");
    return InputHandler::SCROLL_IGNORED;
  }

  return InputHandler::SCROLL_STARTED;
}

gfx::Rect LayerImpl::LayerRectToContentRect(
    const gfx::RectF& layer_rect) const {
  gfx::RectF content_rect =
      gfx::ScaleRect(layer_rect, contents_scale_x(), contents_scale_y());
  // Intersect with content rect to avoid the extra pixel because for some
  // values x and y, ceil((x / y) * y) may be x + 1.
  content_rect.Intersect(gfx::Rect(content_bounds()));
  return gfx::ToEnclosingRect(content_rect);
}

skia::RefPtr<SkPicture> LayerImpl::GetPicture() {
  return skia::RefPtr<SkPicture>();
}

scoped_ptr<LayerImpl> LayerImpl::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return LayerImpl::Create(tree_impl, layer_id_, scroll_offset_);
}

void LayerImpl::PushPropertiesTo(LayerImpl* layer) {
  layer->SetTransformOrigin(transform_origin_);
  layer->SetBackgroundColor(background_color_);
  layer->SetBounds(bounds_);
  layer->SetContentBounds(content_bounds());
  layer->SetContentsScale(contents_scale_x(), contents_scale_y());
  layer->SetDoubleSided(double_sided_);
  layer->SetDrawCheckerboardForMissingTiles(
      draw_checkerboard_for_missing_tiles_);
  layer->SetDrawsContent(DrawsContent());
  layer->SetHideLayerAndSubtree(hide_layer_and_subtree_);
  layer->SetHasRenderSurface(!!render_surface() || layer->HasCopyRequest());
  layer->SetFilters(filters());
  layer->SetBackgroundFilters(background_filters());
  layer->SetMasksToBounds(masks_to_bounds_);
  layer->SetShouldScrollOnMainThread(should_scroll_on_main_thread_);
  layer->SetHaveWheelEventHandlers(have_wheel_event_handlers_);
  layer->SetHaveScrollEventHandlers(have_scroll_event_handlers_);
  layer->SetScrollBlocksOn(scroll_blocks_on_);
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
  layer->SetUseParentBackfaceVisibility(use_parent_backface_visibility_);
  layer->SetTransformAndInvertibility(transform_, transform_is_invertible_);

  layer->SetScrollClipLayer(scroll_clip_layer_ ? scroll_clip_layer_->id()
                                               : Layer::INVALID_ID);
  layer->set_user_scrollable_horizontal(user_scrollable_horizontal_);
  layer->set_user_scrollable_vertical(user_scrollable_vertical_);

  layer->SetScrollCompensationAdjustment(scroll_compensation_adjustment_);

  layer->PushScrollOffset(nullptr);

  layer->Set3dSortingContextId(sorting_context_id_);
  layer->SetNumDescendantsThatDrawContent(num_descendants_that_draw_content_);

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

  layer->SetStackingOrderChanged(stacking_order_changed_);
  layer->SetDebugInfo(debug_info_);

  if (frame_timing_requests_dirty_) {
    layer->PassFrameTimingRequests(&frame_timing_requests_);
    frame_timing_requests_dirty_ = false;
  }

  // Reset any state that should be cleared for the next update.
  stacking_order_changed_ = false;
  update_rect_ = gfx::Rect();
  needs_push_properties_ = false;
  num_dependents_need_push_properties_ = 0;
}

gfx::Vector2dF LayerImpl::FixedContainerSizeDelta() const {
  if (!scroll_clip_layer_)
    return gfx::Vector2dF();

  gfx::Vector2dF delta_from_scroll = scroll_clip_layer_->bounds_delta();

  // In virtual-viewport mode, we don't need to compensate for pinch zoom or
  // scale since the fixed container is the outer viewport, which sits below
  // the page scale.
  if (layer_tree_impl()->settings().use_pinch_virtual_viewport)
    return delta_from_scroll;

  float scale_delta = layer_tree_impl()->page_scale_delta();
  float scale = layer_tree_impl()->current_page_scale_factor() /
                layer_tree_impl()->page_scale_delta();

  delta_from_scroll.Scale(1.f / scale);

  // The delta-from-pinch component requires some explanation: A viewport of
  // size (w,h) will appear to be size (w/s,h/s) under scale s in the content
  // space. If s -> s' on the impl thread, where s' = s * ds, then the apparent
  // viewport size change in the content space due to ds is:
  //
  // (w/s',h/s') - (w/s,h/s) = (w,h)(1/s' - 1/s) = (w,h)(1 - ds)/(s ds)
  //
  gfx::Vector2dF delta_from_pinch =
      gfx::Rect(scroll_clip_layer_->bounds()).bottom_right() - gfx::PointF();
  delta_from_pinch.Scale((1.f - scale_delta) / (scale * scale_delta));

  return delta_from_scroll + delta_from_pinch;
}

base::DictionaryValue* LayerImpl::LayerTreeAsJson() const {
  base::DictionaryValue* result = new base::DictionaryValue;
  result->SetString("LayerType", LayerTypeAsString());

  base::ListValue* list = new base::ListValue;
  list->AppendInteger(bounds().width());
  list->AppendInteger(bounds().height());
  result->Set("Bounds", list);

  list = new base::ListValue;
  list->AppendDouble(position_.x());
  list->AppendDouble(position_.y());
  result->Set("Position", list);

  const gfx::Transform& gfx_transform = draw_properties_.target_space_transform;
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

  if (have_wheel_event_handlers_)
    result->SetBoolean("WheelHandler", have_wheel_event_handlers_);
  if (have_scroll_event_handlers_)
    result->SetBoolean("ScrollHandler", have_scroll_event_handlers_);
  if (!touch_event_handler_region_.IsEmpty()) {
    scoped_ptr<base::Value> region = touch_event_handler_region_.AsValue();
    result->Set("TouchRegion", region.release());
  }

  if (scroll_blocks_on_) {
    list = new base::ListValue;
    if (scroll_blocks_on_ & SCROLL_BLOCKS_ON_START_TOUCH)
      list->AppendString("StartTouch");
    if (scroll_blocks_on_ & SCROLL_BLOCKS_ON_WHEEL_EVENT)
      list->AppendString("WheelEvent");
    if (scroll_blocks_on_ & SCROLL_BLOCKS_ON_SCROLL_EVENT)
      list->AppendString("ScrollEvent");
    result->Set("ScrollBlocksOn", list);
  }

  list = new base::ListValue;
  for (size_t i = 0; i < children_.size(); ++i)
    list->Append(children_[i]->LayerTreeAsJson());
  result->Set("Children", list);

  return result;
}

void LayerImpl::SetStackingOrderChanged(bool stacking_order_changed) {
  if (stacking_order_changed) {
    stacking_order_changed_ = true;
    NoteLayerPropertyChangedForSubtree();
  }
}

void LayerImpl::NoteLayerPropertyChanged() {
  layer_property_changed_ = true;
  layer_tree_impl()->set_needs_update_draw_properties();
  SetNeedsPushProperties();
}

void LayerImpl::NoteLayerPropertyChangedForSubtree() {
  layer_property_changed_ = true;
  layer_tree_impl()->set_needs_update_draw_properties();
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->NoteLayerPropertyChangedForDescendantsInternal();
  SetNeedsPushProperties();
}

void LayerImpl::NoteLayerPropertyChangedForDescendantsInternal() {
  layer_property_changed_ = true;
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->NoteLayerPropertyChangedForDescendantsInternal();
}

void LayerImpl::NoteLayerPropertyChangedForDescendants() {
  layer_tree_impl()->set_needs_update_draw_properties();
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->NoteLayerPropertyChangedForDescendantsInternal();
  SetNeedsPushProperties();
}

const char* LayerImpl::LayerTypeAsString() const {
  return "cc::LayerImpl";
}

void LayerImpl::ResetAllChangeTrackingForSubtree() {
  layer_property_changed_ = false;

  update_rect_ = gfx::Rect();
  damage_rect_ = gfx::RectF();

  if (render_surface_)
    render_surface_->ResetPropertyChangedFlag();

  if (mask_layer_)
    mask_layer_->ResetAllChangeTrackingForSubtree();

  if (replica_layer_) {
    // This also resets the replica mask, if it exists.
    replica_layer_->ResetAllChangeTrackingForSubtree();
  }

  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->ResetAllChangeTrackingForSubtree();

  needs_push_properties_ = false;
  num_dependents_need_push_properties_ = 0;
}

gfx::ScrollOffset LayerImpl::ScrollOffsetForAnimation() const {
  return CurrentScrollOffset();
}

void LayerImpl::OnFilterAnimated(const FilterOperations& filters) {
  SetFilters(filters);
}

void LayerImpl::OnOpacityAnimated(float opacity) {
  SetOpacity(opacity);
}

void LayerImpl::OnTransformAnimated(const gfx::Transform& transform) {
  SetTransform(transform);
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

void LayerImpl::OnAnimationWaitingForDeletion() {}

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

  ScrollbarParametersDidChange(true);
  if (masks_to_bounds())
    NoteLayerPropertyChangedForSubtree();
  else
    NoteLayerPropertyChanged();
}

void LayerImpl::SetBoundsDelta(const gfx::Vector2dF& bounds_delta) {
  if (bounds_delta_ == bounds_delta)
    return;

  bounds_delta_ = bounds_delta;

  ScrollbarParametersDidChange(true);
  if (masks_to_bounds())
    NoteLayerPropertyChangedForSubtree();
  else
    NoteLayerPropertyChanged();
}

void LayerImpl::SetMaskLayer(scoped_ptr<LayerImpl> mask_layer) {
  int new_layer_id = mask_layer ? mask_layer->id() : -1;

  if (mask_layer) {
    DCHECK_EQ(layer_tree_impl(), mask_layer->layer_tree_impl());
    DCHECK_NE(new_layer_id, mask_layer_id_);
  } else if (new_layer_id == mask_layer_id_) {
    return;
  }

  mask_layer_ = mask_layer.Pass();
  mask_layer_id_ = new_layer_id;
  if (mask_layer_)
    mask_layer_->SetParent(this);
  NoteLayerPropertyChangedForSubtree();
}

scoped_ptr<LayerImpl> LayerImpl::TakeMaskLayer() {
  mask_layer_id_ = -1;
  return mask_layer_.Pass();
}

void LayerImpl::SetReplicaLayer(scoped_ptr<LayerImpl> replica_layer) {
  int new_layer_id = replica_layer ? replica_layer->id() : -1;

  if (replica_layer) {
    DCHECK_EQ(layer_tree_impl(), replica_layer->layer_tree_impl());
    DCHECK_NE(new_layer_id, replica_layer_id_);
  } else if (new_layer_id == replica_layer_id_) {
    return;
  }

  replica_layer_ = replica_layer.Pass();
  replica_layer_id_ = new_layer_id;
  if (replica_layer_)
    replica_layer_->SetParent(this);
  NoteLayerPropertyChangedForSubtree();
}

scoped_ptr<LayerImpl> LayerImpl::TakeReplicaLayer() {
  replica_layer_id_ = -1;
  return replica_layer_.Pass();
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
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetTransformOrigin(const gfx::Point3F& transform_origin) {
  if (transform_origin_ == transform_origin)
    return;
  transform_origin_ = transform_origin;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetBackgroundColor(SkColor background_color) {
  if (background_color_ == background_color)
    return;

  background_color_ = background_color;
  NoteLayerPropertyChanged();
}

SkColor LayerImpl::SafeOpaqueBackgroundColor() const {
  SkColor color = background_color();
  if (SkColorGetA(color) == 255 && !contents_opaque()) {
    color = SK_ColorTRANSPARENT;
  } else if (SkColorGetA(color) != 255 && contents_opaque()) {
    for (const LayerImpl* layer = parent(); layer;
         layer = layer->parent()) {
      color = layer->background_color();
      if (SkColorGetA(color) == 255)
        break;
    }
    if (SkColorGetA(color) != 255)
      color = layer_tree_impl()->background_color();
    if (SkColorGetA(color) != 255)
      color = SkColorSetA(color, 255);
  }
  return color;
}

void LayerImpl::SetFilters(const FilterOperations& filters) {
  if (filters_ == filters)
    return;

  filters_ = filters;
  NoteLayerPropertyChangedForSubtree();
}

bool LayerImpl::FilterIsAnimating() const {
  return layer_animation_controller_->IsAnimatingProperty(Animation::FILTER);
}

bool LayerImpl::FilterIsAnimatingOnImplOnly() const {
  Animation* filter_animation =
      layer_animation_controller_->GetAnimation(Animation::FILTER);
  return filter_animation && filter_animation->is_impl_only();
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
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetContentsOpaque(bool opaque) {
  if (contents_opaque_ == opaque)
    return;

  contents_opaque_ = opaque;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetOpacity(float opacity) {
  if (opacity_ == opacity)
    return;

  opacity_ = opacity;
  NoteLayerPropertyChangedForSubtree();
}

bool LayerImpl::OpacityIsAnimating() const {
  return layer_animation_controller_->IsAnimatingProperty(Animation::OPACITY);
}

bool LayerImpl::OpacityIsAnimatingOnImplOnly() const {
  Animation* opacity_animation =
      layer_animation_controller_->GetAnimation(Animation::OPACITY);
  return opacity_animation && opacity_animation->is_impl_only();
}

void LayerImpl::SetBlendMode(SkXfermode::Mode blend_mode) {
  if (blend_mode_ == blend_mode)
    return;

  blend_mode_ = blend_mode;
  NoteLayerPropertyChangedForSubtree();
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
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetShouldFlattenTransform(bool flatten) {
  if (should_flatten_transform_ == flatten)
    return;

  should_flatten_transform_ = flatten;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::Set3dSortingContextId(int id) {
  if (id == sorting_context_id_)
    return;
  sorting_context_id_ = id;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::PassFrameTimingRequests(
    std::vector<FrameTimingRequest>* requests) {
  frame_timing_requests_.swap(*requests);
  frame_timing_requests_dirty_ = true;
  SetNeedsPushProperties();
}

void LayerImpl::SetTransform(const gfx::Transform& transform) {
  if (transform_ == transform)
    return;

  transform_ = transform;
  transform_is_invertible_ = transform_.IsInvertible();
  NoteLayerPropertyChangedForSubtree();
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
  NoteLayerPropertyChangedForSubtree();
}

bool LayerImpl::TransformIsAnimating() const {
  return layer_animation_controller_->IsAnimatingProperty(Animation::TRANSFORM);
}

bool LayerImpl::TransformIsAnimatingOnImplOnly() const {
  Animation* transform_animation =
      layer_animation_controller_->GetAnimation(Animation::TRANSFORM);
  return transform_animation && transform_animation->is_impl_only();
}

void LayerImpl::SetUpdateRect(const gfx::Rect& update_rect) {
  update_rect_ = update_rect;
  SetNeedsPushProperties();
}

void LayerImpl::AddDamageRect(const gfx::RectF& damage_rect) {
  damage_rect_ = gfx::UnionRects(damage_rect_, damage_rect);
}

void LayerImpl::SetContentBounds(const gfx::Size& content_bounds) {
  if (this->content_bounds() == content_bounds)
    return;

  draw_properties_.content_bounds = content_bounds;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetContentsScale(float contents_scale_x,
                                 float contents_scale_y) {
  if (this->contents_scale_x() == contents_scale_x &&
      this->contents_scale_y() == contents_scale_y)
    return;

  draw_properties_.contents_scale_x = contents_scale_x;
  draw_properties_.contents_scale_y = contents_scale_y;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetScrollOffsetDelegate(
    ScrollOffsetDelegate* scroll_offset_delegate) {
  // Having both a scroll parent and a scroll offset delegate is unsupported.
  DCHECK(!scroll_parent_);
  RefreshFromScrollDelegate();
  scroll_offset_delegate_ = scroll_offset_delegate;
  if (scroll_offset_delegate_)
    scroll_offset_delegate_->SetCurrentScrollOffset(CurrentScrollOffset());
}

bool LayerImpl::IsExternalFlingActive() const {
  return scroll_offset_delegate_ &&
         scroll_offset_delegate_->IsExternalFlingActive();
}

void LayerImpl::SetCurrentScrollOffset(const gfx::ScrollOffset& scroll_offset) {
  DCHECK(IsActive());
  if (scroll_offset_->SetCurrent(scroll_offset))
    DidUpdateScrollOffset();
}

void LayerImpl::PushScrollOffsetFromMainThread(
    const gfx::ScrollOffset& scroll_offset) {
  PushScrollOffset(&scroll_offset);
}

void LayerImpl::PushScrollOffsetFromMainThreadAndClobberActiveValue(
    const gfx::ScrollOffset& scroll_offset) {
  scroll_offset_->set_clobber_active_value();
  PushScrollOffset(&scroll_offset);
}

gfx::ScrollOffset LayerImpl::PullDeltaForMainThread() {
  RefreshFromScrollDelegate();
  return scroll_offset_->PullDeltaForMainThread();
}

void LayerImpl::RefreshFromScrollDelegate() {
  if (scroll_offset_delegate_) {
    SetCurrentScrollOffset(
        gfx::ScrollOffset(scroll_offset_delegate_->GetCurrentScrollOffset()));
  }
}

gfx::ScrollOffset LayerImpl::CurrentScrollOffset() const {
  return scroll_offset_->Current(IsActive());
}

gfx::Vector2dF LayerImpl::ScrollDelta() const {
  if (IsActive())
    return gfx::Vector2dF(scroll_offset_->Delta().x(),
                          scroll_offset_->Delta().y());
  else
    return gfx::Vector2dF(scroll_offset_->PendingDelta().get().x(),
                          scroll_offset_->PendingDelta().get().y());
}

void LayerImpl::SetScrollDelta(const gfx::Vector2dF& delta) {
  DCHECK(IsActive());
  SetCurrentScrollOffset(scroll_offset_->ActiveBase() +
                         gfx::ScrollOffset(delta));
}

gfx::ScrollOffset LayerImpl::BaseScrollOffset() const {
  if (IsActive())
    return scroll_offset_->ActiveBase();
  else
    return scroll_offset_->PendingBase();
}

void LayerImpl::PushScrollOffset(const gfx::ScrollOffset* scroll_offset) {
  DCHECK(scroll_offset || IsActive());
  bool changed = false;
  if (scroll_offset) {
    DCHECK(!IsActive() || !layer_tree_impl_->FindPendingTreeLayerById(id()));
    changed |= scroll_offset_->PushFromMainThread(*scroll_offset);
  }
  if (IsActive()) {
    changed |= scroll_offset_->PushPendingToActive();
  }

  if (changed)
    DidUpdateScrollOffset();
}

void LayerImpl::DidUpdateScrollOffset() {
  if (scroll_offset_delegate_) {
    scroll_offset_delegate_->SetCurrentScrollOffset(CurrentScrollOffset());
    scroll_offset_delegate_->Update();
    RefreshFromScrollDelegate();
  }

  NoteLayerPropertyChangedForSubtree();
  ScrollbarParametersDidChange(false);
}

void LayerImpl::SetDoubleSided(bool double_sided) {
  if (double_sided_ == double_sided)
    return;

  double_sided_ = double_sided;
  NoteLayerPropertyChangedForSubtree();
}

SimpleEnclosedRegion LayerImpl::VisibleContentOpaqueRegion() const {
  if (contents_opaque())
    return SimpleEnclosedRegion(visible_content_rect());
  return SimpleEnclosedRegion();
}

void LayerImpl::DidBeginTracing() {}

void LayerImpl::ReleaseResources() {}

void LayerImpl::RecreateResources() {
}

gfx::ScrollOffset LayerImpl::MaxScrollOffset() const {
  if (!scroll_clip_layer_ || bounds().IsEmpty())
    return gfx::ScrollOffset();

  LayerImpl const* page_scale_layer = layer_tree_impl()->page_scale_layer();
  DCHECK(this != page_scale_layer);
  DCHECK(this != layer_tree_impl()->InnerViewportScrollLayer() ||
         IsContainerForFixedPositionLayers());

  float scale_factor = 1.f;
  for (LayerImpl const* current_layer = this;
       current_layer != scroll_clip_layer_->parent();
       current_layer = current_layer->parent()) {
    if (current_layer == page_scale_layer)
      scale_factor = layer_tree_impl()->current_page_scale_factor();
  }

  gfx::SizeF scaled_scroll_bounds =
      gfx::ToFlooredSize(gfx::ScaleSize(BoundsForScrolling(), scale_factor));
  scaled_scroll_bounds = gfx::ToFlooredSize(scaled_scroll_bounds);

  gfx::ScrollOffset max_offset(
      scaled_scroll_bounds.width() - scroll_clip_layer_->bounds().width(),
      scaled_scroll_bounds.height() - scroll_clip_layer_->bounds().height());
  // We need the final scroll offset to be in CSS coords.
  max_offset.Scale(1 / scale_factor);
  max_offset.SetToMax(gfx::ScrollOffset());
  return max_offset;
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

void LayerImpl::SetScrollbarPosition(ScrollbarLayerImplBase* scrollbar_layer,
                                     LayerImpl* scrollbar_clip_layer,
                                     bool on_resize) const {
  DCHECK(scrollbar_layer);
  LayerImpl* page_scale_layer = layer_tree_impl()->page_scale_layer();

  DCHECK(this != page_scale_layer);
  DCHECK(scrollbar_clip_layer);
  gfx::RectF clip_rect(gfx::PointF(),
                       scrollbar_clip_layer->BoundsForScrolling());

  // See comment in MaxScrollOffset() regarding the use of the content layer
  // bounds here.
  gfx::RectF scroll_rect(gfx::PointF(), BoundsForScrolling());

  if (scroll_rect.size().IsEmpty())
    return;

  gfx::ScrollOffset current_offset;
  for (LayerImpl const* current_layer = this;
       current_layer != scrollbar_clip_layer->parent();
       current_layer = current_layer->parent()) {
    current_offset += current_layer->CurrentScrollOffset();
    if (current_layer == page_scale_layer) {
      float scale_factor = layer_tree_impl()->current_page_scale_factor();
      current_offset.Scale(scale_factor);
      scroll_rect.Scale(scale_factor);
    }
  }

  bool scrollbar_needs_animation = false;
  scrollbar_needs_animation |= scrollbar_layer->SetVerticalAdjust(
      scrollbar_clip_layer->bounds_delta().y());
  if (scrollbar_layer->orientation() == HORIZONTAL) {
    float visible_ratio = clip_rect.width() / scroll_rect.width();
    scrollbar_needs_animation |=
        scrollbar_layer->SetCurrentPos(current_offset.x());
    scrollbar_needs_animation |=
        scrollbar_layer->SetMaximum(scroll_rect.width() - clip_rect.width());
    scrollbar_needs_animation |=
        scrollbar_layer->SetVisibleToTotalLengthRatio(visible_ratio);
  } else {
    float visible_ratio = clip_rect.height() / scroll_rect.height();
    scrollbar_needs_animation |=
        scrollbar_layer->SetCurrentPos(current_offset.y());
    scrollbar_needs_animation |=
        scrollbar_layer->SetMaximum(scroll_rect.height() - clip_rect.height());
    scrollbar_needs_animation |=
        scrollbar_layer->SetVisibleToTotalLengthRatio(visible_ratio);
  }
  if (scrollbar_needs_animation) {
    layer_tree_impl()->set_needs_update_draw_properties();
    // TODO(wjmaclean) The scrollbar animator for the pinch-zoom scrollbars
    // should activate for every scroll on the main frame, not just the
    // scrolls that move the pinch virtual viewport (i.e. trigger from
    // either inner or outer viewport).
    if (scrollbar_animation_controller_) {
      // Non-overlay scrollbars shouldn't trigger animations.
      if (scrollbar_layer->is_overlay_scrollbar())
        scrollbar_animation_controller_->DidScrollUpdate(on_resize);
    }
  }
}

void LayerImpl::DidBecomeActive() {
  if (layer_tree_impl_->settings().scrollbar_animator ==
      LayerTreeSettings::NO_ANIMATOR) {
    return;
  }

  bool need_scrollbar_animation_controller = scrollable() && scrollbars_;
  if (!need_scrollbar_animation_controller) {
    scrollbar_animation_controller_ = nullptr;
    return;
  }

  if (scrollbar_animation_controller_)
    return;

  scrollbar_animation_controller_ =
      layer_tree_impl_->CreateScrollbarAnimationController(this);
}

void LayerImpl::ClearScrollbars() {
  if (!scrollbars_)
    return;

  scrollbars_.reset(nullptr);
}

void LayerImpl::AddScrollbar(ScrollbarLayerImplBase* layer) {
  DCHECK(layer);
  DCHECK(!scrollbars_ || scrollbars_->find(layer) == scrollbars_->end());
  if (!scrollbars_)
    scrollbars_.reset(new ScrollbarSet());

  scrollbars_->insert(layer);
}

void LayerImpl::RemoveScrollbar(ScrollbarLayerImplBase* layer) {
  DCHECK(scrollbars_);
  DCHECK(layer);
  DCHECK(scrollbars_->find(layer) != scrollbars_->end());

  scrollbars_->erase(layer);
  if (scrollbars_->empty())
    scrollbars_ = nullptr;
}

bool LayerImpl::HasScrollbar(ScrollbarOrientation orientation) const {
  if (!scrollbars_)
    return false;

  for (ScrollbarSet::iterator it = scrollbars_->begin();
       it != scrollbars_->end();
       ++it)
    if ((*it)->orientation() == orientation)
      return true;

  return false;
}

void LayerImpl::ScrollbarParametersDidChange(bool on_resize) {
  if (!scrollbars_)
    return;

  for (ScrollbarSet::iterator it = scrollbars_->begin();
       it != scrollbars_->end();
       ++it) {
    bool is_scroll_layer = (*it)->ScrollLayerId() == layer_id_;
    bool scroll_layer_resized = is_scroll_layer && on_resize;
    (*it)->ScrollbarParametersDidChange(scroll_layer_resized);
  }
}

void LayerImpl::SetNeedsPushProperties() {
  if (needs_push_properties_)
    return;
  if (!parent_should_know_need_push_properties() && parent_)
    parent_->AddDependentNeedsPushProperties();
  needs_push_properties_ = true;
}

void LayerImpl::AddDependentNeedsPushProperties() {
  DCHECK_GE(num_dependents_need_push_properties_, 0);

  if (!parent_should_know_need_push_properties() && parent_)
    parent_->AddDependentNeedsPushProperties();

  num_dependents_need_push_properties_++;
}

void LayerImpl::RemoveDependentNeedsPushProperties() {
  num_dependents_need_push_properties_--;
  DCHECK_GE(num_dependents_need_push_properties_, 0);

  if (!parent_should_know_need_push_properties() && parent_)
      parent_->RemoveDependentNeedsPushProperties();
}

void LayerImpl::GetAllTilesForTracing(std::set<const Tile*>* tiles) const {
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
  state->SetInteger("gpu_memory_usage", GPUMemoryUsageInBytes());

  MathUtil::AddToTracedValue(
      "scroll_offset", scroll_offset_ ? scroll_offset_->Current(IsActive())
                                      : gfx::ScrollOffset(),
      state);

  MathUtil::AddToTracedValue("transform_origin", transform_origin_, state);

  bool clipped;
  gfx::QuadF layer_quad = MathUtil::MapQuad(
      screen_space_transform(),
      gfx::QuadF(gfx::Rect(content_bounds())),
      &clipped);
  MathUtil::AddToTracedValue("layer_quad", layer_quad, state);
  if (!touch_event_handler_region_.IsEmpty()) {
    state->BeginArray("touch_event_handler_region");
    touch_event_handler_region_.AsValueInto(state);
    state->EndArray();
  }
  if (have_wheel_event_handlers_) {
    gfx::Rect wheel_rect(content_bounds());
    Region wheel_region(wheel_rect);
    state->BeginArray("wheel_event_handler_region");
    wheel_region.AsValueInto(state);
    state->EndArray();
  }
  if (have_scroll_event_handlers_) {
    gfx::Rect scroll_rect(content_bounds());
    Region scroll_region(scroll_rect);
    state->BeginArray("scroll_event_handler_region");
    scroll_region.AsValueInto(state);
    state->EndArray();
  }
  if (!non_fast_scrollable_region_.IsEmpty()) {
    state->BeginArray("non_fast_scrollable_region");
    non_fast_scrollable_region_.AsValueInto(state);
    state->EndArray();
  }
  if (scroll_blocks_on_) {
    state->SetInteger("scroll_blocks_on", scroll_blocks_on_);
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

  state->SetBoolean(
      "has_animation_bounds",
      layer_animation_controller()->HasAnimationThatInflatesBounds());

  gfx::BoxF box;
  if (LayerUtils::GetAnimationBounds(*this, &box))
    MathUtil::AddToTracedValue("animation_bounds", box, state);

  if (debug_info_.get()) {
    std::string str;
    debug_info_->AppendAsTraceFormat(&str);
    base::JSONReader json_reader;
    scoped_ptr<base::Value> debug_info_value(json_reader.ReadToValue(str));

    if (debug_info_value->IsType(base::Value::TYPE_DICTIONARY)) {
      base::DictionaryValue* dictionary_value = nullptr;
      bool converted_to_dictionary =
          debug_info_value->GetAsDictionary(&dictionary_value);
      DCHECK(converted_to_dictionary);
      for (base::DictionaryValue::Iterator it(*dictionary_value); !it.IsAtEnd();
           it.Advance()) {
        state->SetValue(it.key().data(), it.value().DeepCopy());
      }
    } else {
      NOTREACHED();
    }
  }

  if (!frame_timing_requests_.empty()) {
    state->BeginArray("frame_timing_requests");
    for (const auto& request : frame_timing_requests_) {
      state->BeginDictionary();
      state->SetInteger("request_id", request.id());
      MathUtil::AddToTracedValue("request_rect", request.rect(), state);
      state->EndDictionary();
    }
    state->EndArray();
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

void LayerImpl::NotifyAnimationFinished(
    base::TimeTicks monotonic_time,
    Animation::TargetProperty target_property,
    int group) {
  if (target_property == Animation::SCROLL_OFFSET)
    layer_tree_impl_->InputScrollAnimationFinished();
}

void LayerImpl::SetHasRenderSurface(bool should_have_render_surface) {
  if (!!render_surface() == should_have_render_surface)
    return;

  SetNeedsPushProperties();
  layer_tree_impl()->set_needs_update_draw_properties();
  if (should_have_render_surface) {
    render_surface_ = make_scoped_ptr(new RenderSurfaceImpl(this));
    return;
  }
  render_surface_.reset();
}

Region LayerImpl::GetInvalidationRegion() {
  return Region(update_rect_);
}

}  // namespace cc
