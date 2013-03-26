// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_impl.h"

#include "base/debug/trace_event.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "cc/animation/animation_registrar.h"
#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/animation/scrollbar_animation_controller_linear_fade.h"
#include "cc/base/math_util.h"
#include "cc/debug/debug_colors.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/layers/quad_sink.h"
#include "cc/layers/scrollbar_layer_impl.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/proxy.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

LayerImpl::LayerImpl(LayerTreeImpl* tree_impl, int id)
    : parent_(NULL),
      mask_layer_id_(-1),
      replica_layer_id_(-1),
      layer_id_(id),
      layer_tree_impl_(tree_impl),
      anchor_point_(0.5f, 0.5f),
      anchor_point_z_(0.f),
      scrollable_(false),
      should_scroll_on_main_thread_(false),
      have_wheel_event_handlers_(false),
      background_color_(0),
      stacking_order_changed_(false),
      double_sided_(true),
      layer_property_changed_(false),
      layer_surface_property_changed_(false),
      masks_to_bounds_(false),
      contents_opaque_(false),
      opacity_(1.0),
      preserves_3d_(false),
      use_parent_backface_visibility_(false),
      draw_checkerboard_for_missing_tiles_(false),
      draws_content_(false),
      force_render_surface_(false),
      is_container_for_fixed_position_layers_(false),
      fixed_to_container_layer_(false),
      draw_depth_(0.f),
#ifndef NDEBUG
      between_will_draw_and_did_draw_(false),
#endif
      horizontal_scrollbar_layer_(NULL),
      vertical_scrollbar_layer_(NULL) {
  DCHECK_GT(layer_id_, 0);
  DCHECK(layer_tree_impl_);
  layer_tree_impl_->RegisterLayer(this);
  AnimationRegistrar* registrar = layer_tree_impl_->animationRegistrar();
  layer_animation_controller_ =
      registrar->GetAnimationControllerForId(layer_id_);
  layer_animation_controller_->AddObserver(this);
}

LayerImpl::~LayerImpl() {
#ifndef NDEBUG
  DCHECK(!between_will_draw_and_did_draw_);
#endif
  layer_tree_impl_->UnregisterLayer(this);
  layer_animation_controller_->RemoveObserver(this);
}

void LayerImpl::AddChild(scoped_ptr<LayerImpl> child) {
  child->set_parent(this);
  DCHECK_EQ(layer_tree_impl(), child->layer_tree_impl());
  children_.push_back(child.Pass());
  layer_tree_impl()->set_needs_update_draw_properties();
}

scoped_ptr<LayerImpl> LayerImpl::RemoveChild(LayerImpl* child) {
  for (ScopedPtrVector<LayerImpl>::iterator it = children_.begin();
       it != children_.end();
       ++it) {
    if (*it == child) {
      scoped_ptr<LayerImpl> ret = children_.take(it);
      children_.erase(it);
      layer_tree_impl()->set_needs_update_draw_properties();
      return ret.Pass();
    }
  }
  return scoped_ptr<LayerImpl>();
}

void LayerImpl::ClearChildList() {
  if (children_.empty())
    return;

  children_.clear();
  layer_tree_impl()->set_needs_update_draw_properties();
}

void LayerImpl::CreateRenderSurface() {
  DCHECK(!draw_properties_.render_surface);
  draw_properties_.render_surface =
      make_scoped_ptr(new RenderSurfaceImpl(this));
  draw_properties_.render_target = this;
}

scoped_ptr<SharedQuadState> LayerImpl::CreateSharedQuadState() const {
  scoped_ptr<SharedQuadState> state = SharedQuadState::Create();
  state->SetAll(draw_properties_.target_space_transform,
                draw_properties_.content_bounds,
                draw_properties_.visible_content_rect,
                draw_properties_.clip_rect,
                draw_properties_.is_clipped,
                draw_properties_.opacity);
  return state.Pass();
}

void LayerImpl::WillDraw(ResourceProvider* resource_provider) {
#ifndef NDEBUG
  // WillDraw/DidDraw must be matched.
  DCHECK(!between_will_draw_and_did_draw_);
  between_will_draw_and_did_draw_ = true;
#endif
}

