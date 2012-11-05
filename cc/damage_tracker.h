// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DAMAGE_TRACKER_H_
#define CC_DAMAGE_TRACKER_H_

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "ui/gfx/rect_f.h"
#include <vector>

class SkImageFilter;

namespace gfx {
class Rect;
}

namespace WebKit {
class WebFilterOperations;
}

namespace cc {

class LayerImpl;
class RenderSurfaceImpl;

// Computes the region where pixels have actually changed on a RenderSurfaceImpl. This region is used
// to scissor what is actually drawn to the screen to save GPU computation and bandwidth.
class CC_EXPORT DamageTracker {
public:
    static scoped_ptr<DamageTracker> create();
    ~DamageTracker();

    void didDrawDamagedArea() { m_currentDamageRect = gfx::RectF(); }
    void forceFullDamageNextUpdate() { m_forceFullDamageNextUpdate = true; }
    void updateDamageTrackingState(const std::vector<LayerImpl*>& layerList, int targetSurfaceLayerID, bool targetSurfacePropertyChangedOnlyFromDescendant, const gfx::Rect& targetSurfaceContentRect, LayerImpl* targetSurfaceMaskLayer, const WebKit::WebFilterOperations&, SkImageFilter* filter);

    const gfx::RectF& currentDamageRect() { return m_currentDamageRect; }

private:
    DamageTracker();

    gfx::RectF trackDamageFromActiveLayers(const std::vector<LayerImpl*>& layerList, int targetSurfaceLayerID);
    gfx::RectF trackDamageFromSurfaceMask(LayerImpl* targetSurfaceMaskLayer);
    gfx::RectF trackDamageFromLeftoverRects();

    gfx::RectF removeRectFromCurrentFrame(int layerID, bool& layerIsNew);
    void saveRectForNextFrame(int layerID, const gfx::RectF& targetSpaceRect);

    // These helper functions are used only in trackDamageFromActiveLayers().
    void extendDamageForLayer(LayerImpl*, gfx::RectF& targetDamageRect);
    void extendDamageForRenderSurface(LayerImpl*, gfx::RectF& targetDamageRect);

    // To correctly track exposed regions, two hashtables of rects are maintained.
    // The "current" map is used to compute exposed regions of the current frame, while
    // the "next" map is used to collect layer rects that are used in the next frame.
    typedef base::hash_map<int, gfx::RectF> RectMap;
    scoped_ptr<RectMap> m_currentRectHistory;
    scoped_ptr<RectMap> m_nextRectHistory;

    gfx::RectF m_currentDamageRect;
    bool m_forceFullDamageNextUpdate;

    DISALLOW_COPY_AND_ASSIGN(DamageTracker);
};

} // namespace cc

#endif  // CC_DAMAGE_TRACKER_H_
