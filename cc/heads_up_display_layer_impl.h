// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_HEADS_UP_DISPLAY_LAYER_IMPL_H_
#define CC_HEADS_UP_DISPLAY_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cc/cc_export.h"
#include "cc/font_atlas.h"
#include "cc/layer_impl.h"
#include "cc/scoped_resource.h"

class SkCanvas;
class SkPaint;
struct SkRect;

namespace cc {

class DebugRectHistory;
class FontAtlas;
class FrameRateCounter;

class CC_EXPORT HeadsUpDisplayLayerImpl : public LayerImpl {
public:
    static scoped_ptr<HeadsUpDisplayLayerImpl> create(LayerTreeImpl* treeImpl, int id)
    {
        return make_scoped_ptr(new HeadsUpDisplayLayerImpl(treeImpl, id));
    }
    virtual ~HeadsUpDisplayLayerImpl();

    void setFontAtlas(scoped_ptr<FontAtlas>);

    virtual void willDraw(ResourceProvider*) OVERRIDE;
    virtual void appendQuads(QuadSink&, AppendQuadsData&) OVERRIDE;
    void updateHudTexture(ResourceProvider*);
    virtual void didDraw(ResourceProvider*) OVERRIDE;

    virtual void didLoseOutputSurface() OVERRIDE;

    virtual bool layerIsAlwaysDamaged() const OVERRIDE;

private:
    HeadsUpDisplayLayerImpl(LayerTreeImpl* treeImpl, int id);

    virtual const char* layerTypeAsString() const OVERRIDE;

    void drawHudContents(SkCanvas*);
    int drawFPSCounter(SkCanvas*, FrameRateCounter*);
    void drawFPSCounterText(SkCanvas*, SkPaint&, FrameRateCounter*, SkRect);
    void drawFPSCounterGraphAndHistogram(SkCanvas* canvas, SkPaint& paint, FrameRateCounter* fpsCounter, SkRect graphBounds, SkRect histogramBounds);
    void drawDebugRects(SkCanvas*, DebugRectHistory*);

    scoped_ptr<FontAtlas> m_fontAtlas;
    scoped_ptr<ScopedResource> m_hudTexture;
    scoped_ptr<SkCanvas> m_hudCanvas;

    double m_averageFPS;
    double m_minFPS;
    double m_maxFPS;

    base::TimeTicks textUpdateTime;
};

}  // namespace cc

#endif  // CC_HEADS_UP_DISPLAY_LAYER_IMPL_H_
