// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCRendererSoftware.h"

#include "CCDebugBorderDrawQuad.h"
#include "CCSolidColorDrawQuad.h"
#include "CCTextureDrawQuad.h"
#include "CCTileDrawQuad.h"
#include "CCRenderPassDrawQuad.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkLayerRasterizer.h"
#include <public/WebImage.h>
#include <public/WebSize.h>
#include <public/WebTransformationMatrix.h>

using WebKit::WebCompositorSoftwareOutputDevice;
using WebKit::WebImage;
using WebKit::WebSize;
using WebKit::WebTransformationMatrix;

namespace cc {

namespace {

SkRect toSkRect(const FloatRect& rect)
{
    return SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height());
}

SkIRect toSkIRect(const IntRect& rect)
{
    return SkIRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height());
}

void toSkMatrix(SkMatrix* flattened, const WebTransformationMatrix& m)
{
    // Convert from 4x4 to 3x3 by dropping the third row and column.
    flattened->set(0, m.m11());
    flattened->set(1, m.m21());
    flattened->set(2, m.m41());
    flattened->set(3, m.m12());
    flattened->set(4, m.m22());
    flattened->set(5, m.m42());
    flattened->set(6, m.m14());
    flattened->set(7, m.m24());
    flattened->set(8, m.m44());
}

bool isScaleAndTranslate(const SkMatrix& matrix)
{
    return SkScalarNearlyZero(matrix[SkMatrix::kMSkewX]) &&
           SkScalarNearlyZero(matrix[SkMatrix::kMSkewY]) &&
           SkScalarNearlyZero(matrix[SkMatrix::kMPersp0]) &&
           SkScalarNearlyZero(matrix[SkMatrix::kMPersp1]) &&
           SkScalarNearlyZero(matrix[SkMatrix::kMPersp2] - 1.0f);
}

} // anonymous namespace

scoped_ptr<CCRendererSoftware> CCRendererSoftware::create(CCRendererClient* client, CCResourceProvider* resourceProvider, WebCompositorSoftwareOutputDevice* outputDevice)
{
    return make_scoped_ptr(new CCRendererSoftware(client, resourceProvider, outputDevice));
}

CCRendererSoftware::CCRendererSoftware(CCRendererClient* client, CCResourceProvider* resourceProvider, WebCompositorSoftwareOutputDevice* outputDevice)
    : CCDirectRenderer(client, resourceProvider)
    , m_visible(true)
    , m_outputDevice(outputDevice)
    , m_skCurrentCanvas(0)
{
    m_resourceProvider->setDefaultResourceType(CCResourceProvider::Bitmap);

    m_capabilities.maxTextureSize = INT_MAX;
    m_capabilities.bestTextureFormat = GraphicsContext3D::RGBA;
    m_capabilities.contextHasCachedFrontBuffer = true;
    m_capabilities.usingSetVisibility = true;

    viewportChanged();
}

CCRendererSoftware::~CCRendererSoftware()
{
}

const RendererCapabilities& CCRendererSoftware::capabilities() const
{
    return m_capabilities;
}

void CCRendererSoftware::viewportChanged()
{
    m_outputDevice->didChangeViewportSize(WebSize(viewportSize().width(), viewportSize().height()));
}

void CCRendererSoftware::beginDrawingFrame(DrawingFrame& frame)
{
    m_skRootCanvas = make_scoped_ptr(new SkCanvas(m_outputDevice->lock(true)->getSkBitmap()));
}

void CCRendererSoftware::finishDrawingFrame(DrawingFrame& frame)
{
    m_currentFramebufferLock.reset();
    m_skCurrentCanvas = 0;
    m_skRootCanvas.reset();
    m_outputDevice->unlock();
}

bool CCRendererSoftware::flippedFramebuffer() const
{
    return false;
}

void CCRendererSoftware::finish()
{
}

void CCRendererSoftware::bindFramebufferToOutputSurface(DrawingFrame& frame)
{
    m_currentFramebufferLock.reset();
    m_skCurrentCanvas = m_skRootCanvas.get();
}

bool CCRendererSoftware::bindFramebufferToTexture(DrawingFrame& frame, const CCScopedTexture* texture, const IntRect& framebufferRect)
{
    m_currentFramebufferLock = make_scoped_ptr(new CCResourceProvider::ScopedWriteLockSoftware(m_resourceProvider, texture->id()));
    m_skCurrentCanvas = m_currentFramebufferLock->skCanvas();
    initializeMatrices(frame, framebufferRect, false);
    setDrawViewportSize(framebufferRect.size());

    return true;
}

void CCRendererSoftware::enableScissorTestRect(const IntRect& scissorRect)
{
    m_skCurrentCanvas->clipRect(toSkRect(scissorRect), SkRegion::kReplace_Op);
}

