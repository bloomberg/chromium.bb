// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer.h"

#include <algorithm>

#include "base/metrics/histogram.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_events.h"
#include "cc/animation/layer_animation_controller.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAnimationDelegate.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerScrollClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

static int s_next_layer_id = 1;

scoped_refptr<Layer> Layer::Create() {
  return make_scoped_refptr(new Layer());
}

Layer::Layer()
    : needs_display_(false),
      stacking_order_changed_(false),
      layer_id_(s_next_layer_id++),
      ignore_set_needs_commit_(false),
      parent_(NULL),
      layer_tree_host_(NULL),
      scrollable_(false),
      should_scroll_on_main_thread_(false),
      have_wheel_event_handlers_(false),
      anchor_point_(0.5f, 0.5f),
      background_color_(0),
      opacity_(1.f),
      anchor_point_z_(0.f),
      is_container_for_fixed_position_layers_(false),
      fixed_to_container_layer_(false),
      is_drawable_(false),
      masks_to_bounds_(false),
      contents_opaque_(false),
      double_sided_(true),
      preserves_3d_(false),
      use_parent_backface_visibility_(false),
      draw_checkerboard_for_missing_tiles_(false),
      force_render_surface_(false),
      replica_layer_(NULL),
      raster_scale_(1.f),
      automatically_compute_raster_scale_(false),
      bounds_contain_page_scale_(false),
      layer_animation_delegate_(NULL),
      layer_scroll_client_(NULL) {
  if (layer_id_ < 0) {
    s_next_layer_id = 1;
    layer_id_ = s_next_layer_id++;
  }

  layer_animation_controller_ = LayerAnimationController::Create(layer_id_);
  layer_animation_controller_->AddObserver(this);
  AddLayerAnimationEventObserver(layer_animation_controller_.get());
}

Layer::~Layer() {
  // Our parent should be holding a reference to us so there should be no
  // way for us to be destroyed while we still have a parent.
  DCHECK(!parent());

  layer_animation_controller_->RemoveObserver(this);

  // Remove the parent reference from all children and dependents.
  RemoveAllChildren();
  if (mask_layer_) {
    DCHECK_EQ(this, mask_layer_->parent());
    mask_layer_->RemoveFromParent();
  }
  if (replica_layer_) {
    DCHECK_EQ(this, replica_layer_->parent());
    replica_layer_->RemoveFromParent();
  }
}

void Layer::SetLayerTreeHost(LayerTreeHost* host) {
  if (layer_tree_host_ == host)
    return;

  layer_tree_host_ = host;

  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->SetLayerTreeHost(host);

  if (mask_layer_)
    mask_layer_->SetLayerTreeHost(host);
  if (replica_layer_)
    replica_layer_->SetLayerTreeHost(host);

  layer_animation_controller_->SetAnimationRegistrar(
      host ? host->animation_registrar() : NULL);

  if (host && layer_animation_controller_->has_any_animation())
    host->SetNeedsCommit();
  if (host &&
      (!filters_.isEmpty() || !background_filters_.isEmpty() || filter_))
    layer_tree_host_->set_needs_filter_context();
}

void Layer::SetNeedsCommit() {
  if (ignore_set_needs_commit_)
    return;
  if (layer_tree_host_)
    layer_tree_host_->SetNeedsCommit();
}

void Layer::SetNeedsFullTreeSync() {
  if (layer_tree_host_)
    layer_tree_host_->SetNeedsFullTreeSync();
}

gfx::Rect Layer::LayerRectToContentRect(const gfx::RectF& layer_rect) const {
  gfx::RectF content_rect =
      gfx::ScaleRect(layer_rect, contents_scale_x(), contents_scale_y());
  // Intersect with content rect to avoid the extra pixel because for some
  // values x and y, ceil((x / y) * y) may be x + 1.
  content_rect.Intersect(gfx::Rect(content_bounds()));
  return gfx::ToEnclosingRect(content_rect);
}

bool Layer::BlocksPendingCommit() const {
  return false;
}

bool Layer::CanClipSelf() const {
  return false;
}