void LayerImpl::DidDraw(ResourceProvider* resource_provider) {
#ifndef NDEBUG
  DCHECK(between_will_draw_and_did_draw_);
  between_will_draw_and_did_draw_ = false;
#endif
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
    QuadSink* quad_sink,
    const SharedQuadState* shared_quad_state,
    AppendQuadsData* append_quads_data) const {
  if (!ShowDebugBorders())
    return;

  SkColor color;
  float width;
  GetDebugBorderProperties(&color, &width);

  gfx::Rect content_rect(content_bounds());
  scoped_ptr<DebugBorderDrawQuad> debug_border_quad =
      DebugBorderDrawQuad::Create();
  debug_border_quad->SetNew(shared_quad_state, content_rect, color, width);
  quad_sink->Append(debug_border_quad.PassAs<DrawQuad>(), append_quads_data);
}

bool LayerImpl::HasDelegatedContent() const {
  return false;
}

bool LayerImpl::HasContributingDelegatedRenderPasses() const {
  return false;
}

RenderPass::Id LayerImpl::FirstContributingRenderPassId() const {
  return RenderPass::Id(0, 0);
}

RenderPass::Id LayerImpl::NextContributingRenderPassId(RenderPass::Id id)
    const {
  return RenderPass::Id(0, 0);
}

ResourceProvider::ResourceId LayerImpl::ContentsResourceId() const {
  NOTREACHED();
  return 0;
}

void LayerImpl::SetSentScrollDelta(gfx::Vector2d sent_scroll_delta) {
  // Pending tree never has sent scroll deltas
  DCHECK(layer_tree_impl()->IsActiveTree());

  if (sent_scroll_delta_ == sent_scroll_delta)
    return;

  sent_scroll_delta_ = sent_scroll_delta;
}

gfx::Vector2dF LayerImpl::ScrollBy(gfx::Vector2dF scroll) {
  gfx::Vector2dF min_delta = -scroll_offset_;
  gfx::Vector2dF max_delta = max_scroll_offset_ - scroll_offset_;
  // Clamp new_delta so that position + delta stays within scroll bounds.
  gfx::Vector2dF new_delta = (scroll_delta_ + scroll);
  new_delta.ClampToMin(min_delta);
  new_delta.ClampToMax(max_delta);
  gfx::Vector2dF unscrolled = scroll_delta_ + scroll - new_delta;

  SetScrollDelta(new_delta);
  return unscrolled;
}

InputHandlerClient::ScrollStatus LayerImpl::TryScroll(
    gfx::PointF screen_space_point,
    InputHandlerClient::ScrollInputType type) const {
  if (should_scroll_on_main_thread()) {
    TRACE_EVENT0("cc", "LayerImpl::TryScroll: Failed ShouldScrollOnMainThread");
    return InputHandlerClient::ScrollOnMainThread;
  }

  if (!screen_space_transform().IsInvertible()) {
    TRACE_EVENT0("cc", "LayerImpl::TryScroll: Ignored NonInvertibleTransform");
    return InputHandlerClient::ScrollIgnored;
  }

  if (!non_fast_scrollable_region().IsEmpty()) {
    bool clipped = false;
    gfx::Transform inverse_screen_space_transform(
        gfx::Transform::kSkipInitialization);
    if (!screen_space_transform().GetInverse(&inverse_screen_space_transform)) {
      // TODO(shawnsingh): We shouldn't be applying a projection if screen space
      // transform is uninvertible here. Perhaps we should be returning
      // ScrollOnMainThread in this case?
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
      return InputHandlerClient::ScrollOnMainThread;
    }
  }

  if (type == InputHandlerClient::Wheel && have_wheel_event_handlers()) {
    TRACE_EVENT0("cc", "LayerImpl::tryScroll: Failed WheelEventHandlers");
    return InputHandlerClient::ScrollOnMainThread;
  }

  if (!scrollable()) {
    TRACE_EVENT0("cc", "LayerImpl::tryScroll: Ignored not scrollable");
    return InputHandlerClient::ScrollIgnored;
  }

  if (max_scroll_offset_.x() <= 0 && max_scroll_offset_.y() <= 0) {
    TRACE_EVENT0("cc",
                 "LayerImpl::tryScroll: Ignored. Technically scrollable,"
                 " but has no affordance in either direction.");
    return InputHandlerClient::ScrollIgnored;
  }

  return InputHandlerClient::ScrollStarted;
}

