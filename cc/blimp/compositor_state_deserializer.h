// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_COMPOSITOR_STATE_DESERIALIZER_H_
#define CC_BLIMP_COMPOSITOR_STATE_DESERIALIZER_H_

#include <unordered_map>

#include "base/callback.h"
#include "base/macros.h"
#include "cc/base/cc_export.h"

namespace cc {
namespace proto {
class LayerNode;
class LayerProperties;
class LayerTree;
class LayerTreeHost;
}  // namespace proto

class CompositorStateDeserializerClient;
class Layer;
class LayerFactory;
class LayerTreeHost;

// Deserializes the compositor updates into the LayerTreeHost.
class CC_EXPORT CompositorStateDeserializer {
 public:
  // ScrollCallback used to notify the client when the scroll offset for a layer
  // is updated.
  using ScrollCallback = base::Callback<void(int engine_layer_id)>;

  CompositorStateDeserializer(LayerTreeHost* layer_tree_host,
                              ScrollCallback scroll_callback,
                              CompositorStateDeserializerClient* client);
  ~CompositorStateDeserializer();

  // Returns the local layer on the client for the given |engine_layer_id|.
  Layer* GetLayerForEngineId(int engine_layer_id) const;

  // Deserializes the |layer_tree_host_proto| into the LayerTreeHost.
  void DeserializeCompositorUpdate(
      const proto::LayerTreeHost& layer_tree_host_proto);

  // Allows tests to inject the LayerFactory.
  void SetLayerFactoryForTesting(std::unique_ptr<LayerFactory> layer_factory);

 private:
  using EngineIdToLayerMap = std::unordered_map<int, scoped_refptr<Layer>>;

  void SychronizeLayerTreeState(const proto::LayerTree& layer_tree_proto);
  void SynchronizeLayerState(
      const proto::LayerProperties& layer_properties_proto);
  void SynchronizeLayerHierarchyRecursive(Layer* layer,
                                          const proto::LayerNode& layer_node,
                                          EngineIdToLayerMap* new_layer_map);
  scoped_refptr<Layer> GetLayerAndAddToNewMap(
      const proto::LayerNode& layer_node,
      EngineIdToLayerMap* new_layer_map);

  int GetClientIdFromEngineId(int engine_layer_id);
  scoped_refptr<Layer> GetLayer(int engine_layer_id);

  std::unique_ptr<LayerFactory> layer_factory_;
  LayerTreeHost* layer_tree_host_;
  ScrollCallback scroll_callback_;
  CompositorStateDeserializerClient* client_;

  EngineIdToLayerMap engine_id_to_layer_;

  DISALLOW_COPY_AND_ASSIGN(CompositorStateDeserializer);
};

}  // namespace cc

#endif  // CC_BLIMP_COMPOSITOR_STATE_DESERIALIZER_H_