bool Layer::BlocksPendingCommitRecursive() const {
  if (BlocksPendingCommit())
    return true;
  if (mask_layer() && mask_layer()->BlocksPendingCommitRecursive())
    return true;
  if (replica_layer() && replica_layer()->BlocksPendingCommitRecursive())
    return true;
  for (size_t i = 0; i < children_.size(); ++i) {
    if (children_[i]->BlocksPendingCommitRecursive())
      return true;
  }
  return false;
}

void Layer::SetParent(Layer* layer) {
  DCHECK(!layer || !layer->HasAncestor(this));
  parent_ = layer;
  SetLayerTreeHost(parent_ ? parent_->layer_tree_host() : NULL);

  ForceAutomaticRasterScaleToBeRecomputed();
  if (mask_layer_)
    mask_layer_->ForceAutomaticRasterScaleToBeRecomputed();
  if (replica_layer_ && replica_layer_->mask_layer_)
    replica_layer_->mask_layer_->ForceAutomaticRasterScaleToBeRecomputed();
}

bool Layer::HasAncestor(Layer* ancestor) const {
  for (const Layer* layer = parent(); layer; layer = layer->parent()) {
    if (layer == ancestor)
      return true;
  }
  return false;
}

void Layer::AddChild(scoped_refptr<Layer> child) {
  InsertChild(child, children_.size());
}

void Layer::InsertChild(scoped_refptr<Layer> child, size_t index) {
  child->RemoveFromParent();
  child->SetParent(this);
  child->stacking_order_changed_ = true;

  index = std::min(index, children_.size());
  children_.insert(children_.begin() + index, child);
  SetNeedsFullTreeSync();
}

void Layer::RemoveFromParent() {
  if (parent_)
    parent_->RemoveChildOrDependent(this);
}

void Layer::RemoveChildOrDependent(Layer* child) {
  if (mask_layer_ == child) {
    mask_layer_->SetParent(NULL);
    mask_layer_ = NULL;
    SetNeedsFullTreeSync();
    return;
  }
  if (replica_layer_ == child) {
    replica_layer_->SetParent(NULL);
    replica_layer_ = NULL;
    SetNeedsFullTreeSync();
    return;
  }

  for (LayerList::iterator iter = children_.begin();
       iter != children_.end();
       ++iter) {
    if (*iter != child)
      continue;

    child->SetParent(NULL);
    children_.erase(iter);
    SetNeedsFullTreeSync();
    return;
  }
}

void Layer::ReplaceChild(Layer* reference, scoped_refptr<Layer> new_layer) {
  DCHECK(reference);
  DCHECK_EQ(reference->parent(), this);

  if (reference == new_layer)
    return;

  int reference_index = IndexOfChild(reference);
  if (reference_index == -1) {
    NOTREACHED();
    return;
  }

  reference->RemoveFromParent();

  if (new_layer) {
    new_layer->RemoveFromParent();
    InsertChild(new_layer, reference_index);
  }
}

int Layer::IndexOfChild(const Layer* reference) {
  for (size_t i = 0; i < children_.size(); ++i) {
    if (children_[i] == reference)
      return i;
  }
  return -1;
}

void Layer::SetBounds(gfx::Size size) {
  if (bounds() == size)
    return;

  bool first_resize = bounds().IsEmpty() && !size.IsEmpty();

  bounds_ = size;

  if (first_resize)
    SetNeedsDisplay();
  else
    SetNeedsCommit();
}

Layer* Layer::RootLayer() {
  Layer* layer = this;
  while (layer->parent())
    layer = layer->parent();
  return layer;
}

void Layer::RemoveAllChildren() {
  while (children_.size()) {
    Layer* layer = children_[0].get();
    DCHECK_EQ(this, layer->parent());
    layer->RemoveFromParent();
  }
}

void Layer::SetChildren(const LayerList& children) {
  if (children == children_)
    return;

  RemoveAllChildren();
  for (size_t i = 0; i < children.size(); ++i)
    AddChild(children[i]);
}

void Layer::SetAnchorPoint(gfx::PointF anchor_point) {
  if (anchor_point_ == anchor_point)
    return;
  anchor_point_ = anchor_point;
  SetNeedsCommit();
}

void Layer::SetAnchorPointZ(float anchor_point_z) {
  if (anchor_point_z_ == anchor_point_z)
    return;
  anchor_point_z_ = anchor_point_z;
  SetNeedsCommit();
}

