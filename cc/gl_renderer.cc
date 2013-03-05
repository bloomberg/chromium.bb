// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/gl_renderer.h"

#include <set>
#include <string>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "cc/compositor_frame.h"
#include "cc/compositor_frame_metadata.h"
#include "cc/context_provider.h"
#include "cc/damage_tracker.h"
#include "cc/geometry_binding.h"
#include "cc/gl_frame_data.h"
#include "cc/layer_quad.h"
#include "cc/math_util.h"
#include "cc/priority_calculator.h"
#include "cc/proxy.h"
#include "cc/render_pass.h"
#include "cc/render_surface_filters.h"
#include "cc/scoped_resource.h"
#include "cc/single_thread_proxy.h"
#include "cc/stream_video_draw_quad.h"
#include "cc/texture_draw_quad.h"
#include "cc/video_layer_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTexture.h"
#include "third_party/skia/include/gpu/SkGpuDevice.h"
#include "third_party/skia/include/gpu/SkGrTexturePixelRef.h"
#include "ui/gfx/quad_f.h"
#include "ui/gfx/rect_conversions.h"

using WebKit::WebGraphicsContext3D;
using WebKit::WebGraphicsMemoryAllocation;

namespace cc {

namespace {

// TODO(epenner): This should probably be moved to output surface.
//
// This implements a simple fence based on client side swaps.
// This is to isolate the ResourceProvider from 'frames' which
// it shouldn't need to care about, while still allowing us to
// enforce good texture recycling behavior strictly throughout
// the compositor (don't recycle a texture while it's in use).
class SimpleSwapFence : public ResourceProvider::Fence {
public:
    SimpleSwapFence() : m_hasPassed(false) {}
    virtual bool hasPassed() OVERRIDE { return m_hasPassed; }
    void setHasPassed() { m_hasPassed = true; }
private:
    virtual ~SimpleSwapFence() {}
    bool m_hasPassed;
};

bool needsIOSurfaceReadbackWorkaround()
{
#if defined(OS_MACOSX)
    return true;
#else
    return false;
#endif
}

} // anonymous namespace

scoped_ptr<GLRenderer> GLRenderer::create(RendererClient* client, OutputSurface* outputSurface, ResourceProvider* resourceProvider)
{
    scoped_ptr<GLRenderer> renderer(make_scoped_ptr(new GLRenderer(client, outputSurface, resourceProvider)));
    if (!renderer->initialize())
        return scoped_ptr<GLRenderer>();

    return renderer.Pass();
}

GLRenderer::GLRenderer(RendererClient* client, OutputSurface* outputSurface, ResourceProvider* resourceProvider)
    : DirectRenderer(client, resourceProvider)
    , m_offscreenFramebufferId(0)
    , m_sharedGeometryQuad(gfx::RectF(-0.5f, -0.5f, 1.0f, 1.0f))
    , m_outputSurface(outputSurface)
    , m_context(outputSurface->context3d())
    , m_isViewportChanged(false)
    , m_isBackbufferDiscarded(false)
    , m_discardBackbufferWhenNotVisible(false)
    , m_isUsingBindUniform(false)
    , m_visible(true)
    , m_isScissorEnabled(false)
{
    DCHECK(m_context);
}

bool GLRenderer::initialize()
{
    if (!m_context->makeContextCurrent())
        return false;

    m_context->setContextLostCallback(this);
    m_context->pushGroupMarkerEXT("CompositorContext");

    std::string extensionsString = UTF16ToASCII(m_context->getString(GL_EXTENSIONS));
    std::vector<std::string> extensionsList;
    base::SplitString(extensionsString, ' ', &extensionsList);
    std::set<std::string> extensions(extensionsList.begin(), extensionsList.end());

    if (settings().acceleratePainting && extensions.count("GL_EXT_texture_format_BGRA8888")
                                      && extensions.count("GL_EXT_read_format_bgra"))
        m_capabilities.usingAcceleratedPainting = true;
    else
        m_capabilities.usingAcceleratedPainting = false;

    m_capabilities.usingPartialSwap = settings().partialSwapEnabled && extensions.count("GL_CHROMIUM_post_sub_buffer");

    // Use the swapBuffers callback only with the threaded proxy.
    if (m_client->hasImplThread())
        m_capabilities.usingSwapCompleteCallback = extensions.count("GL_CHROMIUM_swapbuffers_complete_callback");
    if (m_capabilities.usingSwapCompleteCallback)
        m_context->setSwapBuffersCompleteCallbackCHROMIUM(this);

    m_capabilities.usingSetVisibility = extensions.count("GL_CHROMIUM_set_visibility");

    if (extensions.count("GL_CHROMIUM_iosurface"))
        DCHECK(extensions.count("GL_ARB_texture_rectangle"));

    m_capabilities.usingGpuMemoryManager = extensions.count("GL_CHROMIUM_gpu_memory_manager")
                                           && settings().useMemoryManagement;
    if (m_capabilities.usingGpuMemoryManager)
        m_context->setMemoryAllocationChangedCallbackCHROMIUM(this);

    m_capabilities.usingEglImage = extensions.count("GL_OES_EGL_image_external");

    m_capabilities.maxTextureSize = m_resourceProvider->maxTextureSize();
    m_capabilities.bestTextureFormat = m_resourceProvider->bestTextureFormat();

    // The updater can access textures while the GLRenderer is using them.
    m_capabilities.allowPartialTextureUpdates = true;

    // Check for texture fast paths. Currently we always use MO8 textures,
    // so we only need to avoid POT textures if we have an NPOT fast-path.
    m_capabilities.avoidPow2Textures = extensions.count("GL_CHROMIUM_fast_NPOT_MO8_textures");

    m_capabilities.usingOffscreenContext3d = true;

    m_isUsingBindUniform = extensions.count("GL_CHROMIUM_bind_uniform_location");

    // Make sure scissoring starts as disabled.
    GLC(m_context, m_context->disable(GL_SCISSOR_TEST));
    DCHECK(!m_isScissorEnabled);

    if (!initializeSharedObjects())
        return false;

    // Make sure the viewport and context gets initialized, even if it is to zero.
    viewportChanged();
    return true;
}

GLRenderer::~GLRenderer()
{
    m_context->setSwapBuffersCompleteCallbackCHROMIUM(0);
    m_context->setMemoryAllocationChangedCallbackCHROMIUM(0);
    m_context->setContextLostCallback(0);
    cleanupSharedObjects();
}

const RendererCapabilities& GLRenderer::capabilities() const
{
    return m_capabilities;
}

WebGraphicsContext3D* GLRenderer::context()
{
    return m_context;
}

void GLRenderer::debugGLCall(WebGraphicsContext3D* context, const char* command, const char* file, int line)
{
    unsigned long error = context->getError();
    if (error != GL_NO_ERROR)
        LOG(ERROR) << "GL command failed: File: " << file << "\n\tLine " << line << "\n\tcommand: " << command << ", error " << static_cast<int>(error) << "\n";
}

void GLRenderer::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;

    enforceMemoryPolicy();

    // TODO: Replace setVisibilityCHROMIUM with an extension to explicitly manage front/backbuffers
    // crbug.com/116049
    if (m_capabilities.usingSetVisibility)
        m_context->setVisibilityCHROMIUM(visible);
}

void GLRenderer::sendManagedMemoryStats(size_t bytesVisible, size_t bytesVisibleAndNearby, size_t bytesAllocated)
{
    WebKit::WebGraphicsManagedMemoryStats stats;
    stats.bytesVisible = bytesVisible;
    stats.bytesVisibleAndNearby = bytesVisibleAndNearby;
    stats.bytesAllocated = bytesAllocated;
    stats.backbufferRequested = !m_isBackbufferDiscarded;
    m_context->sendManagedMemoryStatsCHROMIUM(&stats);
}

void GLRenderer::releaseRenderPassTextures()
{
    m_renderPassTextures.clear();
}

void GLRenderer::viewportChanged()
{
    m_isViewportChanged = true;
}

void GLRenderer::clearFramebuffer(DrawingFrame& frame)
{
    // On DEBUG builds, opaque render passes are cleared to blue to easily see regions that were not drawn on the screen.
    if (frame.currentRenderPass->has_transparent_background)
        GLC(m_context, m_context->clearColor(0, 0, 0, 0));
    else
        GLC(m_context, m_context->clearColor(0, 0, 1, 1));

#ifdef NDEBUG
    if (frame.currentRenderPass->has_transparent_background)
#endif
        m_context->clear(GL_COLOR_BUFFER_BIT);
}

