// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/tree_synchronizer.h"

#include "CCLayerImpl.h"
#include "cc/scrollbar_animation_controller.h"
#include "CCScrollbarLayerImpl.h"
#include "cc/layer.h"
#include "cc/scrollbar_layer.h"

namespace cc {

scoped_ptr<CCLayerImpl> TreeSynchronizer::synchronizeTrees(LayerChromium* layerChromiumRoot, scoped_ptr<CCLayerImpl> oldCCLayerImplRoot, CCLayerTreeHostImpl* hostImpl)
{
    ScopedPtrCCLayerImplMap oldLayers;
    RawPtrCCLayerImplMap newLayers;

    collectExistingCCLayerImplRecursive(oldLayers, oldCCLayerImplRoot.Pass());

    scoped_ptr<CCLayerImpl> newTree = synchronizeTreeRecursive(newLayers, oldLayers, layerChromiumRoot, hostImpl);

    updateScrollbarLayerPointersRecursive(newLayers, layerChromiumRoot);

    return newTree.Pass();
}

void TreeSynchronizer::collectExistingCCLayerImplRecursive(ScopedPtrCCLayerImplMap& oldLayers, scoped_ptr<CCLayerImpl> ccLayerImpl)
{
    if (!ccLayerImpl)
        return;

    ScopedPtrVector<CCLayerImpl>& children = ccLayerImpl->m_children;
    for (size_t i = 0; i < children.size(); ++i)
        collectExistingCCLayerImplRecursive(oldLayers, children.take(i));

    collectExistingCCLayerImplRecursive(oldLayers, ccLayerImpl->m_maskLayer.Pass());
    collectExistingCCLayerImplRecursive(oldLayers, ccLayerImpl->m_replicaLayer.Pass());

    int id = ccLayerImpl->id();
    oldLayers.set(id, ccLayerImpl.Pass());
}

scoped_ptr<CCLayerImpl> TreeSynchronizer::reuseOrCreateCCLayerImpl(RawPtrCCLayerImplMap& newLayers, ScopedPtrCCLayerImplMap& oldLayers, LayerChromium* layer)
{
    scoped_ptr<CCLayerImpl> ccLayerImpl = oldLayers.take(layer->id());

    if (!ccLayerImpl)
        ccLayerImpl = layer->createCCLayerImpl();

    newLayers[layer->id()] = ccLayerImpl.get();
    return ccLayerImpl.Pass();
}

scoped_ptr<CCLayerImpl> TreeSynchronizer::synchronizeTreeRecursive(RawPtrCCLayerImplMap& newLayers, ScopedPtrCCLayerImplMap& oldLayers, LayerChromium* layer, CCLayerTreeHostImpl* hostImpl)
{
    if (!layer)
        return scoped_ptr<CCLayerImpl>();

    scoped_ptr<CCLayerImpl> ccLayerImpl = reuseOrCreateCCLayerImpl(newLayers, oldLayers, layer);

    ccLayerImpl->clearChildList();
    const std::vector<scoped_refptr<LayerChromium> >& children = layer->children();
    for (size_t i = 0; i < children.size(); ++i)
        ccLayerImpl->addChild(synchronizeTreeRecursive(newLayers, oldLayers, children[i].get(), hostImpl));

    ccLayerImpl->setMaskLayer(synchronizeTreeRecursive(newLayers, oldLayers, layer->maskLayer(), hostImpl));
    ccLayerImpl->setReplicaLayer(synchronizeTreeRecursive(newLayers, oldLayers, layer->replicaLayer(), hostImpl));

    layer->pushPropertiesTo(ccLayerImpl.get());
    ccLayerImpl->setLayerTreeHostImpl(hostImpl);

    // Remove all dangling pointers. The pointers will be setup later in updateScrollbarLayerPointersRecursive phase
    if (CCScrollbarAnimationController* scrollbarController = ccLayerImpl->scrollbarAnimationController()) {
        scrollbarController->setHorizontalScrollbarLayer(0);
        scrollbarController->setVerticalScrollbarLayer(0);
    }

    return ccLayerImpl.Pass();
}

void TreeSynchronizer::updateScrollbarLayerPointersRecursive(const RawPtrCCLayerImplMap& newLayers, LayerChromium* layer)
{
    if (!layer)
        return;

    const std::vector<scoped_refptr<LayerChromium> >& children = layer->children();
    for (size_t i = 0; i < children.size(); ++i)
        updateScrollbarLayerPointersRecursive(newLayers, children[i].get());

    ScrollbarLayerChromium* scrollbarLayer = layer->toScrollbarLayerChromium();
    if (!scrollbarLayer)
        return;

    RawPtrCCLayerImplMap::const_iterator iter = newLayers.find(scrollbarLayer->id());
    CCScrollbarLayerImpl* ccScrollbarLayerImpl = iter != newLayers.end() ? static_cast<CCScrollbarLayerImpl*>(iter->second) : NULL;
    iter = newLayers.find(scrollbarLayer->scrollLayerId());
    CCLayerImpl* ccScrollLayerImpl = iter != newLayers.end() ? iter->second : NULL;

    ASSERT(ccScrollbarLayerImpl);
    ASSERT(ccScrollLayerImpl);

    if (ccScrollbarLayerImpl->orientation() == WebKit::WebScrollbar::Horizontal)
        ccScrollLayerImpl->setHorizontalScrollbarLayer(ccScrollbarLayerImpl);
    else
        ccScrollLayerImpl->setVerticalScrollbarLayer(ccScrollbarLayerImpl);
}

} // namespace cc
