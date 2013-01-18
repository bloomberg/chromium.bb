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
    , contents_textures_purged_(false) {
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
  currently_scrolling_layer_ = NULL;

  render_surface_layer_list_.clear();
  SetNeedsUpdateDrawProperties();
  return root_layer_.Pass();
}

void LayerTreeImpl::ClearCurrentlyScrollingLayer() {
  currently_scrolling_layer_ = NULL;
  scrolling_layer_id_from_previous_tree_ = 0;
}

void LayerTreeImpl::UpdateMaxScrollOffset() {
  if (!root_scroll_layer() || !root_scroll_layer()->children().size())
    return;

  gfx::SizeF view_bounds = device_viewport_size();
  if (LayerImpl* clip_layer = root_scroll_layer()->parent()) {
    // Compensate for non-overlay scrollbars.
    if (clip_layer->masksToBounds())
      view_bounds = gfx::ScaleSize(clip_layer->bounds(), device_scale_factor());
  }

  gfx::Size content_bounds = ContentSize();
  if (settings().pageScalePinchZoomEnabled) {
    // Pinch with pageScale scrolls entirely in layout space.  ContentSize
    // returns the bounds including the page scale factor, so calculate the
    // pre page-scale layout size here.
    float page_scale_factor = pinch_zoom_viewport().page_scale_factor();
    content_bounds.set_width(content_bounds.width() / page_scale_factor);
    content_bounds.set_height(content_bounds.height() / page_scale_factor);
  } else {
    view_bounds.Scale(1 / pinch_zoom_viewport().page_scale_delta());
  }

  gfx::Vector2dF max_scroll = gfx::Rect(content_bounds).bottom_right() -
      gfx::RectF(view_bounds).bottom_right();
  max_scroll.Scale(1 / device_scale_factor());

  // The viewport may be larger than the contents in some cases, such as
  // having a vertical scrollbar but no horizontal overflow.
  max_scroll.ClampToMin(gfx::Vector2dF());

  root_scroll_layer()->setMaxScrollOffset(gfx::ToFlooredVector2d(max_scroll));
}

void LayerTreeImpl::UpdateDrawProperties() {
  render_surface_layer_list_.clear();
  if (!RootLayer())
    return;

  if (root_scroll_layer()) {
    root_scroll_layer()->setImplTransform(
        layer_tree_host_impl_->implTransform());
  }

  {
    TRACE_EVENT0("cc", "LayerTreeImpl::UpdateDrawProperties");
    LayerTreeHostCommon::calculateDrawProperties(
        RootLayer(),
        device_viewport_size(),
        device_scale_factor(),
        pinch_zoom_viewport().page_scale_factor(),
        MaxTextureSize(),
        settings().canUseLCDText,
        render_surface_layer_list_);
  }
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
  SetNeedsUpdateDrawProperties();
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
  DCHECK(!layer_tree_host_impl_->needsUpdateDrawProperties());
  return render_surface_layer_list_;
}

gfx::Size LayerTreeImpl::ContentSize() const {
  // TODO(aelias): Hardcoding the first child here is weird. Think of
  // a cleaner way to get the contentBounds on the Impl side.
  if (!root_scroll_layer() || root_scroll_layer()->children().empty())
    return gfx::Size();
  return root_scroll_layer()->children()[0]->contentBounds();
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

void LayerTreeImpl::SetNeedsUpdateDrawProperties() {
  layer_tree_host_impl_->setNeedsUpdateDrawProperties();
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
