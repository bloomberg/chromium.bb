// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_impl.h"

#include "cc/layer_tree_host_common.h"
#include "cc/layer_tree_host_impl.h"

namespace cc {

LayerTreeImpl::LayerTreeImpl(LayerTreeHostImpl* layer_tree_host_impl)
    : layer_tree_host_impl_(layer_tree_host_impl)
    , source_frame_number_(-1)
    , hud_layer_(0)
    , root_scroll_layer_(0)
    , currently_scrolling_layer_(0)
    , scrolling_layer_id_from_previous_tree_(0) {
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
  root_scroll_layer_ = findRootScrollLayer(root_layer_.get());
  currently_scrolling_layer_ = 0;

  if (root_layer_ && scrolling_layer_id_from_previous_tree_) {
    currently_scrolling_layer_ = LayerTreeHostCommon::findLayerInSubtree(
        root_layer_.get(),
        scrolling_layer_id_from_previous_tree_);
  }

  scrolling_layer_id_from_previous_tree_ = 0;

  layer_tree_host_impl_->OnCanDrawStateChangedForTree(this);
}

scoped_ptr<LayerImpl> LayerTreeImpl::DetachLayerTree() {
  // Clear all data structures that have direct references to the layer tree.
  scrolling_layer_id_from_previous_tree_ =
    currently_scrolling_layer_ ? currently_scrolling_layer_->id() : 0;
  currently_scrolling_layer_ = NULL;

  return root_layer_.Pass();
}

void LayerTreeImpl::ClearCurrentlyScrollingLayer() {
  currently_scrolling_layer_ = NULL;
  scrolling_layer_id_from_previous_tree_ = 0;
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


} // namespace cc
