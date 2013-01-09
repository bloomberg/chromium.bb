// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resource_update_controller.h"

#include <limits>

#include "base/debug/trace_event.h"
#include "cc/prioritized_resource.h"
#include "cc/resource_provider.h"
#include "cc/texture_copier.h"
#include "cc/thread.h"
#include "skia/ext/refptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSharedGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/gpu/SkGpuDevice.h"

using WebKit::WebGraphicsContext3D;
using WebKit::WebSharedGraphicsContext3D;

namespace {

// Number of partial updates we allow.
const size_t partialTextureUpdatesMax = 12;

// Measured in seconds.
const double textureUpdateTickRate = 0.004;

// Measured in seconds.
const double uploaderBusyTickRate = 0.001;

// Number of blocking update intervals to allow.
const size_t maxBlockingUpdateIntervals = 4;

skia::RefPtr<SkCanvas> createAcceleratedCanvas(
    GrContext* grContext, gfx::Size canvasSize, unsigned textureId)
{
    GrPlatformTextureDesc textureDesc;
    textureDesc.fFlags = kRenderTarget_GrPlatformTextureFlag;
    textureDesc.fWidth = canvasSize.width();
    textureDesc.fHeight = canvasSize.height();
    textureDesc.fConfig = kSkia8888_GrPixelConfig;
    textureDesc.fTextureHandle = textureId;
    skia::RefPtr<GrTexture> target =
        skia::AdoptRef(grContext->createPlatformTexture(textureDesc));
    skia::RefPtr<SkDevice> device =
        skia::AdoptRef(new SkGpuDevice(grContext, target.get()));
    return skia::AdoptRef(new SkCanvas(device.get()));
}

}  // namespace

namespace cc {

size_t ResourceUpdateController::maxPartialTextureUpdates()
{
    return partialTextureUpdatesMax;
}

size_t ResourceUpdateController::maxFullUpdatesPerTick(
    ResourceProvider* resourceProvider)
{
    double texturesPerSecond = resourceProvider->estimatedUploadsPerSecond();
    size_t texturesPerTick = floor(textureUpdateTickRate * texturesPerSecond);
    return texturesPerTick ? texturesPerTick : 1;
}

ResourceUpdateController::ResourceUpdateController(ResourceUpdateControllerClient* client, Thread* thread, scoped_ptr<ResourceUpdateQueue> queue, ResourceProvider* resourceProvider, bool hasImplThread)
    : m_client(client)
    , m_hasImplThread(hasImplThread)
    , m_queue(queue.Pass())
    , m_resourceProvider(resourceProvider)
    , m_textureUpdatesPerTick(maxFullUpdatesPerTick(resourceProvider))
    , m_firstUpdateAttempt(true)
    , m_thread(thread)
    , m_weakFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this))
    , m_taskPosted(false)
{
}

ResourceUpdateController::~ResourceUpdateController()
{
}

void ResourceUpdateController::performMoreUpdates(
    base::TimeTicks timeLimit)
{
    m_timeLimit = timeLimit;

    // Update already in progress.
    if (m_taskPosted)
        return;

    // Call updateMoreTexturesNow() directly unless it's the first update
    // attempt. This ensures that we empty the update queue in a finite
    // amount of time.
    if (!m_firstUpdateAttempt)
        updateMoreTexturesNow();

    // Post a 0-delay task when no updates were left. When it runs,
    // readyToFinalizeTextureUpdates() will be called.
    if (!updateMoreTexturesIfEnoughTimeRemaining()) {
        m_taskPosted = true;
        m_thread->postTask(
            base::Bind(&ResourceUpdateController::onTimerFired,
                       m_weakFactory.GetWeakPtr()));
    }

    m_firstUpdateAttempt = false;
}

void ResourceUpdateController::discardUploadsToEvictedResources()
{
    m_queue->clearUploadsToEvictedResources();
}

