// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_iterator.h"

#include "cc/layer.h"
#include "cc/layer_impl.h"
#include "cc/render_surface.h"
#include "cc/render_surface_impl.h"

namespace cc {

template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
void LayerIteratorActions::BackToFront::begin(LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>& it)
{
    it.m_targetRenderSurfaceLayerIndex = 0;
    it.m_currentLayerIndex = LayerIteratorValue::LayerIndexRepresentingTargetRenderSurface;

    m_highestTargetRenderSurfaceLayer = 0;
}

template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
void LayerIteratorActions::BackToFront::end(LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>& it)
{
    it.m_targetRenderSurfaceLayerIndex = LayerIteratorValue::InvalidTargetRenderSurfaceLayerIndex;
    it.m_currentLayerIndex = 0;
}

template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
void LayerIteratorActions::BackToFront::next(LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>& it)
{
    // If the current layer has a RS, move to its layer list. Otherwise, visit the next layer in the current RS layer list.
    if (it.currentLayerRepresentsContributingRenderSurface()) {
        // Save our position in the childLayer list for the RenderSurfaceImpl, then jump to the next RenderSurfaceImpl. Save where we
        // came from in the next RenderSurfaceImpl so we can get back to it.
        it.targetRenderSurface()->m_currentLayerIndexHistory = it.m_currentLayerIndex;
        int previousTargetRenderSurfaceLayer = it.m_targetRenderSurfaceLayerIndex;

        it.m_targetRenderSurfaceLayerIndex = ++m_highestTargetRenderSurfaceLayer;
        it.m_currentLayerIndex = LayerIteratorValue::LayerIndexRepresentingTargetRenderSurface;

        it.targetRenderSurface()->m_targetRenderSurfaceLayerIndexHistory = previousTargetRenderSurfaceLayer;
    } else {
        ++it.m_currentLayerIndex;

        int targetRenderSurfaceNumChildren = it.targetRenderSurfaceChildren().size();
        while (it.m_currentLayerIndex == targetRenderSurfaceNumChildren) {
            // Jump back to the previous RenderSurfaceImpl, and get back the position where we were in that list, and move to the next position there.
            if (!it.m_targetRenderSurfaceLayerIndex) {
                // End of the list
                it.m_targetRenderSurfaceLayerIndex = LayerIteratorValue::InvalidTargetRenderSurfaceLayerIndex;
                it.m_currentLayerIndex = 0;
                return;
            }
            it.m_targetRenderSurfaceLayerIndex = it.targetRenderSurface()->m_targetRenderSurfaceLayerIndexHistory;
            it.m_currentLayerIndex = it.targetRenderSurface()->m_currentLayerIndexHistory + 1;

            targetRenderSurfaceNumChildren = it.targetRenderSurfaceChildren().size();
        }
    }
}

template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
void LayerIteratorActions::FrontToBack::begin(LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>& it)
{
    it.m_targetRenderSurfaceLayerIndex = 0;
    it.m_currentLayerIndex = it.targetRenderSurfaceChildren().size() - 1;
    goToHighestInSubtree(it);
}

template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
void LayerIteratorActions::FrontToBack::end(LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>& it)
{
    it.m_targetRenderSurfaceLayerIndex = LayerIteratorValue::InvalidTargetRenderSurfaceLayerIndex;
    it.m_currentLayerIndex = 0;
}

template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
void LayerIteratorActions::FrontToBack::next(LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>& it)
{
    // Moves to the previous layer in the current RS layer list. Then we check if the
    // new current layer has its own RS, in which case there are things in that RS layer list that are higher, so
    // we find the highest layer in that subtree.
    // If we move back past the front of the list, we jump up to the previous RS layer list, picking up again where we
    // had previously recursed into the current RS layer list.

    if (!it.currentLayerRepresentsTargetRenderSurface()) {
        // Subtracting one here will eventually cause the current layer to become that layer
        // representing the target render surface.
        --it.m_currentLayerIndex;
        goToHighestInSubtree(it);
    } else {
        while (it.currentLayerRepresentsTargetRenderSurface()) {
            if (!it.m_targetRenderSurfaceLayerIndex) {
                // End of the list
                it.m_targetRenderSurfaceLayerIndex = LayerIteratorValue::InvalidTargetRenderSurfaceLayerIndex;
                it.m_currentLayerIndex = 0;
                return;
            }
            it.m_targetRenderSurfaceLayerIndex = it.targetRenderSurface()->m_targetRenderSurfaceLayerIndexHistory;
            it.m_currentLayerIndex = it.targetRenderSurface()->m_currentLayerIndexHistory;
        }
    }
}

template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
void LayerIteratorActions::FrontToBack::goToHighestInSubtree(LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>& it)
{
    if (it.currentLayerRepresentsTargetRenderSurface())
        return;
    while (it.currentLayerRepresentsContributingRenderSurface()) {
        // Save where we were in the current target surface, move to the next one, and save the target surface that we
        // came from there so we can go back to it.
        it.targetRenderSurface()->m_currentLayerIndexHistory = it.m_currentLayerIndex;
        int previousTargetRenderSurfaceLayer = it.m_targetRenderSurfaceLayerIndex;

        for (LayerType* layer = it.currentLayer(); it.targetRenderSurfaceLayer() != layer; ++it.m_targetRenderSurfaceLayerIndex) { }
        it.m_currentLayerIndex = it.targetRenderSurfaceChildren().size() - 1;

        it.targetRenderSurface()->m_targetRenderSurfaceLayerIndexHistory = previousTargetRenderSurfaceLayer;
    }
}

typedef std::vector<scoped_refptr<Layer> > LayerList;
typedef std::vector<LayerImpl*> LayerImplList;

// Declare each of the above functions for Layer and LayerImpl classes so that they are linked.
template CC_EXPORT void LayerIteratorActions::BackToFront::begin(LayerIterator<Layer, LayerList, RenderSurface, BackToFront> &);
template CC_EXPORT void LayerIteratorActions::BackToFront::end(LayerIterator<Layer, LayerList, RenderSurface, BackToFront>&);
template CC_EXPORT void LayerIteratorActions::BackToFront::next(LayerIterator<Layer, LayerList, RenderSurface, BackToFront>&);

template CC_EXPORT void LayerIteratorActions::BackToFront::begin(LayerIterator<LayerImpl, LayerImplList, RenderSurfaceImpl, BackToFront>&);
template CC_EXPORT void LayerIteratorActions::BackToFront::end(LayerIterator<LayerImpl, LayerImplList, RenderSurfaceImpl, BackToFront>&);
template CC_EXPORT void LayerIteratorActions::BackToFront::next(LayerIterator<LayerImpl, LayerImplList, RenderSurfaceImpl, BackToFront>&);

template CC_EXPORT void LayerIteratorActions::FrontToBack::next(LayerIterator<Layer, LayerList, RenderSurface, FrontToBack>&);
template CC_EXPORT void LayerIteratorActions::FrontToBack::end(LayerIterator<Layer, LayerList, RenderSurface, FrontToBack>&);
template CC_EXPORT void LayerIteratorActions::FrontToBack::begin(LayerIterator<Layer, LayerList, RenderSurface, FrontToBack>&);
template CC_EXPORT void LayerIteratorActions::FrontToBack::goToHighestInSubtree(LayerIterator<Layer, LayerList, RenderSurface, FrontToBack>&);

template CC_EXPORT void LayerIteratorActions::FrontToBack::next(LayerIterator<LayerImpl, LayerImplList, RenderSurfaceImpl, FrontToBack>&);
template CC_EXPORT void LayerIteratorActions::FrontToBack::end(LayerIterator<LayerImpl, LayerImplList, RenderSurfaceImpl, FrontToBack>&);
template CC_EXPORT void LayerIteratorActions::FrontToBack::begin(LayerIterator<LayerImpl, LayerImplList, RenderSurfaceImpl, FrontToBack>&);
template CC_EXPORT void LayerIteratorActions::FrontToBack::goToHighestInSubtree(LayerIterator<LayerImpl, LayerImplList, RenderSurfaceImpl, FrontToBack>&);

}  // namespace cc
