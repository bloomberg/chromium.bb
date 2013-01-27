// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYER_TREE_HOST_COMMON_H_
#define CC_LAYER_TREE_HOST_COMMON_H_

#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "cc/scoped_ptr_vector.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/vector2d.h"

namespace cc {

class LayerImpl;
class Layer;

class CC_EXPORT LayerTreeHostCommon {
public:
    static gfx::Rect calculateVisibleRect(const gfx::Rect& targetSurfaceRect, const gfx::Rect& layerBoundRect, const gfx::Transform&);

    static void calculateDrawProperties(Layer* rootLayer, const gfx::Size& deviceViewportSize, float deviceScaleFactor, float pageScaleFactor, int maxTextureSize, bool canUseLCDText, std::vector<scoped_refptr<Layer> >& renderSurfaceLayerList);
    static void calculateDrawProperties(LayerImpl* rootLayer, const gfx::Size& deviceViewportSize, float deviceScaleFactor, float pageScaleFactor, int maxTextureSize, bool canUseLCDText, std::vector<LayerImpl*>& renderSurfaceLayerList, bool updateTilePriorities);

    // Performs hit testing for a given renderSurfaceLayerList.
    static LayerImpl* findLayerThatIsHitByPoint(const gfx::PointF& screenSpacePoint, const std::vector<LayerImpl*>& renderSurfaceLayerList);

    static LayerImpl* findLayerThatIsHitByPointInTouchHandlerRegion(const gfx::PointF& screenSpacePoint, const std::vector<LayerImpl*>& renderSurfaceLayerList);

    static bool layerHasTouchEventHandlersAt(const gfx::PointF& screenSpacePoint, LayerImpl* layerImpl);

    template<typename LayerType> static bool renderSurfaceContributesToTarget(LayerType*, int targetSurfaceLayerID);

    template<class Function, typename LayerType> static void callFunctionForSubtree(LayerType* rootLayer);

    // Returns a layer with the given id if one exists in the subtree starting
    // from the given root layer (including mask and replica layers).
    template<typename LayerType> static LayerType* findLayerInSubtree(LayerType* rootLayer, int layerId);

    static Layer* getChildAsRawPtr(const std::vector<scoped_refptr<Layer> >& children, size_t index)
    {
        return children[index].get();
    }

    static LayerImpl* getChildAsRawPtr(const ScopedPtrVector<LayerImpl>& children, size_t index)
    {
        return children[index];
    }

    struct ScrollUpdateInfo {
        int layerId;
        gfx::Vector2d scrollDelta;
    };
};

struct CC_EXPORT ScrollAndScaleSet {
    ScrollAndScaleSet();
    ~ScrollAndScaleSet();

    std::vector<LayerTreeHostCommon::ScrollUpdateInfo> scrolls;
    float pageScaleDelta;
};

template<typename LayerType>
bool LayerTreeHostCommon::renderSurfaceContributesToTarget(LayerType* layer, int targetSurfaceLayerID)
{
    // A layer will either contribute its own content, or its render surface's content, to
    // the target surface. The layer contributes its surface's content when both the
    // following are true:
    //  (1) The layer actually has a renderSurface, and
    //  (2) The layer's renderSurface is not the same as the targetSurface.
    //
    // Otherwise, the layer just contributes itself to the target surface.

    return layer->renderSurface() && layer->id() != targetSurfaceLayerID;
}

template<typename LayerType>
LayerType* LayerTreeHostCommon::findLayerInSubtree(LayerType* rootLayer, int layerId)
{
    if (rootLayer->id() == layerId)
        return rootLayer;

    if (rootLayer->maskLayer() && rootLayer->maskLayer()->id() == layerId)
        return rootLayer->maskLayer();

    if (rootLayer->replicaLayer() && rootLayer->replicaLayer()->id() == layerId)
        return rootLayer->replicaLayer();

    for (size_t i = 0; i < rootLayer->children().size(); ++i) {
        if (LayerType* found = findLayerInSubtree(getChildAsRawPtr(rootLayer->children(), i), layerId))
            return found;
    }
    return 0;
}

template<class Function, typename LayerType>
void LayerTreeHostCommon::callFunctionForSubtree(LayerType* rootLayer)
{
    Function()(rootLayer);
   
    if (LayerType* maskLayer = rootLayer->maskLayer())
        Function()(maskLayer);
    if (LayerType* replicaLayer = rootLayer->replicaLayer()) {
        Function()(replicaLayer);
        if (LayerType* maskLayer = replicaLayer->maskLayer())
            Function()(maskLayer);
    }

    for (size_t i = 0; i < rootLayer->children().size(); ++i)
        callFunctionForSubtree<Function>(getChildAsRawPtr(rootLayer->children(), i));
}

}  // namespace cc

#endif  // CC_LAYER_TREE_HOST_COMMON_H_