void GLRenderer::beginDrawingFrame(DrawingFrame& frame)
{
    // FIXME: Remove this once backbuffer is automatically recreated on first use
    ensureBackbuffer();

    if (viewportSize().IsEmpty())
        return;

    TRACE_EVENT0("cc", "GLRenderer::drawLayers");
    if (m_isViewportChanged) {
        // Only reshape when we know we are going to draw. Otherwise, the reshape
        // can leave the window at the wrong size if we never draw and the proper
        // viewport size is never set.
        m_isViewportChanged = false;
        m_outputSurface->Reshape(gfx::Size(viewportWidth(), viewportHeight()));
    }

    makeContextCurrent();
    // Bind the common vertex attributes used for drawing all the layers.
    m_sharedGeometry->prepareForDraw();

    GLC(m_context, m_context->disable(GL_DEPTH_TEST));
    GLC(m_context, m_context->disable(GL_CULL_FACE));
    GLC(m_context, m_context->colorMask(true, true, true, true));
    GLC(m_context, m_context->enable(GL_BLEND));
    m_blendShadow = true;
    GLC(m_context, m_context->blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    GLC(context(), context()->activeTexture(GL_TEXTURE0));
    m_programShadow = 0;
}

void GLRenderer::doNoOp()
{
    GLC(m_context, m_context->bindFramebuffer(GL_FRAMEBUFFER, 0));
    GLC(m_context, m_context->flush());
}

void GLRenderer::drawQuad(DrawingFrame& frame, const DrawQuad* quad)
{
    DCHECK(quad->rect.Contains(quad->visible_rect));
    if (quad->material != DrawQuad::TEXTURE_CONTENT) {
      flushTextureQuadCache();
    }

    switch (quad->material) {
    case DrawQuad::INVALID:
        NOTREACHED();
        break;
    case DrawQuad::CHECKERBOARD:
        drawCheckerboardQuad(frame, CheckerboardDrawQuad::MaterialCast(quad));
        break;
    case DrawQuad::DEBUG_BORDER:
        drawDebugBorderQuad(frame, DebugBorderDrawQuad::MaterialCast(quad));
        break;
    case DrawQuad::IO_SURFACE_CONTENT:
        drawIOSurfaceQuad(frame, IOSurfaceDrawQuad::MaterialCast(quad));
        break;
    case DrawQuad::RENDER_PASS:
        drawRenderPassQuad(frame, RenderPassDrawQuad::MaterialCast(quad));
        break;
    case DrawQuad::SOLID_COLOR:
        drawSolidColorQuad(frame, SolidColorDrawQuad::MaterialCast(quad));
        break;
    case DrawQuad::STREAM_VIDEO_CONTENT:
        drawStreamVideoQuad(frame, StreamVideoDrawQuad::MaterialCast(quad));
        break;
    case DrawQuad::TEXTURE_CONTENT:
        enqueueTextureQuad(frame, TextureDrawQuad::MaterialCast(quad));
        break;
    case DrawQuad::TILED_CONTENT:
        drawTileQuad(frame, TileDrawQuad::MaterialCast(quad));
        break;
    case DrawQuad::YUV_VIDEO_CONTENT:
        drawYUVVideoQuad(frame, YUVVideoDrawQuad::MaterialCast(quad));
        break;
    }
}

void GLRenderer::drawCheckerboardQuad(const DrawingFrame& frame, const CheckerboardDrawQuad* quad)
{
    setBlendEnabled(quad->ShouldDrawWithBlending());

    const TileCheckerboardProgram* program = tileCheckerboardProgram();
    DCHECK(program && (program->initialized() || isContextLost()));
    setUseProgram(program->program());

    SkColor color = quad->color;
    GLC(context(), context()->uniform4f(program->fragmentShader().colorLocation(), SkColorGetR(color) * (1.0f / 255.0f), SkColorGetG(color) * (1.0f / 255.0f), SkColorGetB(color) * (1.0f / 255.0f), 1));

    const int checkerboardWidth = 16;
    float frequency = 1.0f / checkerboardWidth;

    gfx::Rect tileRect = quad->rect;
    float texOffsetX = tileRect.x() % checkerboardWidth;
    float texOffsetY = tileRect.y() % checkerboardWidth;
    float texScaleX = tileRect.width();
    float texScaleY = tileRect.height();
    GLC(context(), context()->uniform4f(program->fragmentShader().texTransformLocation(), texOffsetX, texOffsetY, texScaleX, texScaleY));

    GLC(context(), context()->uniform1f(program->fragmentShader().frequencyLocation(), frequency));

    setShaderOpacity(quad->opacity(), program->fragmentShader().alphaLocation());
    drawQuadGeometry(frame, quad->quadTransform(), quad->rect, program->vertexShader().matrixLocation());
}

void GLRenderer::drawDebugBorderQuad(const DrawingFrame& frame, const DebugBorderDrawQuad* quad)
{
    setBlendEnabled(quad->ShouldDrawWithBlending());

    static float glMatrix[16];
    const SolidColorProgram* program = solidColorProgram();
    DCHECK(program && (program->initialized() || isContextLost()));
    setUseProgram(program->program());

    // Use the full quadRect for debug quads to not move the edges based on partial swaps.
    const gfx::Rect& layerRect = quad->rect;
    gfx::Transform renderMatrix = quad->quadTransform();
    renderMatrix.Translate(0.5f * layerRect.width() + layerRect.x(), 0.5f * layerRect.height() + layerRect.y());
    renderMatrix.Scale(layerRect.width(), layerRect.height());
    GLRenderer::toGLMatrix(&glMatrix[0], frame.projectionMatrix * renderMatrix);
    GLC(context(), context()->uniformMatrix4fv(program->vertexShader().matrixLocation(), 1, false, &glMatrix[0]));

    SkColor color = quad->color;
    float alpha = SkColorGetA(color) * (1.0f / 255.0f);

    GLC(context(), context()->uniform4f(program->fragmentShader().colorLocation(), (SkColorGetR(color) * (1.0f / 255.0f)) * alpha, (SkColorGetG(color) * (1.0f / 255.0f)) * alpha, (SkColorGetB(color) * (1.0f / 255.0f)) * alpha, alpha));

    GLC(context(), context()->lineWidth(quad->width));

    // The indices for the line are stored in the same array as the triangle indices.
    GLC(context(), context()->drawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_SHORT, 0));
}

static inline SkBitmap applyFilters(GLRenderer* renderer, const WebKit::WebFilterOperations& filters, ScopedResource* sourceTextureResource)
{
    if (filters.isEmpty())
        return SkBitmap();

    cc::ContextProvider* offscreenContexts = renderer->resourceProvider()->offscreenContextProvider();
    if (!offscreenContexts || !offscreenContexts->Context3d() || !offscreenContexts->GrContext())
        return SkBitmap();

    ResourceProvider::ScopedWriteLockGL lock(renderer->resourceProvider(), sourceTextureResource->id());

    // Flush the compositor context to ensure that textures there are available
    // in the shared context.  Do this after locking/creating the compositor
    // texture.
    renderer->resourceProvider()->flush();

    // Make sure skia uses the correct GL context.
    offscreenContexts->Context3d()->makeContextCurrent();

    SkBitmap source = RenderSurfaceFilters::apply(filters, lock.textureId(), sourceTextureResource->size(), offscreenContexts->GrContext());

    // Flush skia context so that all the rendered stuff appears on the
    // texture.
    offscreenContexts->GrContext()->flush();

    // Flush the GL context so rendering results from this context are
    // visible in the compositor's context.
    offscreenContexts->Context3d()->flush();

    // Use the compositor's GL context again.
    renderer->resourceProvider()->graphicsContext3D()->makeContextCurrent();
    return source;
}

static SkBitmap applyImageFilter(GLRenderer* renderer, SkImageFilter* filter, ScopedResource* sourceTextureResource)
{
    if (!filter)
        return SkBitmap();

    cc::ContextProvider* offscreenContexts = renderer->resourceProvider()->offscreenContextProvider();
    if (!offscreenContexts || !offscreenContexts->Context3d() || !offscreenContexts->GrContext())
        return SkBitmap();

    ResourceProvider::ScopedWriteLockGL lock(renderer->resourceProvider(), sourceTextureResource->id());

    // Flush the compositor context to ensure that textures there are available
    // in the shared context.  Do this after locking/creating the compositor
    // texture.
    renderer->resourceProvider()->flush();

    // Make sure skia uses the correct GL context.
    offscreenContexts->Context3d()->makeContextCurrent();

    // Wrap the source texture in a Ganesh platform texture.
    GrBackendTextureDesc backendTextureDescription;
    backendTextureDescription.fWidth = sourceTextureResource->size().width();
    backendTextureDescription.fHeight = sourceTextureResource->size().height();
    backendTextureDescription.fConfig = kSkia8888_GrPixelConfig;
    backendTextureDescription.fTextureHandle = lock.textureId();
    backendTextureDescription.fOrigin = kTopLeft_GrSurfaceOrigin;
    skia::RefPtr<GrTexture> texture = skia::AdoptRef(offscreenContexts->GrContext()->wrapBackendTexture(backendTextureDescription));

    // Place the platform texture inside an SkBitmap.
    SkBitmap source;
    source.setConfig(SkBitmap::kARGB_8888_Config, sourceTextureResource->size().width(), sourceTextureResource->size().height());
    skia::RefPtr<SkGrPixelRef> pixelRef = skia::AdoptRef(new SkGrPixelRef(texture.get()));
    source.setPixelRef(pixelRef.get());

    // Create a scratch texture for backing store.
    GrTextureDesc desc;
    desc.fFlags = kRenderTarget_GrTextureFlagBit | kNoStencil_GrTextureFlagBit;
    desc.fSampleCnt = 0;
    desc.fWidth = source.width();
    desc.fHeight = source.height();
    desc.fConfig = kSkia8888_GrPixelConfig;
    desc.fOrigin = kTopLeft_GrSurfaceOrigin;
    GrAutoScratchTexture scratchTexture(offscreenContexts->GrContext(), desc, GrContext::kExact_ScratchTexMatch);
    skia::RefPtr<GrTexture> backingStore = skia::AdoptRef(scratchTexture.detach());

    // Create a device and canvas using that backing store.
    SkGpuDevice device(offscreenContexts->GrContext(), backingStore.get());
    SkCanvas canvas(&device);

    // Draw the source bitmap through the filter to the canvas.
    SkPaint paint;
    paint.setImageFilter(filter);
    canvas.clear(0x0);
    canvas.drawSprite(source, 0, 0, &paint);

    // Flush skia context so that all the rendered stuff appears on the
    // texture.
    offscreenContexts->GrContext()->flush();

    // Flush the GL context so rendering results from this context are
    // visible in the compositor's context.
    offscreenContexts->Context3d()->flush();

    // Use the compositor's GL context again.
    renderer->resourceProvider()->graphicsContext3D()->makeContextCurrent();

    return device.accessBitmap(false);
}

scoped_ptr<ScopedResource> GLRenderer::drawBackgroundFilters(
    DrawingFrame& frame, const RenderPassDrawQuad* quad,
    const gfx::Transform& contentsDeviceTransform,
    const gfx::Transform& contentsDeviceTransformInverse)
{
    // This method draws a background filter, which applies a filter to any pixels behind the quad and seen through its background.
    // The algorithm works as follows:
    // 1. Compute a bounding box around the pixels that will be visible through the quad.
    // 2. Read the pixels in the bounding box into a buffer R.
    // 3. Apply the background filter to R, so that it is applied in the pixels' coordinate space.
    // 4. Apply the quad's inverse transform to map the pixels in R into the quad's content space. This implicitly
    // clips R by the content bounds of the quad since the destination texture has bounds matching the quad's content.
    // 5. Draw the background texture for the contents using the same transform as used to draw the contents itself. This is done
    // without blending to replace the current background pixels with the new filtered background.
    // 6. Draw the contents of the quad over drop of the new background with blending, as per usual. The filtered background
    // pixels will show through any non-opaque pixels in this draws.
    //
    // Pixel copies in this algorithm occur at steps 2, 3, 4, and 5.

    // FIXME: When this algorithm changes, update LayerTreeHost::prioritizeTextures() accordingly.

    const WebKit::WebFilterOperations& filters = quad->background_filters;
    if (filters.isEmpty())
        return scoped_ptr<ScopedResource>();

    // FIXME: We only allow background filters on an opaque render surface because other surfaces may contain
    // translucent pixels, and the contents behind those translucent pixels wouldn't have the filter applied.
    if (frame.currentRenderPass->has_transparent_background)
        return scoped_ptr<ScopedResource>();
    DCHECK(!frame.currentTexture);

    // FIXME: Do a single readback for both the surface and replica and cache the filtered results (once filter textures are not reused).
    gfx::Rect deviceRect = gfx::ToEnclosingRect(MathUtil::mapClippedRect(contentsDeviceTransform, sharedGeometryQuad().BoundingBox()));

    int top, right, bottom, left;
    filters.getOutsets(top, right, bottom, left);
    deviceRect.Inset(-left, -top, -right, -bottom);

    deviceRect.Intersect(frame.currentRenderPass->output_rect);

    scoped_ptr<ScopedResource> deviceBackgroundTexture = ScopedResource::create(m_resourceProvider);
    if (!getFramebufferTexture(deviceBackgroundTexture.get(), deviceRect))
        return scoped_ptr<ScopedResource>();

    SkBitmap filteredDeviceBackground = applyFilters(this, filters, deviceBackgroundTexture.get());
    if (!filteredDeviceBackground.getTexture())
        return scoped_ptr<ScopedResource>();

    GrTexture* texture = reinterpret_cast<GrTexture*>(filteredDeviceBackground.getTexture());
    int filteredDeviceBackgroundTextureId = texture->getTextureHandle();

    scoped_ptr<ScopedResource> backgroundTexture = ScopedResource::create(m_resourceProvider);
    if (!backgroundTexture->Allocate(quad->rect.size(), GL_RGBA, ResourceProvider::TextureUsageFramebuffer))
        return scoped_ptr<ScopedResource>();

    const RenderPass* targetRenderPass = frame.currentRenderPass;
    bool usingBackgroundTexture = useScopedTexture(frame, backgroundTexture.get(), quad->rect);

    if (usingBackgroundTexture) {
        // Copy the readback pixels from device to the background texture for the surface.
        gfx::Transform deviceToFramebufferTransform;
        deviceToFramebufferTransform.Translate(quad->rect.width() * 0.5f, quad->rect.height() * 0.5f);
        deviceToFramebufferTransform.Scale(quad->rect.width(), quad->rect.height());
        deviceToFramebufferTransform.PreconcatTransform(contentsDeviceTransformInverse);
        copyTextureToFramebuffer(frame, filteredDeviceBackgroundTextureId, deviceRect, deviceToFramebufferTransform);
    }

    useRenderPass(frame, targetRenderPass);

    if (!usingBackgroundTexture)
        return scoped_ptr<ScopedResource>();
    return backgroundTexture.Pass();
}

