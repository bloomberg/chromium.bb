// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/tree_synchronizer.h"

#include <stddef.h>

#include <set>
#include <unordered_map>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

using ScopedPtrLayerImplMap = std::unordered_map<int, scoped_ptr<LayerImpl>>;
using RawPtrLayerImplMap = std::unordered_map<int, LayerImpl*>;

void CollectExistingLayerImplRecursive(ScopedPtrLayerImplMap* old_layers,
                                       scoped_ptr<LayerImpl> layer_impl) {
  if (!layer_impl)
    return;

  OwnedLayerImplList& children = layer_impl->children();
  for (auto& child : children)
    CollectExistingLayerImplRecursive(old_layers, std::move(child));

  CollectExistingLayerImplRecursive(old_layers, layer_impl->TakeMaskLayer());
  CollectExistingLayerImplRecursive(old_layers, layer_impl->TakeReplicaLayer());

  int id = layer_impl->id();
  (*old_layers)[id] = std::move(layer_impl);
}

template <typename LayerType>
scoped_ptr<LayerImpl> SynchronizeTreesInternal(
    LayerType* layer_root,
    scoped_ptr<LayerImpl> old_layer_impl_root,
    LayerTreeImpl* tree_impl) {
  DCHECK(tree_impl);

  TRACE_EVENT0("cc", "TreeSynchronizer::SynchronizeTrees");
  ScopedPtrLayerImplMap old_layers;
  RawPtrLayerImplMap new_layers;

  CollectExistingLayerImplRecursive(&old_layers,
                                    std::move(old_layer_impl_root));

  scoped_ptr<LayerImpl> new_tree = SynchronizeTreesRecursive(
      &new_layers, &old_layers, layer_root, tree_impl);

  return new_tree;
}

scoped_ptr<LayerImpl> TreeSynchronizer::SynchronizeTrees(
    Layer* layer_root,
    scoped_ptr<LayerImpl> old_layer_impl_root,
    LayerTreeImpl* tree_impl) {
  return SynchronizeTreesInternal(layer_root, std::move(old_layer_impl_root),
                                  tree_impl);
}

scoped_ptr<LayerImpl> TreeSynchronizer::SynchronizeTrees(
    LayerImpl* layer_root,
    scoped_ptr<LayerImpl> old_layer_impl_root,
    LayerTreeImpl* tree_impl) {
  return SynchronizeTreesInternal(layer_root, std::move(old_layer_impl_root),
                                  tree_impl);
}

template <typename LayerType>
scoped_ptr<LayerImpl> ReuseOrCreateLayerImpl(RawPtrLayerImplMap* new_layers,
                                             ScopedPtrLayerImplMap* old_layers,
                                             LayerType* layer,
                                             LayerTreeImpl* tree_impl) {
  scoped_ptr<LayerImpl> layer_impl = std::move((*old_layers)[layer->id()]);

  if (!layer_impl)
    layer_impl = layer->CreateLayerImpl(tree_impl);

  (*new_layers)[layer->id()] = layer_impl.get();
  return layer_impl;
}

template <typename LayerType>
scoped_ptr<LayerImpl> SynchronizeTreesRecursiveInternal(
    RawPtrLayerImplMap* new_layers,
    ScopedPtrLayerImplMap* old_layers,
    LayerType* layer,
    LayerTreeImpl* tree_impl) {
  if (!layer)
    return nullptr;

  scoped_ptr<LayerImpl> layer_impl =
      ReuseOrCreateLayerImpl(new_layers, old_layers, layer, tree_impl);

  layer_impl->ClearChildList();
  for (size_t i = 0; i < layer->children().size(); ++i) {
    layer_impl->AddChild(SynchronizeTreesRecursiveInternal(
        new_layers, old_layers, layer->child_at(i), tree_impl));
  }

  layer_impl->SetMaskLayer(SynchronizeTreesRecursiveInternal(
      new_layers, old_layers, layer->mask_layer(), tree_impl));
  layer_impl->SetReplicaLayer(SynchronizeTreesRecursiveInternal(
      new_layers, old_layers, layer->replica_layer(), tree_impl));

  return layer_impl;
}

scoped_ptr<LayerImpl> SynchronizeTreesRecursive(
    RawPtrLayerImplMap* new_layers,
    ScopedPtrLayerImplMap* old_layers,
    Layer* layer,
    LayerTreeImpl* tree_impl) {
  return SynchronizeTreesRecursiveInternal(
      new_layers, old_layers, layer, tree_impl);
}

scoped_ptr<LayerImpl> SynchronizeTreesRecursive(
    RawPtrLayerImplMap* new_layers,
    ScopedPtrLayerImplMap* old_layers,
    LayerImpl* layer,
    LayerTreeImpl* tree_impl) {
  return SynchronizeTreesRecursiveInternal(
      new_layers, old_layers, layer, tree_impl);
}

static void CheckScrollAndClipPointersRecursive(Layer* layer,
                                                LayerImpl* layer_impl) {
  DCHECK_EQ(!!layer, !!layer_impl);
  if (!layer)
    return;

  // Having a scroll parent on the impl thread implies having one the main
  // thread, too. The main thread may have a scroll parent that is not in the
  // tree because it's been removed but not deleted. In this case, the layer
  // impl will have no scroll parent. Same argument applies for clip parents and
  // scroll/clip children.
  DCHECK(!layer_impl->scroll_parent() || !!layer->scroll_parent());
  DCHECK(!layer_impl->clip_parent() || !!layer->clip_parent());
  DCHECK(!layer_impl->scroll_children() || !!layer->scroll_children());
  DCHECK(!layer_impl->clip_children() || !!layer->clip_children());

  if (layer_impl->scroll_parent())
    DCHECK_EQ(layer->scroll_parent()->id(), layer_impl->scroll_parent()->id());

  if (layer_impl->clip_parent())
    DCHECK_EQ(layer->clip_parent()->id(), layer_impl->clip_parent()->id());

  if (layer_impl->scroll_children()) {
    for (std::set<Layer*>::iterator it = layer->scroll_children()->begin();
         it != layer->scroll_children()->end();
         ++it) {
      DCHECK_EQ((*it)->scroll_parent(), layer);
    }
    for (std::set<LayerImpl*>::iterator it =
             layer_impl->scroll_children()->begin();
         it != layer_impl->scroll_children()->end();
         ++it) {
      DCHECK_EQ((*it)->scroll_parent(), layer_impl);
    }
  }

  if (layer_impl->clip_children()) {
    for (std::set<Layer*>::iterator it = layer->clip_children()->begin();
         it != layer->clip_children()->end();
         ++it) {
      DCHECK_EQ((*it)->clip_parent(), layer);
    }
    for (std::set<LayerImpl*>::iterator it =
             layer_impl->clip_children()->begin();
         it != layer_impl->clip_children()->end();
         ++it) {
      DCHECK_EQ((*it)->clip_parent(), layer_impl);
    }
  }

  for (size_t i = 0u; i < layer->children().size(); ++i) {
    CheckScrollAndClipPointersRecursive(layer->child_at(i),
                                        layer_impl->child_at(i));
  }
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

#if DCHECK_IS_ON()
  if (host_tree->root_layer() && impl_tree->root_layer())
    CheckScrollAndClipPointersRecursive(host_tree->root_layer(),
                                        impl_tree->root_layer());
#endif
}

}  // namespace cc
