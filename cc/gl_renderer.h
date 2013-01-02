// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_GL_RENDERER_H_
#define CC_GL_RENDERER_H_

#include "cc/cc_export.h"
#include "cc/checkerboard_draw_quad.h"
#include "cc/debug_border_draw_quad.h"
#include "cc/direct_renderer.h"
#include "cc/gl_renderer_draw_cache.h"
#include "cc/io_surface_draw_quad.h"
#include "cc/output_surface.h"
#include "cc/render_pass_draw_quad.h"
#include "cc/renderer.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/tile_draw_quad.h"
#include "cc/yuv_video_draw_quad.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsMemoryAllocation.h"
#include "ui/gfx/quad_f.h"

namespace cc {

class ScopedResource;
class StreamVideoDrawQuad;
class TextureDrawQuad;
class GeometryBinding;
class ScopedEnsureFramebufferAllocation;

// Class that handles drawing of composited render layers using GL.
class CC_EXPORT GLRenderer : public DirectRenderer,
                             public NON_EXPORTED_BASE(WebKit::WebGraphicsContext3D::WebGraphicsSwapBuffersCompleteCallbackCHROMIUM),
                             public NON_EXPORTED_BASE(WebKit::WebGraphicsContext3D::WebGraphicsMemoryAllocationChangedCallbackCHROMIUM),
                             public NON_EXPORTED_BASE(WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback) {
public:
    static scoped_ptr<GLRenderer> create(RendererClient*, OutputSurface*, ResourceProvider*);

    virtual ~GLRenderer();

    virtual const RendererCapabilities& capabilities() const OVERRIDE;

    WebKit::WebGraphicsContext3D* context();

    virtual void viewportChanged() OVERRIDE;

    // waits for rendering to finish
    virtual void finish() OVERRIDE;

    virtual void doNoOp() OVERRIDE;
    // puts backbuffer onscreen
    virtual bool swapBuffers() OVERRIDE;

    virtual void getFramebufferPixels(void *pixels, const gfx::Rect&) OVERRIDE;

    virtual bool isContextLost() OVERRIDE;

    virtual void setVisible(bool) OVERRIDE;

    virtual void sendManagedMemoryStats(size_t bytesVisible, size_t bytesVisibleAndNearby, size_t bytesAllocated) OVERRIDE;

protected:
    GLRenderer(RendererClient*, OutputSurface*, ResourceProvider*);

    static void debugGLCall(WebKit::WebGraphicsContext3D*, const char* command, const char* file, int line);

    bool isBackbufferDiscarded() const { return m_isBackbufferDiscarded; }
    bool initialize();

    const gfx::QuadF& sharedGeometryQuad() const { return m_sharedGeometryQuad; }
    const GeometryBinding* sharedGeometry() const { return m_sharedGeometry.get(); }

    bool getFramebufferTexture(ScopedResource*, const gfx::Rect& deviceRect);
    void releaseRenderPassTextures();

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
    virtual void finishDrawingQuadList() OVERRIDE;

private:
    static void toGLMatrix(float*, const gfx::Transform&);
    static ManagedMemoryPolicy::PriorityCutoff priorityCutoff(WebKit::WebGraphicsMemoryAllocation::PriorityCutoff);

    void drawCheckerboardQuad(const DrawingFrame&, const CheckerboardDrawQuad*);
    void drawDebugBorderQuad(const DrawingFrame&, const DebugBorderDrawQuad*);
    scoped_ptr<ScopedResource> drawBackgroundFilters(
        DrawingFrame&, const RenderPassDrawQuad*,
        const gfx::Transform& contentsDeviceTransform,
        const gfx::Transform& contentsDeviceTransformInverse);
    void drawRenderPassQuad(DrawingFrame&, const RenderPassDrawQuad*);
    void drawSolidColorQuad(const DrawingFrame&, const SolidColorDrawQuad*);
    void drawStreamVideoQuad(const DrawingFrame&, const StreamVideoDrawQuad*);
    void drawTextureQuad(const DrawingFrame&, const TextureDrawQuad*);
    void enqueueTextureQuad(const DrawingFrame&, const TextureDrawQuad*);
    void flushTextureQuadCache();
    void drawIOSurfaceQuad(const DrawingFrame&, const IOSurfaceDrawQuad*);
    void drawTileQuad(const DrawingFrame&, const TileDrawQuad*);
    void drawYUVVideoQuad(const DrawingFrame&, const YUVVideoDrawQuad*);

    void setShaderOpacity(float opacity, int alphaLocation);
    void setShaderQuadF(const gfx::QuadF&, int quadLocation);
    void drawQuadGeometry(const DrawingFrame&, const gfx::Transform& drawTransform, const gfx::RectF& quadRect, int matrixLocation);
    void setBlendEnabled(bool enabled);
    void setUseProgram(unsigned program);

    void copyTextureToFramebuffer(const DrawingFrame&, int textureId, const gfx::Rect&, const gfx::Transform& drawMatrix);

    bool useScopedTexture(DrawingFrame&, const ScopedResource*, const gfx::Rect& viewportRect);

    bool makeContextCurrent();

    bool initializeSharedObjects();
    void cleanupSharedObjects();

    // WebKit::WebGraphicsContext3D::WebGraphicsSwapBuffersCompleteCallbackCHROMIUM implementation.
    virtual void onSwapBuffersComplete() OVERRIDE;

    // WebKit::WebGraphicsContext3D::WebGraphicsMemoryAllocationChangedCallbackCHROMIUM implementation.
    virtual void onMemoryAllocationChanged(WebKit::WebGraphicsMemoryAllocation) OVERRIDE;
    void discardBackbuffer();
    void ensureBackbuffer();
    void enforceMemoryPolicy();

    // WebGraphicsContext3D::WebGraphicsContextLostCallback implementation.
    virtual void onContextLost() OVERRIDE;

    RendererCapabilities m_capabilities;

    unsigned m_offscreenFramebufferId;

    scoped_ptr<GeometryBinding> m_sharedGeometry;
    gfx::QuadF m_sharedGeometryQuad;

    // This block of bindings defines all of the programs used by the compositor itself.

    // Tiled layer shaders.
    typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexAlpha> TileProgram;
    typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexClampAlphaAA> TileProgramAA;
    typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexClampSwizzleAlphaAA> TileProgramSwizzleAA;
    typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexOpaque> TileProgramOpaque;
    typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexSwizzleAlpha> TileProgramSwizzle;
    typedef ProgramBinding<VertexShaderTile, FragmentShaderRGBATexSwizzleOpaque> TileProgramSwizzleOpaque;
    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderCheckerboard> TileCheckerboardProgram;

    // Render surface shaders.
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexAlpha> RenderPassProgram;
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexAlphaMask> RenderPassMaskProgram;
    typedef ProgramBinding<VertexShaderQuad, FragmentShaderRGBATexAlphaAA> RenderPassProgramAA;
    typedef ProgramBinding<VertexShaderQuad, FragmentShaderRGBATexAlphaMaskAA> RenderPassMaskProgramAA;

    // Texture shaders.
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexVaryingAlpha> TextureProgram;
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexFlipVaryingAlpha> TextureProgramFlip;
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexRectVaryingAlpha> TextureIOSurfaceProgram;

    // Video shaders.
    typedef ProgramBinding<VertexShaderVideoTransform, FragmentShaderOESImageExternal> VideoStreamTextureProgram;
    typedef ProgramBinding<VertexShaderPosTexYUVStretch, FragmentShaderYUVVideo> VideoYUVProgram;

    // Special purpose / effects shaders.
    typedef ProgramBinding<VertexShaderPos, FragmentShaderColor> SolidColorProgram;

    const TileProgram* tileProgram();
    const TileProgramOpaque* tileProgramOpaque();
    const TileProgramAA* tileProgramAA();
    const TileProgramSwizzle* tileProgramSwizzle();
    const TileProgramSwizzleOpaque* tileProgramSwizzleOpaque();
    const TileProgramSwizzleAA* tileProgramSwizzleAA();
    const TileCheckerboardProgram* tileCheckerboardProgram();

    const RenderPassProgram* renderPassProgram();
    const RenderPassProgramAA* renderPassProgramAA();
    const RenderPassMaskProgram* renderPassMaskProgram();
    const RenderPassMaskProgramAA* renderPassMaskProgramAA();

    const TextureProgram* textureProgram();
    const TextureProgramFlip* textureProgramFlip();
    const TextureIOSurfaceProgram* textureIOSurfaceProgram();

    const VideoYUVProgram* videoYUVProgram();
    const VideoStreamTextureProgram* videoStreamTextureProgram();

    const SolidColorProgram* solidColorProgram();

    scoped_ptr<TileProgram> m_tileProgram;
    scoped_ptr<TileProgramOpaque> m_tileProgramOpaque;
    scoped_ptr<TileProgramAA> m_tileProgramAA;
    scoped_ptr<TileProgramSwizzle> m_tileProgramSwizzle;
    scoped_ptr<TileProgramSwizzleOpaque> m_tileProgramSwizzleOpaque;
    scoped_ptr<TileProgramSwizzleAA> m_tileProgramSwizzleAA;
    scoped_ptr<TileCheckerboardProgram> m_tileCheckerboardProgram;

    scoped_ptr<RenderPassProgram> m_renderPassProgram;
    scoped_ptr<RenderPassProgramAA> m_renderPassProgramAA;
    scoped_ptr<RenderPassMaskProgram> m_renderPassMaskProgram;
    scoped_ptr<RenderPassMaskProgramAA> m_renderPassMaskProgramAA;

    scoped_ptr<TextureProgram> m_textureProgram;
    scoped_ptr<TextureProgramFlip> m_textureProgramFlip;
    scoped_ptr<TextureIOSurfaceProgram> m_textureIOSurfaceProgram;

    scoped_ptr<VideoYUVProgram> m_videoYUVProgram;
    scoped_ptr<VideoStreamTextureProgram> m_videoStreamTextureProgram;

    scoped_ptr<SolidColorProgram> m_solidColorProgram;

    OutputSurface* m_outputSurface;
    WebKit::WebGraphicsContext3D* m_context;

    gfx::Rect m_swapBufferRect;
    gfx::Rect m_scissorRect;
    bool m_isViewportChanged;
    bool m_isBackbufferDiscarded;
    bool m_discardBackbufferWhenNotVisible;
    bool m_isUsingBindUniform;
    bool m_visible;
    bool m_isScissorEnabled;
    bool m_blendShadow;
    unsigned m_programShadow;
    TexturedQuadDrawCache m_drawCache;

    scoped_ptr<ResourceProvider::ScopedWriteLockGL> m_currentFramebufferLock;

    DISALLOW_COPY_AND_ASSIGN(GLRenderer);
};


// Setting DEBUG_GL_CALLS to 1 will call glGetError() after almost every GL
// call made by the compositor. Useful for debugging rendering issues but
// will significantly degrade performance.
#define DEBUG_GL_CALLS 0

#if DEBUG_GL_CALLS && !defined(NDEBUG)
#define GLC(context, x) (x, GLRenderer::debugGLCall(&*context, #x, __FILE__, __LINE__))
#else
#define GLC(context, x) (x)
#endif


}

#endif  // CC_GL_RENDERER_H_
