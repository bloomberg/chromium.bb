// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCDamageTracker_h
#define CCDamageTracker_h

#include "FloatRect.h"
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebKit {
class WebFilterOperations;
}

namespace cc {

class CCLayerImpl;
class CCRenderSurface;

// Computes the region where pixels have actually changed on a RenderSurface. This region is used
// to scissor what is actually drawn to the screen to save GPU computation and bandwidth.
class CCDamageTracker {
public:
    static PassOwnPtr<CCDamageTracker> create();
    ~CCDamageTracker();

    void didDrawDamagedArea() { m_currentDamageRect = FloatRect(); }
    void forceFullDamageNextUpdate() { m_forceFullDamageNextUpdate = true; }
    void updateDamageTrackingState(const Vector<CCLayerImpl*>& layerList, int targetSurfaceLayerID, bool targetSurfacePropertyChangedOnlyFromDescendant, const IntRect& targetSurfaceContentRect, CCLayerImpl* targetSurfaceMaskLayer, const WebKit::WebFilterOperations&);

    const FloatRect& currentDamageRect() { return m_currentDamageRect; }

private:
    CCDamageTracker();

    FloatRect trackDamageFromActiveLayers(const Vector<CCLayerImpl*>& layerList, int targetSurfaceLayerID);
    FloatRect trackDamageFromSurfaceMask(CCLayerImpl* targetSurfaceMaskLayer);
    FloatRect trackDamageFromLeftoverRects();

    FloatRect removeRectFromCurrentFrame(int layerID, bool& layerIsNew);
    void saveRectForNextFrame(int layerID, const FloatRect& targetSpaceRect);

    // These helper functions are used only in trackDamageFromActiveLayers().
    void extendDamageForLayer(CCLayerImpl*, FloatRect& targetDamageRect);
    void extendDamageForRenderSurface(CCLayerImpl*, FloatRect& targetDamageRect);

    // To correctly track exposed regions, two hashtables of rects are maintained.
    // The "current" map is used to compute exposed regions of the current frame, while
    // the "next" map is used to collect layer rects that are used in the next frame.
    typedef HashMap<int, FloatRect> RectMap;
    OwnPtr<RectMap> m_currentRectHistory;
    OwnPtr<RectMap> m_nextRectHistory;

    FloatRect m_currentDamageRect;
    bool m_forceFullDamageNextUpdate;
};

} // namespace cc

#endif // CCDamageTracker_h