void ResourceUpdateController::updateTexture(ResourceUpdate update)
{
    if (update.picture) {
        PrioritizedResource* texture = update.texture;
        gfx::Rect pictureRect = update.content_rect;
        gfx::Rect sourceRect = update.source_rect;
        gfx::Vector2d destOffset = update.dest_offset;

        texture->acquireBackingTexture(m_resourceProvider);
        DCHECK(texture->haveBackingTexture());

        DCHECK(m_resourceProvider->resourceType(texture->resourceId()) ==
               ResourceProvider::GLTexture);

        WebGraphicsContext3D* paintContext = m_hasImplThread ?
            WebSharedGraphicsContext3D::compositorThreadContext() :
            WebSharedGraphicsContext3D::mainThreadContext();
        GrContext* paintGrContext = m_hasImplThread ?
            WebSharedGraphicsContext3D::compositorThreadGrContext() :
            WebSharedGraphicsContext3D::mainThreadGrContext();

        // Flush the context in which the backing texture is created so that it
        // is available in other shared contexts. It is important to do here
        // because the backing texture is created in one context while it is
        // being written to in another.
        ResourceProvider::ScopedWriteLockGL lock(
            m_resourceProvider, texture->resourceId());
        m_resourceProvider->flush();

        // Make sure ganesh uses the correct GL context.
        paintContext->makeContextCurrent();

        // Create an accelerated canvas to draw on.
        skia::RefPtr<SkCanvas> canvas = createAcceleratedCanvas(
            paintGrContext, texture->size(), lock.textureId());

        // The compositor expects the textures to be upside-down so it can flip
        // the final composited image. Ganesh renders the image upright so we
        // need to do a y-flip.
        canvas->translate(0.0, texture->size().height());
        canvas->scale(1.0, -1.0);
        // Clip to the destination on the texture that must be updated.
        canvas->clipRect(SkRect::MakeXYWH(destOffset.x(),
                                          destOffset.y(),
                                          sourceRect.width(),
                                          sourceRect.height()));
        // Translate the origin of pictureRect to destOffset.
        // Note that destOffset is defined relative to sourceRect.
        canvas->translate(
            pictureRect.x() - sourceRect.x() + destOffset.x(),
            pictureRect.y() - sourceRect.y() + destOffset.y());
        canvas->drawPicture(*update.picture);

        // Flush ganesh context so that all the rendered stuff appears on the
        // texture.
        paintGrContext->flush();

        // Flush the GL context so rendering results from this context are
        // visible in the compositor's context.
        paintContext->flush();
    }

    if (update.bitmap) {
        update.bitmap->lockPixels();
        update.texture->setPixels(
            m_resourceProvider,
            static_cast<const uint8_t*>(update.bitmap->getPixels()),
            update.content_rect,
            update.source_rect,
            update.dest_offset);
        update.bitmap->unlockPixels();
    }
}

void ResourceUpdateController::finalize()
{
    while (m_queue->fullUploadSize())
        updateTexture(m_queue->takeFirstFullUpload());

    while (m_queue->partialUploadSize())
        updateTexture(m_queue->takeFirstPartialUpload());

    m_resourceProvider->flushUploads();

    if (m_queue->copySize()) {
        TextureCopier* copier = m_resourceProvider->textureCopier();
        while (m_queue->copySize())
            copier->copyTexture(m_queue->takeFirstCopy());

        // If we've performed any texture copies, we need to insert a flush
        // here into the compositor context before letting the main thread
        // proceed as it may make draw calls to the source texture of one of
        // our copy operations.
        copier->flush();
    }
}

void ResourceUpdateController::onTimerFired()
{
    m_taskPosted = false;
    if (!updateMoreTexturesIfEnoughTimeRemaining())
        m_client->readyToFinalizeTextureUpdates();
}

base::TimeTicks ResourceUpdateController::now() const
{
    return base::TimeTicks::Now();
}

base::TimeDelta ResourceUpdateController::updateMoreTexturesTime() const
{
    return base::TimeDelta::FromMilliseconds(textureUpdateTickRate * 1000);
}

size_t ResourceUpdateController::updateMoreTexturesSize() const
{
    return m_textureUpdatesPerTick;
}

size_t ResourceUpdateController::maxBlockingUpdates() const
{
    return updateMoreTexturesSize() * maxBlockingUpdateIntervals;
}

base::TimeDelta ResourceUpdateController::pendingUpdateTime() const
{
    base::TimeDelta updateOneResourceTime =
        updateMoreTexturesTime() / updateMoreTexturesSize();
    return updateOneResourceTime * m_resourceProvider->numBlockingUploads();
}

bool ResourceUpdateController::updateMoreTexturesIfEnoughTimeRemaining()
{
    while (m_resourceProvider->numBlockingUploads() < maxBlockingUpdates()) {
        if (!m_queue->fullUploadSize())
            return false;

        if (!m_timeLimit.is_null()) {
            // Estimated completion time of all pending updates.
            base::TimeTicks completionTime = this->now() + pendingUpdateTime();

            // Time remaining based on current completion estimate.
            base::TimeDelta timeRemaining = m_timeLimit - completionTime;

            if (timeRemaining < updateMoreTexturesTime())
                return true;
        }

        updateMoreTexturesNow();
    }

    m_taskPosted = true;
    m_thread->postDelayedTask(
        base::Bind(&ResourceUpdateController::onTimerFired,
                   m_weakFactory.GetWeakPtr()),
        uploaderBusyTickRate * 1000);
    return true;
}

void ResourceUpdateController::updateMoreTexturesNow()
{
    size_t uploads = std::min(
        m_queue->fullUploadSize(), updateMoreTexturesSize());

    if (!uploads)
        return;

    while (m_queue->fullUploadSize() && uploads--)
        updateTexture(m_queue->takeFirstFullUpload());

    m_resourceProvider->flushUploads();
}

}  // namespace cc