void GLRenderer::drawRenderPassQuad(DrawingFrame& frame, const RenderPassDrawQuad* quad)
{
    setBlendEnabled(quad->ShouldDrawWithBlending());

    CachedResource* contentsTexture = m_renderPassTextures.get(quad->render_pass_id);
    if (!contentsTexture || !contentsTexture->id())
        return;

    gfx::Transform quadRectMatrix;
    quadRectTransform(&quadRectMatrix, quad->quadTransform(), quad->rect);
    gfx::Transform contentsDeviceTransform = frame.windowMatrix * frame.projectionMatrix * quadRectMatrix;
    contentsDeviceTransform.FlattenTo2d();

    // Can only draw surface if device matrix is invertible.
    gfx::Transform contentsDeviceTransformInverse(gfx::Transform::kSkipInitialization);
    if (!contentsDeviceTransform.GetInverse(&contentsDeviceTransformInverse))
        return;

    scoped_ptr<ScopedResource> backgroundTexture = drawBackgroundFilters(
        frame, quad, contentsDeviceTransform, contentsDeviceTransformInverse);

    // FIXME: Cache this value so that we don't have to do it for both the surface and its replica.
    // Apply filters to the contents texture.
    SkBitmap filterBitmap;
    if (quad->filter) {
        filterBitmap = applyImageFilter(this, quad->filter.get(), contentsTexture);
    } else {
        filterBitmap = applyFilters(this, quad->filters, contentsTexture);
    }

    // Draw the background texture if there is one.
    if (backgroundTexture) {
        DCHECK(backgroundTexture->size() == quad->rect.size());
        ResourceProvider::ScopedReadLockGL lock(m_resourceProvider, backgroundTexture->id());
        copyTextureToFramebuffer(frame, lock.textureId(), quad->rect, quad->quadTransform());
    }

    bool clipped = false;
    gfx::QuadF deviceQuad = MathUtil::mapQuad(contentsDeviceTransform, sharedGeometryQuad(), clipped);
    DCHECK(!clipped);
    LayerQuad deviceLayerBounds = LayerQuad(gfx::QuadF(deviceQuad.BoundingBox()));
    LayerQuad deviceLayerEdges = LayerQuad(deviceQuad);

    // Use anti-aliasing programs only when necessary.
    bool useAA = (!deviceQuad.IsRectilinear() || !deviceQuad.BoundingBox().IsExpressibleAsRect());
    if (useAA) {
        deviceLayerBounds.inflateAntiAliasingDistance();
        deviceLayerEdges.inflateAntiAliasingDistance();
    }

    scoped_ptr<ResourceProvider::ScopedReadLockGL> maskResourceLock;
    unsigned maskTextureId = 0;
    if (quad->mask_resource_id) {
        maskResourceLock.reset(new ResourceProvider::ScopedReadLockGL(m_resourceProvider, quad->mask_resource_id));
        maskTextureId = maskResourceLock->textureId();
    }

    // FIXME: use the backgroundTexture and blend the background in with this draw instead of having a separate copy of the background texture.

    scoped_ptr<ResourceProvider::ScopedReadLockGL> contentsResourceLock;
    if (filterBitmap.getTexture()) {
        GrTexture* texture = reinterpret_cast<GrTexture*>(filterBitmap.getTexture());
        context()->bindTexture(GL_TEXTURE_2D, texture->getTextureHandle());
    } else
        contentsResourceLock = make_scoped_ptr(new ResourceProvider::ScopedSamplerGL(m_resourceProvider, contentsTexture->id(),
                                                                                     GL_TEXTURE_2D, GL_LINEAR));

    int shaderQuadLocation = -1;
    int shaderEdgeLocation = -1;
    int shaderMaskSamplerLocation = -1;
    int shaderMaskTexCoordScaleLocation = -1;
    int shaderMaskTexCoordOffsetLocation = -1;
    int shaderMatrixLocation = -1;
    int shaderAlphaLocation = -1;
    int shaderTexTransformLocation = -1;
    int shaderTexScaleLocation = -1;

    if (useAA && maskTextureId) {
        const RenderPassMaskProgramAA* program = renderPassMaskProgramAA();
        setUseProgram(program->program());
        GLC(context(), context()->uniform1i(program->fragmentShader().samplerLocation(), 0));

        shaderQuadLocation = program->vertexShader().pointLocation();
        shaderEdgeLocation = program->fragmentShader().edgeLocation();
        shaderMaskSamplerLocation = program->fragmentShader().maskSamplerLocation();
        shaderMaskTexCoordScaleLocation = program->fragmentShader().maskTexCoordScaleLocation();
        shaderMaskTexCoordOffsetLocation = program->fragmentShader().maskTexCoordOffsetLocation();
        shaderMatrixLocation = program->vertexShader().matrixLocation();
        shaderAlphaLocation = program->fragmentShader().alphaLocation();
        shaderTexScaleLocation = program->vertexShader().texScaleLocation();
    } else if (!useAA && maskTextureId) {
        const RenderPassMaskProgram* program = renderPassMaskProgram();
        setUseProgram(program->program());
        GLC(context(), context()->uniform1i(program->fragmentShader().samplerLocation(), 0));

        shaderMaskSamplerLocation = program->fragmentShader().maskSamplerLocation();
        shaderMaskTexCoordScaleLocation = program->fragmentShader().maskTexCoordScaleLocation();
        shaderMaskTexCoordOffsetLocation = program->fragmentShader().maskTexCoordOffsetLocation();
        shaderMatrixLocation = program->vertexShader().matrixLocation();
        shaderAlphaLocation = program->fragmentShader().alphaLocation();
        shaderTexTransformLocation = program->vertexShader().texTransformLocation();
    } else if (useAA && !maskTextureId) {
        const RenderPassProgramAA* program = renderPassProgramAA();
        setUseProgram(program->program());
        GLC(context(), context()->uniform1i(program->fragmentShader().samplerLocation(), 0));

        shaderQuadLocation = program->vertexShader().pointLocation();
        shaderEdgeLocation = program->fragmentShader().edgeLocation();
        shaderMatrixLocation = program->vertexShader().matrixLocation();
        shaderAlphaLocation = program->fragmentShader().alphaLocation();
        shaderTexScaleLocation = program->vertexShader().texScaleLocation();
    } else {
        const RenderPassProgram* program = renderPassProgram();
        setUseProgram(program->program());
        GLC(context(), context()->uniform1i(program->fragmentShader().samplerLocation(), 0));

        shaderMatrixLocation = program->vertexShader().matrixLocation();
        shaderAlphaLocation = program->fragmentShader().alphaLocation();
        shaderTexTransformLocation = program->vertexShader().texTransformLocation();
    }

    float tex_scale_x = quad->rect.width() / static_cast<float>(contentsTexture->size().width());
    float tex_scale_y = quad->rect.height() / static_cast<float>(contentsTexture->size().height());
    DCHECK_LE(tex_scale_x, 1.0f);
    DCHECK_LE(tex_scale_y, 1.0f);

    if (shaderTexTransformLocation != -1) {
        GLC(context(), context()->uniform4f(shaderTexTransformLocation,
                                            0.0f, 0.0f,
                                            tex_scale_x, tex_scale_y));
    } else if (shaderTexScaleLocation != -1) {
        GLC(context(), context()->uniform2f(shaderTexScaleLocation,
                                            tex_scale_x, tex_scale_y));
    } else {
        DCHECK(isContextLost());
    }

    if (shaderMaskSamplerLocation != -1) {
        DCHECK(shaderMaskTexCoordScaleLocation != 1);
        DCHECK(shaderMaskTexCoordOffsetLocation != 1);
        GLC(context(), context()->activeTexture(GL_TEXTURE1));
        GLC(context(), context()->uniform1i(shaderMaskSamplerLocation, 1));
        GLC(context(), context()->uniform2f(shaderMaskTexCoordOffsetLocation,
                                            quad->mask_uv_rect.x(), quad->mask_uv_rect.y()));
        GLC(context(), context()->uniform2f(shaderMaskTexCoordScaleLocation,
                                            quad->mask_uv_rect.width() / tex_scale_x, quad->mask_uv_rect.height() / tex_scale_y));
        m_resourceProvider->bindForSampling(quad->mask_resource_id, GL_TEXTURE_2D, GL_LINEAR);
        GLC(context(), context()->activeTexture(GL_TEXTURE0));
    }

    if (shaderEdgeLocation != -1) {
        float edge[24];
        deviceLayerEdges.toFloatArray(edge);
        deviceLayerBounds.toFloatArray(&edge[12]);
        GLC(context(), context()->uniform3fv(shaderEdgeLocation, 8, edge));
    }

    // Map device space quad to surface space. contentsDeviceTransform has no 3d component since it was flattened, so we don't need to project.
    gfx::QuadF surfaceQuad = MathUtil::mapQuad(contentsDeviceTransformInverse, deviceLayerEdges.ToQuadF(), clipped);
    DCHECK(!clipped);

    setShaderOpacity(quad->opacity(), shaderAlphaLocation);
    setShaderQuadF(surfaceQuad, shaderQuadLocation);
    drawQuadGeometry(frame, quad->quadTransform(), quad->rect, shaderMatrixLocation);

    // Flush the compositor context before the filter bitmap goes out of
    // scope, so the draw gets processed before the filter texture gets deleted.
    if (filterBitmap.getTexture())
        m_context->flush();
}

