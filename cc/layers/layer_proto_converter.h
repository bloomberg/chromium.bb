// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_PROTO_CONVERTER_H_
#define CC_LAYERS_LAYER_PROTO_CONVERTER_H_

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer.h"

namespace cc {

namespace proto {
class LayerNode;
}

// A class to faciliate (de)serialization of a Layer tree to protocol buffers.
class CC_EXPORT LayerProtoConverter {
 public:
  // Starting at |root_layer|, serializes the layer hierarchy into the
  // proto::LayerNode.
  static void SerializeLayerHierarchy(const scoped_refptr<Layer> root_layer,
                                      proto::LayerNode* root_node);

  // Recursively iterate over the given LayerNode proto and read the structure
  // into a local Layer structure, re-using existing Layers. returns the new
  // root Layer after updating the hierarchy (may be the same as
  // |existing_root|).
  static scoped_refptr<Layer> DeserializeLayerHierarchy(
      scoped_refptr<Layer> existing_root,
      const proto::LayerNode& root_node);

  // Returns the Layer with proto.id() as the Layer id, if it exists in
  // |layer_id_map|. Otherwise, a new Layer is constructed of the type given
  // from proto.type().
  static scoped_refptr<Layer> FindOrAllocateAndConstruct(
      const proto::LayerNode& proto,
      const Layer::LayerIdMap& layer_id_map);

 private:
  LayerProtoConverter();
  ~LayerProtoConverter();

  using LayerIdMap = base::hash_map<int, scoped_refptr<Layer>>;
  // Start at |layer| and recursively add |layer| and all its children and
  // special layers to |layer_id_map|.
  static void RecursivelyFindAllLayers(const scoped_refptr<Layer>& layer,
                                       LayerIdMap* layer_id_map);
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_PROTO_CONVERTER_H_
