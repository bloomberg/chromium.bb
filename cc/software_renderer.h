// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SOFTWARE_RENDERER_H_
#define CC_SOFTWARE_RENDERER_H_

#include "base/basictypes.h"
#include "cc/cc_export.h"
#include "cc/direct_renderer.h"

namespace cc {

class DebugBorderDrawQuad;
class RendererClient;
class RenderPassDrawQuad;
class ResourceProvider;
class SoftwareOutputDevice;
class SolidColorDrawQuad;
class TextureDrawQuad;
class TileDrawQuad;

class CC_EXPORT SoftwareRenderer : public DirectRenderer {
public:
    static scoped_ptr<SoftwareRenderer> create(RendererClient*, ResourceProvider*, SoftwareOutputDevice*);
    virtual ~SoftwareRenderer();

    virtual const RendererCapabilities& capabilities() const OVERRIDE;

    virtual void viewportChanged() OVERRIDE;

    virtual void finish() OVERRIDE;

    virtual bool swapBuffers() OVERRIDE;

    virtual void getFramebufferPixels(void *pixels, const gfx::Rect&) OVERRIDE;

    virtual void setVisible(bool) OVERRIDE;

    virtual void sendManagedMemoryStats(size_t bytesVisible, size_t bytesVisibleAndNearby, size_t bytesAllocated) OVERRIDE  { }

protected:
    virtual void bindFramebufferToOutputSurface(DrawingFrame&) OVERRIDE;
    virtual bool bindFramebufferToTexture(DrawingFrame&, const ScopedResource*, const gfx::Rect& framebufferRect) OVERRIDE;
    virtual void setDrawViewportSize(const gfx::Size&) OVERRIDE;
    virtual void setScissorTestRect(const gfx::Rect& scissorRect) OVERRIDE;
    virtual void clearFramebuffer(DrawingFrame&) OVERRIDE;
    virtual void drawQuad(DrawingFrame&, const DrawQuad*) OVERRIDE;
    virtual void beginDrawingFrame(DrawingFrame&) OVERRIDE;
    virtual void finishDrawingFrame(DrawingFrame&) OVERRIDE;
    virtual bool flippedFramebuffer() const OVERRIDE;
    virtual void ensureScissorTestEnabled() OVERRIDE;
    virtual void ensureScissorTestDisabled() OVERRIDE;

private:
    SoftwareRenderer(RendererClient*, ResourceProvider*, SoftwareOutputDevice*);

    void clearCanvas(SkColor color);
    void setClipRect(const gfx::Rect& rect);
    bool isSoftwareResource(ResourceProvider::ResourceId) const;

    void drawDebugBorderQuad(const DrawingFrame&, const DebugBorderDrawQuad*);
    void drawSolidColorQuad(const DrawingFrame&, const SolidColorDrawQuad*);
    void drawTextureQuad(const DrawingFrame&, const TextureDrawQuad*);
    void drawTileQuad(const DrawingFrame&, const TileDrawQuad*);
    void drawRenderPassQuad(const DrawingFrame& frame, const RenderPassDrawQuad*);
    void drawUnsupportedQuad(const DrawingFrame&, const DrawQuad*);

    RendererCapabilities m_capabilities;
    bool m_visible;
    bool m_isScissorEnabled;
    gfx::Rect m_scissorRect;

    SoftwareOutputDevice* m_outputDevice;
    scoped_ptr<SkCanvas> m_skRootCanvas;
    SkCanvas* m_skCurrentCanvas;
    SkPaint m_skCurrentPaint;
    scoped_ptr<ResourceProvider::ScopedWriteLockSoftware> m_currentFramebufferLock;

    DISALLOW_COPY_AND_ASSIGN(SoftwareRenderer);
};

}

#endif  // CC_SOFTWARE_RENDERER_H_
