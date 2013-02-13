// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_HEADS_UP_DISPLAY_LAYER_IMPL_H_
#define CC_HEADS_UP_DISPLAY_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cc/cc_export.h"
#include "cc/layer_impl.h"
#include "cc/scoped_resource.h"

class SkCanvas;
class SkPaint;
class SkTypeface;
struct SkRect;

namespace cc {

class DebugRectHistory;
class FrameRateCounter;
class MemoryHistory;
class PaintTimeCounter;

class CC_EXPORT HeadsUpDisplayLayerImpl : public LayerImpl {
public:
    static scoped_ptr<HeadsUpDisplayLayerImpl> create(LayerTreeImpl* treeImpl, int id)
    {
        return make_scoped_ptr(new HeadsUpDisplayLayerImpl(treeImpl, id));
    }
    virtual ~HeadsUpDisplayLayerImpl();

    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* treeImpl) OVERRIDE;

    virtual void willDraw(ResourceProvider*) OVERRIDE;
    virtual void appendQuads(QuadSink&, AppendQuadsData&) OVERRIDE;
    void updateHudTexture(ResourceProvider*);
    virtual void didDraw(ResourceProvider*) OVERRIDE;

    virtual void didLoseOutputSurface() OVERRIDE;

    virtual bool layerIsAlwaysDamaged() const OVERRIDE;

private:
    struct Graph {
        Graph(double indicatorValue, double startUpperBound);

        // Eases the upper bound, which limits what is currently visible in the graph,
        // so that the graph always scales to either it's max or defaultUpperBound.
        static double updateUpperBound(Graph*);

        double value;
        double min;
        double max;

        double currentUpperBound;
        const double defaultUpperBound;
        const double indicator;
    };

    HeadsUpDisplayLayerImpl(LayerTreeImpl* treeImpl, int id);

    virtual const char* layerTypeAsString() const OVERRIDE;

    void drawHudContents(SkCanvas*);

    void drawText(SkCanvas*, SkPaint*, const std::string&, const SkPaint::Align&, const int& size, const int& x, const int& y);
    void drawText(SkCanvas*, SkPaint*, const std::string&, const SkPaint::Align&, const int& size, const SkPoint& pos);
    void drawGraphBackground(SkCanvas*, SkPaint*, const SkRect& bounds);
    void drawGraphLines(SkCanvas*, SkPaint*, const SkRect& bounds, const Graph&);

    void drawPlaformLayerTree(SkCanvas*);
    int drawFPSDisplay(SkCanvas*, FrameRateCounter*, const int& top);
    int drawMemoryDisplay(SkCanvas*, MemoryHistory*, const int& top);
    int drawPaintTimeDisplay(SkCanvas*, PaintTimeCounter*, const int& top);

    void drawDebugRects(SkCanvas*, DebugRectHistory*);

    scoped_ptr<ScopedResource> m_hudTexture;
    scoped_ptr<SkCanvas> m_hudCanvas;

    skia::RefPtr<SkTypeface> m_typeface;

    Graph m_fpsGraph;
    Graph m_paintTimeGraph;

    base::TimeTicks m_timeOfLastGraphUpdate;
};

}  // namespace cc

#endif  // CC_HEADS_UP_DISPLAY_LAYER_IMPL_H_