void CCRendererSoftware::disableScissorTest()
{
    IntRect canvasRect(IntPoint(), viewportSize());
    m_skCurrentCanvas->clipRect(toSkRect(canvasRect), SkRegion::kReplace_Op);
}

void CCRendererSoftware::clearFramebuffer(DrawingFrame& frame)
{
    if (frame.currentRenderPass->hasTransparentBackground()) {
        m_skCurrentCanvas->clear(SkColorSetARGB(0, 0, 0, 0));
    } else {
#ifndef NDEBUG
        // On DEBUG builds, opaque render passes are cleared to blue to easily see regions that were not drawn on the screen.
        m_skCurrentCanvas->clear(SkColorSetARGB(255, 0, 0, 255));
#endif
    }
}

void CCRendererSoftware::setDrawViewportSize(const IntSize& viewportSize)
{
}

bool CCRendererSoftware::isSoftwareResource(CCResourceProvider::ResourceId id) const
{
    switch (m_resourceProvider->resourceType(id)) {
    case CCResourceProvider::GLTexture:
        return false;
    case CCResourceProvider::Bitmap:
        return true;
    }

    CRASH();
    return false;
}

void CCRendererSoftware::drawQuad(DrawingFrame& frame, const CCDrawQuad* quad)
{
    WebTransformationMatrix quadRectMatrix;
    quadRectTransform(&quadRectMatrix, quad->quadTransform(), quad->quadRect());
    WebTransformationMatrix contentsDeviceTransform = (frame.windowMatrix * frame.projectionMatrix * quadRectMatrix).to2dTransform();
    SkMatrix skDeviceMatrix;
    toSkMatrix(&skDeviceMatrix, contentsDeviceTransform);
    m_skCurrentCanvas->setMatrix(skDeviceMatrix);

    m_skCurrentPaint.reset();
    if (!isScaleAndTranslate(skDeviceMatrix)) {
      m_skCurrentPaint.setAntiAlias(true);
      m_skCurrentPaint.setFilterBitmap(true);
    }
    if (quad->needsBlending()) {
        m_skCurrentPaint.setAlpha(quad->opacity() * 255);
        m_skCurrentPaint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
    } else {
        m_skCurrentPaint.setXfermodeMode(SkXfermode::kSrc_Mode);
    }

    switch (quad->material()) {
    case CCDrawQuad::DebugBorder:
        drawDebugBorderQuad(frame, CCDebugBorderDrawQuad::materialCast(quad));
        break;
    case CCDrawQuad::SolidColor:
        drawSolidColorQuad(frame, CCSolidColorDrawQuad::materialCast(quad));
        break;
    case CCDrawQuad::TextureContent:
        drawTextureQuad(frame, CCTextureDrawQuad::materialCast(quad));
        break;
    case CCDrawQuad::TiledContent:
        drawTileQuad(frame, CCTileDrawQuad::materialCast(quad));
        break;
    case CCDrawQuad::RenderPass:
        drawRenderPassQuad(frame, CCRenderPassDrawQuad::materialCast(quad));
        break;
    default:
        drawUnsupportedQuad(frame, quad);
        break;
    }

    m_skCurrentCanvas->resetMatrix();
}

void CCRendererSoftware::drawDebugBorderQuad(const DrawingFrame& frame, const CCDebugBorderDrawQuad* quad)
{
    // We need to apply the matrix manually to have pixel-sized stroke width.
    SkPoint vertices[4];
    toSkRect(quadVertexRect()).toQuad(vertices);
    SkPoint transformedVertices[4];
    m_skCurrentCanvas->getTotalMatrix().mapPoints(transformedVertices, vertices, 4);
    m_skCurrentCanvas->resetMatrix();

    m_skCurrentPaint.setColor(quad->color());
    m_skCurrentPaint.setAlpha(quad->opacity() * SkColorGetA(quad->color()));
    m_skCurrentPaint.setStyle(SkPaint::kStroke_Style);
    m_skCurrentPaint.setStrokeWidth(quad->width());
    m_skCurrentCanvas->drawPoints(SkCanvas::kPolygon_PointMode, 4, transformedVertices, m_skCurrentPaint);
}

void CCRendererSoftware::drawSolidColorQuad(const DrawingFrame& frame, const CCSolidColorDrawQuad* quad)
{
    m_skCurrentPaint.setColor(quad->color());
    m_skCurrentPaint.setAlpha(quad->opacity() * SkColorGetA(quad->color()));
    m_skCurrentCanvas->drawRect(toSkRect(quadVertexRect()), m_skCurrentPaint);
}

