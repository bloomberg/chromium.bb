// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "CCLayerIterator.h"

#include "CCLayerImpl.h"
#include "CCRenderSurface.h"
#include "LayerChromium.h"
#include "RenderSurfaceChromium.h"

namespace cc {

template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
void CCLayerIteratorActions::BackToFront::begin(CCLayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>& it)
{
    it.m_targetRenderSurfaceLayerIndex = 0;
    it.m_currentLayerIndex = CCLayerIteratorValue::LayerIndexRepresentingTargetRenderSurface;

    m_highestTargetRenderSurfaceLayer = 0;
}

template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
void CCLayerIteratorActions::BackToFront::end(CCLayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>& it)
{
    it.m_targetRenderSurfaceLayerIndex = CCLayerIteratorValue::InvalidTargetRenderSurfaceLayerIndex;
    it.m_currentLayerIndex = 0;
}

template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
void CCLayerIteratorActions::BackToFront::next(CCLayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>& it)
{
    // If the current layer has a RS, move to its layer list. Otherwise, visit the next layer in the current RS layer list.
    if (it.currentLayerRepresentsContributingRenderSurface()) {
        // Save our position in the childLayer list for the RenderSurface, then jump to the next RenderSurface. Save where we
        // came from in the next RenderSurface so we can get back to it.
        it.targetRenderSurface()->m_currentLayerIndexHistory = it.m_currentLayerIndex;
        int previousTargetRenderSurfaceLayer = it.m_targetRenderSurfaceLayerIndex;

        it.m_targetRenderSurfaceLayerIndex = ++m_highestTargetRenderSurfaceLayer;
        it.m_currentLayerIndex = CCLayerIteratorValue::LayerIndexRepresentingTargetRenderSurface;

        it.targetRenderSurface()->m_targetRenderSurfaceLayerIndexHistory = previousTargetRenderSurfaceLayer;
    } else {
        ++it.m_currentLayerIndex;

        int targetRenderSurfaceNumChildren = it.targetRenderSurfaceChildren().size();
        while (it.m_currentLayerIndex == targetRenderSurfaceNumChildren) {
            // Jump back to the previous RenderSurface, and get back the position where we were in that list, and move to the next position there.
            if (!it.m_targetRenderSurfaceLayerIndex) {
                // End of the list
                it.m_targetRenderSurfaceLayerIndex = CCLayerIteratorValue::InvalidTargetRenderSurfaceLayerIndex;
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
void CCLayerIteratorActions::FrontToBack::begin(CCLayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>& it)
{
    it.m_targetRenderSurfaceLayerIndex = 0;
    it.m_currentLayerIndex = it.targetRenderSurfaceChildren().size() - 1;
    goToHighestInSubtree(it);
}

template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
void CCLayerIteratorActions::FrontToBack::end(CCLayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>& it)
{
    it.m_targetRenderSurfaceLayerIndex = CCLayerIteratorValue::InvalidTargetRenderSurfaceLayerIndex;
    it.m_currentLayerIndex = 0;
}

template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
void CCLayerIteratorActions::FrontToBack::next(CCLayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>& it)
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
                it.m_targetRenderSurfaceLayerIndex = CCLayerIteratorValue::InvalidTargetRenderSurfaceLayerIndex;
                it.m_currentLayerIndex = 0;
                return;
            }
            it.m_targetRenderSurfaceLayerIndex = it.targetRenderSurface()->m_targetRenderSurfaceLayerIndexHistory;
            it.m_currentLayerIndex = it.targetRenderSurface()->m_currentLayerIndexHistory;
        }
    }
}

template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
void CCLayerIteratorActions::FrontToBack::goToHighestInSubtree(CCLayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>& it)
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

// Declare each of the above functions for LayerChromium and CCLayerImpl classes so that they are linked.
template void CCLayerIteratorActions::BackToFront::begin(CCLayerIterator<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, BackToFront> &);
template void CCLayerIteratorActions::BackToFront::end(CCLayerIterator<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, BackToFront>&);
template void CCLayerIteratorActions::BackToFront::next(CCLayerIterator<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, BackToFront>&);

template void CCLayerIteratorActions::BackToFront::begin(CCLayerIterator<CCLayerImpl, Vector<CCLayerImpl*>, CCRenderSurface, BackToFront>&);
template void CCLayerIteratorActions::BackToFront::end(CCLayerIterator<CCLayerImpl, Vector<CCLayerImpl*>, CCRenderSurface, BackToFront>&);
template void CCLayerIteratorActions::BackToFront::next(CCLayerIterator<CCLayerImpl, Vector<CCLayerImpl*>, CCRenderSurface, BackToFront>&);

template void CCLayerIteratorActions::FrontToBack::next(CCLayerIterator<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, FrontToBack>&);
template void CCLayerIteratorActions::FrontToBack::end(CCLayerIterator<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, FrontToBack>&);
template void CCLayerIteratorActions::FrontToBack::begin(CCLayerIterator<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, FrontToBack>&);
template void CCLayerIteratorActions::FrontToBack::goToHighestInSubtree(CCLayerIterator<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, FrontToBack>&);

template void CCLayerIteratorActions::FrontToBack::next(CCLayerIterator<CCLayerImpl, Vector<CCLayerImpl*>, CCRenderSurface, FrontToBack>&);
template void CCLayerIteratorActions::FrontToBack::end(CCLayerIterator<CCLayerImpl, Vector<CCLayerImpl*>, CCRenderSurface, FrontToBack>&);
template void CCLayerIteratorActions::FrontToBack::begin(CCLayerIterator<CCLayerImpl, Vector<CCLayerImpl*>, CCRenderSurface, FrontToBack>&);
template void CCLayerIteratorActions::FrontToBack::goToHighestInSubtree(CCLayerIterator<CCLayerImpl, Vector<CCLayerImpl*>, CCRenderSurface, FrontToBack>&);

} // namespace cc

#endif // USE(ACCELERATED_COMPOSITING)
