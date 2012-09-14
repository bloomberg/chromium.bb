// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "TreeSynchronizer.h"

#include "CCLayerImpl.h"
#include "CCScrollbarAnimationController.h"
#include "CCScrollbarLayerImpl.h"
#include "LayerChromium.h"
#include "ScrollbarLayerChromium.h"
#include <wtf/RefPtr.h>

namespace cc {

PassOwnPtr<CCLayerImpl> TreeSynchronizer::synchronizeTrees(LayerChromium* layerChromiumRoot, PassOwnPtr<CCLayerImpl> oldCCLayerImplRoot, CCLayerTreeHostImpl* hostImpl)
{
    OwnPtrCCLayerImplMap oldLayers;
    RawPtrCCLayerImplMap newLayers;

    collectExistingCCLayerImplRecursive(oldLayers, oldCCLayerImplRoot);

    OwnPtr<CCLayerImpl> newTree = synchronizeTreeRecursive(newLayers, oldLayers, layerChromiumRoot, hostImpl);

    updateScrollbarLayerPointersRecursive(newLayers, layerChromiumRoot);

    return newTree.release();
}

void TreeSynchronizer::collectExistingCCLayerImplRecursive(OwnPtrCCLayerImplMap& oldLayers, PassOwnPtr<CCLayerImpl> popCCLayerImpl)
{
    OwnPtr<CCLayerImpl> ccLayerImpl = popCCLayerImpl;

    if (!ccLayerImpl)
        return;

    Vector<OwnPtr<CCLayerImpl> >& children = ccLayerImpl->m_children;
    for (size_t i = 0; i < children.size(); ++i)
        collectExistingCCLayerImplRecursive(oldLayers, children[i].release());

    collectExistingCCLayerImplRecursive(oldLayers, ccLayerImpl->m_maskLayer.release());
    collectExistingCCLayerImplRecursive(oldLayers, ccLayerImpl->m_replicaLayer.release());

    int id = ccLayerImpl->id();
    oldLayers.set(id, ccLayerImpl.release());
}

PassOwnPtr<CCLayerImpl> TreeSynchronizer::reuseOrCreateCCLayerImpl(RawPtrCCLayerImplMap& newLayers, OwnPtrCCLayerImplMap& oldLayers, LayerChromium* layer)
{
    OwnPtr<CCLayerImpl> ccLayerImpl = oldLayers.take(layer->id());

    if (!ccLayerImpl)
        ccLayerImpl = layer->createCCLayerImpl();

    newLayers.set(layer->id(), ccLayerImpl.get());
    return ccLayerImpl.release();
}

PassOwnPtr<CCLayerImpl> TreeSynchronizer::synchronizeTreeRecursive(RawPtrCCLayerImplMap& newLayers, OwnPtrCCLayerImplMap& oldLayers, LayerChromium* layer, CCLayerTreeHostImpl* hostImpl)
{
    if (!layer)
        return nullptr;

    OwnPtr<CCLayerImpl> ccLayerImpl = reuseOrCreateCCLayerImpl(newLayers, oldLayers, layer);

    ccLayerImpl->clearChildList();
    const Vector<RefPtr<LayerChromium> >& children = layer->children();
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

    return ccLayerImpl.release();
}

void TreeSynchronizer::updateScrollbarLayerPointersRecursive(const RawPtrCCLayerImplMap& newLayers, LayerChromium* layer)
{
    if (!layer)
        return;

    const Vector<RefPtr<LayerChromium> >& children = layer->children();
    for (size_t i = 0; i < children.size(); ++i)
        updateScrollbarLayerPointersRecursive(newLayers, children[i].get());

    ScrollbarLayerChromium* scrollbarLayer = layer->toScrollbarLayerChromium();
    if (!scrollbarLayer)
        return;

    CCScrollbarLayerImpl* ccScrollbarLayerImpl = static_cast<CCScrollbarLayerImpl*>(newLayers.get(scrollbarLayer->id()));
    ASSERT(ccScrollbarLayerImpl);
    CCLayerImpl* ccScrollLayerImpl = newLayers.get(scrollbarLayer->scrollLayerId());
    ASSERT(ccScrollLayerImpl);

    if (ccScrollbarLayerImpl->orientation() == WebKit::WebScrollbar::Horizontal)
        ccScrollLayerImpl->setHorizontalScrollbarLayer(ccScrollbarLayerImpl);
    else
        ccScrollLayerImpl->setVerticalScrollbarLayer(ccScrollbarLayerImpl);
}

} // namespace cc