void Layer::SetBackgroundColor(SkColor background_color) {
  if (background_color_ == background_color)
    return;
  background_color_ = background_color;
  SetNeedsCommit();
}

void Layer::CalculateContentsScale(
    float ideal_contents_scale,
    bool animating_transform_to_screen,
    float* contents_scale_x,
    float* contents_scale_y,
    gfx::Size* content_bounds) {
  *contents_scale_x = 1;
  *contents_scale_y = 1;
  *content_bounds = bounds();
}

void Layer::SetMasksToBounds(bool masks_to_bounds) {
  if (masks_to_bounds_ == masks_to_bounds)
    return;
  masks_to_bounds_ = masks_to_bounds;
  SetNeedsCommit();
}

void Layer::SetMaskLayer(Layer* mask_layer) {
  if (mask_layer_ == mask_layer)
    return;
  if (mask_layer_) {
    DCHECK_EQ(this, mask_layer_->parent());
    mask_layer_->RemoveFromParent();
  }
  mask_layer_ = mask_layer;
  if (mask_layer_) {
    DCHECK(!mask_layer_->parent());
    mask_layer_->RemoveFromParent();
    mask_layer_->SetParent(this);
    mask_layer_->SetIsMask(true);
  }
  SetNeedsFullTreeSync();
}

void Layer::SetReplicaLayer(Layer* layer) {
  if (replica_layer_ == layer)
    return;
  if (replica_layer_) {
    DCHECK_EQ(this, replica_layer_->parent());
    replica_layer_->RemoveFromParent();
  }
  replica_layer_ = layer;
  if (replica_layer_) {
    DCHECK(!replica_layer_->parent());
    replica_layer_->RemoveFromParent();
    replica_layer_->SetParent(this);
  }
  SetNeedsFullTreeSync();
}

void Layer::SetFilters(const WebKit::WebFilterOperations& filters) {
  if (filters_ == filters)
    return;
  DCHECK(!filter_);
  filters_ = filters;
  SetNeedsCommit();
  if (!filters.isEmpty() && layer_tree_host_)
    layer_tree_host_->set_needs_filter_context();
}

void Layer::SetFilter(const skia::RefPtr<SkImageFilter>& filter) {
  if (filter_.get() == filter.get())
    return;
  DCHECK(filters_.isEmpty());
  filter_ = filter;
  SetNeedsCommit();
  if (filter && layer_tree_host_)
    layer_tree_host_->set_needs_filter_context();
}

void Layer::SetBackgroundFilters(const WebKit::WebFilterOperations& filters) {
  if (background_filters_ == filters)
    return;
  background_filters_ = filters;
  SetNeedsCommit();
  if (!filters.isEmpty() && layer_tree_host_)
    layer_tree_host_->set_needs_filter_context();
}

void Layer::SetOpacity(float opacity) {
  if (opacity_ == opacity)
    return;
  opacity_ = opacity;
  SetNeedsCommit();
}

bool Layer::OpacityIsAnimating() const {
  return layer_animation_controller_->IsAnimatingProperty(Animation::Opacity);
}

bool Layer::OpacityCanAnimateOnImplThread() const {
  return false;
}

void Layer::SetContentsOpaque(bool opaque) {
  if (contents_opaque_ == opaque)
    return;
  contents_opaque_ = opaque;
  SetNeedsCommit();
}

void Layer::SetPosition(gfx::PointF position) {
  if (position_ == position)
    return;
  position_ = position;
  SetNeedsCommit();
}

void Layer::SetSublayerTransform(const gfx::Transform& sublayer_transform) {
  if (sublayer_transform_ == sublayer_transform)
    return;
  sublayer_transform_ = sublayer_transform;
  SetNeedsCommit();
}

void Layer::SetTransform(const gfx::Transform& transform) {
  if (transform_ == transform)
    return;
  transform_ = transform;
  SetNeedsCommit();
}

bool Layer::TransformIsAnimating() const {
  return layer_animation_controller_->IsAnimatingProperty(Animation::Transform);
}

void Layer::SetScrollOffset(gfx::Vector2d scroll_offset) {
  if (scroll_offset_ == scroll_offset)
    return;
  scroll_offset_ = scroll_offset;
  if (layer_scroll_client_)
    layer_scroll_client_->didScroll();
  SetNeedsCommit();
}

