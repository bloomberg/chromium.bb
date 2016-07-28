// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/layer.h"
#include "cc/proto/layer_tree.pb.h"
#include "cc/trees/layer_tree.h"

namespace cc {

LayerTree::LayerTree(std::unique_ptr<AnimationHost> animation_host)
    : in_paint_layer_contents_(false),
      animation_host_(std::move(animation_host)) {
  DCHECK(animation_host_);
}

LayerTree::~LayerTree() {}

void LayerTree::RegisterLayer(Layer* layer) {
  DCHECK(!LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  layer_id_map_[layer->id()] = layer;
  if (layer->element_id()) {
    animation_host_->RegisterElement(layer->element_id(),
                                     ElementListType::ACTIVE);
  }
}

void LayerTree::UnregisterLayer(Layer* layer) {
  DCHECK(LayerById(layer->id()));
  DCHECK(!in_paint_layer_contents_);
  if (layer->element_id()) {
    animation_host_->UnregisterElement(layer->element_id(),
                                       ElementListType::ACTIVE);
  }
  RemoveLayerShouldPushProperties(layer);
  layer_id_map_.erase(layer->id());
}

Layer* LayerTree::LayerById(int id) const {
  LayerIdMap::const_iterator iter = layer_id_map_.find(id);
  return iter != layer_id_map_.end() ? iter->second : nullptr;
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

void LayerTree::ToProtobuf(proto::LayerTree* proto) {
  for (auto* layer : layers_that_should_push_properties_) {
    proto->add_layers_that_should_push_properties(layer->id());
  }
  proto->set_in_paint_layer_contents(in_paint_layer_contents());
}

void LayerTree::FromProtobuf(const proto::LayerTree& proto) {
  for (auto layer_id : proto.layers_that_should_push_properties()) {
    AddLayerShouldPushProperties(layer_id_map_[layer_id]);
  }
  in_paint_layer_contents_ = proto.in_paint_layer_contents();
}

}  // namespace cc
