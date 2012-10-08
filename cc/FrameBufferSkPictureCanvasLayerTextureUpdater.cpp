// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "FrameBufferSkPictureCanvasLayerTextureUpdater.h"

#include "CCProxy.h"
#include "LayerPainterChromium.h"
#include "SkCanvas.h"
#include "SkGpuDevice.h"
#include <public/WebGraphicsContext3D.h>
#include <public/WebSharedGraphicsContext3D.h>

using WebKit::WebGraphicsContext3D;
using WebKit::WebSharedGraphicsContext3D;

namespace cc {

static PassOwnPtr<SkCanvas> createAcceleratedCanvas(GrContext* grContext,
                                                    IntSize canvasSize,
                                                    unsigned textureId)
{
    GrPlatformTextureDesc textureDesc;
    textureDesc.fFlags = kRenderTarget_GrPlatformTextureFlag;
    textureDesc.fWidth = canvasSize.width();
    textureDesc.fHeight = canvasSize.height();
    textureDesc.fConfig = kSkia8888_GrPixelConfig;
    textureDesc.fTextureHandle = textureId;
    SkAutoTUnref<GrTexture> target(grContext->createPlatformTexture(textureDesc));
    SkAutoTUnref<SkDevice> device(new SkGpuDevice(grContext, target.get()));
    return adoptPtr(new SkCanvas(device.get()));
}

FrameBufferSkPictureCanvasLayerTextureUpdater::Texture::Texture(FrameBufferSkPictureCanvasLayerTextureUpdater* textureUpdater, PassOwnPtr<CCPrioritizedTexture> texture)
    : LayerTextureUpdater::Texture(texture)
    , m_textureUpdater(textureUpdater)
{
}

FrameBufferSkPictureCanvasLayerTextureUpdater::Texture::~Texture()
{
}

void FrameBufferSkPictureCanvasLayerTextureUpdater::Texture::updateRect(CCResourceProvider* resourceProvider, const IntRect& sourceRect, const IntSize& destOffset)
{
    WebGraphicsContext3D* sharedContext = CCProxy::hasImplThread() ? WebSharedGraphicsContext3D::compositorThreadContext() : WebSharedGraphicsContext3D::mainThreadContext();
    GrContext* sharedGrContext = CCProxy::hasImplThread() ? WebSharedGraphicsContext3D::compositorThreadGrContext() : WebSharedGraphicsContext3D::mainThreadGrContext();
    if (!sharedContext || !sharedGrContext)
        return;
    textureUpdater()->updateTextureRect(sharedContext, sharedGrContext, resourceProvider, texture(), sourceRect, destOffset);
}

PassRefPtr<FrameBufferSkPictureCanvasLayerTextureUpdater> FrameBufferSkPictureCanvasLayerTextureUpdater::create(PassOwnPtr<LayerPainterChromium> painter)
{
    return adoptRef(new FrameBufferSkPictureCanvasLayerTextureUpdater(painter));
}

FrameBufferSkPictureCanvasLayerTextureUpdater::FrameBufferSkPictureCanvasLayerTextureUpdater(PassOwnPtr<LayerPainterChromium> painter)
    : SkPictureCanvasLayerTextureUpdater(painter)
{
}

FrameBufferSkPictureCanvasLayerTextureUpdater::~FrameBufferSkPictureCanvasLayerTextureUpdater()
{
}

PassOwnPtr<LayerTextureUpdater::Texture> FrameBufferSkPictureCanvasLayerTextureUpdater::createTexture(CCPrioritizedTextureManager* manager)
{
    return adoptPtr(new Texture(this, CCPrioritizedTexture::create(manager)));
}

LayerTextureUpdater::SampledTexelFormat FrameBufferSkPictureCanvasLayerTextureUpdater::sampledTexelFormat(GC3Denum textureFormat)
{
    // Here we directly render to the texture, so the component order is always correct.
    return LayerTextureUpdater::SampledTexelFormatRGBA;
}

void FrameBufferSkPictureCanvasLayerTextureUpdater::updateTextureRect(WebGraphicsContext3D* context, GrContext* grContext, CCResourceProvider* resourceProvider, CCPrioritizedTexture* texture, const IntRect& sourceRect, const IntSize& destOffset)
{
    texture->acquireBackingTexture(resourceProvider);
    // Flush the context in which the backing texture is created so that it
    // is available in other shared contexts. It is important to do here
    // because the backing texture is created in one context while it is
    // being written to in another.
    resourceProvider->flush();
    CCResourceProvider::ScopedWriteLockGL lock(resourceProvider, texture->resourceId());

    // Make sure ganesh uses the correct GL context.
    context->makeContextCurrent();
    // Create an accelerated canvas to draw on.
    OwnPtr<SkCanvas> canvas = createAcceleratedCanvas(grContext, texture->size(), lock.textureId());

    // The compositor expects the textures to be upside-down so it can flip
    // the final composited image. Ganesh renders the image upright so we
    // need to do a y-flip.
    canvas->translate(0.0, texture->size().height());
    canvas->scale(1.0, -1.0);
    // Clip to the destination on the texture that must be updated.
    canvas->clipRect(SkRect::MakeXYWH(destOffset.width(), destOffset.height(), sourceRect.width(), sourceRect.height()));
    // Translate the origin of contentRect to that of destRect.
    // Note that destRect is defined relative to sourceRect.
    canvas->translate(contentRect().x() - sourceRect.x() + destOffset.width(),
                      contentRect().y() - sourceRect.y() + destOffset.height());
    drawPicture(canvas.get());

    // Flush ganesh context so that all the rendered stuff appears on the texture.
    grContext->flush();

    // Flush the GL context so rendering results from this context are visible in the compositor's context.
    context->flush();
}

} // namespace cc
#endif // USE(ACCELERATED_COMPOSITING)
