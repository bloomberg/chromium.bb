// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCLayerTreeHostCommon_h
#define CCLayerTreeHostCommon_h

#include "base/memory/ref_counted.h"
#include "cc/scoped_ptr_vector.h"
#include "IntRect.h"
#include "IntSize.h"
#include <public/WebTransformationMatrix.h>
#include <wtf/Vector.h>

namespace cc {

class LayerImpl;
class LayerSorter;
class Layer;

class LayerTreeHostCommon {
public:
    static IntRect calculateVisibleRect(const IntRect& targetSurfaceRect, const IntRect& layerBoundRect, const WebKit::WebTransformationMatrix&);

    static void calculateDrawTransforms(Layer* rootLayer, const IntSize& deviceViewportSize, float deviceScaleFactor, int maxTextureSize, std::vector<scoped_refptr<Layer> >& renderSurfaceLayerList);
    static void calculateDrawTransforms(LayerImpl* rootLayer, const IntSize& deviceViewportSize, float deviceScaleFactor, LayerSorter*, int maxTextureSize, std::vector<LayerImpl*>& renderSurfaceLayerList);

    // Performs hit testing for a given renderSurfaceLayerList.
    static LayerImpl* findLayerThatIsHitByPoint(const IntPoint& screenSpacePoint, std::vector<LayerImpl*>& renderSurfaceLayerList);

    template<typename LayerType> static bool renderSurfaceContributesToTarget(LayerType*, int targetSurfaceLayerID);

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
        IntSize scrollDelta;
    };
};

struct ScrollAndScaleSet {
    ScrollAndScaleSet();
    ~ScrollAndScaleSet();

    Vector<LayerTreeHostCommon::ScrollUpdateInfo> scrolls;
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

} // namespace cc

#endif
