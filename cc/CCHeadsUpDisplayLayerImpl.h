// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCHeadsUpDisplayLayerImpl_h
#define CCHeadsUpDisplayLayerImpl_h

#include "CCFontAtlas.h"
#include "CCLayerImpl.h"
#include "CCScopedTexture.h"

class SkCanvas;

namespace cc {

class CCDebugRectHistory;
class CCFontAtlas;
class CCFrameRateCounter;

class CCHeadsUpDisplayLayerImpl : public CCLayerImpl {
public:
    static PassOwnPtr<CCHeadsUpDisplayLayerImpl> create(int id)
    {
        return adoptPtr(new CCHeadsUpDisplayLayerImpl(id));
    }
    virtual ~CCHeadsUpDisplayLayerImpl();

    void setFontAtlas(PassOwnPtr<CCFontAtlas>);

    virtual void willDraw(CCResourceProvider*) OVERRIDE;
    virtual void appendQuads(CCQuadSink&, CCAppendQuadsData&) OVERRIDE;
    void updateHudTexture(CCResourceProvider*);
    virtual void didDraw(CCResourceProvider*) OVERRIDE;

    virtual void didLoseContext() OVERRIDE;

    virtual bool layerIsAlwaysDamaged() const OVERRIDE { return true; }

private:
    explicit CCHeadsUpDisplayLayerImpl(int);

    virtual const char* layerTypeAsString() const OVERRIDE { return "HeadsUpDisplayLayer"; }

    void drawHudContents(SkCanvas*);
    void drawFPSCounter(SkCanvas*, CCFrameRateCounter*, int top, int height);
    void drawFPSCounterText(SkCanvas*, CCFrameRateCounter*, int top, int width, int height);
    void drawDebugRects(SkCanvas*, CCDebugRectHistory*);

    OwnPtr<CCFontAtlas> m_fontAtlas;
    OwnPtr<CCScopedTexture> m_hudTexture;
    OwnPtr<SkCanvas> m_hudCanvas;
};

}

#endif // CCHeadsUpDisplayLayerImpl_h
