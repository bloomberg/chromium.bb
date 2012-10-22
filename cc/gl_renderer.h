// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCRendererGL_h
#define CCRendererGL_h

#include "CCCheckerboardDrawQuad.h"
#include "CCDebugBorderDrawQuad.h"
#include "CCDirectRenderer.h"
#include "CCIOSurfaceDrawQuad.h"
#include "cc/render_pass_draw_quad.h"
#include "cc/renderer.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/tile_draw_quad.h"
#include "cc/yuv_video_draw_quad.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {

class ScopedTexture;
class StreamVideoDrawQuad;
class TextureDrawQuad;
class GeometryBinding;
class ScopedEnsureFramebufferAllocation;

// Class that handles drawing of composited render layers using GL.
class GLRenderer : public DirectRenderer,
                     public WebKit::WebGraphicsContext3D::WebGraphicsSwapBuffersCompleteCallbackCHROMIUM,
                     public WebKit::WebGraphicsContext3D::WebGraphicsMemoryAllocationChangedCallbackCHROMIUM ,
                     public WebKit::WebGraphicsContext3D::WebGraphicsContextLostCallback {
public:
    static scoped_ptr<GLRenderer> create(RendererClient*, ResourceProvider*);

    virtual ~GLRenderer();

    virtual const RendererCapabilities& capabilities() const OVERRIDE;

    WebKit::WebGraphicsContext3D* context();

    virtual void viewportChanged() OVERRIDE;

    // waits for rendering to finish
    virtual void finish() OVERRIDE;

    virtual void doNoOp() OVERRIDE;
    // puts backbuffer onscreen
    virtual bool swapBuffers() OVERRIDE;

    virtual void getFramebufferPixels(void *pixels, const IntRect&) OVERRIDE;

    virtual bool isContextLost() OVERRIDE;

    virtual void setVisible(bool) OVERRIDE;

protected:
    GLRenderer(RendererClient*, ResourceProvider*);

    static void debugGLCall(WebKit::WebGraphicsContext3D*, const char* command, const char* file, int line);

    bool isFramebufferDiscarded() const { return m_isFramebufferDiscarded; }
    bool initialize();

    const FloatQuad& sharedGeometryQuad() const { return m_sharedGeometryQuad; }
    const GeometryBinding* sharedGeometry() const { return m_sharedGeometry.get(); }

    bool getFramebufferTexture(ScopedTexture*, const IntRect& deviceRect);
    void releaseRenderPassTextures();

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
    static void toGLMatrix(float*, const WebKit::WebTransformationMatrix&);

    void drawCheckerboardQuad(const DrawingFrame&, const CheckerboardDrawQuad*);
    void drawDebugBorderQuad(const DrawingFrame&, const DebugBorderDrawQuad*);
    scoped_ptr<ScopedTexture> drawBackgroundFilters(DrawingFrame&, const RenderPassDrawQuad*, const WebKit::WebFilterOperations&, const WebKit::WebTransformationMatrix& deviceTransform);
    void drawRenderPassQuad(DrawingFrame&, const RenderPassDrawQuad*);
    void drawSolidColorQuad(const DrawingFrame&, const SolidColorDrawQuad*);
    void drawStreamVideoQuad(const DrawingFrame&, const StreamVideoDrawQuad*);
    void drawTextureQuad(const DrawingFrame&, const TextureDrawQuad*);
    void drawIOSurfaceQuad(const DrawingFrame&, const IOSurfaceDrawQuad*);
    void drawTileQuad(const DrawingFrame&, const TileDrawQuad*);
    void drawYUVVideoQuad(const DrawingFrame&, const YUVVideoDrawQuad*);

    void setShaderOpacity(float opacity, int alphaLocation);
    void setShaderFloatQuad(const FloatQuad&, int quadLocation);
    void drawQuadGeometry(const DrawingFrame&, const WebKit::WebTransformationMatrix& drawTransform, const gfx::RectF& quadRect, int matrixLocation);

    void copyTextureToFramebuffer(const DrawingFrame&, int textureId, const gfx::Rect&, const WebKit::WebTransformationMatrix& drawMatrix);

    bool useScopedTexture(DrawingFrame&, const ScopedTexture*, const gfx::Rect& viewportRect);

    bool makeContextCurrent();

    bool initializeSharedObjects();
    void cleanupSharedObjects();

    // WebKit::WebGraphicsContext3D::WebGraphicsSwapBuffersCompleteCallbackCHROMIUM implementation.
    virtual void onSwapBuffersComplete() OVERRIDE;

    // WebKit::WebGraphicsContext3D::WebGraphicsMemoryAllocationChangedCallbackCHROMIUM implementation.
    virtual void onMemoryAllocationChanged(WebKit::WebGraphicsMemoryAllocation) OVERRIDE;
    void onMemoryAllocationChangedOnImplThread(WebKit::WebGraphicsMemoryAllocation);
    void discardFramebuffer();
    void ensureFramebuffer();
    void enforceMemoryPolicy();

    // WebGraphicsContext3D::WebGraphicsContextLostCallback implementation.
    virtual void onContextLost() OVERRIDE;

    RendererCapabilities m_capabilities;

    unsigned m_offscreenFramebufferId;

    scoped_ptr<GeometryBinding> m_sharedGeometry;
    FloatQuad m_sharedGeometryQuad;

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
    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderRGBATexAlpha> RenderPassProgram;
    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderRGBATexAlphaMask> RenderPassMaskProgram;
    typedef ProgramBinding<VertexShaderQuad, FragmentShaderRGBATexAlphaAA> RenderPassProgramAA;
    typedef ProgramBinding<VertexShaderQuad, FragmentShaderRGBATexAlphaMaskAA> RenderPassMaskProgramAA;

    // Texture shaders.
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexAlpha> TextureProgram;
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexFlipAlpha> TextureProgramFlip;
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexRectAlpha> TextureIOSurfaceProgram;

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

    WebKit::WebGraphicsContext3D* m_context;

    gfx::Rect m_swapBufferRect;
    bool m_isViewportChanged;
    bool m_isFramebufferDiscarded;
    bool m_discardFramebufferWhenNotVisible;
    bool m_isUsingBindUniform;
    bool m_visible;

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

#endif
