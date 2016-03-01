// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_list_impl.h"

#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/layer_list_host_impl.h"

namespace cc {

LayerListImpl::LayerListImpl(LayerListHostImpl* host_impl)
    : layer_list_host_(host_impl) {}

LayerListImpl::~LayerListImpl() {}

AnimationRegistrar* LayerListImpl::GetAnimationRegistrar() const {
  return layer_list_host_->animation_registrar();
}

LayerImpl* LayerListImpl::LayerById(int id) const {
  LayerIdMap::const_iterator iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : NULL;
}

void LayerListImpl::RegisterLayer(LayerImpl* layer) {
  DCHECK(!LayerById(layer->id()));
  layer_id_map_[layer->id()] = layer;
  if (layer_list_host_->animation_host())
    layer_list_host_->animation_host()->RegisterLayer(
        layer->id(),
        IsActiveList() ? LayerListType::ACTIVE : LayerListType::PENDING);
}

void LayerListImpl::UnregisterLayer(LayerImpl* layer) {
  DCHECK(LayerById(layer->id()));
  if (layer_list_host_->animation_host())
    layer_list_host_->animation_host()->UnregisterLayer(
        layer->id(),
        IsActiveList() ? LayerListType::ACTIVE : LayerListType::PENDING);
  layer_id_map_.erase(layer->id());
}

size_t LayerListImpl::NumLayers() {
  return layer_id_map_.size();
}

LayerImpl* LayerListImpl::FindActiveLayerById(int id) {
  LayerListImpl* list = layer_list_host_->active_list();
  if (!list)
    return nullptr;
  return list->LayerById(id);
}

LayerImpl* LayerListImpl::FindPendingLayerById(int id) {
  LayerListImpl* list = layer_list_host_->pending_list();
  if (!list)
    return nullptr;
  return list->LayerById(id);
}

void LayerListImpl::AddToElementMap(LayerImpl* layer) {
  if (!layer->element_id() || !layer->mutable_properties())
    return;

  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "LayerListImpl::AddToElementMap", "element_id",
               layer->element_id(), "layer_id", layer->id());

  ElementLayers& layers = element_layers_map_[layer->element_id()];
  if ((!layers.main || layer->IsActive()) && !layer->scrollable()) {
    layers.main = layer;
  } else if ((!layers.scroll || layer->IsActive()) && layer->scrollable()) {
    TRACE_EVENT2("compositor-worker", "LayerListImpl::AddToElementMap scroll",
                 "element_id", layer->element_id(), "layer_id", layer->id());
    layers.scroll = layer;
  }
}

void LayerListImpl::RemoveFromElementMap(LayerImpl* layer) {
  if (!layer->element_id())
    return;

  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("compositor-worker"),
               "LayerListImpl::RemoveFromElementMap", "element_id",
               layer->element_id(), "layer_id", layer->id());

  ElementLayers& layers = element_layers_map_[layer->element_id()];
  if (!layer->scrollable())
    layers.main = nullptr;
  if (layer->scrollable())
    layers.scroll = nullptr;

  if (!layers.main && !layers.scroll)
    element_layers_map_.erase(layer->element_id());
}

LayerListImpl::ElementLayers LayerListImpl::GetMutableLayers(
    uint64_t element_id) {
  auto iter = element_layers_map_.find(element_id);
  if (iter == element_layers_map_.end())
    return ElementLayers();

  return iter->second;
}

bool LayerListImpl::IsActiveList() const {
  return layer_list_host_->active_list() == this;
}

bool LayerListImpl::IsPendingList() const {
  return layer_list_host_->pending_list() == this;
}

}  // namespace cc
