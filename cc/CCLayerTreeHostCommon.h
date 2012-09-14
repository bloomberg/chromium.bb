// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCLayerTreeHostCommon_h
#define CCLayerTreeHostCommon_h

#include "IntRect.h"
#include "IntSize.h"
#include <public/WebTransformationMatrix.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace cc {

class CCLayerImpl;
class CCLayerSorter;
class LayerChromium;

class CCLayerTreeHostCommon {
public:
    static IntRect calculateVisibleRect(const IntRect& targetSurfaceRect, const IntRect& layerBoundRect, const WebKit::WebTransformationMatrix&);

    static void calculateDrawTransforms(LayerChromium* rootLayer, const IntSize& deviceViewportSize, float deviceScaleFactor, int maxTextureSize, Vector<RefPtr<LayerChromium> >& renderSurfaceLayerList);
    static void calculateDrawTransforms(CCLayerImpl* rootLayer, const IntSize& deviceViewportSize, float deviceScaleFactor, CCLayerSorter*, int maxTextureSize, Vector<CCLayerImpl*>& renderSurfaceLayerList);

    static void calculateVisibleRects(Vector<CCLayerImpl*>& renderSurfaceLayerList);
    static void calculateVisibleRects(Vector<RefPtr<LayerChromium> >& renderSurfaceLayerList);

    // Performs hit testing for a given renderSurfaceLayerList.
    static CCLayerImpl* findLayerThatIsHitByPoint(const IntPoint& viewportPoint, Vector<CCLayerImpl*>& renderSurfaceLayerList);

    template<typename LayerType> static bool renderSurfaceContributesToTarget(LayerType*, int targetSurfaceLayerID);

    // Returns a layer with the given id if one exists in the subtree starting
    // from the given root layer (including mask and replica layers).
    template<typename LayerType> static LayerType* findLayerInSubtree(LayerType* rootLayer, int layerId);

    struct ScrollUpdateInfo {
        int layerId;
        IntSize scrollDelta;
    };
};

struct CCScrollAndScaleSet {
    Vector<CCLayerTreeHostCommon::ScrollUpdateInfo> scrolls;
    float pageScaleDelta;
};

template<typename LayerType>
bool CCLayerTreeHostCommon::renderSurfaceContributesToTarget(LayerType* layer, int targetSurfaceLayerID)
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
LayerType* CCLayerTreeHostCommon::findLayerInSubtree(LayerType* rootLayer, int layerId)
{
    if (rootLayer->id() == layerId)
        return rootLayer;

    if (rootLayer->maskLayer() && rootLayer->maskLayer()->id() == layerId)
        return rootLayer->maskLayer();

    if (rootLayer->replicaLayer() && rootLayer->replicaLayer()->id() == layerId)
        return rootLayer->replicaLayer();

    for (size_t i = 0; i < rootLayer->children().size(); ++i) {
        if (LayerType* found = findLayerInSubtree(rootLayer->children()[i].get(), layerId))
            return found;
    }
    return 0;
}

} // namespace cc

#endif
