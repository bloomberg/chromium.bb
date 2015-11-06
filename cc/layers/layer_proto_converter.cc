// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_proto_converter.h"

#include "base/stl_util.h"
#include "cc/layers/layer.h"
#include "cc/proto/layer.pb.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

LayerProtoConverter::LayerProtoConverter() {}

LayerProtoConverter::~LayerProtoConverter() {}

// static
void LayerProtoConverter::SerializeLayerHierarchy(
    const scoped_refptr<Layer> root_layer,
    proto::LayerNode* root_node) {
  root_layer->ToLayerNodeProto(root_node);
}

// static
scoped_refptr<Layer> LayerProtoConverter::DeserializeLayerHierarchy(
    scoped_refptr<Layer> existing_root,
    const proto::LayerNode& root_node) {
  LayerIdMap layer_id_map;
  RecursivelyFindAllLayers(existing_root, &layer_id_map);

  scoped_refptr<Layer> new_root = existing_root;
  if (!existing_root ||
      (root_node.has_id() && root_node.id() != existing_root->id())) {
    // The root node has changed or there was no root node,
    // so find or create the new root.
    new_root = FindOrAllocateAndConstruct(root_node, layer_id_map);
  }
  new_root->FromLayerNodeProto(root_node, layer_id_map);
  return new_root;
}

// static
void LayerProtoConverter::RecursivelyFindAllLayers(
    const scoped_refptr<Layer>& layer,
    LayerIdMap* layer_id_map) {
  LayerTreeHostCommon::CallFunctionForSubtree(
      layer.get(),
      [layer_id_map](Layer* layer) { (*layer_id_map)[layer->id()] = layer; });
}

// static
scoped_refptr<Layer> LayerProtoConverter::FindOrAllocateAndConstruct(
    const proto::LayerNode& proto,
    const Layer::LayerIdMap& layer_id_map) {
  DCHECK(proto.has_id());
  Layer::LayerIdMap::const_iterator iter = layer_id_map.find(proto.id());
  if (iter != layer_id_map.end())
    return iter->second;
  DCHECK(proto.has_type());
  switch (proto.type()) {
    case proto::Base:
      return Layer::Create(LayerSettings()).get();
  }
  // TODO(nyquist): Add the rest of the necessary LayerTypes. This function
  // should not return null.
  NOTREACHED();
  return nullptr;
}

}  // namespace cc