bool LayerImpl::DrawCheckerboardForMissingTiles() const {
  return draw_checkerboard_for_missing_tiles_ &&
      !layer_tree_impl()->settings().background_color_instead_of_checkerboard;
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

bool LayerImpl::CanClipSelf() const {
  return false;
}

bool LayerImpl::AreVisibleResourcesReady() const {
  return true;
}

scoped_ptr<LayerImpl> LayerImpl::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return LayerImpl::Create(tree_impl, layer_id_);
}

void LayerImpl::PushPropertiesTo(LayerImpl* layer) {
  layer->SetAnchorPoint(anchor_point_);
  layer->SetAnchorPointZ(anchor_point_z_);
  layer->SetBackgroundColor(background_color_);
  layer->SetBounds(bounds_);
  layer->SetContentBounds(content_bounds());
  layer->SetContentsScale(contents_scale_x(), contents_scale_y());
  layer->SetDebugName(debug_name_);
  layer->SetDoubleSided(double_sided_);
  layer->SetDrawCheckerboardForMissingTiles(
      draw_checkerboard_for_missing_tiles_);
  layer->SetForceRenderSurface(force_render_surface_);
  layer->SetDrawsContent(DrawsContent());
  layer->SetFilters(filters());
  layer->SetFilter(filter());
  layer->SetBackgroundFilters(background_filters());
  layer->SetMasksToBounds(masks_to_bounds_);
  layer->SetShouldScrollOnMainThread(should_scroll_on_main_thread_);
  layer->SetHaveWheelEventHandlers(have_wheel_event_handlers_);
  layer->SetNonFastScrollableRegion(non_fast_scrollable_region_);
  layer->SetTouchEventHandlerRegion(touch_event_handler_region_);
  layer->SetContentsOpaque(contents_opaque_);
  layer->SetOpacity(opacity_);
  layer->SetPosition(position_);
  layer->SetIsContainerForFixedPositionLayers(
      is_container_for_fixed_position_layers_);
  layer->SetFixedToContainerLayer(fixed_to_container_layer_);
  layer->SetPreserves3d(preserves_3d());
  layer->SetUseParentBackfaceVisibility(use_parent_backface_visibility_);
  layer->SetSublayerTransform(sublayer_transform_);
  layer->SetTransform(transform_);

  layer->SetScrollable(scrollable_);
  layer->SetScrollOffset(scroll_offset_);
  layer->SetMaxScrollOffset(max_scroll_offset_);

  // If the main thread commits multiple times before the impl thread actually
  // draws, then damage tracking will become incorrect if we simply clobber the
  // update_rect here. The LayerImpl's update_rect needs to accumulate (i.e.
  // union) any update changes that have occurred on the main thread.
  update_rect_.Union(layer->update_rect());
  layer->set_update_rect(update_rect_);

  layer->SetScrollDelta(layer->scroll_delta() - layer->sent_scroll_delta());
  layer->SetSentScrollDelta(gfx::Vector2d());

  layer->SetStackingOrderChanged(stacking_order_changed_);

  layer_animation_controller_->PushAnimationUpdatesTo(
      layer->layer_animation_controller());

  // Reset any state that should be cleared for the next update.
  stacking_order_changed_ = false;
  update_rect_ = gfx::RectF();
}

std::string LayerImpl::IndentString(int indent) {
  std::string str;
  for (int i = 0; i != indent; ++i)
    str.append("  ");
  return str;
}