void Layer::SetMaxScrollOffset(gfx::Vector2d max_scroll_offset) {
  if (max_scroll_offset_ == max_scroll_offset)
    return;
  max_scroll_offset_ = max_scroll_offset;
  SetNeedsCommit();
}

void Layer::SetScrollable(bool scrollable) {
  if (scrollable_ == scrollable)
    return;
  scrollable_ = scrollable;
  SetNeedsCommit();
}

void Layer::SetShouldScrollOnMainThread(bool should_scroll_on_main_thread) {
  if (should_scroll_on_main_thread_ == should_scroll_on_main_thread)
    return;
  should_scroll_on_main_thread_ = should_scroll_on_main_thread;
  SetNeedsCommit();
}

void Layer::SetHaveWheelEventHandlers(bool have_wheel_event_handlers) {
  if (have_wheel_event_handlers_ == have_wheel_event_handlers)
    return;
  have_wheel_event_handlers_ = have_wheel_event_handlers;
  SetNeedsCommit();
}

void Layer::SetNonFastScrollableRegion(const Region& region) {
  if (non_fast_scrollable_region_ == region)
    return;
  non_fast_scrollable_region_ = region;
  SetNeedsCommit();
}

void Layer::SetTouchEventHandlerRegion(const Region& region) {
  if (touch_event_handler_region_ == region)
    return;
  touch_event_handler_region_ = region;
}

void Layer::SetDrawCheckerboardForMissingTiles(bool checkerboard) {
  if (draw_checkerboard_for_missing_tiles_ == checkerboard)
    return;
  draw_checkerboard_for_missing_tiles_ = checkerboard;
  SetNeedsCommit();
}

void Layer::SetForceRenderSurface(bool force) {
  if (force_render_surface_ == force)
    return;
  force_render_surface_ = force;
  SetNeedsCommit();
}

void Layer::SetImplTransform(const gfx::Transform& transform) {
  if (impl_transform_ == transform)
    return;
  impl_transform_ = transform;
  SetNeedsCommit();
}

void Layer::SetDoubleSided(bool double_sided) {
  if (double_sided_ == double_sided)
    return;
  double_sided_ = double_sided;
  SetNeedsCommit();
}

void Layer::SetIsDrawable(bool is_drawable) {
  if (is_drawable_ == is_drawable)
    return;

  is_drawable_ = is_drawable;
  SetNeedsCommit();
}

void Layer::SetNeedsDisplayRect(const gfx::RectF& dirty_rect) {
  update_rect_.Union(dirty_rect);
  needs_display_ = true;

  // Simply mark the contents as dirty. For non-root layers, the call to
  // SetNeedsCommit will schedule a fresh compositing pass.
  // For the root layer, SetNeedsCommit has no effect.
  if (DrawsContent() && !update_rect_.IsEmpty())
    SetNeedsCommit();
}

bool Layer::DescendantIsFixedToContainerLayer() const {
  for (size_t i = 0; i < children_.size(); ++i) {
    if (children_[i]->fixed_to_container_layer() ||
        children_[i]->DescendantIsFixedToContainerLayer())
      return true;
  }
  return false;
}

void Layer::SetIsContainerForFixedPositionLayers(bool container) {
  if (is_container_for_fixed_position_layers_ == container)
    return;
  is_container_for_fixed_position_layers_ = container;

  if (layer_tree_host_ && layer_tree_host_->CommitRequested())
    return;

  // Only request a commit if we have a fixed positioned descendant.
  if (DescendantIsFixedToContainerLayer())
    SetNeedsCommit();
}

void Layer::SetFixedToContainerLayer(bool fixed_to_container_layer) {
  if (fixed_to_container_layer_ == fixed_to_container_layer)
    return;
  fixed_to_container_layer_ = fixed_to_container_layer;
  SetNeedsCommit();
}

