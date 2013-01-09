// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tree_synchronizer.h"

#include "base/debug/trace_event.h"
#include "cc/layer.h"
#include "cc/layer_impl.h"
#include "cc/scrollbar_animation_controller.h"
#include "cc/scrollbar_layer.h"
#include "cc/scrollbar_layer_impl.h"

namespace cc {

scoped_ptr<LayerImpl> TreeSynchronizer::synchronizeTrees(Layer* layerRoot, scoped_ptr<LayerImpl> oldLayerImplRoot, LayerTreeImpl* treeImpl)
{
    DCHECK(treeImpl);

    TRACE_EVENT0("cc", "TreeSynchronizer::synchronizeTrees");
    ScopedPtrLayerImplMap oldLayers;
    RawPtrLayerImplMap newLayers;

    collectExistingLayerImplRecursive(oldLayers, oldLayerImplRoot.Pass());

    scoped_ptr<LayerImpl> newTree = synchronizeTreeRecursive(newLayers, oldLayers, layerRoot, treeImpl);

    updateScrollbarLayerPointersRecursive(newLayers, layerRoot);

    return newTree.Pass();
}

void TreeSynchronizer::collectExistingLayerImplRecursive(ScopedPtrLayerImplMap& oldLayers, scoped_ptr<LayerImpl> layerImpl)
{
    if (!layerImpl)
        return;

    ScopedPtrVector<LayerImpl>& children = layerImpl->m_children;
    for (ScopedPtrVector<LayerImpl>::iterator it = children.begin(); it != children.end(); ++it)
        collectExistingLayerImplRecursive(oldLayers, children.take(it));

    collectExistingLayerImplRecursive(oldLayers, layerImpl->takeMaskLayer());
    collectExistingLayerImplRecursive(oldLayers, layerImpl->takeReplicaLayer());

    int id = layerImpl->id();
    oldLayers.set(id, layerImpl.Pass());
}

scoped_ptr<LayerImpl> TreeSynchronizer::reuseOrCreateLayerImpl(RawPtrLayerImplMap& newLayers, ScopedPtrLayerImplMap& oldLayers, Layer* layer, LayerTreeImpl* treeImpl)
{
    scoped_ptr<LayerImpl> layerImpl = oldLayers.take(layer->id());

    if (!layerImpl)
        layerImpl = layer->createLayerImpl(treeImpl);

    newLayers[layer->id()] = layerImpl.get();
    return layerImpl.Pass();
}

scoped_ptr<LayerImpl> TreeSynchronizer::synchronizeTreeRecursive(RawPtrLayerImplMap& newLayers, ScopedPtrLayerImplMap& oldLayers, Layer* layer, LayerTreeImpl* treeImpl)
{
    if (!layer)
        return scoped_ptr<LayerImpl>();

    scoped_ptr<LayerImpl> layerImpl = reuseOrCreateLayerImpl(newLayers, oldLayers, layer, treeImpl);

    layerImpl->clearChildList();
    const std::vector<scoped_refptr<Layer> >& children = layer->children();
    for (size_t i = 0; i < children.size(); ++i)
        layerImpl->addChild(synchronizeTreeRecursive(newLayers, oldLayers, children[i].get(), treeImpl));

    layerImpl->setMaskLayer(synchronizeTreeRecursive(newLayers, oldLayers, layer->maskLayer(), treeImpl));
    layerImpl->setReplicaLayer(synchronizeTreeRecursive(newLayers, oldLayers, layer->replicaLayer(), treeImpl));

    layer->pushPropertiesTo(layerImpl.get());

    // Remove all dangling pointers. The pointers will be setup later in updateScrollbarLayerPointersRecursive phase
    if (ScrollbarAnimationController* scrollbarController = layerImpl->scrollbarAnimationController()) {
        scrollbarController->setHorizontalScrollbarLayer(0);
        scrollbarController->setVerticalScrollbarLayer(0);
    }

    return layerImpl.Pass();
}

void TreeSynchronizer::updateScrollbarLayerPointersRecursive(const RawPtrLayerImplMap& newLayers, Layer* layer)
{
    if (!layer)
        return;

    const std::vector<scoped_refptr<Layer> >& children = layer->children();
    for (size_t i = 0; i < children.size(); ++i)
        updateScrollbarLayerPointersRecursive(newLayers, children[i].get());

    ScrollbarLayer* scrollbarLayer = layer->toScrollbarLayer();
    if (!scrollbarLayer)
        return;

    RawPtrLayerImplMap::const_iterator iter = newLayers.find(scrollbarLayer->id());
    ScrollbarLayerImpl* scrollbarLayerImpl = iter != newLayers.end() ? static_cast<ScrollbarLayerImpl*>(iter->second) : NULL;
    iter = newLayers.find(scrollbarLayer->scrollLayerId());
    LayerImpl* scrollLayerImpl = iter != newLayers.end() ? iter->second : NULL;

    DCHECK(scrollbarLayerImpl);
    DCHECK(scrollLayerImpl);

    if (scrollbarLayerImpl->orientation() == WebKit::WebScrollbar::Horizontal)
        scrollLayerImpl->setHorizontalScrollbarLayer(scrollbarLayerImpl);
    else
        scrollLayerImpl->setVerticalScrollbarLayer(scrollbarLayerImpl);
}

}  // namespace cc
