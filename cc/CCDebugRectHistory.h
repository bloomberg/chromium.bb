// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCDebugRectHistory_h
#define CCDebugRectHistory_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatRect.h"
#include "IntRect.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace cc {

class CCLayerImpl;
struct CCLayerTreeSettings;

// There are currently six types of debug rects:
//
// - Paint rects (update rects): regions of a layer that needed to be re-uploaded to the
//   texture resource; in most cases implying that they had to be repainted, too.
//
// - Property-changed rects: enclosing bounds of layers that cause changes to the screen
//   even if the layer did not change internally. (For example, if the layer's opacity or
//   position changes.)
//
// - Surface damage rects: the aggregate damage on a target surface that is caused by all
//   layers and surfaces that contribute to it. This includes (1) paint rects, (2) property-
//   changed rects, and (3) newly exposed areas.
//
// - Screen space rects: this is the region the contents occupy in screen space.
//
// - Replica screen space rects: this is the region the replica's contents occupy in screen space.
//
// - Occluding rects: these are the regions that contribute to the occluded region.
//
enum DebugRectType { PaintRectType, PropertyChangedRectType, SurfaceDamageRectType, ScreenSpaceRectType, ReplicaScreenSpaceRectType, OccludingRectType };

struct CCDebugRect {
    CCDebugRect(DebugRectType newType, FloatRect newRect)
            : type(newType)
            , rect(newRect) { }

    DebugRectType type;
    FloatRect rect;
};

// This class maintains a history of rects of various types that can be used
// for debugging purposes. The overhead of collecting rects is performed only if
// the appropriate CCLayerTreeSettings are enabled.
class CCDebugRectHistory {
    WTF_MAKE_NONCOPYABLE(CCDebugRectHistory);
public:
    static PassOwnPtr<CCDebugRectHistory> create()
    {
        return adoptPtr(new CCDebugRectHistory());
    }

    // Note: Saving debug rects must happen before layers' change tracking is reset.
    void saveDebugRectsForCurrentFrame(CCLayerImpl* rootLayer, const Vector<CCLayerImpl*>& renderSurfaceLayerList, const Vector<IntRect>& occludingScreenSpaceRects, const CCLayerTreeSettings&);

    const Vector<CCDebugRect>& debugRects() { return m_debugRects; }

private:
    CCDebugRectHistory();

    void savePaintRects(CCLayerImpl*);
    void savePropertyChangedRects(const Vector<CCLayerImpl*>& renderSurfaceLayerList);
    void saveSurfaceDamageRects(const Vector<CCLayerImpl* >& renderSurfaceLayerList);
    void saveScreenSpaceRects(const Vector<CCLayerImpl* >& renderSurfaceLayerList);
    void saveOccludingRects(const Vector<IntRect>& occludingScreenSpaceRects);

    Vector<CCDebugRect> m_debugRects;
};

} // namespace cc

#endif // USE(ACCELERATED_COMPOSITING)

#endif
