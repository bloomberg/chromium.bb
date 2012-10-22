// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCHeadsUpDisplayLayerImpl_h
#define CCHeadsUpDisplayLayerImpl_h

#include "CCFontAtlas.h"
#include "CCLayerImpl.h"
#include "base/memory/scoped_ptr.h"
#include "cc/scoped_texture.h"

class SkCanvas;

namespace cc {

class DebugRectHistory;
class FontAtlas;
class FrameRateCounter;

class HeadsUpDisplayLayerImpl : public LayerImpl {
public:
    static scoped_ptr<HeadsUpDisplayLayerImpl> create(int id)
    {
        return make_scoped_ptr(new HeadsUpDisplayLayerImpl(id));
    }
    virtual ~HeadsUpDisplayLayerImpl();

    void setFontAtlas(scoped_ptr<FontAtlas>);

    virtual void willDraw(ResourceProvider*) OVERRIDE;
    virtual void appendQuads(QuadSink&, AppendQuadsData&) OVERRIDE;
    void updateHudTexture(ResourceProvider*);
    virtual void didDraw(ResourceProvider*) OVERRIDE;

    virtual void didLoseContext() OVERRIDE;

    virtual bool layerIsAlwaysDamaged() const OVERRIDE;

private:
    explicit HeadsUpDisplayLayerImpl(int);

    virtual const char* layerTypeAsString() const OVERRIDE;

    void drawHudContents(SkCanvas*);
    void drawFPSCounter(SkCanvas*, FrameRateCounter*, int top, int height);
    void drawFPSCounterText(SkCanvas*, FrameRateCounter*, int top, int width, int height);
    void drawDebugRects(SkCanvas*, DebugRectHistory*);

    scoped_ptr<FontAtlas> m_fontAtlas;
    scoped_ptr<ScopedTexture> m_hudTexture;
    scoped_ptr<SkCanvas> m_hudCanvas;
};

}  // namespace cc

#endif // CCHeadsUpDisplayLayerImpl_h
