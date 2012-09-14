// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCTextureUpdateController.h"

#include "GraphicsContext3D.h"
#include "TextureCopier.h"
#include "TextureUploader.h"
#include <wtf/CurrentTime.h>

namespace {

// Number of textures to update with each call to updateMoreTexturesIfEnoughTimeRemaining().
static const size_t textureUpdatesPerTick = 12;

// Measured in seconds.
static const double textureUpdateTickRate = 0.004;

// Flush interval when performing texture uploads.
static const int textureUploadFlushPeriod = 4;

} // anonymous namespace

namespace cc {

size_t CCTextureUpdateController::maxPartialTextureUpdates()
{
    return textureUpdatesPerTick;
}

void CCTextureUpdateController::updateTextures(CCResourceProvider* resourceProvider, TextureCopier* copier, TextureUploader* uploader, CCTextureUpdateQueue* queue, size_t count)
{
    if (queue->fullUploadSize() || queue->partialUploadSize()) {
        if (uploader->isBusy())
            return;

        uploader->beginUploads();

        size_t fullUploadCount = 0;
        while (queue->fullUploadSize() && fullUploadCount < count) {
            uploader->uploadTexture(resourceProvider, queue->takeFirstFullUpload());
            fullUploadCount++;
            if (!(fullUploadCount % textureUploadFlushPeriod))
                resourceProvider->shallowFlushIfSupported();
        }

        // Make sure there are no dangling uploads without a flush.
        if (fullUploadCount % textureUploadFlushPeriod)
            resourceProvider->shallowFlushIfSupported();

        bool moreUploads = queue->fullUploadSize();

        ASSERT(queue->partialUploadSize() <= count);
        // We need another update batch if the number of updates remaining
        // in |count| is greater than the remaining partial entries.
        if ((count - fullUploadCount) < queue->partialUploadSize())
            moreUploads = true;

        if (moreUploads) {
            uploader->endUploads();
            return;
        }

        size_t partialUploadCount = 0;
        while (queue->partialUploadSize()) {
            uploader->uploadTexture(resourceProvider, queue->takeFirstPartialUpload());
            partialUploadCount++;
            if (!(partialUploadCount % textureUploadFlushPeriod))
                resourceProvider->shallowFlushIfSupported();
        }

        // Make sure there are no dangling partial uploads without a flush.
        if (partialUploadCount % textureUploadFlushPeriod)
            resourceProvider->shallowFlushIfSupported();

        uploader->endUploads();
    }

    size_t copyCount = 0;
    while (queue->copySize()) {
        copier->copyTexture(queue->takeFirstCopy());
        copyCount++;
    }

    // If we've performed any texture copies, we need to insert a flush here into the compositor context
    // before letting the main thread proceed as it may make draw calls to the source texture of one of
    // our copy operations.
    if (copyCount)
        copier->flush();
}

CCTextureUpdateController::CCTextureUpdateController(CCTextureUpdateControllerClient* client, CCThread* thread, PassOwnPtr<CCTextureUpdateQueue> queue, CCResourceProvider* resourceProvider, TextureCopier* copier, TextureUploader* uploader)
    : m_client(client)
    , m_timer(adoptPtr(new CCTimer(thread, this)))
    , m_queue(queue)
    , m_resourceProvider(resourceProvider)
    , m_copier(copier)
    , m_uploader(uploader)
    , m_monotonicTimeLimit(0)
    , m_firstUpdateAttempt(true)
{
}

CCTextureUpdateController::~CCTextureUpdateController()
{
}

void CCTextureUpdateController::updateMoreTextures(double monotonicTimeLimit)
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
        // updateTexturesCompleted() will be called.
        if (!updateMoreTexturesIfEnoughTimeRemaining())
            m_timer->startOneShot(0);

        m_firstUpdateAttempt = false;
    } else
        updateMoreTexturesNow();
}

void CCTextureUpdateController::onTimerFired()
{
    if (!updateMoreTexturesIfEnoughTimeRemaining())
        m_client->updateTexturesCompleted();
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
    return textureUpdatesPerTick;
}

bool CCTextureUpdateController::updateMoreTexturesIfEnoughTimeRemaining()
{
    if (!m_queue->hasMoreUpdates())
        return false;

    bool hasTimeRemaining = monotonicTimeNow() < m_monotonicTimeLimit - updateMoreTexturesTime();
    if (hasTimeRemaining)
        updateMoreTexturesNow();

    return true;
}

void CCTextureUpdateController::updateMoreTexturesNow()
{
    m_timer->startOneShot(updateMoreTexturesTime());
    updateTextures(m_resourceProvider, m_copier, m_uploader, m_queue.get(), updateMoreTexturesSize());
}

}