void LayerImpl::DumpLayerProperties(std::string* str, int indent) const {
  std::string indent_str = IndentString(indent);
  str->append(indent_str);
  base::StringAppendF(str, "layer ID: %d\n", layer_id_);

  str->append(indent_str);
  base::StringAppendF(
      str, "bounds: %d, %d\n", bounds().width(), bounds().height());

  if (draw_properties_.render_target) {
    str->append(indent_str);
    base::StringAppendF(
        str, "renderTarget: %d\n", draw_properties_.render_target->layer_id_);
  }

  str->append(indent_str);
  base::StringAppendF(str, "position: %f, %f\n", position_.x(), position_.y());

  str->append(indent_str);
  base::StringAppendF(str, "contentsOpaque: %d\n", contents_opaque_);

  str->append(indent_str);
  const gfx::Transform& transform = draw_properties_.target_space_transform;
  base::StringAppendF(str,
                      "drawTransform: %f, %f, %f, %f  //  %f, %f, %f, %f"
                      "  //  %f, %f, %f, %f  //  %f, %f, %f, %f\n",
                      transform.matrix().getDouble(0, 0),
                      transform.matrix().getDouble(0, 1),
                      transform.matrix().getDouble(0, 2),
                      transform.matrix().getDouble(0, 3),
                      transform.matrix().getDouble(1, 0),
                      transform.matrix().getDouble(1, 1),
                      transform.matrix().getDouble(1, 2),
                      transform.matrix().getDouble(1, 3),
                      transform.matrix().getDouble(2, 0),
                      transform.matrix().getDouble(2, 1),
                      transform.matrix().getDouble(2, 2),
                      transform.matrix().getDouble(2, 3),
                      transform.matrix().getDouble(3, 0),
                      transform.matrix().getDouble(3, 1),
                      transform.matrix().getDouble(3, 2),
                      transform.matrix().getDouble(3, 3));

  str->append(indent_str);
  base::StringAppendF(
      str, "draws_content: %s\n", draws_content_ ? "yes" : "no");
}

std::string LayerImpl::LayerTreeAsText() const {
  std::string str;
  DumpLayer(&str, 0);
  return str;
}

void LayerImpl::DumpLayer(std::string* str, int indent) const {
  str->append(IndentString(indent));
  base::StringAppendF(str, "%s(%s)\n", LayerTypeAsString(), debug_name_.data());
  DumpLayerProperties(str, indent+2);
  if (replica_layer_) {
    str->append(IndentString(indent+2));
    str->append("Replica:\n");
    replica_layer_->DumpLayer(str, indent+3);
  }
  if (mask_layer_) {
    str->append(IndentString(indent+2));
    str->append("Mask:\n");
    mask_layer_->DumpLayer(str, indent+3);
  }
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->DumpLayer(str, indent+1);
}