struct SolidColorProgramUniforms {
    unsigned program;
    unsigned matrixLocation;
    unsigned colorLocation;
    unsigned pointLocation;
    unsigned texScaleLocation;
    unsigned edgeLocation;
};

template<class T>
static void solidColorUniformLocation(T program, SolidColorProgramUniforms& uniforms)
{
    uniforms.program = program->program();
    uniforms.matrixLocation = program->vertexShader().matrixLocation();
    uniforms.colorLocation = program->fragmentShader().colorLocation();
    uniforms.pointLocation = program->vertexShader().pointLocation();
    uniforms.texScaleLocation = program->vertexShader().texScaleLocation();
    uniforms.edgeLocation = program->fragmentShader().edgeLocation();
}

bool GLRenderer::setupQuadForAntialiasing(const gfx::Transform& deviceTransform, const DrawQuad* quad,
                                          gfx::QuadF* localQuad, float edge[24]) const
{
    gfx::Rect tileRect = quad->visible_rect;

    bool clipped = false;
    gfx::QuadF deviceLayerQuad = MathUtil::mapQuad(deviceTransform, gfx::QuadF(quad->visibleContentRect()), clipped);
    DCHECK(!clipped);

    // TODO(reveman): Axis-aligned is not enough to avoid anti-aliasing.
    // Bounding rectangle for quad also needs to be expressible as
    // an integer rectangle. crbug.com/169374
    bool isAxisAlignedInTarget = deviceLayerQuad.IsRectilinear();
    bool useAA = !clipped && !isAxisAlignedInTarget && quad->IsEdge();

    if (!useAA)
      return false;

    LayerQuad deviceLayerBounds = LayerQuad(gfx::QuadF(deviceLayerQuad.BoundingBox()));
    deviceLayerBounds.inflateAntiAliasingDistance();

    LayerQuad deviceLayerEdges = LayerQuad(deviceLayerQuad);
    deviceLayerEdges.inflateAntiAliasingDistance();

    deviceLayerEdges.toFloatArray(edge);
    deviceLayerBounds.toFloatArray(&edge[12]);

    gfx::PointF bottomRight = tileRect.bottom_right();
    gfx::PointF bottomLeft = tileRect.bottom_left();
    gfx::PointF topLeft = tileRect.origin();
    gfx::PointF topRight = tileRect.top_right();

    // Map points to device space.
    bottomRight = MathUtil::mapPoint(deviceTransform, bottomRight, clipped);
    DCHECK(!clipped);
    bottomLeft = MathUtil::mapPoint(deviceTransform, bottomLeft, clipped);
    DCHECK(!clipped);
    topLeft = MathUtil::mapPoint(deviceTransform, topLeft, clipped);
    DCHECK(!clipped);
    topRight = MathUtil::mapPoint(deviceTransform, topRight, clipped);
    DCHECK(!clipped);

    LayerQuad::Edge bottomEdge(bottomRight, bottomLeft);
    LayerQuad::Edge leftEdge(bottomLeft, topLeft);
    LayerQuad::Edge topEdge(topLeft, topRight);
    LayerQuad::Edge rightEdge(topRight, bottomRight);

    // Only apply anti-aliasing to edges not clipped by culling or scissoring.
    if (quad->IsTopEdge() && tileRect.y() == quad->rect.y())
        topEdge = deviceLayerEdges.top();
    if (quad->IsLeftEdge() && tileRect.x() == quad->rect.x())
        leftEdge = deviceLayerEdges.left();
    if (quad->IsRightEdge() && tileRect.right() == quad->rect.right())
        rightEdge = deviceLayerEdges.right();
    if (quad->IsBottomEdge() && tileRect.bottom() == quad->rect.bottom())
        bottomEdge = deviceLayerEdges.bottom();

    float sign = gfx::QuadF(tileRect).IsCounterClockwise() ? -1 : 1;
    bottomEdge.scale(sign);
    leftEdge.scale(sign);
    topEdge.scale(sign);
    rightEdge.scale(sign);

    // Create device space quad.
    LayerQuad deviceQuad(leftEdge, topEdge, rightEdge, bottomEdge);

    // Map device space quad to local space. deviceTransform has no 3d component since it was flattened, so we don't need to project.
    // We should have already checked that the transform was uninvertible above.
    gfx::Transform inverseDeviceTransform(gfx::Transform::kSkipInitialization);
    bool didInvert = deviceTransform.GetInverse(&inverseDeviceTransform);
    DCHECK(didInvert);
    *localQuad = MathUtil::mapQuad(inverseDeviceTransform, deviceQuad.ToQuadF(), clipped);
    // We should not DCHECK(!clipped) here, because anti-aliasing inflation may cause deviceQuad to become
    // clipped. To our knowledge this scenario does not need to be handled differently than the unclipped case.

    return true;
}

void GLRenderer::drawSolidColorQuad(const DrawingFrame& frame, const SolidColorDrawQuad* quad)
{
    gfx::Rect tileRect = quad->visible_rect;

    gfx::Transform deviceTransform = frame.windowMatrix * frame.projectionMatrix * quad->quadTransform();
    deviceTransform.FlattenTo2d();
    if (!deviceTransform.IsInvertible())
        return;

    gfx::QuadF localQuad = gfx::QuadF(gfx::RectF(tileRect));
    float edge[24];
    bool useAA = setupQuadForAntialiasing(deviceTransform, quad, &localQuad, edge);

    SolidColorProgramUniforms uniforms;
    if (useAA)
        solidColorUniformLocation(solidColorProgramAA(), uniforms);
    else
        solidColorUniformLocation(solidColorProgram(), uniforms);
    setUseProgram(uniforms.program);

    SkColor color = quad->color;
    float opacity = quad->opacity();
    float alpha = (SkColorGetA(color) * (1.0f / 255.0f)) * opacity;

    GLC(context(), context()->uniform4f(uniforms.colorLocation,
                                        (SkColorGetR(color) * (1.0f / 255.0f)) * alpha,
                                        (SkColorGetG(color) * (1.0f / 255.0f)) * alpha,
                                        (SkColorGetB(color) * (1.0f / 255.0f)) * alpha,
                                        alpha));

    GLC(context(), context()->uniform2f(uniforms.texScaleLocation, 1.0f, 1.0f));

    if (useAA) {
        GLC(context(), context()->uniform3fv(uniforms.edgeLocation, 8, edge));
    }

    // Enable blending when the quad properties require it or if we decided
    // to use antialiasing.
    setBlendEnabled(quad->ShouldDrawWithBlending() || useAA);

    // Normalize to tileRect.
    localQuad.Scale(1.0f / tileRect.width(), 1.0f / tileRect.height());

    setShaderQuadF(localQuad, uniforms.pointLocation);

    // The transform and vertex data are used to figure out the extents that the
    // un-antialiased quad should have and which vertex this is and the float
    // quad passed in via uniform is the actual geometry that gets used to draw
    // it. This is why this centered rect is used and not the original quadRect.
    gfx::RectF centeredRect(gfx::PointF(-0.5 * tileRect.width(), -0.5 * tileRect.height()), tileRect.size());
    drawQuadGeometry(frame, quad->quadTransform(), centeredRect, uniforms.matrixLocation);
}

struct TileProgramUniforms {
    unsigned program;
    unsigned samplerLocation;
    unsigned vertexTexTransformLocation;
    unsigned fragmentTexTransformLocation;
    unsigned edgeLocation;
    unsigned matrixLocation;
    unsigned alphaLocation;
    unsigned pointLocation;
};

template<class T>
static void tileUniformLocation(T program, TileProgramUniforms& uniforms)
{
    uniforms.program = program->program();
    uniforms.vertexTexTransformLocation = program->vertexShader().vertexTexTransformLocation();
    uniforms.matrixLocation = program->vertexShader().matrixLocation();
    uniforms.pointLocation = program->vertexShader().pointLocation();

    uniforms.samplerLocation = program->fragmentShader().samplerLocation();
    uniforms.alphaLocation = program->fragmentShader().alphaLocation();
    uniforms.fragmentTexTransformLocation = program->fragmentShader().fragmentTexTransformLocation();
    uniforms.edgeLocation = program->fragmentShader().edgeLocation();
}

