// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCRendererSoftware_h
#define CCRendererSoftware_h

#include "base/basictypes.h"
#include "cc/direct_renderer.h"
#include <public/WebCompositorSoftwareOutputDevice.h>

namespace cc {

class DebugBorderDrawQuad;
class RendererClient;
class ResourceProvider;
class SolidColorDrawQuad;
class TextureDrawQuad;
class TileDrawQuad;
class RenderPassDrawQuad;

class SoftwareRenderer : public DirectRenderer {
public:
    static scoped_ptr<SoftwareRenderer> create(RendererClient*, ResourceProvider*, WebKit::WebCompositorSoftwareOutputDevice*);
    virtual ~SoftwareRenderer();

    virtual const RendererCapabilities& capabilities() const OVERRIDE;

    virtual void viewportChanged() OVERRIDE;

    virtual void finish() OVERRIDE;

    virtual bool swapBuffers() OVERRIDE;

    virtual void getFramebufferPixels(void *pixels, const IntRect&) OVERRIDE;

    virtual void setVisible(bool) OVERRIDE;

protected:
    virtual void bindFramebufferToOutputSurface(DrawingFrame&) OVERRIDE;
    virtual bool bindFramebufferToTexture(DrawingFrame&, const ScopedTexture*, const gfx::Rect& framebufferRect) OVERRIDE;
    virtual void setDrawViewportSize(const gfx::Size&) OVERRIDE;
    virtual void enableScissorTestRect(const gfx::Rect& scissorRect) OVERRIDE;
    virtual void disableScissorTest() OVERRIDE;
    virtual void clearFramebuffer(DrawingFrame&) OVERRIDE;
    virtual void drawQuad(DrawingFrame&, const DrawQuad*) OVERRIDE;
    virtual void beginDrawingFrame(DrawingFrame&) OVERRIDE;
    virtual void finishDrawingFrame(DrawingFrame&) OVERRIDE;
    virtual bool flippedFramebuffer() const OVERRIDE;

private:
    SoftwareRenderer(RendererClient*, ResourceProvider*, WebKit::WebCompositorSoftwareOutputDevice*);

    bool isSoftwareResource(ResourceProvider::ResourceId) const;

    void drawDebugBorderQuad(const DrawingFrame&, const DebugBorderDrawQuad*);
    void drawSolidColorQuad(const DrawingFrame&, const SolidColorDrawQuad*);
    void drawTextureQuad(const DrawingFrame&, const TextureDrawQuad*);
    void drawTileQuad(const DrawingFrame&, const TileDrawQuad*);
    void drawRenderPassQuad(const DrawingFrame& frame, const RenderPassDrawQuad*);
    void drawUnsupportedQuad(const DrawingFrame&, const DrawQuad*);

    RendererCapabilities m_capabilities;
    bool m_visible;

    WebKit::WebCompositorSoftwareOutputDevice* m_outputDevice;
    scoped_ptr<SkCanvas> m_skRootCanvas;
    SkCanvas* m_skCurrentCanvas;
    SkPaint m_skCurrentPaint;
    scoped_ptr<ResourceProvider::ScopedWriteLockSoftware> m_currentFramebufferLock;

    DISALLOW_COPY_AND_ASSIGN(SoftwareRenderer);
};

}

#endif
