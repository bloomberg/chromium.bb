// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCRendererSoftware_h
#define CCRendererSoftware_h

#include "CCDirectRenderer.h"
#include "CCLayerTreeHost.h"
#include <public/WebCompositorSoftwareOutputDevice.h>

namespace cc {

class CCDebugBorderDrawQuad;
class CCRendererClient;
class CCResourceProvider;
class CCSolidColorDrawQuad;
class CCTextureDrawQuad;
class CCTileDrawQuad;

class CCRendererSoftware : public CCDirectRenderer {
    WTF_MAKE_NONCOPYABLE(CCRendererSoftware);
public:
    static PassOwnPtr<CCRendererSoftware> create(CCRendererClient*, CCResourceProvider*, WebKit::WebCompositorSoftwareOutputDevice*);
    virtual ~CCRendererSoftware();

    virtual const RendererCapabilities& capabilities() const OVERRIDE;

    virtual void viewportChanged() OVERRIDE;

    virtual void finish() OVERRIDE;

    virtual bool swapBuffers() OVERRIDE;

    virtual void getFramebufferPixels(void *pixels, const IntRect&) OVERRIDE;

    virtual void setVisible(bool) OVERRIDE;

protected:
    virtual void bindFramebufferToOutputSurface(DrawingFrame&) OVERRIDE;
    virtual bool bindFramebufferToTexture(DrawingFrame&, const CCScopedTexture*, const IntRect& framebufferRect) OVERRIDE;
    virtual void setDrawViewportSize(const IntSize&) OVERRIDE;
    virtual void enableScissorTestRect(const IntRect& scissorRect) OVERRIDE;
    virtual void disableScissorTest() OVERRIDE;
    virtual void clearFramebuffer(DrawingFrame&) OVERRIDE;
    virtual void drawQuad(DrawingFrame&, const CCDrawQuad*) OVERRIDE;
    virtual void beginDrawingFrame(DrawingFrame&) OVERRIDE;
    virtual void finishDrawingFrame(DrawingFrame&) OVERRIDE;
    virtual bool flippedFramebuffer() const OVERRIDE;

private:
    CCRendererSoftware(CCRendererClient*, CCResourceProvider*, WebKit::WebCompositorSoftwareOutputDevice*);

    bool isSoftwareResource(CCResourceProvider::ResourceId) const;

    void drawDebugBorderQuad(const DrawingFrame&, const CCDebugBorderDrawQuad*);
    void drawSolidColorQuad(const DrawingFrame&, const CCSolidColorDrawQuad*);
    void drawTextureQuad(const DrawingFrame&, const CCTextureDrawQuad*);
    void drawTileQuad(const DrawingFrame&, const CCTileDrawQuad*);
    void drawUnsupportedQuad(const DrawingFrame&, const CCDrawQuad*);

    RendererCapabilities m_capabilities;
    bool m_visible;

    WebKit::WebCompositorSoftwareOutputDevice* m_outputDevice;
    SkCanvas m_skRootCanvas;
    SkCanvas* m_skCurrentCanvas;
    SkPaint m_skCurrentPaint;
    OwnPtr<CCResourceProvider::ScopedWriteLockSoftware> m_currentFramebufferLock;
};

}

#endif
