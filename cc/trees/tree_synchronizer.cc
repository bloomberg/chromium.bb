// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/tree_synchronizer.h"

#include <stddef.h>

#include <set>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

template <typename LayerType>
void SynchronizeTreesInternal(LayerType* layer_root, LayerTreeImpl* tree_impl) {
  DCHECK(tree_impl);

  TRACE_EVENT0("cc", "TreeSynchronizer::SynchronizeTrees");
  std::unique_ptr<OwnedLayerImplList> old_layers(tree_impl->DetachLayers());

  OwnedLayerImplMap old_layer_map;
  for (auto& it : *old_layers)
    old_layer_map[it->id()] = std::move(it);

  PushLayerList(&old_layer_map, layer_root, tree_impl);

  for (auto& it : old_layer_map) {
    if (it.second) {
      // Need to ensure that layer destruction doesn't tear down other layers
      // linked to this LayerImpl that have been used in the new tree.
      it.second->ClearLinksToOtherLayers();
    }
  }
}

void TreeSynchronizer::SynchronizeTrees(Layer* layer_root,
                                        LayerTreeImpl* tree_impl) {
  if (!layer_root)
    tree_impl->DetachLayers();
  else
    SynchronizeTreesInternal(layer_root, tree_impl);
}

void TreeSynchronizer::SynchronizeTrees(LayerImpl* layer_root,
                                        LayerTreeImpl* tree_impl) {
  if (!layer_root)
    tree_impl->DetachLayers();
  else
    SynchronizeTreesInternal(layer_root, tree_impl);
}

template <typename LayerType>
std::unique_ptr<LayerImpl> ReuseOrCreateLayerImpl(OwnedLayerImplMap* old_layers,
                                                  LayerType* layer,
                                                  LayerTreeImpl* tree_impl) {
  if (!layer)
    return nullptr;
  std::unique_ptr<LayerImpl> layer_impl = std::move((*old_layers)[layer->id()]);
  if (!layer_impl)
    layer_impl = layer->CreateLayerImpl(tree_impl);
  return layer_impl;
}

static void SynchronizeReplicaLayer(LayerImpl* layer_impl,
                                    std::unique_ptr<LayerImpl> new_replica,
                                    LayerTreeImpl* tree_impl) {
  if (layer_impl->replica_layer() &&
      layer_impl->replica_layer() == new_replica.get()) {
    // In this case, we only need to update the ownership, as we're essentially
    // just resetting the replica layer.
    tree_impl->AddLayer(std::move(new_replica));
  } else {
    layer_impl->SetReplicaLayer(std::move(new_replica));
  }
}

static void SynchronizeMaskLayer(LayerImpl* layer_impl,
                                 std::unique_ptr<LayerImpl> new_mask,
                                 LayerTreeImpl* tree_impl) {
  if (layer_impl->mask_layer() && layer_impl->mask_layer() == new_mask.get()) {
    // In this case, we only need to update the ownership, as we're essentially
    // just resetting the mask layer.
    tree_impl->AddLayer(std::move(new_mask));
  } else {
    layer_impl->SetMaskLayer(std::move(new_mask));
  }
}

template <typename LayerTreeType>
void PushLayerListInternal(OwnedLayerImplMap* old_layers,
                           LayerTreeType* host,
                           LayerTreeImpl* tree_impl) {
  tree_impl->ClearLayerList();
  for (auto* layer : *host) {
    std::unique_ptr<LayerImpl> layer_impl(
        ReuseOrCreateLayerImpl(old_layers, layer, tree_impl));

    std::unique_ptr<LayerImpl> mask_layer(
        ReuseOrCreateLayerImpl(old_layers, layer->mask_layer(), tree_impl));
    SynchronizeMaskLayer(layer_impl.get(), std::move(mask_layer), tree_impl);

    std::unique_ptr<LayerImpl> replica_layer(
        ReuseOrCreateLayerImpl(old_layers, layer->replica_layer(), tree_impl));
    SynchronizeReplicaLayer(layer_impl.get(), std::move(replica_layer),
                            tree_impl);
    if (layer->replica_layer()) {
      std::unique_ptr<LayerImpl> replica_mask_layer(ReuseOrCreateLayerImpl(
          old_layers, layer->replica_layer()->mask_layer(), tree_impl));
      SynchronizeMaskLayer(layer_impl->replica_layer(),
                           std::move(replica_mask_layer), tree_impl);
    }
    tree_impl->AddToLayerList(layer_impl.get());
    tree_impl->AddLayer(std::move(layer_impl));
  }
}

void PushLayerList(OwnedLayerImplMap* old_layers,
                   Layer* old_root,
                   LayerTreeImpl* tree_impl) {
  PushLayerListInternal(old_layers, old_root->layer_tree_host(), tree_impl);
  tree_impl->SetRootLayerFromLayerList();
}

void PushLayerList(OwnedLayerImplMap* old_layers,
                   LayerImpl* old_root,
                   LayerTreeImpl* tree_impl) {
  PushLayerListInternal(old_layers, old_root->layer_tree_impl(), tree_impl);
  tree_impl->SetRootLayerFromLayerList();
}

template <typename LayerType>
static void PushLayerPropertiesInternal(
    std::unordered_set<LayerType*> layers_that_should_push_properties,
    LayerTreeImpl* impl_tree) {
  for (auto layer : layers_that_should_push_properties) {
    LayerImpl* layer_impl = impl_tree->LayerById(layer->id());
    DCHECK(layer_impl);
    layer->PushPropertiesTo(layer_impl);
  }
}

void TreeSynchronizer::PushLayerProperties(LayerTreeImpl* pending_tree,
                                           LayerTreeImpl* active_tree) {
  PushLayerPropertiesInternal(pending_tree->LayersThatShouldPushProperties(),
                              active_tree);
}

void TreeSynchronizer::PushLayerProperties(LayerTreeHost* host_tree,
                                           LayerTreeImpl* impl_tree) {
  PushLayerPropertiesInternal(host_tree->LayersThatShouldPushProperties(),
                              impl_tree);
}

}  // namespace cc
