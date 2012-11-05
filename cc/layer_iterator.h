// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_ITERATOR_H_
#define CC_LAYER_ITERATOR_H_

#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "cc/layer_tree_host_common.h"

namespace cc {

// These classes provide means to iterate over the RenderSurfaceImpl-Layer tree.

// Example code follows, for a tree of Layer/RenderSurface objects. See below for details.
//
// void doStuffOnLayers(const std::vector<scoped_refptr<Layer> >& renderSurfaceLayerList)
// {
//     typedef LayerIterator<Layer, RenderSurface, LayerIteratorActions::FrontToBack> LayerIteratorType;
//
//     LayerIteratorType end = LayerIteratorType::end(&renderSurfaceLayerList);
//     for (LayerIteratorType it = LayerIteratorType::begin(&renderSurfaceLayerList); it != end; ++it) {
//         // Only one of these will be true
//         if (it.representsTargetRenderSurface())
//             foo(*it); // *it is a layer representing a target RenderSurfaceImpl
//         if (it.representsContributingRenderSurface())
//             bar(*it); // *it is a layer representing a RenderSurfaceImpl that contributes to the layer's target RenderSurfaceImpl
//         if (it.representsItself())
//             baz(*it); // *it is a layer representing itself, as it contributes to its own target RenderSurfaceImpl
//     }
// }

// A RenderSurfaceImpl R may be referred to in one of two different contexts. One RenderSurfaceImpl is "current" at any time, for
// whatever operation is being performed. This current surface is referred to as a target surface. For example, when R is
// being painted it would be the target surface. Once R has been painted, its contents may be included into another
// surface S. While S is considered the target surface when it is being painted, R is called a contributing surface
// in this context as it contributes to the content of the target surface S.
//
// The iterator's current position in the tree always points to some layer. The state of the iterator indicates the role of the
// layer, and will be one of the following three states. A single layer L will appear in the iteration process in at least one,
// and possibly all, of these states.
// 1. Representing the target surface: The iterator in this state, pointing at layer L, indicates that the target RenderSurfaceImpl
// is now the surface owned by L. This will occur exactly once for each RenderSurfaceImpl in the tree.
// 2. Representing a contributing surface: The iterator in this state, pointing at layer L, refers to the RenderSurfaceImpl owned
// by L as a contributing surface, without changing the current target RenderSurfaceImpl.
// 3. Representing itself: The iterator in this state, pointing at layer L, refers to the layer itself, as a child of the
// current target RenderSurfaceImpl.
//
// The BackToFront iterator will return a layer representing the target surface before returning layers representing themselves
// as children of the current target surface. Whereas the FrontToBack ordering will iterate over children layers of a surface
// before the layer representing the surface as a target surface.
//
// To use the iterators:
//
// Create a stepping iterator and end iterator by calling LayerIterator::begin() and LayerIterator::end() and passing in the
// list of layers owning target RenderSurfaces. Step through the tree by incrementing the stepping iterator while it is != to
// the end iterator. At each step the iterator knows what the layer is representing, and you can query the iterator to decide
// what actions to perform with the layer given what it represents.

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Non-templated constants
struct LayerIteratorValue {
    static const int InvalidTargetRenderSurfaceLayerIndex = -1;
    // This must be -1 since the iterator action code assumes that this value can be
    // reached by subtracting one from the position of the first layer in the current
    // target surface's child layer list, which is 0.
    static const int LayerIndexRepresentingTargetRenderSurface = -1;
};

// The position of a layer iterator that is independent of its many template types.
template <typename LayerType>
struct LayerIteratorPosition {
    bool representsTargetRenderSurface;
    bool representsContributingRenderSurface;
    bool representsItself;
    LayerType* targetRenderSurfaceLayer;
    LayerType* currentLayer;
};

// An iterator class for walking over layers in the RenderSurfaceImpl-Layer tree.
template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename IteratorActionType>
class LayerIterator {
    typedef LayerIterator<LayerType, LayerList, RenderSurfaceType, IteratorActionType> LayerIteratorType;

public:
    LayerIterator() : m_renderSurfaceLayerList(0) { }

    static LayerIteratorType begin(const LayerList* renderSurfaceLayerList) { return LayerIteratorType(renderSurfaceLayerList, true); }
    static LayerIteratorType end(const LayerList* renderSurfaceLayerList) { return LayerIteratorType(renderSurfaceLayerList, false); }

    LayerIteratorType& operator++() { m_actions.next(*this); return *this; }
    bool operator==(const LayerIterator& other) const
    {
        return m_targetRenderSurfaceLayerIndex == other.m_targetRenderSurfaceLayerIndex
            && m_currentLayerIndex == other.m_currentLayerIndex;
    }
    bool operator!=(const LayerIteratorType& other) const { return !(*this == other); }