void GLRenderer::drawTileQuad(const DrawingFrame& frame, const TileDrawQuad* quad)
{
    gfx::Rect tileRect = quad->visible_rect;

    gfx::RectF texCoordRect = quad->tex_coord_rect;
    float texToGeomScaleX = quad->rect.width() / texCoordRect.width();
    float texToGeomScaleY = quad->rect.height() / texCoordRect.height();

    // texCoordRect corresponds to quadRect, but quadVisibleRect may be
    // smaller than quadRect due to occlusion or clipping. Adjust
    // texCoordRect to match.
    gfx::Vector2d topLeftDiff = tileRect.origin() - quad->rect.origin();
    gfx::Vector2d bottomRightDiff =
        tileRect.bottom_right() - quad->rect.bottom_right();
    texCoordRect.Inset(topLeftDiff.x() / texToGeomScaleX,
                       topLeftDiff.y() / texToGeomScaleY,
                       -bottomRightDiff.x() / texToGeomScaleX,
                       -bottomRightDiff.y() / texToGeomScaleY);

    gfx::RectF clampGeomRect(tileRect);
    gfx::RectF clampTexRect(texCoordRect);
    // Clamp texture coordinates to avoid sampling outside the layer
    // by deflating the tile region half a texel or half a texel
    // minus epsilon for one pixel layers. The resulting clamp region
    // is mapped to the unit square by the vertex shader and mapped
    // back to normalized texture coordinates by the fragment shader
    // after being clamped to 0-1 range.
    const float epsilon = 1 / 1024.0f;
    float texClampX = std::min(0.5f, 0.5f * clampTexRect.width() - epsilon);
    float texClampY = std::min(0.5f, 0.5f * clampTexRect.height() - epsilon);
    float geomClampX = std::min(texClampX * texToGeomScaleX,
                                0.5f * clampGeomRect.width() - epsilon);
    float geomClampY = std::min(texClampY * texToGeomScaleY,
                                0.5f * clampGeomRect.height() - epsilon);
    clampGeomRect.Inset(geomClampX, geomClampY, geomClampX, geomClampY);
    clampTexRect.Inset(texClampX, texClampY, texClampX, texClampY);

    // Map clamping rectangle to unit square.
    float vertexTexTranslateX = -clampGeomRect.x() / clampGeomRect.width();
    float vertexTexTranslateY = -clampGeomRect.y() / clampGeomRect.height();
    float vertexTexScaleX = tileRect.width() / clampGeomRect.width();
    float vertexTexScaleY = tileRect.height() / clampGeomRect.height();

    // Map to normalized texture coordinates.
    const gfx::Size& textureSize = quad->texture_size;
    float fragmentTexTranslateX = clampTexRect.x() / textureSize.width();
    float fragmentTexTranslateY = clampTexRect.y() / textureSize.height();
    float fragmentTexScaleX = clampTexRect.width() / textureSize.width();
    float fragmentTexScaleY = clampTexRect.height() / textureSize.height();

    gfx::Transform deviceTransform = frame.windowMatrix * frame.projectionMatrix * quad->quadTransform();
    deviceTransform.FlattenTo2d();
    if (!deviceTransform.IsInvertible())
        return;

    gfx::QuadF localQuad = gfx::QuadF(gfx::RectF(tileRect));
    float edge[24];
    bool useAA = setupQuadForAntialiasing(deviceTransform, quad, &localQuad, edge);

    TileProgramUniforms uniforms;
    if (useAA) {
        if (quad->swizzle_contents)
            tileUniformLocation(tileProgramSwizzleAA(), uniforms);
        else
            tileUniformLocation(tileProgramAA(), uniforms);
    } else {
        if (quad->ShouldDrawWithBlending()) {
            if (quad->swizzle_contents)
                tileUniformLocation(tileProgramSwizzle(), uniforms);
            else
                tileUniformLocation(tileProgram(), uniforms);
        } else {
            if (quad->swizzle_contents)
                tileUniformLocation(tileProgramSwizzleOpaque(), uniforms);
            else
                tileUniformLocation(tileProgramOpaque(), uniforms);
        }
    }

    setUseProgram(uniforms.program);
    GLC(context(), context()->uniform1i(uniforms.samplerLocation, 0));
    bool scaled = (texToGeomScaleX != 1 || texToGeomScaleY != 1);
    GLenum filter = (useAA || scaled || !quad->quadTransform().IsIdentityOrIntegerTranslation()) ? GL_LINEAR : GL_NEAREST;
    ResourceProvider::ScopedSamplerGL quadResourceLock(m_resourceProvider, quad->resource_id, GL_TEXTURE_2D, filter);

    if (useAA) {
        GLC(context(), context()->uniform3fv(uniforms.edgeLocation, 8, edge));
        GLC(context(), context()->uniform4f(uniforms.vertexTexTransformLocation, vertexTexTranslateX, vertexTexTranslateY, vertexTexScaleX, vertexTexScaleY));
        GLC(context(), context()->uniform4f(uniforms.fragmentTexTransformLocation, fragmentTexTranslateX, fragmentTexTranslateY, fragmentTexScaleX, fragmentTexScaleY));
    } else {
        // Move fragment shader transform to vertex shader. We can do this while
        // still producing correct results as fragmentTexTransformLocation
        // should always be non-negative when tiles are transformed in a way
        // that could result in sampling outside the layer.
        vertexTexScaleX *= fragmentTexScaleX;
        vertexTexScaleY *= fragmentTexScaleY;
        vertexTexTranslateX *= fragmentTexScaleX;
        vertexTexTranslateY *= fragmentTexScaleY;
        vertexTexTranslateX += fragmentTexTranslateX;
        vertexTexTranslateY += fragmentTexTranslateY;

        GLC(context(), context()->uniform4f(uniforms.vertexTexTransformLocation, vertexTexTranslateX, vertexTexTranslateY, vertexTexScaleX, vertexTexScaleY));
    }

    // Enable blending when the quad properties require it or if we decided
    // to use antialiasing.
    setBlendEnabled(quad->ShouldDrawWithBlending() || useAA);

    // Normalize to tileRect.
    localQuad.Scale(1.0f / tileRect.width(), 1.0f / tileRect.height());

    setShaderOpacity(quad->opacity(), uniforms.alphaLocation);
    setShaderQuadF(localQuad, uniforms.pointLocation);

    // The transform and vertex data are used to figure out the extents that the
    // un-antialiased quad should have and which vertex this is and the float
    // quad passed in via uniform is the actual geometry that gets used to draw
    // it. This is why this centered rect is used and not the original quadRect.
    gfx::RectF centeredRect(gfx::PointF(-0.5f * tileRect.width(), -0.5f * tileRect.height()), tileRect.size());
    drawQuadGeometry(frame, quad->quadTransform(), centeredRect, uniforms.matrixLocation);
}

void GLRenderer::drawYUVVideoQuad(const DrawingFrame& frame, const YUVVideoDrawQuad* quad)
{
    setBlendEnabled(quad->ShouldDrawWithBlending());

    const VideoYUVProgram* program = videoYUVProgram();
    DCHECK(program && (program->initialized() || isContextLost()));

    const VideoLayerImpl::FramePlane& yPlane = quad->y_plane;
    const VideoLayerImpl::FramePlane& uPlane = quad->u_plane;
    const VideoLayerImpl::FramePlane& vPlane = quad->v_plane;

    GLC(context(), context()->activeTexture(GL_TEXTURE1));
    ResourceProvider::ScopedSamplerGL yPlaneLock(m_resourceProvider, yPlane.resourceId, GL_TEXTURE_2D, GL_LINEAR);
    GLC(context(), context()->activeTexture(GL_TEXTURE2));
    ResourceProvider::ScopedSamplerGL uPlaneLock(m_resourceProvider, uPlane.resourceId, GL_TEXTURE_2D, GL_LINEAR);
    GLC(context(), context()->activeTexture(GL_TEXTURE3));
    ResourceProvider::ScopedSamplerGL vPlaneLock(m_resourceProvider, vPlane.resourceId, GL_TEXTURE_2D, GL_LINEAR);

    setUseProgram(program->program());

    GLC(context(), context()->uniform2f(program->vertexShader().texScaleLocation(), quad->tex_scale.width(), quad->tex_scale.height()));
    GLC(context(), context()->uniform1i(program->fragmentShader().yTextureLocation(), 1));
    GLC(context(), context()->uniform1i(program->fragmentShader().uTextureLocation(), 2));
    GLC(context(), context()->uniform1i(program->fragmentShader().vTextureLocation(), 3));

    // These values are magic numbers that are used in the transformation from YUV to RGB color values.
    // They are taken from the following webpage: http://www.fourcc.org/fccyvrgb.php
    float yuv2RGB[9] = {
        1.164f, 1.164f, 1.164f,
        0.0f, -.391f, 2.018f,
        1.596f, -.813f, 0.0f,
    };
    GLC(context(), context()->uniformMatrix3fv(program->fragmentShader().yuvMatrixLocation(), 1, 0, yuv2RGB));

    // These values map to 16, 128, and 128 respectively, and are computed
    // as a fraction over 256 (e.g. 16 / 256 = 0.0625).
    // They are used in the YUV to RGBA conversion formula:
    //   Y - 16   : Gives 16 values of head and footroom for overshooting
    //   U - 128  : Turns unsigned U into signed U [-128,127]
    //   V - 128  : Turns unsigned V into signed V [-128,127]
    float yuvAdjust[3] = {
        -0.0625f,
        -0.5f,
        -0.5f,
    };
    GLC(context(), context()->uniform3fv(program->fragmentShader().yuvAdjLocation(), 1, yuvAdjust));

    setShaderOpacity(quad->opacity(), program->fragmentShader().alphaLocation());
    drawQuadGeometry(frame, quad->quadTransform(), quad->rect, program->vertexShader().matrixLocation());

    // Reset active texture back to texture 0.
    GLC(context(), context()->activeTexture(GL_TEXTURE0));
}

void GLRenderer::drawStreamVideoQuad(const DrawingFrame& frame, const StreamVideoDrawQuad* quad)
{
    setBlendEnabled(quad->ShouldDrawWithBlending());

    static float glMatrix[16];

    DCHECK(m_capabilities.usingEglImage);

    const VideoStreamTextureProgram* program = videoStreamTextureProgram();
    setUseProgram(program->program());

    toGLMatrix(&glMatrix[0], quad->matrix);
    GLC(context(), context()->uniformMatrix4fv(program->vertexShader().texMatrixLocation(), 1, false, glMatrix));

    GLC(context(), context()->bindTexture(GL_TEXTURE_EXTERNAL_OES, quad->texture_id));

    GLC(context(), context()->uniform1i(program->fragmentShader().samplerLocation(), 0));

    setShaderOpacity(quad->opacity(), program->fragmentShader().alphaLocation());
    drawQuadGeometry(frame, quad->quadTransform(), quad->rect, program->vertexShader().matrixLocation());
}

struct TextureProgramBinding {
    template<class Program> void set(
        Program* program, WebKit::WebGraphicsContext3D* context)
    {
        DCHECK(program && (program->initialized() || context->isContextLost()));
        programId = program->program();
        samplerLocation = program->fragmentShader().samplerLocation();
        matrixLocation = program->vertexShader().matrixLocation();
        alphaLocation = program->fragmentShader().alphaLocation();
    }
    int programId;
    int samplerLocation;
    int matrixLocation;
    int alphaLocation;
};

struct TexTransformTextureProgramBinding : TextureProgramBinding {
    template<class Program> void set(
        Program* program, WebKit::WebGraphicsContext3D* context)
    {
        TextureProgramBinding::set(program, context);
        texTransformLocation = program->vertexShader().texTransformLocation();
        vertexOpacityLocation = program->vertexShader().vertexOpacityLocation();
    }
    int texTransformLocation;
    int vertexOpacityLocation;
};

void GLRenderer::flushTextureQuadCache()
{
    // Check to see if we have anything to draw.
    if (m_drawCache.program_id == 0)
      return;

    // Set the correct blending mode.
    setBlendEnabled(m_drawCache.needs_blending);

    // Bind the program to the GL state.
    setUseProgram(m_drawCache.program_id);

    // Bind the correct texture sampler location.
    GLC(context(), context()->uniform1i(m_drawCache.sampler_location, 0));

    // Assume the current active textures is 0.
    ResourceProvider::ScopedReadLockGL lockedQuad(m_resourceProvider, m_drawCache.resource_id);
    GLC(context(), context()->bindTexture(GL_TEXTURE_2D, lockedQuad.textureId()));

    // set up premultiplied alpha.
    if (!m_drawCache.use_premultiplied_alpha) {
      // As it turns out, the premultiplied alpha blending function (ONE, ONE_MINUS_SRC_ALPHA)
        // will never cause the alpha channel to be set to anything less than 1.0f if it is
        // initialized to that value! Therefore, premultipliedAlpha being false is the first
        // situation we can generally see an alpha channel less than 1.0f coming out of the
        // compositor. This is causing platform differences in some layout tests (see
        // https://bugs.webkit.org/show_bug.cgi?id=82412), so in this situation, use a separate
        // blend function for the alpha channel to avoid modifying it. Don't use colorMask for this
        // as it has performance implications on some platforms.
        GLC(context(), context()->blendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE));
    }

    COMPILE_ASSERT(sizeof(Float4) == 4 * sizeof(float), struct_is_densely_packed);
    COMPILE_ASSERT(sizeof(Float16) == 16 * sizeof(float), struct_is_densely_packed);

    // Upload the tranforms for both points and uvs.
    GLC(m_context, m_context->uniformMatrix4fv(static_cast<int>(m_drawCache.matrix_location), static_cast<int>(m_drawCache.matrix_data.size()), false, reinterpret_cast<float*>(&m_drawCache.matrix_data.front())));
    GLC(m_context, m_context->uniform4fv(static_cast<int>(m_drawCache.uv_xform_location), static_cast<int>(m_drawCache.uv_xform_data.size()), reinterpret_cast<float*>(&m_drawCache.uv_xform_data.front())));
    GLC(m_context, m_context->uniform1fv(static_cast<int>(m_drawCache.vertex_opacity_location), static_cast<int>(m_drawCache.vertex_opacity_data.size()), static_cast<float*>(&m_drawCache.vertex_opacity_data.front())));

    // Draw the quads!
    GLC(m_context, m_context->drawElements(GL_TRIANGLES, 6 * m_drawCache.matrix_data.size(), GL_UNSIGNED_SHORT, 0));

    // Clean up after ourselves (reset state set above).
     if (!m_drawCache.use_premultiplied_alpha)
        GLC(m_context, m_context->blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));

    // Clear the cache.
    m_drawCache.program_id = 0;
    m_drawCache.uv_xform_data.resize(0);
    m_drawCache.vertex_opacity_data.resize(0);
    m_drawCache.matrix_data.resize(0);
}

