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

class ClientPictureCache;
class CompositorStateDeserializerClient;
class DeserializedContentLayerClient;
class Layer;
class LayerFactory;
class LayerTreeHost;

// Deserializes the compositor updates into the LayerTreeHost.
class CC_EXPORT CompositorStateDeserializer {
 public:
  // ScrollCallback used to notify the client when the scroll offset for a layer
  // is updated.
  using ScrollCallback = base::Callback<void(int engine_layer_id)>;

  CompositorStateDeserializer(
      LayerTreeHost* layer_tree_host,
      std::unique_ptr<ClientPictureCache> client_picture_cache,
      const ScrollCallback& scroll_callback,
      CompositorStateDeserializerClient* client);
  ~CompositorStateDeserializer();

  // Returns the local layer on the client for the given |engine_layer_id|.
  Layer* GetLayerForEngineId(int engine_layer_id) const;

  // Returns the local layer id on the client for the given |engine_layer_id|.
  int GetClientIdFromEngineId(int engine_layer_id) const;

  // Deserializes the |layer_tree_host_proto| into the LayerTreeHost.
  void DeserializeCompositorUpdate(
      const proto::LayerTreeHost& layer_tree_host_proto);

  // Allows tests to inject the LayerFactory.
  void SetLayerFactoryForTesting(std::unique_ptr<LayerFactory> layer_factory);

 private:
  // A holder for the Layer and any data tied to it.
  struct LayerData {
    LayerData();
    LayerData(LayerData&& other);
    ~LayerData();

    LayerData& operator=(LayerData&& other);

    scoped_refptr<Layer> layer;

    // Set only for PictureLayers.
    std::unique_ptr<DeserializedContentLayerClient> content_layer_client;

   private:
    DISALLOW_COPY_AND_ASSIGN(LayerData);
  };

  using EngineIdToLayerMap = std::unordered_map<int, LayerData>;
  // Map of the scrollbar layer id to the corresponding scroll layer id. Both
  // ids refer to the engine layer id.
  using ScrollbarLayerToScrollLayerId = std::unordered_map<int, int>;

  void SychronizeLayerTreeState(const proto::LayerTree& layer_tree_proto);
  void SynchronizeLayerState(
      const proto::LayerProperties& layer_properties_proto);
  void SynchronizeLayerHierarchyRecursive(
      Layer* layer,
      const proto::LayerNode& layer_node,
      EngineIdToLayerMap* new_layer_map,
      ScrollbarLayerToScrollLayerId* scrollbar_layer_to_scroll_layer);
  scoped_refptr<Layer> GetLayerAndAddToNewMap(
      const proto::LayerNode& layer_node,
      EngineIdToLayerMap* new_layer_map,
      ScrollbarLayerToScrollLayerId* scrollbar_layer_to_scroll_layer);

  scoped_refptr<Layer> GetLayer(int engine_layer_id) const;
  DeserializedContentLayerClient* GetContentLayerClient(
      int engine_layer_id) const;

  std::unique_ptr<LayerFactory> layer_factory_;

  LayerTreeHost* layer_tree_host_;
  std::unique_ptr<ClientPictureCache> client_picture_cache_;
  ScrollCallback scroll_callback_;
  CompositorStateDeserializerClient* client_;

  EngineIdToLayerMap engine_id_to_layer_;

  DISALLOW_COPY_AND_ASSIGN(CompositorStateDeserializer);
};

}  // namespace cc

#endif  // CC_BLIMP_COMPOSITOR_STATE_DESERIALIZER_H_