    LayerType* operator->() const { return currentLayer(); }
    LayerType* operator*() const { return currentLayer(); }

    bool representsTargetRenderSurface() const { return currentLayerRepresentsTargetRenderSurface(); }
    bool representsContributingRenderSurface() const { return !representsTargetRenderSurface() && currentLayerRepresentsContributingRenderSurface(); }
    bool representsItself() const { return !representsTargetRenderSurface() && !representsContributingRenderSurface(); }

    LayerType* targetRenderSurfaceLayer() const { return getRawPtr((*m_renderSurfaceLayerList)[m_targetRenderSurfaceLayerIndex]); }

    operator const LayerIteratorPosition<LayerType>() const
    {
        LayerIteratorPosition<LayerType> position;
        position.representsTargetRenderSurface = representsTargetRenderSurface();
        position.representsContributingRenderSurface = representsContributingRenderSurface();
        position.representsItself = representsItself();
        position.targetRenderSurfaceLayer = targetRenderSurfaceLayer();
        position.currentLayer = currentLayer();
        return position;
    }

private:
    LayerIterator(const LayerList* renderSurfaceLayerList, bool start)
        : m_renderSurfaceLayerList(renderSurfaceLayerList)
        , m_targetRenderSurfaceLayerIndex(0)
    {
        for (size_t i = 0; i < renderSurfaceLayerList->size(); ++i) {
            if (!(*renderSurfaceLayerList)[i]->renderSurface()) {
                NOTREACHED();
                m_actions.end(*this);
                return;
            }
        }

        if (start && !renderSurfaceLayerList->empty())
            m_actions.begin(*this);
        else
            m_actions.end(*this);
    }

    inline static Layer* getRawPtr(const scoped_refptr<Layer>& ptr) { return ptr.get(); }
    inline static LayerImpl* getRawPtr(LayerImpl* ptr) { return ptr; }

    inline LayerType* currentLayer() const { return currentLayerRepresentsTargetRenderSurface() ? targetRenderSurfaceLayer() : getRawPtr(targetRenderSurfaceChildren()[m_currentLayerIndex]); }

    inline bool currentLayerRepresentsContributingRenderSurface() const { return LayerTreeHostCommon::renderSurfaceContributesToTarget<LayerType>(currentLayer(), targetRenderSurfaceLayer()->id()); }
    inline bool currentLayerRepresentsTargetRenderSurface() const { return m_currentLayerIndex == LayerIteratorValue::LayerIndexRepresentingTargetRenderSurface; }

    inline RenderSurfaceType* targetRenderSurface() const { return targetRenderSurfaceLayer()->renderSurface(); }
    inline const LayerList& targetRenderSurfaceChildren() const { return targetRenderSurface()->layerList(); }

    IteratorActionType m_actions;
    const LayerList* m_renderSurfaceLayerList;

    // The iterator's current position.

    // A position in the renderSurfaceLayerList. This points to a layer which owns the current target surface.
    // This is a value from 0 to n-1 (n = size of renderSurfaceLayerList = number of surfaces). A value outside of
    // this range (for example, LayerIteratorValue::InvalidTargetRenderSurfaceLayerIndex) is used to
    // indicate a position outside the bounds of the tree.
    int m_targetRenderSurfaceLayerIndex;
    // A position in the list of layers that are children of the current target surface. When pointing to one of
    // these layers, this is a value from 0 to n-1 (n = number of children). Since the iterator must also stop at
    // the layers representing the target surface, this is done by setting the currentLayerIndex to a value of
    // LayerIteratorValue::LayerRepresentingTargetRenderSurface.
    int m_currentLayerIndex;

    friend struct LayerIteratorActions;
};

// Orderings for iterating over the RenderSurfaceImpl-Layer tree.
struct CC_EXPORT LayerIteratorActions {
    // Walks layers sorted by z-order from back to front.
    class CC_EXPORT BackToFront {
    public:
        template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
        void begin(LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>&);

        template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
        void end(LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>&);

        template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
        void next(LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>&);

    private:
        int m_highestTargetRenderSurfaceLayer;
    };

    // Walks layers sorted by z-order from front to back
    class CC_EXPORT FrontToBack {
    public:
        template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
        void begin(LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>&);

        template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
        void end(LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>&);

        template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
        void next(LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>&);

    private:
        template <typename LayerType, typename LayerList, typename RenderSurfaceType, typename ActionType>
        void goToHighestInSubtree(LayerIterator<LayerType, LayerList, RenderSurfaceType, ActionType>&);
    };
};

} // namespace cc

#endif  // CC_LAYER_ITERATOR_H_
