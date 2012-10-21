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

class CCDebugRectHistory;
class CCFontAtlas;
class CCFrameRateCounter;

class CCHeadsUpDisplayLayerImpl : public CCLayerImpl {
public:
    static scoped_ptr<CCHeadsUpDisplayLayerImpl> create(int id)
    {
        return make_scoped_ptr(new CCHeadsUpDisplayLayerImpl(id));
    }
    virtual ~CCHeadsUpDisplayLayerImpl();

    void setFontAtlas(scoped_ptr<CCFontAtlas>);

    virtual void willDraw(CCResourceProvider*) OVERRIDE;
    virtual void appendQuads(CCQuadSink&, CCAppendQuadsData&) OVERRIDE;
    void updateHudTexture(CCResourceProvider*);
    virtual void didDraw(CCResourceProvider*) OVERRIDE;

    virtual void didLoseContext() OVERRIDE;

    virtual bool layerIsAlwaysDamaged() const OVERRIDE;

private:
    explicit CCHeadsUpDisplayLayerImpl(int);

    virtual const char* layerTypeAsString() const OVERRIDE;

    void drawHudContents(SkCanvas*);
    void drawFPSCounter(SkCanvas*, CCFrameRateCounter*, int top, int height);
    void drawFPSCounterText(SkCanvas*, CCFrameRateCounter*, int top, int width, int height);
    void drawDebugRects(SkCanvas*, CCDebugRectHistory*);

    scoped_ptr<CCFontAtlas> m_fontAtlas;
    scoped_ptr<CCScopedTexture> m_hudTexture;
    scoped_ptr<SkCanvas> m_hudCanvas;
};

}  // namespace cc

#endif // CCHeadsUpDisplayLayerImpl_h