void Layer::PushPropertiesTo(LayerImpl* layer) {
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
  if (!layer->OpacityIsAnimatingOnImplOnly() && !OpacityIsAnimating())
    layer->SetOpacity(opacity_);
  DCHECK(!(OpacityIsAnimating() && layer->OpacityIsAnimatingOnImplOnly()));
  layer->SetPosition(position_);
  layer->SetIsContainerForFixedPositionLayers(
      is_container_for_fixed_position_layers_);
  layer->SetFixedToContainerLayer(fixed_to_container_layer_);
  layer->SetPreserves3d(preserves_3d());
  layer->SetUseParentBackfaceVisibility(use_parent_backface_visibility_);
  layer->SetSublayerTransform(sublayer_transform_);
  if (!layer->TransformIsAnimatingOnImplOnly() && !TransformIsAnimating())
    layer->SetTransform(transform_);
  DCHECK(!(TransformIsAnimating() && layer->TransformIsAnimatingOnImplOnly()));

  layer->SetScrollable(scrollable_);
  layer->SetScrollOffset(scroll_offset_);
  layer->SetMaxScrollOffset(max_scroll_offset_);

  // If the main thread commits multiple times before the impl thread actually
  // draws, then damage tracking will become incorrect if we simply clobber the
  // update_rect here. The LayerImpl's update_rect needs to accumulate (i.e.
  // union) any update changes that have occurred on the main thread.
  update_rect_.Union(layer->update_rect());
  layer->set_update_rect(update_rect_);

  if (layer->layer_tree_impl()->settings().impl_side_painting) {
    DCHECK(layer->layer_tree_impl()->IsPendingTree());
    LayerImpl* active_twin =
        layer->layer_tree_impl()->FindActiveTreeLayerById(id());
    // Update the scroll delta from the active layer, which may have
    // adjusted its scroll delta prior to this pending layer being created.
    // This code is identical to that in LayerImpl::SetScrollDelta.
    if (active_twin) {
      DCHECK(layer->sent_scroll_delta().IsZero());
      layer->SetScrollDelta(active_twin->scroll_delta() -
                            active_twin->sent_scroll_delta());
    }
  } else {
    layer->SetScrollDelta(layer->scroll_delta() - layer->sent_scroll_delta());
    layer->SetSentScrollDelta(gfx::Vector2d());
  }

  layer->SetStackingOrderChanged(stacking_order_changed_);

  layer_animation_controller_->PushAnimationUpdatesTo(
      layer->layer_animation_controller());

  // Reset any state that should be cleared for the next update.
  stacking_order_changed_ = false;
  update_rect_ = gfx::RectF();
}

scoped_ptr<LayerImpl> Layer::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return LayerImpl::Create(tree_impl, layer_id_);
}

bool Layer::DrawsContent() const {
  return is_drawable_;
}

bool Layer::NeedMoreUpdates() {
  return false;
}

void Layer::SetDebugName(const std::string& debug_name) {
  debug_name_ = debug_name;
  SetNeedsCommit();
}

void Layer::SetRasterScale(float scale) {
  if (raster_scale_ == scale)
    return;
  raster_scale_ = scale;

  // When automatically computed, this acts like a draw property.
  if (automatically_compute_raster_scale_)
    return;
  SetNeedsDisplay();
}

void Layer::SetAutomaticallyComputeRasterScale(bool automatic) {
  if (automatically_compute_raster_scale_ == automatic)
    return;
  automatically_compute_raster_scale_ = automatic;

  if (automatically_compute_raster_scale_)
    ForceAutomaticRasterScaleToBeRecomputed();
  else
    SetRasterScale(1);
}

void Layer::ForceAutomaticRasterScaleToBeRecomputed() {
  if (!automatically_compute_raster_scale_)
    return;
  if (!raster_scale_)
    return;
  raster_scale_ = 0.f;
  SetNeedsCommit();
}

void Layer::SetBoundsContainPageScale(bool bounds_contain_page_scale) {
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->SetBoundsContainPageScale(bounds_contain_page_scale);

  if (bounds_contain_page_scale == bounds_contain_page_scale_)
    return;

  bounds_contain_page_scale_ = bounds_contain_page_scale;
  SetNeedsDisplay();
}

void Layer::CreateRenderSurface() {
  DCHECK(!draw_properties_.render_surface);
  draw_properties_.render_surface = make_scoped_ptr(new RenderSurface(this));
  draw_properties_.render_target = this;
}