void GLRenderer::enqueueTextureQuad(const DrawingFrame& frame, const TextureDrawQuad* quad)
{
    // Choose the correct texture program binding
    TexTransformTextureProgramBinding binding;
    if (quad->flipped)
        binding.set(textureProgramFlip(), context());
    else
        binding.set(textureProgram(), context());

    int resourceID = quad->resource_id;

    if (m_drawCache.program_id != binding.programId ||
        m_drawCache.resource_id != resourceID ||
        m_drawCache.use_premultiplied_alpha != quad->premultiplied_alpha ||
        m_drawCache.needs_blending != quad->ShouldDrawWithBlending() ||
        m_drawCache.matrix_data.size() >= 8) {
        flushTextureQuadCache();
        m_drawCache.program_id = binding.programId;
        m_drawCache.resource_id = resourceID;
        m_drawCache.use_premultiplied_alpha = quad->premultiplied_alpha;
        m_drawCache.needs_blending = quad->ShouldDrawWithBlending();

        m_drawCache.uv_xform_location = binding.texTransformLocation;
        m_drawCache.vertex_opacity_location = binding.vertexOpacityLocation;
        m_drawCache.matrix_location = binding.matrixLocation;
        m_drawCache.sampler_location = binding.samplerLocation;
    }

    // Generate the uv-transform
    const gfx::PointF& uv0 = quad->uv_top_left;
    const gfx::PointF& uv1 = quad->uv_bottom_right;
    Float4 uv = {uv0.x(), uv0.y(), uv1.x() - uv0.x(), uv1.y() - uv0.y()};
    m_drawCache.uv_xform_data.push_back(uv);

    // Generate the vertex opacity
    const float opacity = quad->opacity();
    m_drawCache.vertex_opacity_data.push_back(quad->vertex_opacity[0] * opacity);
    m_drawCache.vertex_opacity_data.push_back(quad->vertex_opacity[1] * opacity);
    m_drawCache.vertex_opacity_data.push_back(quad->vertex_opacity[2] * opacity);
    m_drawCache.vertex_opacity_data.push_back(quad->vertex_opacity[3] * opacity);

    // Generate the transform matrix
    gfx::Transform quadRectMatrix;
    quadRectTransform(&quadRectMatrix, quad->quadTransform(), quad->rect);
    quadRectMatrix = frame.projectionMatrix * quadRectMatrix;

    Float16 m;
    quadRectMatrix.matrix().asColMajorf(m.data);
    m_drawCache.matrix_data.push_back(m);
}

void GLRenderer::drawTextureQuad(const DrawingFrame& frame, const TextureDrawQuad* quad)
{
    TexTransformTextureProgramBinding binding;
    if (quad->flipped)
        binding.set(textureProgramFlip(), context());
    else
        binding.set(textureProgram(), context());
    setUseProgram(binding.programId);
    GLC(context(), context()->uniform1i(binding.samplerLocation, 0));
    const gfx::PointF& uv0 = quad->uv_top_left;
    const gfx::PointF& uv1 = quad->uv_bottom_right;
    GLC(context(), context()->uniform4f(binding.texTransformLocation, uv0.x(), uv0.y(), uv1.x() - uv0.x(), uv1.y() - uv0.y()));

    GLC(context(), context()->uniform1fv(binding.vertexOpacityLocation, 4, quad->vertex_opacity));

    ResourceProvider::ScopedSamplerGL quadResourceLock(m_resourceProvider, quad->resource_id, GL_TEXTURE_2D, GL_LINEAR);

    if (!quad->premultiplied_alpha) {
        // As it turns out, the premultiplied alpha blending function (ONE, ONE_MINUS_SRC_ALPHA)
        // will never cause the alpha channel to be set to anything less than 1.0f if it is
        // initialized to that value! Therefore, premultipliedAlpha being false is the first
        // situation we can generally see an alpha channel less than 1.0f coming out of the
        // compositor. This is causing platform differences in some layout tests (see
        // https://bugs.webkit.org/show_bug.cgi?id=82412), so in this situation, use a separate
        // blend function for the alpha channel to avoid modifying it. Don't use colorMask for this
        // as it has performance implications on some platforms.
        GLC(context(), context()->blendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE));
    }

    drawQuadGeometry(frame, quad->quadTransform(), quad->rect, binding.matrixLocation);

    if (!quad->premultiplied_alpha)
        GLC(m_context, m_context->blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
}

void GLRenderer::drawIOSurfaceQuad(const DrawingFrame& frame, const IOSurfaceDrawQuad* quad)
{
    setBlendEnabled(quad->ShouldDrawWithBlending());

    TexTransformTextureProgramBinding binding;
    binding.set(textureIOSurfaceProgram(), context());

    setUseProgram(binding.programId);
    GLC(context(), context()->uniform1i(binding.samplerLocation, 0));
    if (quad->orientation == IOSurfaceDrawQuad::FLIPPED)
        GLC(context(), context()->uniform4f(binding.texTransformLocation, 0, quad->io_surface_size.height(), quad->io_surface_size.width(), quad->io_surface_size.height() * -1.0f));
    else
        GLC(context(), context()->uniform4f(binding.texTransformLocation, 0, 0, quad->io_surface_size.width(), quad->io_surface_size.height()));

    const float vertex_opacity[] = {quad->opacity(), quad->opacity(), quad->opacity(), quad->opacity()};
    GLC(context(), context()->uniform1fv(binding.vertexOpacityLocation, 4, vertex_opacity));

    GLC(context(), context()->bindTexture(GL_TEXTURE_RECTANGLE_ARB, quad->io_surface_texture_id));

    drawQuadGeometry(frame, quad->quadTransform(), quad->rect, binding.matrixLocation);

    GLC(context(), context()->bindTexture(GL_TEXTURE_RECTANGLE_ARB, 0));
}

void GLRenderer::finishDrawingFrame(DrawingFrame& frame)
{
    m_currentFramebufferLock.reset();
    m_swapBufferRect.Union(gfx::ToEnclosingRect(frame.rootDamageRect));

    GLC(m_context, m_context->disable(GL_BLEND));
    m_blendShadow = false;

    if (settings().compositorFrameMessage) {
        CompositorFrame compositor_frame;
        compositor_frame.metadata = m_client->makeCompositorFrameMetadata();
        m_outputSurface->SendFrameToParentCompositor(&compositor_frame);
    }
}

void GLRenderer::finishDrawingQuadList()
{
    flushTextureQuadCache();
}

bool GLRenderer::flippedFramebuffer() const
{
    return true;
}

void GLRenderer::ensureScissorTestEnabled()
{
    if (m_isScissorEnabled)
        return;

    flushTextureQuadCache();
    GLC(m_context, m_context->enable(GL_SCISSOR_TEST));
    m_isScissorEnabled = true;
}

void GLRenderer::ensureScissorTestDisabled()
{
    if (!m_isScissorEnabled)
        return;

    flushTextureQuadCache();
    GLC(m_context, m_context->disable(GL_SCISSOR_TEST));
    m_isScissorEnabled = false;
}

void GLRenderer::toGLMatrix(float* glMatrix, const gfx::Transform& transform)
{
    transform.matrix().asColMajorf(glMatrix);
}

void GLRenderer::setShaderQuadF(const gfx::QuadF& quad, int quadLocation)
{
    if (quadLocation == -1)
        return;

    float point[8];
    point[0] = quad.p1().x();
    point[1] = quad.p1().y();
    point[2] = quad.p2().x();
    point[3] = quad.p2().y();
    point[4] = quad.p3().x();
    point[5] = quad.p3().y();
    point[6] = quad.p4().x();
    point[7] = quad.p4().y();
    GLC(m_context, m_context->uniform2fv(quadLocation, 4, point));
}

void GLRenderer::setShaderOpacity(float opacity, int alphaLocation)
{
    if (alphaLocation != -1)
        GLC(m_context, m_context->uniform1f(alphaLocation, opacity));
}

void GLRenderer::setBlendEnabled(bool enabled)
{
    if (enabled == m_blendShadow)
        return;

    if (enabled)
        GLC(m_context, m_context->enable(GL_BLEND));
    else
        GLC(m_context, m_context->disable(GL_BLEND));
    m_blendShadow = enabled;
}

void GLRenderer::setUseProgram(unsigned program)
{
    if (program == m_programShadow)
        return;
    GLC(m_context, m_context->useProgram(program));
    m_programShadow = program;
}

void GLRenderer::drawQuadGeometry(const DrawingFrame& frame, const gfx::Transform& drawTransform, const gfx::RectF& quadRect, int matrixLocation)
{
    gfx::Transform quadRectMatrix;
    quadRectTransform(&quadRectMatrix, drawTransform, quadRect);
    static float glMatrix[16];
    toGLMatrix(&glMatrix[0], frame.projectionMatrix * quadRectMatrix);
    GLC(m_context, m_context->uniformMatrix4fv(matrixLocation, 1, false, &glMatrix[0]));

    GLC(m_context, m_context->drawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0));
}

void GLRenderer::copyTextureToFramebuffer(const DrawingFrame& frame, int textureId, const gfx::Rect& rect, const gfx::Transform& drawMatrix)
{
    const RenderPassProgram* program = renderPassProgram();

    GLC(context(), context()->bindTexture(GL_TEXTURE_2D, textureId));

    setUseProgram(program->program());
    GLC(context(), context()->uniform1i(program->fragmentShader().samplerLocation(), 0));
    GLC(context(), context()->uniform4f(program->vertexShader().texTransformLocation(),
                                        0.0f, 0.0f, 1.0f, 1.0f));
    setShaderOpacity(1, program->fragmentShader().alphaLocation());
    drawQuadGeometry(frame, drawMatrix, rect, program->vertexShader().matrixLocation());
}

