// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_impl.h"

#include "base/debug/trace_event.h"
#include "cc/layer_tree_host_common.h"
#include "cc/layer_tree_host_impl.h"
#include "ui/gfx/vector2d_conversions.h"

namespace cc {

LayerTreeImpl::LayerTreeImpl(LayerTreeHostImpl* layer_tree_host_impl)
    : layer_tree_host_impl_(layer_tree_host_impl)
    , source_frame_number_(-1)
    , hud_layer_(0)
    , root_scroll_layer_(0)
    , currently_scrolling_layer_(0)
    , background_color_(0)
    , has_transparent_background_(false)
    , scrolling_layer_id_from_previous_tree_(0)
    , contents_textures_purged_(false)
    , needs_update_draw_properties_(true) {
}

LayerTreeImpl::~LayerTreeImpl() {
  // Need to explicitly clear the tree prior to destroying this so that
  // the LayerTreeImpl pointer is still valid in the LayerImpl dtor.
  root_layer_.reset();
}

static LayerImpl* findRootScrollLayer(LayerImpl* layer)
{
    if (!layer)
        return 0;

    if (layer->scrollable())
        return layer;

    for (size_t i = 0; i < layer->children().size(); ++i) {
        LayerImpl* found = findRootScrollLayer(layer->children()[i]);
        if (found)
            return found;
    }

    return 0;
}

void LayerTreeImpl::SetRootLayer(scoped_ptr<LayerImpl> layer) {
  root_layer_ = layer.Pass();
  root_scroll_layer_ = NULL;
  currently_scrolling_layer_ = NULL;

  layer_tree_host_impl_->OnCanDrawStateChangedForTree(this);
}

void LayerTreeImpl::FindRootScrollLayer() {
  root_scroll_layer_ = findRootScrollLayer(root_layer_.get());

  if (root_layer_ && scrolling_layer_id_from_previous_tree_) {
    currently_scrolling_layer_ = LayerTreeHostCommon::findLayerInSubtree(
        root_layer_.get(),
        scrolling_layer_id_from_previous_tree_);
  }

  scrolling_layer_id_from_previous_tree_ = 0;
}

scoped_ptr<LayerImpl> LayerTreeImpl::DetachLayerTree() {
  // Clear all data structures that have direct references to the layer tree.
  scrolling_layer_id_from_previous_tree_ =
    currently_scrolling_layer_ ? currently_scrolling_layer_->id() : 0;
  root_scroll_layer_ = NULL;
  currently_scrolling_layer_ = NULL;

  render_surface_layer_list_.clear();
  set_needs_update_draw_properties();
  return root_layer_.Pass();
}

LayerImpl* LayerTreeImpl::RootScrollLayer() {
  DCHECK(IsActiveTree());
  return root_scroll_layer_;
}

LayerImpl* LayerTreeImpl::CurrentlyScrollingLayer() {
  DCHECK(IsActiveTree());
  return currently_scrolling_layer_;
}

void LayerTreeImpl::ClearCurrentlyScrollingLayer() {
  currently_scrolling_layer_ = NULL;
  scrolling_layer_id_from_previous_tree_ = 0;
}

void LayerTreeImpl::UpdateMaxScrollOffset() {
  if (!root_scroll_layer_ || !root_scroll_layer_->children().size())
    return;

  gfx::SizeF view_bounds;
  if (!settings().pageScalePinchZoomEnabled) {
    view_bounds = device_viewport_size();
    if (LayerImpl* clip_layer = root_scroll_layer_->parent()) {
      // Compensate for non-overlay scrollbars.
      if (clip_layer->masksToBounds())
        view_bounds = gfx::ScaleSize(clip_layer->bounds(), device_scale_factor());
    }
    view_bounds.Scale(1 / pinch_zoom_viewport().page_scale_delta());
  } else {
    view_bounds = layout_viewport_size();
  }

  gfx::Vector2dF max_scroll = gfx::Rect(ScrollableSize()).bottom_right() -
      gfx::RectF(view_bounds).bottom_right();

  // The viewport may be larger than the contents in some cases, such as
  // having a vertical scrollbar but no horizontal overflow.
  max_scroll.ClampToMin(gfx::Vector2dF());

  root_scroll_layer_->setMaxScrollOffset(gfx::ToFlooredVector2d(max_scroll));
}

void LayerTreeImpl::UpdateDrawProperties() {
  if (!needs_update_draw_properties_)
    return;

  needs_update_draw_properties_ = false;
  render_surface_layer_list_.clear();

  // For maxTextureSize.
  if (!layer_tree_host_impl_->renderer())
      return;

  if (!RootLayer())
    return;

  if (root_scroll_layer_) {
    root_scroll_layer_->setImplTransform(
        layer_tree_host_impl_->implTransform());
    // Setting the impl transform re-sets this.
    needs_update_draw_properties_ = false;
  }

  {
    TRACE_EVENT1("cc", "LayerTreeImpl::UpdateDrawProperties", "IsActive", IsActiveTree());
    LayerTreeHostCommon::calculateDrawProperties(
        RootLayer(),
        device_viewport_size(),
        device_scale_factor(),
        pinch_zoom_viewport().page_scale_factor(),
        MaxTextureSize(),
        settings().canUseLCDText,
        render_surface_layer_list_);
  }

  DCHECK(!needs_update_draw_properties_) <<
      "calcDrawProperties should not set_needs_update_draw_properties()";
}

static void ClearRenderSurfacesOnLayerImplRecursive(LayerImpl* current)
{
    DCHECK(current);
    for (size_t i = 0; i < current->children().size(); ++i)
        ClearRenderSurfacesOnLayerImplRecursive(current->children()[i]);
    current->clearRenderSurface();
}

void LayerTreeImpl::ClearRenderSurfaces() {
  ClearRenderSurfacesOnLayerImplRecursive(RootLayer());
  render_surface_layer_list_.clear();
  set_needs_update_draw_properties();
}

bool LayerTreeImpl::AreVisibleResourcesReady() const {
  TRACE_EVENT0("cc", "LayerTreeImpl::AreVisibleResourcesReady");

  typedef LayerIterator<LayerImpl,
                        std::vector<LayerImpl*>,
                        RenderSurfaceImpl,
                        LayerIteratorActions::BackToFront> LayerIteratorType;
  LayerIteratorType end = LayerIteratorType::end(&render_surface_layer_list_);
  for (LayerIteratorType it = LayerIteratorType::begin(
           &render_surface_layer_list_); it != end; ++it) {
    if (it.representsItself() && !(*it)->areVisibleResourcesReady())
      return false;
  }

  return true;
}

const LayerTreeImpl::LayerList& LayerTreeImpl::RenderSurfaceLayerList() const {
  // If this assert triggers, then the list is dirty.
  DCHECK(!needs_update_draw_properties_);
  return render_surface_layer_list_;
}

gfx::Size LayerTreeImpl::ScrollableSize() const {
  if (!root_scroll_layer_ || root_scroll_layer_->children().empty())
    return gfx::Size();
  return root_scroll_layer_->children()[0]->bounds();
}

LayerImpl* LayerTreeImpl::LayerById(int id) {
  LayerIdMap::iterator iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : NULL;
}

void LayerTreeImpl::RegisterLayer(LayerImpl* layer) {
  DCHECK(!LayerById(layer->id()));
  layer_id_map_[layer->id()] = layer;
}

void LayerTreeImpl::UnregisterLayer(LayerImpl* layer) {
  DCHECK(LayerById(layer->id()));
  layer_id_map_.erase(layer->id());
}

void LayerTreeImpl::PushPersistedState(LayerTreeImpl* pendingTree) {
  int id = currently_scrolling_layer_ ? currently_scrolling_layer_->id() : 0;
  pendingTree->set_currently_scrolling_layer(
      LayerTreeHostCommon::findLayerInSubtree(pendingTree->RootLayer(), id));
}

static void DidBecomeActiveRecursive(LayerImpl* layer) {
  layer->didBecomeActive();
  for (size_t i = 0; i < layer->children().size(); ++i)
    DidBecomeActiveRecursive(layer->children()[i]);
}

void LayerTreeImpl::DidBecomeActive() {
  if (RootLayer())
    DidBecomeActiveRecursive(RootLayer());
  FindRootScrollLayer();
  UpdateMaxScrollOffset();
}

bool LayerTreeImpl::ContentsTexturesPurged() const {
  return contents_textures_purged_;
}

void LayerTreeImpl::SetContentsTexturesPurged() {
  contents_textures_purged_ = true;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree(this);
}

void LayerTreeImpl::ResetContentsTexturesPurged() {
  contents_textures_purged_ = false;
  layer_tree_host_impl_->OnCanDrawStateChangedForTree(this);
}

Proxy* LayerTreeImpl::proxy() const {
  return layer_tree_host_impl_->proxy();
}

const LayerTreeSettings& LayerTreeImpl::settings() const {
  return layer_tree_host_impl_->settings();
}

OutputSurface* LayerTreeImpl::output_surface() const {
  return layer_tree_host_impl_->outputSurface();
}

ResourceProvider* LayerTreeImpl::resource_provider() const {
  return layer_tree_host_impl_->resourceProvider();
}

TileManager* LayerTreeImpl::tile_manager() const {
  return layer_tree_host_impl_->tileManager();
}

FrameRateCounter* LayerTreeImpl::frame_rate_counter() const {
  return layer_tree_host_impl_->fpsCounter();
}

PaintTimeCounter* LayerTreeImpl::paint_time_counter() const {
  return layer_tree_host_impl_->paintTimeCounter();
}

bool LayerTreeImpl::IsActiveTree() const {
  return layer_tree_host_impl_->activeTree() == this;
}

bool LayerTreeImpl::IsPendingTree() const {
  return layer_tree_host_impl_->pendingTree() == this;
}

bool LayerTreeImpl::IsRecycleTree() const {
  return layer_tree_host_impl_->recycleTree() == this;
}

LayerImpl* LayerTreeImpl::FindActiveTreeLayerById(int id) {
  LayerTreeImpl* tree = layer_tree_host_impl_->activeTree();
  if (!tree)
    return NULL;
  return tree->LayerById(id);
}

LayerImpl* LayerTreeImpl::FindPendingTreeLayerById(int id) {
  LayerTreeImpl* tree = layer_tree_host_impl_->pendingTree();
  if (!tree)
    return NULL;
  return tree->LayerById(id);
}

int LayerTreeImpl::MaxTextureSize() const {
  return layer_tree_host_impl_->rendererCapabilities().maxTextureSize;
}

bool LayerTreeImpl::PinchGestureActive() const {
  return layer_tree_host_impl_->pinchGestureActive();
}

void LayerTreeImpl::SetNeedsRedraw() {
  layer_tree_host_impl_->setNeedsRedraw();
}

const LayerTreeDebugState& LayerTreeImpl::debug_state() const {
  return layer_tree_host_impl_->debugState();
}

float LayerTreeImpl::device_scale_factor() const {
  return layer_tree_host_impl_->deviceScaleFactor();
}

const gfx::Size& LayerTreeImpl::device_viewport_size() const {
  return layer_tree_host_impl_->deviceViewportSize();
}

const gfx::Size& LayerTreeImpl::layout_viewport_size() const {
  return layer_tree_host_impl_->layoutViewportSize();
}

std::string LayerTreeImpl::layer_tree_as_text() const {
  return layer_tree_host_impl_->layerTreeAsText();
}

DebugRectHistory* LayerTreeImpl::debug_rect_history() const {
  return layer_tree_host_impl_->debugRectHistory();
}

AnimationRegistrar* LayerTreeImpl::animationRegistrar() const {
  return layer_tree_host_impl_->animationRegistrar();
}

const PinchZoomViewport& LayerTreeImpl::pinch_zoom_viewport() const {
  return layer_tree_host_impl_->pinchZoomViewport();
}

} // namespace cc