void Layer::OnOpacityAnimated(float opacity) {
  // This is called due to an ongoing accelerated animation. Since this
  // animation is also being run on the impl thread, there is no need to request
  // a commit to push this value over, so set the value directly rather than
  // calling SetOpacity.
  opacity_ = opacity;
}

void Layer::OnTransformAnimated(const gfx::Transform& transform) {
  // This is called due to an ongoing accelerated animation. Since this
  // animation is also being run on the impl thread, there is no need to request
  // a commit to push this value over, so set this value directly rather than
  // calling SetTransform.
  transform_ = transform;
}

bool Layer::IsActive() const {
  return true;
}

bool Layer::AddAnimation(scoped_ptr <Animation> animation) {
  if (!layer_animation_controller_->animation_registrar())
    return false;

  UMA_HISTOGRAM_BOOLEAN("Renderer.AnimationAddedToOrphanLayer",
                        !layer_tree_host_);
  layer_animation_controller_->AddAnimation(animation.Pass());
  SetNeedsCommit();
  return true;
}

void Layer::PauseAnimation(int animation_id, double time_offset) {
  layer_animation_controller_->PauseAnimation(animation_id, time_offset);
  SetNeedsCommit();
}

void Layer::RemoveAnimation(int animation_id) {
  layer_animation_controller_->RemoveAnimation(animation_id);
  SetNeedsCommit();
}

void Layer::SuspendAnimations(double monotonic_time) {
  layer_animation_controller_->SuspendAnimations(monotonic_time);
  SetNeedsCommit();
}

void Layer::ResumeAnimations(double monotonic_time) {
  layer_animation_controller_->ResumeAnimations(monotonic_time);
  SetNeedsCommit();
}

void Layer::SetLayerAnimationController(
    scoped_refptr<LayerAnimationController> controller) {
  RemoveLayerAnimationEventObserver(layer_animation_controller_.get());
  layer_animation_controller_->RemoveObserver(this);
  layer_animation_controller_ = controller;
  layer_animation_controller_->set_force_sync();
  layer_animation_controller_->AddObserver(this);
  AddLayerAnimationEventObserver(layer_animation_controller_.get());
  SetNeedsCommit();
}

scoped_refptr<LayerAnimationController>
Layer::ReleaseLayerAnimationController() {
  layer_animation_controller_->RemoveObserver(this);
  scoped_refptr<LayerAnimationController> to_return =
      layer_animation_controller_;
  layer_animation_controller_ = LayerAnimationController::Create(id());
  layer_animation_controller_->AddObserver(this);
  layer_animation_controller_->SetAnimationRegistrar(
      to_return->animation_registrar());
  return to_return;
}

bool Layer::HasActiveAnimation() const {
  return layer_animation_controller_->HasActiveAnimation();
}

void Layer::NotifyAnimationStarted(const AnimationEvent& event,
                                   double wall_clock_time) {
  FOR_EACH_OBSERVER(LayerAnimationEventObserver, layer_animation_observers_,
                    OnAnimationStarted(event));
  if (layer_animation_delegate_)
    layer_animation_delegate_->notifyAnimationStarted(wall_clock_time);
}

void Layer::NotifyAnimationFinished(double wall_clock_time) {
  if (layer_animation_delegate_)
    layer_animation_delegate_->notifyAnimationFinished(wall_clock_time);
}

void Layer::NotifyAnimationPropertyUpdate(const AnimationEvent& event) {
  if (event.target_property == Animation::Opacity)
    SetOpacity(event.opacity);
  else
    SetTransform(event.transform);
}

void Layer::AddLayerAnimationEventObserver(
    LayerAnimationEventObserver* animation_observer) {
  if (!layer_animation_observers_.HasObserver(animation_observer))
    layer_animation_observers_.AddObserver(animation_observer);
}

void Layer::RemoveLayerAnimationEventObserver(
    LayerAnimationEventObserver* animation_observer) {
  layer_animation_observers_.RemoveObserver(animation_observer);
}

Region Layer::VisibleContentOpaqueRegion() const {
  if (contents_opaque())
    return visible_content_rect();
  return Region();
}

ScrollbarLayer* Layer::ToScrollbarLayer() {
  return NULL;
}

RenderingStatsInstrumentation* Layer::rendering_stats_instrumentation() const {
  return layer_tree_host_->rendering_stats_instrumentation();
}

}  // namespace cc
