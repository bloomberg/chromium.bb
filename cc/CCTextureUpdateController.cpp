// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCTextureUpdateController.h"

#include "GraphicsContext3D.h"
#include "TextureCopier.h"
#include "TextureUploader.h"
#include "TraceEvent.h"
#include <limits>
#include <wtf/CurrentTime.h>

namespace {

// Number of partial updates we allow.
static const size_t maxPartialTextureUpdatesMax = 12;

// Measured in seconds.
static const double textureUpdateTickRate = 0.004;

// Measured in seconds.
static const double uploaderBusyTickRate = 0.001;

// Flush interval when performing texture uploads.
static const int textureUploadFlushPeriod = 4;

} // anonymous namespace

namespace cc {

size_t CCTextureUpdateController::maxPartialTextureUpdates()
{
    return maxPartialTextureUpdatesMax;
}

size_t CCTextureUpdateController::maxFullUpdatesPerTick(TextureUploader* uploader)
{
    double texturesPerSecond = uploader->estimatedTexturesPerSecond();
    size_t texturesPerTick = floor(textureUpdateTickRate * texturesPerSecond);
    return texturesPerTick ? texturesPerTick : 1;
}

void CCTextureUpdateController::updateTextures(CCResourceProvider* resourceProvider, TextureUploader* uploader, CCTextureUpdateQueue* queue)
{
    size_t uploadCount = 0;
    while (queue->fullUploadSize()) {
        if (!(uploadCount % textureUploadFlushPeriod) && uploadCount)
            resourceProvider->shallowFlushIfSupported();

        uploader->uploadTexture(
            resourceProvider, queue->takeFirstFullUpload());
        uploadCount++;
    }

    while (queue->partialUploadSize()) {
        if (!(uploadCount % textureUploadFlushPeriod) && uploadCount)
            resourceProvider->shallowFlushIfSupported();

        uploader->uploadTexture(
            resourceProvider, queue->takeFirstPartialUpload());
        uploadCount++;
    }

    if (uploadCount)
        resourceProvider->shallowFlushIfSupported();

    if (queue->copySize()) {
        TextureCopier* copier = resourceProvider->textureCopier();
        while (queue->copySize())
            copier->copyTexture(queue->takeFirstCopy());

        // If we've performed any texture copies, we need to insert a flush
        // here into the compositor context before letting the main thread
        // proceed as it may make draw calls to the source texture of one of
        // our copy operations.
        copier->flush();
    }
}

CCTextureUpdateController::CCTextureUpdateController(CCTextureUpdateControllerClient* client, CCThread* thread, PassOwnPtr<CCTextureUpdateQueue> queue, CCResourceProvider* resourceProvider, TextureUploader* uploader)
    : m_client(client)
    , m_timer(adoptPtr(new CCTimer(thread, this)))
    , m_queue(queue)
    , m_resourceProvider(resourceProvider)
    , m_uploader(uploader)
    , m_monotonicTimeLimit(0)
    , m_textureUpdatesPerTick(maxFullUpdatesPerTick(uploader))
    , m_firstUpdateAttempt(true)
{
}

CCTextureUpdateController::~CCTextureUpdateController()
{
}

void CCTextureUpdateController::performMoreUpdates(
    double monotonicTimeLimit)
{
    ASSERT(monotonicTimeLimit >= m_monotonicTimeLimit);
    m_monotonicTimeLimit = monotonicTimeLimit;

    // Update already in progress.
    if (m_timer->isActive())
        return;

    // Call updateMoreTexturesNow() directly unless it's the first update
    // attempt. This ensures that we empty the update queue in a finite
    // amount of time.
    if (m_firstUpdateAttempt) {
        // Post a 0-delay task when no updates were left. When it runs,
        // readyToFinalizeTextureUpdates() will be called.
        if (!updateMoreTexturesIfEnoughTimeRemaining())
            m_timer->startOneShot(0);

        m_firstUpdateAttempt = false;
    } else
        updateMoreTexturesNow();
}

void CCTextureUpdateController::discardUploadsToEvictedResources()
{
    m_queue->clearUploadsToEvictedResources();
}

void CCTextureUpdateController::finalize()
{
    updateTextures(m_resourceProvider, m_uploader, m_queue.get());
}

void CCTextureUpdateController::onTimerFired()
{
    if (!updateMoreTexturesIfEnoughTimeRemaining())
        m_client->readyToFinalizeTextureUpdates();
}

double CCTextureUpdateController::monotonicTimeNow() const
{
    return monotonicallyIncreasingTime();
}

double CCTextureUpdateController::updateMoreTexturesTime() const
{
    return textureUpdateTickRate;
}

size_t CCTextureUpdateController::updateMoreTexturesSize() const
{
    return m_textureUpdatesPerTick;
}

bool CCTextureUpdateController::updateMoreTexturesIfEnoughTimeRemaining()
{
    // Uploader might be busy when we're too aggressive in our upload time
    // estimate. We use a different timeout here to prevent unnecessary
    // amounts of idle time.
    if (m_uploader->isBusy()) {
        m_timer->startOneShot(uploaderBusyTickRate);
        return true;
    }

    if (!m_queue->fullUploadSize())
        return false;

    bool hasTimeRemaining = monotonicTimeNow() < m_monotonicTimeLimit - updateMoreTexturesTime();
    if (hasTimeRemaining)
        updateMoreTexturesNow();

    return true;
}

void CCTextureUpdateController::updateMoreTexturesNow()
{
    size_t uploads = std::min(
        m_queue->fullUploadSize(), updateMoreTexturesSize());
    m_timer->startOneShot(
        updateMoreTexturesTime() / updateMoreTexturesSize() * uploads);

    if (!uploads)
        return;

    size_t uploadCount = 0;
    m_uploader->beginUploads();
    while (m_queue->fullUploadSize() && uploadCount < uploads) {
        if (!(uploadCount % textureUploadFlushPeriod) && uploadCount)
            m_resourceProvider->shallowFlushIfSupported();
        m_uploader->uploadTexture(m_resourceProvider, m_queue->takeFirstFullUpload());
        uploadCount++;
    }
    m_uploader->endUploads();
    m_resourceProvider->shallowFlushIfSupported();
}

}