void GLRenderer::finish()
{
    TRACE_EVENT0("cc", "GLRenderer::finish");
    m_context->finish();
}

bool GLRenderer::swapBuffers()
{
    DCHECK(m_visible);
    DCHECK(!m_isBackbufferDiscarded);

    TRACE_EVENT0("cc", "GLRenderer::swapBuffers");
    // We're done! Time to swapbuffers!

    if (m_capabilities.usingPartialSwap) {
        // If supported, we can save significant bandwidth by only swapping the damaged/scissored region (clamped to the viewport)
        m_swapBufferRect.Intersect(gfx::Rect(gfx::Point(), viewportSize()));
        int flippedYPosOfRectBottom = viewportHeight() - m_swapBufferRect.y() - m_swapBufferRect.height();
        m_outputSurface->PostSubBuffer(gfx::Rect(m_swapBufferRect.x(), flippedYPosOfRectBottom, m_swapBufferRect.width(), m_swapBufferRect.height()));
    } else {
        m_outputSurface->SwapBuffers();
    }

    m_swapBufferRect = gfx::Rect();

    // We don't have real fences, so we mark read fences as passed
    // assuming a double-buffered GPU pipeline. A texture can be
    // written to after one full frame has past since it was last read.
    if (m_lastSwapFence)
        static_cast<SimpleSwapFence*>(m_lastSwapFence.get())->setHasPassed();
    m_lastSwapFence = m_resourceProvider->getReadLockFence();
    m_resourceProvider->setReadLockFence(new SimpleSwapFence());

    return true;
}

void GLRenderer::receiveCompositorFrameAck(const CompositorFrameAck& ack) {
    onSwapBuffersComplete();
}

void GLRenderer::onSwapBuffersComplete()
{
    m_client->onSwapBuffersComplete();
}

void GLRenderer::onMemoryAllocationChanged(WebGraphicsMemoryAllocation allocation)
{
    // Just ignore the memory manager when it says to set the limit to zero
    // bytes. This will happen when the memory manager thinks that the renderer
    // is not visible (which the renderer knows better).
    if (allocation.bytesLimitWhenVisible) {
        ManagedMemoryPolicy policy(
            allocation.bytesLimitWhenVisible,
            priorityCutoff(allocation.priorityCutoffWhenVisible),
            allocation.bytesLimitWhenNotVisible,
            priorityCutoff(allocation.priorityCutoffWhenNotVisible));

        if (allocation.enforceButDoNotKeepAsPolicy)
            m_client->enforceManagedMemoryPolicy(policy);
        else
            m_client->setManagedMemoryPolicy(policy);
    }

    bool oldDiscardBackbufferWhenNotVisible = m_discardBackbufferWhenNotVisible;
    m_discardBackbufferWhenNotVisible = !allocation.suggestHaveBackbuffer;
    enforceMemoryPolicy();
    if (allocation.enforceButDoNotKeepAsPolicy)
        m_discardBackbufferWhenNotVisible = oldDiscardBackbufferWhenNotVisible;
}

ManagedMemoryPolicy::PriorityCutoff GLRenderer::priorityCutoff(WebKit::WebGraphicsMemoryAllocation::PriorityCutoff priorityCutoff)
{
    // This is simple a 1:1 map, the names differ only because the WebKit names should be to match the cc names.
    switch (priorityCutoff) {
    case WebKit::WebGraphicsMemoryAllocation::PriorityCutoffAllowNothing:
        return ManagedMemoryPolicy::CUTOFF_ALLOW_NOTHING;
    case WebKit::WebGraphicsMemoryAllocation::PriorityCutoffAllowVisibleOnly:
        return ManagedMemoryPolicy::CUTOFF_ALLOW_REQUIRED_ONLY;
    case WebKit::WebGraphicsMemoryAllocation::PriorityCutoffAllowVisibleAndNearby:
        return ManagedMemoryPolicy::CUTOFF_ALLOW_NICE_TO_HAVE;
    case WebKit::WebGraphicsMemoryAllocation::PriorityCutoffAllowEverything:
        return ManagedMemoryPolicy::CUTOFF_ALLOW_EVERYTHING;
    }
    NOTREACHED();
    return ManagedMemoryPolicy::CUTOFF_ALLOW_NOTHING;
}

void GLRenderer::enforceMemoryPolicy()
{
    if (!m_visible) {
        TRACE_EVENT0("cc", "GLRenderer::enforceMemoryPolicy dropping resources");
        releaseRenderPassTextures();
        if (m_discardBackbufferWhenNotVisible)
            discardBackbuffer();
        m_resourceProvider->releaseCachedData();
        GLC(m_context, m_context->flush());
    }
}

void GLRenderer::discardBackbuffer()
{
    if (m_isBackbufferDiscarded)
        return;

    m_outputSurface->DiscardBackbuffer();

    m_isBackbufferDiscarded = true;

    // Damage tracker needs a full reset every time framebuffer is discarded.
    m_client->setFullRootLayerDamage();
}

void GLRenderer::ensureBackbuffer()
{
    if (!m_isBackbufferDiscarded)
        return;

    m_outputSurface->EnsureBackbuffer();
    m_isBackbufferDiscarded = false;
}

void GLRenderer::onContextLost()
{
    m_client->didLoseOutputSurface();
}


