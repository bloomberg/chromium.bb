// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_RECT_HISTORY_H_
#define CC_DEBUG_RECT_HISTORY_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include <vector>

namespace cc {

class LayerImpl;
class LayerTreeDebugState;

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
// - Non-Occluding rects: these are the regions of composited layers that do not
//   contribute to the occluded region.
//
enum DebugRectType { PaintRectType, PropertyChangedRectType, SurfaceDamageRectType, ScreenSpaceRectType, ReplicaScreenSpaceRectType, OccludingRectType, NonOccludingRectType };

struct DebugRect {
    DebugRect(DebugRectType newType, gfx::RectF newRect)
            : type(newType)
            , rect(newRect) { }

    DebugRectType type;
    gfx::RectF rect;
};

// This class maintains a history of rects of various types that can be used
// for debugging purposes. The overhead of collecting rects is performed only if
// the appropriate LayerTreeSettings are enabled.
class DebugRectHistory {
public:
    static scoped_ptr<DebugRectHistory> create();

    ~DebugRectHistory();

    // Note: Saving debug rects must happen before layers' change tracking is reset.
    void saveDebugRectsForCurrentFrame(LayerImpl* rootLayer, const std::vector<LayerImpl*>& renderSurfaceLayerList, const std::vector<gfx::Rect>& occludingScreenSpaceRects, const std::vector<gfx::Rect>& nonOccludingScreenSpaceRects, const LayerTreeDebugState& debugState);

    const std::vector<DebugRect>& debugRects() { return m_debugRects; }

private:
    DebugRectHistory();

    void savePaintRects(LayerImpl*);
    void savePropertyChangedRects(const std::vector<LayerImpl*>& renderSurfaceLayerList);
    void saveSurfaceDamageRects(const std::vector<LayerImpl* >& renderSurfaceLayerList);
    void saveScreenSpaceRects(const std::vector<LayerImpl* >& renderSurfaceLayerList);
    void saveOccludingRects(const std::vector<gfx::Rect>& occludingScreenSpaceRects);
    void saveNonOccludingRects(const std::vector<gfx::Rect>& nonOccludingScreenSpaceRects);

    std::vector<DebugRect> m_debugRects;

    DISALLOW_COPY_AND_ASSIGN(DebugRectHistory);
};

}  // namespace cc

#endif  // CC_DEBUG_RECT_HISTORY_H_