void CCRendererSoftware::drawTextureQuad(const DrawingFrame& frame, const CCTextureDrawQuad* quad)
{
    if (!isSoftwareResource(quad->resourceId())) {
        drawUnsupportedQuad(frame, quad);
        return;
    }

    // FIXME: Add support for non-premultiplied alpha.
    CCResourceProvider::ScopedReadLockSoftware quadResourceLock(m_resourceProvider, quad->resourceId());
    FloatRect uvRect = quad->uvRect();
    uvRect.scale(quad->quadRect().width(), quad->quadRect().height());
    SkIRect skUvRect = toSkIRect(enclosingIntRect(uvRect));
    if (quad->flipped())
        m_skCurrentCanvas->scale(1, -1);
    m_skCurrentCanvas->drawBitmapRect(*quadResourceLock.skBitmap(), &skUvRect, toSkRect(quadVertexRect()), &m_skCurrentPaint);
}

void CCRendererSoftware::drawTileQuad(const DrawingFrame& frame, const CCTileDrawQuad* quad)
{
    DCHECK(isSoftwareResource(quad->resourceId()));
    CCResourceProvider::ScopedReadLockSoftware quadResourceLock(m_resourceProvider, quad->resourceId());

    SkIRect uvRect = toSkIRect(IntRect(quad->textureOffset(), quad->quadRect().size()));
    m_skCurrentCanvas->drawBitmapRect(*quadResourceLock.skBitmap(), &uvRect, toSkRect(quadVertexRect()), &m_skCurrentPaint);
}

void CCRendererSoftware::drawRenderPassQuad(const DrawingFrame& frame, const CCRenderPassDrawQuad* quad)
{
    CachedTexture* contentsTexture = m_renderPassTextures.get(quad->renderPassId());
    if (!contentsTexture || !contentsTexture->id())
        return;

    DCHECK(isSoftwareResource(contentsTexture->id()));
    CCResourceProvider::ScopedReadLockSoftware contentsTextureLock(m_resourceProvider, contentsTexture->id());

    const SkBitmap* bitmap = contentsTextureLock.skBitmap();

    SkRect sourceRect;
    bitmap->getBounds(&sourceRect);

    SkRect destRect = toSkRect(quadVertexRect());

    SkMatrix matrix;
    matrix.setRectToRect(sourceRect, destRect, SkMatrix::kFill_ScaleToFit);

    SkAutoTUnref<SkShader> shader(SkShader::CreateBitmapShader(*bitmap,
                                                               SkShader::kClamp_TileMode,
                                                               SkShader::kClamp_TileMode));
    shader->setLocalMatrix(matrix);
    m_skCurrentPaint.setShader(shader);

    if (quad->maskResourceId()) {
        CCResourceProvider::ScopedReadLockSoftware maskResourceLock(m_resourceProvider, quad->maskResourceId());
        const SkBitmap* maskBitmap = maskResourceLock.skBitmap();

        SkMatrix maskMat;
        maskMat.setRectToRect(toSkRect(quad->quadRect()), destRect, SkMatrix::kFill_ScaleToFit);
        maskMat.postTranslate(quad->maskTexCoordOffsetX(), quad->maskTexCoordOffsetY());

        SkAutoTUnref<SkShader> maskShader(SkShader::CreateBitmapShader(*maskBitmap,
                                                                       SkShader::kClamp_TileMode,
                                                                       SkShader::kClamp_TileMode));
        maskShader->setLocalMatrix(maskMat);

        SkPaint maskPaint;
        maskPaint.setShader(maskShader);

        SkAutoTUnref<SkLayerRasterizer> maskRasterizer(new SkLayerRasterizer);
        maskRasterizer->addLayer(maskPaint);

        m_skCurrentPaint.setRasterizer(maskRasterizer);
        m_skCurrentCanvas->drawRect(destRect, m_skCurrentPaint);
    } else {
        // FIXME: Apply background filters and blend with contents
        m_skCurrentCanvas->drawRect(destRect, m_skCurrentPaint);
    }
}

void CCRendererSoftware::drawUnsupportedQuad(const DrawingFrame& frame, const CCDrawQuad* quad)
{
    m_skCurrentPaint.setColor(SK_ColorMAGENTA);
    m_skCurrentPaint.setAlpha(quad->opacity() * 255);
    m_skCurrentCanvas->drawRect(toSkRect(quadVertexRect()), m_skCurrentPaint);
}

bool CCRendererSoftware::swapBuffers()
{
    if (CCProxy::hasImplThread())
        m_client->onSwapBuffersComplete();
    return true;
}

void CCRendererSoftware::getFramebufferPixels(void *pixels, const IntRect& rect)
{
    SkBitmap fullBitmap = m_outputDevice->lock(false)->getSkBitmap();
    SkBitmap subsetBitmap;
    SkIRect invertRect = SkIRect::MakeXYWH(rect.x(), viewportSize().height() - rect.maxY(), rect.width(), rect.height());
    fullBitmap.extractSubset(&subsetBitmap, invertRect);
    subsetBitmap.copyPixelsTo(pixels, rect.width() * rect.height() * 4, rect.width() * 4);
    m_outputDevice->unlock();
}

void CCRendererSoftware::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
}

}