void GLRenderer::getFramebufferPixels(void *pixels, const gfx::Rect& rect)
{
    DCHECK(rect.right() <= viewportWidth());
    DCHECK(rect.bottom() <= viewportHeight());

    if (!pixels)
        return;

    makeContextCurrent();

    bool doWorkaround = needsIOSurfaceReadbackWorkaround();

    GLuint temporaryTexture = 0;
    GLuint temporaryFBO = 0;

    if (doWorkaround) {
        // On Mac OS X, calling glReadPixels against an FBO whose color attachment is an
        // IOSurface-backed texture causes corruption of future glReadPixels calls, even those on
        // different OpenGL contexts. It is believed that this is the root cause of top crasher
        // http://crbug.com/99393. <rdar://problem/10949687>

        temporaryTexture = m_context->createTexture();
        GLC(m_context, m_context->bindTexture(GL_TEXTURE_2D, temporaryTexture));
        GLC(m_context, m_context->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GLC(m_context, m_context->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GLC(m_context, m_context->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLC(m_context, m_context->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        // Copy the contents of the current (IOSurface-backed) framebuffer into a temporary texture.
        GLC(m_context, m_context->copyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, viewportSize().width(), viewportSize().height(), 0));
        temporaryFBO = m_context->createFramebuffer();
        // Attach this texture to an FBO, and perform the readback from that FBO.
        GLC(m_context, m_context->bindFramebuffer(GL_FRAMEBUFFER, temporaryFBO));
        GLC(m_context, m_context->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, temporaryTexture, 0));

        DCHECK(m_context->checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    }

    scoped_array<uint8_t> srcPixels(new uint8_t[rect.width() * rect.height() * 4]);
    GLC(m_context, m_context->readPixels(rect.x(), viewportSize().height() - rect.bottom(), rect.width(), rect.height(),
                                         GL_RGBA, GL_UNSIGNED_BYTE, srcPixels.get()));

    uint8_t* destPixels = static_cast<uint8_t*>(pixels);
    size_t rowBytes = rect.width() * 4;
    int numRows = rect.height();
    size_t totalBytes = numRows * rowBytes;
    for (size_t destY = 0; destY < totalBytes; destY += rowBytes) {
        // Flip Y axis.
        size_t srcY = totalBytes - destY - rowBytes;
        // Swizzle BGRA -> RGBA.
        for (size_t x = 0; x < rowBytes; x += 4) {
            destPixels[destY + (x+0)] = srcPixels.get()[srcY + (x+2)];
            destPixels[destY + (x+1)] = srcPixels.get()[srcY + (x+1)];
            destPixels[destY + (x+2)] = srcPixels.get()[srcY + (x+0)];
            destPixels[destY + (x+3)] = srcPixels.get()[srcY + (x+3)];
        }
    }

    if (doWorkaround) {
        // Clean up.
        GLC(m_context, m_context->bindFramebuffer(GL_FRAMEBUFFER, 0));
        GLC(m_context, m_context->bindTexture(GL_TEXTURE_2D, 0));
        GLC(m_context, m_context->deleteFramebuffer(temporaryFBO));
        GLC(m_context, m_context->deleteTexture(temporaryTexture));
    }

    enforceMemoryPolicy();
}

bool GLRenderer::getFramebufferTexture(ScopedResource* texture, const gfx::Rect& deviceRect)
{
    DCHECK(!texture->id() || (texture->size() == deviceRect.size() && texture->format() == GL_RGB));

    if (!texture->id() && !texture->Allocate(deviceRect.size(), GL_RGB, ResourceProvider::TextureUsageAny))
        return false;

    ResourceProvider::ScopedWriteLockGL lock(m_resourceProvider, texture->id());
    GLC(m_context, m_context->bindTexture(GL_TEXTURE_2D, lock.textureId()));
    GLC(m_context, m_context->copyTexImage2D(GL_TEXTURE_2D, 0, texture->format(),
                                             deviceRect.x(), deviceRect.y(), deviceRect.width(), deviceRect.height(), 0));
    return true;
}

bool GLRenderer::useScopedTexture(DrawingFrame& frame, const ScopedResource* texture, const gfx::Rect& viewportRect)
{
    DCHECK(texture->id());
    frame.currentRenderPass = 0;
    frame.currentTexture = texture;

    return bindFramebufferToTexture(frame, texture, viewportRect);
}

void GLRenderer::bindFramebufferToOutputSurface(DrawingFrame& frame)
{
    m_currentFramebufferLock.reset();
    m_outputSurface->BindFramebuffer();
}

bool GLRenderer::bindFramebufferToTexture(DrawingFrame& frame, const ScopedResource* texture, const gfx::Rect& framebufferRect)
{
    DCHECK(texture->id());

    GLC(m_context, m_context->bindFramebuffer(GL_FRAMEBUFFER, m_offscreenFramebufferId));
    m_currentFramebufferLock = make_scoped_ptr(new ResourceProvider::ScopedWriteLockGL(m_resourceProvider, texture->id()));
    unsigned textureId = m_currentFramebufferLock->textureId();
    GLC(m_context, m_context->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0));

    DCHECK(m_context->checkFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE || isContextLost());

    initializeMatrices(frame, framebufferRect, false);
    setDrawViewportSize(framebufferRect.size());

    return true;
}

void GLRenderer::setScissorTestRect(const gfx::Rect& scissorRect)
{
    ensureScissorTestEnabled();

    // Don't unnecessarily ask the context to change the scissor, because it
    // may cause undesired GPU pipeline flushes.
    if (scissorRect == m_scissorRect)
        return;

    m_scissorRect = scissorRect;
    flushTextureQuadCache();
    GLC(m_context, m_context->scissor(scissorRect.x(), scissorRect.y(), scissorRect.width(), scissorRect.height()));
}

void GLRenderer::setDrawViewportSize(const gfx::Size& viewportSize)
{
    GLC(m_context, m_context->viewport(0, 0, viewportSize.width(), viewportSize.height()));
}

bool GLRenderer::makeContextCurrent()
{
    return m_context->makeContextCurrent();
}

bool GLRenderer::initializeSharedObjects()
{
    TRACE_EVENT0("cc", "GLRenderer::initializeSharedObjects");
    makeContextCurrent();

    // Create an FBO for doing offscreen rendering.
    GLC(m_context, m_offscreenFramebufferId = m_context->createFramebuffer());

    // We will always need these programs to render, so create the programs eagerly so that the shader compilation can
    // start while we do other work. Other programs are created lazily on first access.
    m_sharedGeometry = make_scoped_ptr(new GeometryBinding(m_context, quadVertexRect()));
    m_renderPassProgram = make_scoped_ptr(new RenderPassProgram(m_context));
    m_tileProgram = make_scoped_ptr(new TileProgram(m_context));
    m_tileProgramOpaque = make_scoped_ptr(new TileProgramOpaque(m_context));

    GLC(m_context, m_context->flush());

    return true;
}

const GLRenderer::TileCheckerboardProgram* GLRenderer::tileCheckerboardProgram()
{
    if (!m_tileCheckerboardProgram)
        m_tileCheckerboardProgram = make_scoped_ptr(new TileCheckerboardProgram(m_context));
    if (!m_tileCheckerboardProgram->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::checkerboardProgram::initalize");
        m_tileCheckerboardProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_tileCheckerboardProgram.get();
}

const GLRenderer::SolidColorProgram* GLRenderer::solidColorProgram()
{
    if (!m_solidColorProgram)
        m_solidColorProgram = make_scoped_ptr(new SolidColorProgram(m_context));
    if (!m_solidColorProgram->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::solidColorProgram::initialize");
        m_solidColorProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_solidColorProgram.get();
}

const GLRenderer::SolidColorProgramAA* GLRenderer::solidColorProgramAA()
{
    if (!m_solidColorProgramAA)
         m_solidColorProgramAA = make_scoped_ptr(new SolidColorProgramAA(m_context));
    if (!m_solidColorProgramAA->initialized()) {
         TRACE_EVENT0("cc", "GLRenderer::solidColorProgramAA::initialize");
         m_solidColorProgramAA->initialize(m_context, m_isUsingBindUniform);
    }
    return m_solidColorProgramAA.get();
}

const GLRenderer::RenderPassProgram* GLRenderer::renderPassProgram()
{
    DCHECK(m_renderPassProgram);
    if (!m_renderPassProgram->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::renderPassProgram::initialize");
        m_renderPassProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_renderPassProgram.get();
}

const GLRenderer::RenderPassProgramAA* GLRenderer::renderPassProgramAA()
{
    if (!m_renderPassProgramAA)
        m_renderPassProgramAA = make_scoped_ptr(new RenderPassProgramAA(m_context));
    if (!m_renderPassProgramAA->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::renderPassProgramAA::initialize");
        m_renderPassProgramAA->initialize(m_context, m_isUsingBindUniform);
    }
    return m_renderPassProgramAA.get();
}

const GLRenderer::RenderPassMaskProgram* GLRenderer::renderPassMaskProgram()
{
    if (!m_renderPassMaskProgram)
        m_renderPassMaskProgram = make_scoped_ptr(new RenderPassMaskProgram(m_context));
    if (!m_renderPassMaskProgram->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::renderPassMaskProgram::initialize");
        m_renderPassMaskProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_renderPassMaskProgram.get();
}

const GLRenderer::RenderPassMaskProgramAA* GLRenderer::renderPassMaskProgramAA()
{
    if (!m_renderPassMaskProgramAA)
        m_renderPassMaskProgramAA = make_scoped_ptr(new RenderPassMaskProgramAA(m_context));
    if (!m_renderPassMaskProgramAA->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::renderPassMaskProgramAA::initialize");
        m_renderPassMaskProgramAA->initialize(m_context, m_isUsingBindUniform);
    }
    return m_renderPassMaskProgramAA.get();
}

const GLRenderer::TileProgram* GLRenderer::tileProgram()
{
    DCHECK(m_tileProgram);
    if (!m_tileProgram->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::tileProgram::initialize");
        m_tileProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_tileProgram.get();
}

const GLRenderer::TileProgramOpaque* GLRenderer::tileProgramOpaque()
{
    DCHECK(m_tileProgramOpaque);
    if (!m_tileProgramOpaque->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::tileProgramOpaque::initialize");
        m_tileProgramOpaque->initialize(m_context, m_isUsingBindUniform);
    }
    return m_tileProgramOpaque.get();
}

const GLRenderer::TileProgramAA* GLRenderer::tileProgramAA()
{
    if (!m_tileProgramAA)
        m_tileProgramAA = make_scoped_ptr(new TileProgramAA(m_context));
    if (!m_tileProgramAA->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::tileProgramAA::initialize");
        m_tileProgramAA->initialize(m_context, m_isUsingBindUniform);
    }
    return m_tileProgramAA.get();
}

const GLRenderer::TileProgramSwizzle* GLRenderer::tileProgramSwizzle()
{
    if (!m_tileProgramSwizzle)
        m_tileProgramSwizzle = make_scoped_ptr(new TileProgramSwizzle(m_context));
    if (!m_tileProgramSwizzle->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::tileProgramSwizzle::initialize");
        m_tileProgramSwizzle->initialize(m_context, m_isUsingBindUniform);
    }
    return m_tileProgramSwizzle.get();
}

const GLRenderer::TileProgramSwizzleOpaque* GLRenderer::tileProgramSwizzleOpaque()
{
    if (!m_tileProgramSwizzleOpaque)
        m_tileProgramSwizzleOpaque = make_scoped_ptr(new TileProgramSwizzleOpaque(m_context));
    if (!m_tileProgramSwizzleOpaque->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::tileProgramSwizzleOpaque::initialize");
        m_tileProgramSwizzleOpaque->initialize(m_context, m_isUsingBindUniform);
    }
    return m_tileProgramSwizzleOpaque.get();
}

const GLRenderer::TileProgramSwizzleAA* GLRenderer::tileProgramSwizzleAA()
{
    if (!m_tileProgramSwizzleAA)
        m_tileProgramSwizzleAA = make_scoped_ptr(new TileProgramSwizzleAA(m_context));
    if (!m_tileProgramSwizzleAA->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::tileProgramSwizzleAA::initialize");
        m_tileProgramSwizzleAA->initialize(m_context, m_isUsingBindUniform);
    }
    return m_tileProgramSwizzleAA.get();
}

const GLRenderer::TextureProgram* GLRenderer::textureProgram()
{
    if (!m_textureProgram)
        m_textureProgram = make_scoped_ptr(new TextureProgram(m_context));
    if (!m_textureProgram->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::textureProgram::initialize");
        m_textureProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_textureProgram.get();
}

const GLRenderer::TextureProgramFlip* GLRenderer::textureProgramFlip()
{
    if (!m_textureProgramFlip)
        m_textureProgramFlip = make_scoped_ptr(new TextureProgramFlip(m_context));
    if (!m_textureProgramFlip->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::textureProgramFlip::initialize");
        m_textureProgramFlip->initialize(m_context, m_isUsingBindUniform);
    }
    return m_textureProgramFlip.get();
}

const GLRenderer::TextureIOSurfaceProgram* GLRenderer::textureIOSurfaceProgram()
{
    if (!m_textureIOSurfaceProgram)
        m_textureIOSurfaceProgram = make_scoped_ptr(new TextureIOSurfaceProgram(m_context));
    if (!m_textureIOSurfaceProgram->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::textureIOSurfaceProgram::initialize");
        m_textureIOSurfaceProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_textureIOSurfaceProgram.get();
}

const GLRenderer::VideoYUVProgram* GLRenderer::videoYUVProgram()
{
    if (!m_videoYUVProgram)
        m_videoYUVProgram = make_scoped_ptr(new VideoYUVProgram(m_context));
    if (!m_videoYUVProgram->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::videoYUVProgram::initialize");
        m_videoYUVProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_videoYUVProgram.get();
}

const GLRenderer::VideoStreamTextureProgram* GLRenderer::videoStreamTextureProgram()
{
    if (!m_videoStreamTextureProgram)
        m_videoStreamTextureProgram = make_scoped_ptr(new VideoStreamTextureProgram(m_context));
    if (!m_videoStreamTextureProgram->initialized()) {
        TRACE_EVENT0("cc", "GLRenderer::streamTextureProgram::initialize");
        m_videoStreamTextureProgram->initialize(m_context, m_isUsingBindUniform);
    }
    return m_videoStreamTextureProgram.get();
}

void GLRenderer::cleanupSharedObjects()
{
    makeContextCurrent();

    m_sharedGeometry.reset();

    if (m_tileProgram)
        m_tileProgram->cleanup(m_context);
    if (m_tileProgramOpaque)
        m_tileProgramOpaque->cleanup(m_context);
    if (m_tileProgramSwizzle)
        m_tileProgramSwizzle->cleanup(m_context);
    if (m_tileProgramSwizzleOpaque)
        m_tileProgramSwizzleOpaque->cleanup(m_context);
    if (m_tileProgramAA)
        m_tileProgramAA->cleanup(m_context);
    if (m_tileProgramSwizzleAA)
        m_tileProgramSwizzleAA->cleanup(m_context);
    if (m_tileCheckerboardProgram)
        m_tileCheckerboardProgram->cleanup(m_context);

    if (m_renderPassMaskProgram)
        m_renderPassMaskProgram->cleanup(m_context);
    if (m_renderPassProgram)
        m_renderPassProgram->cleanup(m_context);
    if (m_renderPassMaskProgramAA)
        m_renderPassMaskProgramAA->cleanup(m_context);
    if (m_renderPassProgramAA)
        m_renderPassProgramAA->cleanup(m_context);

    if (m_textureProgram)
        m_textureProgram->cleanup(m_context);
    if (m_textureProgramFlip)
        m_textureProgramFlip->cleanup(m_context);
    if (m_textureIOSurfaceProgram)
        m_textureIOSurfaceProgram->cleanup(m_context);

    if (m_videoYUVProgram)
        m_videoYUVProgram->cleanup(m_context);
    if (m_videoStreamTextureProgram)
        m_videoStreamTextureProgram->cleanup(m_context);

    if (m_solidColorProgram)
        m_solidColorProgram->cleanup(m_context);
    if (m_solidColorProgramAA)
      m_solidColorProgramAA->cleanup(m_context);

    if (m_offscreenFramebufferId)
        GLC(m_context, m_context->deleteFramebuffer(m_offscreenFramebufferId));

    releaseRenderPassTextures();
}

bool GLRenderer::isContextLost()
{
    return (m_context->getGraphicsResetStatusARB() != GL_NO_ERROR);
}

}  // namespace cc
