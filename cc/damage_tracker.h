// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCDamageTracker_h
#define CCDamageTracker_h

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "FloatRect.h"
#include <vector>

namespace WebKit {
class WebFilterOperations;
}

namespace cc {

class LayerImpl;
class RenderSurfaceImpl;

// Computes the region where pixels have actually changed on a RenderSurfaceImpl. This region is used
// to scissor what is actually drawn to the screen to save GPU computation and bandwidth.
class DamageTracker {
public:
    static scoped_ptr<DamageTracker> create();
    ~DamageTracker();

    void didDrawDamagedArea() { m_currentDamageRect = FloatRect(); }
    void forceFullDamageNextUpdate() { m_forceFullDamageNextUpdate = true; }
    void updateDamageTrackingState(const std::vector<LayerImpl*>& layerList, int targetSurfaceLayerID, bool targetSurfacePropertyChangedOnlyFromDescendant, const IntRect& targetSurfaceContentRect, LayerImpl* targetSurfaceMaskLayer, const WebKit::WebFilterOperations&);

    const FloatRect& currentDamageRect() { return m_currentDamageRect; }

private:
    DamageTracker();

    FloatRect trackDamageFromActiveLayers(const std::vector<LayerImpl*>& layerList, int targetSurfaceLayerID);
    FloatRect trackDamageFromSurfaceMask(LayerImpl* targetSurfaceMaskLayer);
    FloatRect trackDamageFromLeftoverRects();

    FloatRect removeRectFromCurrentFrame(int layerID, bool& layerIsNew);
    void saveRectForNextFrame(int layerID, const FloatRect& targetSpaceRect);

    // These helper functions are used only in trackDamageFromActiveLayers().
    void extendDamageForLayer(LayerImpl*, FloatRect& targetDamageRect);
    void extendDamageForRenderSurface(LayerImpl*, FloatRect& targetDamageRect);

    // To correctly track exposed regions, two hashtables of rects are maintained.
    // The "current" map is used to compute exposed regions of the current frame, while
    // the "next" map is used to collect layer rects that are used in the next frame.
    typedef base::hash_map<int, FloatRect> RectMap;
    scoped_ptr<RectMap> m_currentRectHistory;
    scoped_ptr<RectMap> m_nextRectHistory;

    FloatRect m_currentDamageRect;
    bool m_forceFullDamageNextUpdate;
};

} // namespace cc

#endif // CCDamageTracker_h
