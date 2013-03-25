// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/tree_synchronizer.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer.h"
#include "cc/layers/scrollbar_layer_impl.h"

namespace cc {

typedef ScopedPtrHashMap<int, LayerImpl> ScopedPtrLayerImplMap;
typedef base::hash_map<int, LayerImpl*> RawPtrLayerImplMap;

void CollectExistingLayerImplRecursive(ScopedPtrLayerImplMap* old_layers,
                                       scoped_ptr<LayerImpl> layer_impl) {
  if (!layer_impl)
    return;

  ScopedPtrVector<LayerImpl>& children = layer_impl->children();
  for (ScopedPtrVector<LayerImpl>::iterator it = children.begin();
       it != children.end();
       ++it)
    CollectExistingLayerImplRecursive(old_layers, children.take(it));

  CollectExistingLayerImplRecursive(old_layers, layer_impl->TakeMaskLayer());
  CollectExistingLayerImplRecursive(old_layers, layer_impl->TakeReplicaLayer());

  int id = layer_impl->id();
  old_layers->set(id, layer_impl.Pass());
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

  CollectExistingLayerImplRecursive(&old_layers, old_layer_impl_root.Pass());

  scoped_ptr<LayerImpl> new_tree = SynchronizeTreesRecursive(
      &new_layers, &old_layers, layer_root, tree_impl);

  UpdateScrollbarLayerPointersRecursive(&new_layers, layer_root);

  return new_tree.Pass();
}

scoped_ptr<LayerImpl> TreeSynchronizer::SynchronizeTrees(
    Layer* layer_root,
    scoped_ptr<LayerImpl> old_layer_impl_root,
    LayerTreeImpl* tree_impl) {
  return SynchronizeTreesInternal(
      layer_root, old_layer_impl_root.Pass(), tree_impl);
}

scoped_ptr<LayerImpl> TreeSynchronizer::SynchronizeTrees(
    LayerImpl* layer_root,
    scoped_ptr<LayerImpl> old_layer_impl_root,
    LayerTreeImpl* tree_impl) {
  return SynchronizeTreesInternal(
      layer_root, old_layer_impl_root.Pass(), tree_impl);
}

template <typename LayerType>
scoped_ptr<LayerImpl> ReuseOrCreateLayerImpl(RawPtrLayerImplMap* new_layers,
                                             ScopedPtrLayerImplMap* old_layers,
                                             LayerType* layer,
                                             LayerTreeImpl* tree_impl) {
  scoped_ptr<LayerImpl> layer_impl = old_layers->take(layer->id());

  if (!layer_impl)
    layer_impl = layer->CreateLayerImpl(tree_impl);

  (*new_layers)[layer->id()] = layer_impl.get();
  return layer_impl.Pass();
}

template <typename LayerType>
scoped_ptr<LayerImpl> SynchronizeTreesRecursiveInternal(
    RawPtrLayerImplMap* new_layers,
    ScopedPtrLayerImplMap* old_layers,
    LayerType* layer,
    LayerTreeImpl* tree_impl) {
  if (!layer)
    return scoped_ptr<LayerImpl>();

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

  // Remove all dangling pointers. The pointers will be setup later in
  // UpdateScrollbarLayerPointersRecursive phase
  layer_impl->SetHorizontalScrollbarLayer(NULL);
  layer_impl->SetVerticalScrollbarLayer(NULL);

  return layer_impl.Pass();
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

template <typename LayerType, typename ScrollbarLayerType>
void UpdateScrollbarLayerPointersRecursiveInternal(
    const RawPtrLayerImplMap* new_layers,
    LayerType* layer) {
  if (!layer)
    return;

  for (size_t i = 0; i < layer->children().size(); ++i) {
    UpdateScrollbarLayerPointersRecursiveInternal<
        LayerType, ScrollbarLayerType>(new_layers, layer->child_at(i));
  }

  ScrollbarLayerType* scrollbar_layer = layer->ToScrollbarLayer();
  // Pinch-zoom scrollbars will have an invalid scroll_layer_id, but they are
  // managed by LayerTreeImpl and not LayerImpl, so should not be
  // processed here.
  if (!scrollbar_layer || (scrollbar_layer->scroll_layer_id() ==
                          Layer::PINCH_ZOOM_ROOT_SCROLL_LAYER_ID))
    return;

  RawPtrLayerImplMap::const_iterator iter =
      new_layers->find(scrollbar_layer->id());
  ScrollbarLayerImpl* scrollbar_layer_impl =
      iter != new_layers->end() ? static_cast<ScrollbarLayerImpl*>(iter->second)
                               : NULL;
  iter = new_layers->find(scrollbar_layer->scroll_layer_id());
  LayerImpl* scroll_layer_impl =
      iter != new_layers->end() ? iter->second : NULL;

  DCHECK(scrollbar_layer_impl);
  DCHECK(scroll_layer_impl);

  if (scrollbar_layer->Orientation() == WebKit::WebScrollbar::Horizontal)
    scroll_layer_impl->SetHorizontalScrollbarLayer(scrollbar_layer_impl);
  else
    scroll_layer_impl->SetVerticalScrollbarLayer(scrollbar_layer_impl);
}

void UpdateScrollbarLayerPointersRecursive(const RawPtrLayerImplMap* new_layers,
                                           Layer* layer) {
  UpdateScrollbarLayerPointersRecursiveInternal<Layer, ScrollbarLayer>(
      new_layers, layer);
}

void UpdateScrollbarLayerPointersRecursive(const RawPtrLayerImplMap* new_layers,
                                           LayerImpl* layer) {
  UpdateScrollbarLayerPointersRecursiveInternal<LayerImpl, ScrollbarLayerImpl>(
      new_layers, layer);
}

template <typename LayerType>
void PushPropertiesInternal(LayerType* layer, LayerImpl* layer_impl) {
  if (!layer) {
    DCHECK(!layer_impl);
    return;
  }

  DCHECK_EQ(layer->id(), layer_impl->id());
  layer->PushPropertiesTo(layer_impl);

  PushPropertiesInternal(layer->mask_layer(), layer_impl->mask_layer());
  PushPropertiesInternal(layer->replica_layer(), layer_impl->replica_layer());

  const ScopedPtrVector<LayerImpl>& impl_children = layer_impl->children();
  DCHECK_EQ(layer->children().size(), impl_children.size());

  for (size_t i = 0; i < layer->children().size(); ++i) {
    PushPropertiesInternal(layer->child_at(i), impl_children[i]);
  }
}

void TreeSynchronizer::PushProperties(Layer* layer, LayerImpl* layer_impl) {
  PushPropertiesInternal(layer, layer_impl);
}

void TreeSynchronizer::PushProperties(LayerImpl* layer, LayerImpl* layer_impl) {
  PushPropertiesInternal(layer, layer_impl);
}

}  // namespace cc