base::DictionaryValue* LayerImpl::LayerTreeAsJson() const {
  base::ListValue* list;
  base::DictionaryValue* result = new base::DictionaryValue;
  result->SetString("LayerType", LayerTypeAsString());

  list = new base::ListValue;
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
  result->SetDouble("Opacity", opacity());

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

bool LayerImpl::LayerSurfacePropertyChanged() const {
  if (layer_surface_property_changed_)
    return true;

  // If this layer's surface property hasn't changed, we want to see if
  // some layer above us has changed this property. This is done for the
  // case when such parent layer does not draw content, and therefore will
  // not be traversed by the damage tracker. We need to make sure that
  // property change on such layer will be caught by its descendants.
  LayerImpl* current = this->parent_;
  while (current && !current->draw_properties_.render_surface) {
    if (current->layer_surface_property_changed_)
      return true;
    current = current->parent_;
  }

  return false;
}

void LayerImpl::NoteLayerSurfacePropertyChanged() {
  layer_surface_property_changed_ = true;
  layer_tree_impl()->set_needs_update_draw_properties();
}

void LayerImpl::NoteLayerPropertyChanged() {
  layer_property_changed_ = true;
  layer_tree_impl()->set_needs_update_draw_properties();
}

void LayerImpl::NoteLayerPropertyChangedForSubtree() {
  NoteLayerPropertyChanged();
  NoteLayerPropertyChangedForDescendants();
}

void LayerImpl::NoteLayerPropertyChangedForDescendants() {
  layer_tree_impl()->set_needs_update_draw_properties();
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->NoteLayerPropertyChangedForSubtree();
}

const char* LayerImpl::LayerTypeAsString() const {
  return "Layer";
}

void LayerImpl::ResetAllChangeTrackingForSubtree() {
  layer_property_changed_ = false;
  layer_surface_property_changed_ = false;

  update_rect_ = gfx::RectF();

  if (draw_properties_.render_surface)
    draw_properties_.render_surface->ResetPropertyChangedFlag();

  if (mask_layer_)
    mask_layer_->ResetAllChangeTrackingForSubtree();

  if (replica_layer_) {
    // This also resets the replica mask, if it exists.
    replica_layer_->ResetAllChangeTrackingForSubtree();
  }

  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->ResetAllChangeTrackingForSubtree();
}

bool LayerImpl::LayerIsAlwaysDamaged() const {
  return false;
}

void LayerImpl::OnOpacityAnimated(float opacity) {
  SetOpacity(opacity);
}

void LayerImpl::OnTransformAnimated(const gfx::Transform& transform) {
  SetTransform(transform);
}

bool LayerImpl::IsActive() const {
  return layer_tree_impl_->IsActiveTree();
}

void LayerImpl::SetBounds(gfx::Size bounds) {
  if (bounds_ == bounds)
    return;

  bounds_ = bounds;

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
    mask_layer_->set_parent(this);
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
    replica_layer_->set_parent(this);
  NoteLayerPropertyChangedForSubtree();
}

scoped_ptr<LayerImpl> LayerImpl::TakeReplicaLayer() {
  replica_layer_id_ = -1;
  return replica_layer_.Pass();
}

ScrollbarLayerImpl* LayerImpl::ToScrollbarLayer() {
  return NULL;
}

void LayerImpl::SetDrawsContent(bool draws_content) {
  if (draws_content_ == draws_content)
    return;

  draws_content_ = draws_content;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetAnchorPoint(gfx::PointF anchor_point) {
  if (anchor_point_ == anchor_point)
    return;

  anchor_point_ = anchor_point;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetAnchorPointZ(float anchor_point_z) {
  if (anchor_point_z_ == anchor_point_z)
    return;

  anchor_point_z_ = anchor_point_z;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetBackgroundColor(SkColor background_color) {
  if (background_color_ == background_color)
    return;

  background_color_ = background_color;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetFilters(const WebKit::WebFilterOperations& filters) {
  if (filters_ == filters)
    return;

  DCHECK(!filter_);
  filters_ = filters;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetBackgroundFilters(
    const WebKit::WebFilterOperations& filters) {
  if (background_filters_ == filters)
    return;

  background_filters_ = filters;
  NoteLayerPropertyChanged();
}

void LayerImpl::SetFilter(const skia::RefPtr<SkImageFilter>& filter) {
  if (filter_.get() == filter.get())
    return;

  DCHECK(filters_.isEmpty());
  filter_ = filter;
  NoteLayerPropertyChangedForSubtree();
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
  NoteLayerSurfacePropertyChanged();
}

bool LayerImpl::OpacityIsAnimating() const {
  return layer_animation_controller_->IsAnimatingProperty(Animation::Opacity);
}

bool LayerImpl::OpacityIsAnimatingOnImplOnly() const {
  Animation* opacity_animation =
      layer_animation_controller_->GetAnimation(Animation::Opacity);
  return opacity_animation && opacity_animation->is_impl_only();
}

void LayerImpl::SetPosition(gfx::PointF position) {
  if (position_ == position)
    return;

  position_ = position;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetPreserves3d(bool preserves3_d) {
  if (preserves_3d_ == preserves3_d)
    return;

  preserves_3d_ = preserves3_d;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetSublayerTransform(const gfx::Transform& sublayer_transform) {
  if (sublayer_transform_ == sublayer_transform)
    return;

  sublayer_transform_ = sublayer_transform;
  // Sublayer transform does not affect the current layer; it affects only its
  // children.
  NoteLayerPropertyChangedForDescendants();
}

void LayerImpl::SetTransform(const gfx::Transform& transform) {
  if (transform_ == transform)
    return;

  transform_ = transform;
  NoteLayerSurfacePropertyChanged();
}

bool LayerImpl::TransformIsAnimating() const {
  return layer_animation_controller_->IsAnimatingProperty(Animation::Transform);
}

bool LayerImpl::TransformIsAnimatingOnImplOnly() const {
  Animation* transform_animation =
      layer_animation_controller_->GetAnimation(Animation::Transform);
  return transform_animation && transform_animation->is_impl_only();
}

void LayerImpl::SetContentBounds(gfx::Size content_bounds) {
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

void LayerImpl::CalculateContentsScale(
    float ideal_contents_scale,
    bool animating_transform_to_screen,
    float* contents_scale_x,
    float* contents_scale_y,
    gfx::Size* content_bounds) {
  // Base LayerImpl has all of its content scales and content bounds pushed
  // from its Layer during commit and just reuses those values as-is.
  *contents_scale_x = this->contents_scale_x();
  *contents_scale_y = this->contents_scale_y();
  *content_bounds = this->content_bounds();
}

void LayerImpl::UpdateScrollbarPositions() {
  gfx::Vector2dF current_offset = scroll_offset_ + scroll_delta_;

  gfx::RectF viewport(PointAtOffsetFromOrigin(current_offset), bounds_);
  gfx::SizeF scrollable_size(max_scroll_offset_.x() + bounds_.width(),
                             max_scroll_offset_.y() + bounds_.height());
  if (horizontal_scrollbar_layer_) {
    horizontal_scrollbar_layer_->SetCurrentPos(current_offset.x());
    horizontal_scrollbar_layer_->SetTotalSize(bounds_.width());
    horizontal_scrollbar_layer_->SetMaximum(max_scroll_offset_.x());
    horizontal_scrollbar_layer_->SetViewportWithinScrollableArea(
        viewport, scrollable_size);
  }
  if (vertical_scrollbar_layer_) {
    vertical_scrollbar_layer_->SetCurrentPos(current_offset.y());
    vertical_scrollbar_layer_->SetTotalSize(bounds_.height());
    vertical_scrollbar_layer_->SetMaximum(max_scroll_offset_.y());
    vertical_scrollbar_layer_->SetViewportWithinScrollableArea(
        viewport, scrollable_size);
  }

  if (current_offset == last_scroll_offset_)
    return;
  last_scroll_offset_ = current_offset;

  if (scrollbar_animation_controller_ &&
      !scrollbar_animation_controller_->IsScrollGestureInProgress()) {
    scrollbar_animation_controller_->DidProgrammaticallyUpdateScroll(
        base::TimeTicks::Now());
  }

  // Get the current_offset_.y() value for a sanity-check on scrolling
  // benchmark metrics. Specifically, we want to make sure
  // BasicMouseWheelSmoothScrollGesture has proper scroll curves.
  if (layer_tree_impl()->IsActiveTree()) {
    TRACE_COUNTER_ID1("gpu", "scroll_offset_y", this->id(), current_offset.y());
  }
}

void LayerImpl::SetScrollOffset(gfx::Vector2d scroll_offset) {
  if (scroll_offset_ == scroll_offset)
    return;

  scroll_offset_ = scroll_offset;
  NoteLayerPropertyChangedForSubtree();
  UpdateScrollbarPositions();
}

void LayerImpl::SetScrollDelta(gfx::Vector2dF scroll_delta) {
  if (scroll_delta_ == scroll_delta)
    return;

  if (layer_tree_impl()->IsActiveTree()) {
    LayerImpl* pending_twin = layer_tree_impl()->FindPendingTreeLayerById(id());
    if (pending_twin) {
      // The pending twin can't mirror the scroll delta of the active
      // layer.  Although the delta - sent scroll delta difference is
      // identical for both twins, the sent scroll delta for the pending
      // layer is zero, as anything that has been sent has been baked
      // into the layer's position/scroll offset as a part of commit.
      DCHECK(pending_twin->sent_scroll_delta().IsZero());
      pending_twin->SetScrollDelta(scroll_delta - sent_scroll_delta());
    }
  }

  scroll_delta_ = scroll_delta;
  NoteLayerPropertyChangedForSubtree();

  UpdateScrollbarPositions();
}

gfx::Vector2dF LayerImpl::TotalScrollOffset() const {
  return scroll_offset_ + scroll_delta_;
}

void LayerImpl::SetImplTransform(const gfx::Transform& transform) {
  if (impl_transform_ == transform)
    return;

  impl_transform_ = transform;
  NoteLayerPropertyChangedForSubtree();
}

void LayerImpl::SetDoubleSided(bool double_sided) {
  if (double_sided_ == double_sided)
    return;

  double_sided_ = double_sided;
  NoteLayerPropertyChangedForSubtree();
}

Region LayerImpl::VisibleContentOpaqueRegion() const {
  if (contents_opaque())
    return visible_content_rect();
  return Region();
}

void LayerImpl::DidLoseOutputSurface() {}

void LayerImpl::SetMaxScrollOffset(gfx::Vector2d max_scroll_offset) {
  if (max_scroll_offset_ == max_scroll_offset)
    return;
  max_scroll_offset_ = max_scroll_offset;

  layer_tree_impl()->set_needs_update_draw_properties();
  UpdateScrollbarPositions();
}

void LayerImpl::SetScrollbarOpacity(float opacity) {
  if (horizontal_scrollbar_layer_)
    horizontal_scrollbar_layer_->SetOpacity(opacity);
  if (vertical_scrollbar_layer_)
    vertical_scrollbar_layer_->SetOpacity(opacity);
}

inline scoped_ptr<ScrollbarAnimationController>
CreateScrollbarAnimationControllerWithFade(LayerImpl* layer) {
  base::TimeDelta fadeout_delay = base::TimeDelta::FromMilliseconds(300);
  base::TimeDelta fadeout_length = base::TimeDelta::FromMilliseconds(300);
  return ScrollbarAnimationControllerLinearFade::Create(
      layer, fadeout_delay, fadeout_length)
      .PassAs<ScrollbarAnimationController>();
}

void LayerImpl::DidBecomeActive() {
  if (!layer_tree_impl_->settings().use_linear_fade_scrollbar_animator)
    return;

  bool need_scrollbar_animation_controller = horizontal_scrollbar_layer_ ||
                                             vertical_scrollbar_layer_;
  if (need_scrollbar_animation_controller) {
    if (!scrollbar_animation_controller_) {
      scrollbar_animation_controller_ =
          CreateScrollbarAnimationControllerWithFade(this);
    }
  } else {
    scrollbar_animation_controller_.reset();
  }
}
void LayerImpl::SetHorizontalScrollbarLayer(
    ScrollbarLayerImpl* scrollbar_layer) {
  horizontal_scrollbar_layer_ = scrollbar_layer;
  if (horizontal_scrollbar_layer_)
    horizontal_scrollbar_layer_->set_scroll_layer_id(id());
}

void LayerImpl::SetVerticalScrollbarLayer(ScrollbarLayerImpl* scrollbar_layer) {
  vertical_scrollbar_layer_ = scrollbar_layer;
  if (vertical_scrollbar_layer_)
    vertical_scrollbar_layer_->set_scroll_layer_id(id());
}

void LayerImpl::AsValueInto(base::DictionaryValue* dict) const {
  dict->SetInteger("id", id());
  dict->Set("bounds", MathUtil::AsValue(bounds()).release());
  dict->SetInteger("draws_content", DrawsContent());

  bool clipped;
  gfx::QuadF layer_quad = MathUtil::MapQuad(
      screen_space_transform(),
      gfx::QuadF(gfx::Rect(content_bounds())),
      &clipped);
  dict->Set("layer_quad", MathUtil::AsValue(layer_quad).release());
}

scoped_ptr<base::Value> LayerImpl::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  AsValueInto(state.get());
  return state.PassAs<base::Value>();
}

}  // namespace cc
