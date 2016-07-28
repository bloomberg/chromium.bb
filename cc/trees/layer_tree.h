// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_H_
#define CC_TREES_LAYER_TREE_H_

#include <unordered_map>
#include <unordered_set>

#include "base/macros.h"
#include "cc/base/cc_export.h"

namespace cc {

namespace proto {
class LayerTree;
class LayerUpdate;
}

class AnimationHost;
class Layer;

class CC_EXPORT LayerTree {
 public:
  using LayerSet = std::unordered_set<Layer*>;
  using LayerIdMap = std::unordered_map<int, Layer*>;

  explicit LayerTree(std::unique_ptr<AnimationHost> animation_host);
  ~LayerTree();

  void RegisterLayer(Layer* layer);
  void UnregisterLayer(Layer* layer);
  Layer* LayerById(int id) const;
  bool UpdateLayers(const LayerList& update_layer_list,
                    bool* content_is_suitable_for_gpu);

  void AddLayerShouldPushProperties(Layer* layer);
  void RemoveLayerShouldPushProperties(Layer* layer);
  std::unordered_set<Layer*>& LayersThatShouldPushProperties();
  bool LayerNeedsPushPropertiesForTesting(Layer* layer) const;

  void ToProtobuf(proto::LayerTree* proto);
  void FromProtobuf(const proto::LayerTree& proto);

  AnimationHost* animation_host() const { return animation_host_.get(); }

  bool in_paint_layer_contents() const { return in_paint_layer_contents_; }

 private:
  friend class LayerTreeHostSerializationTest;

  // Set of layers that need to push properties.
  LayerSet layers_that_should_push_properties_;

  // Layer id to Layer map.
  LayerIdMap layer_id_map_;

  bool in_paint_layer_contents_;

  std::unique_ptr<AnimationHost> animation_host_;

  DISALLOW_COPY_AND_ASSIGN(LayerTree);
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_H_
