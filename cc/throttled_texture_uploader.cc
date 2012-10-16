// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "cc/throttled_texture_uploader.h"

#include "CCPrioritizedTexture.h"
#include "cc/proxy.h"
#include "Extensions3DChromium.h"
#include "TraceEvent.h"
#include "third_party/skia/include/gpu/SkGpuDevice.h"
#include <algorithm>
#include <public/Platform.h>
#include <public/WebGraphicsContext3D.h>
#include <public/WebSharedGraphicsContext3D.h>
#include <vector>

using WebKit::WebGraphicsContext3D;
using WebKit::WebSharedGraphicsContext3D;

namespace {

// How many previous uploads to use when predicting future throughput.
static const size_t uploadHistorySize = 100;

// Global estimated number of textures per second to maintain estimates across
// subsequent instances of ThrottledTextureUploader.
// More than one thread will not access this variable, so we do not need to synchronize access.
static double estimatedTexturesPerSecondGlobal = 48.0 * 60.0;

PassOwnPtr<SkCanvas> createAcceleratedCanvas(GrContext* grContext,
                                             cc::IntSize canvasSize,
                                             unsigned textureId)
{
    GrPlatformTextureDesc textureDesc;
    textureDesc.fFlags = kRenderTarget_GrPlatformTextureFlag;
    textureDesc.fWidth = canvasSize.width();
    textureDesc.fHeight = canvasSize.height();
    textureDesc.fConfig = kSkia8888_GrPixelConfig;
    textureDesc.fTextureHandle = textureId;
    SkAutoTUnref<GrTexture> target(
        grContext->createPlatformTexture(textureDesc));
    SkAutoTUnref<SkDevice> device(new SkGpuDevice(grContext, target.get()));
    return adoptPtr(new SkCanvas(device.get()));
}

} // anonymous namespace

namespace cc {

ThrottledTextureUploader::Query::Query(WebKit::WebGraphicsContext3D* context)
    : m_context(context)
    , m_queryId(0)
    , m_value(0)
    , m_hasValue(false)
    , m_isNonBlocking(false)
{
    m_queryId = m_context->createQueryEXT();
}

ThrottledTextureUploader::Query::~Query()
{
    m_context->deleteQueryEXT(m_queryId);
}

void ThrottledTextureUploader::Query::begin()
{
    m_hasValue = false;
    m_isNonBlocking = false;
    m_context->beginQueryEXT(Extensions3DChromium::COMMANDS_ISSUED_CHROMIUM, m_queryId);
}

void ThrottledTextureUploader::Query::end()
{
    m_context->endQueryEXT(Extensions3DChromium::COMMANDS_ISSUED_CHROMIUM);
}

bool ThrottledTextureUploader::Query::isPending()
{
    unsigned available = 1;
    m_context->getQueryObjectuivEXT(m_queryId, Extensions3DChromium::QUERY_RESULT_AVAILABLE_EXT, &available);
    return !available;
}

void ThrottledTextureUploader::Query::wait()
{
    value();
    return;
}

unsigned ThrottledTextureUploader::Query::value()
{
    if (!m_hasValue) {
        m_context->getQueryObjectuivEXT(m_queryId, Extensions3DChromium::QUERY_RESULT_EXT, &m_value);
        m_hasValue = true;
    }
    return m_value;
}

void ThrottledTextureUploader::Query::markAsNonBlocking()
{
    m_isNonBlocking = true;
}

bool ThrottledTextureUploader::Query::isNonBlocking()
{
    return m_isNonBlocking;
}

ThrottledTextureUploader::ThrottledTextureUploader(WebKit::WebGraphicsContext3D* context)
    : m_context(context)
    , m_texturesPerSecondHistory(uploadHistorySize, estimatedTexturesPerSecondGlobal)
    , m_numBlockingTextureUploads(0)
{
}

ThrottledTextureUploader::~ThrottledTextureUploader()
{
}

size_t ThrottledTextureUploader::numBlockingUploads()
{
    processQueries();
    return m_numBlockingTextureUploads;
}

void ThrottledTextureUploader::markPendingUploadsAsNonBlocking()
{
    for (Deque<OwnPtr<Query> >::iterator it = m_pendingQueries.begin();
         it != m_pendingQueries.end(); ++it) {
        if (it->get()->isNonBlocking())
            continue;

        m_numBlockingTextureUploads--;
        it->get()->markAsNonBlocking();
    }

    ASSERT(!m_numBlockingTextureUploads);
}

double ThrottledTextureUploader::estimatedTexturesPerSecond()
{
    processQueries();

    // The history should never be empty because we initialize all elements with an estimate.
    ASSERT(m_texturesPerSecondHistory.size() == uploadHistorySize);

    // Sort the history and use the median as our estimate.
    std::vector<double> sortedHistory(m_texturesPerSecondHistory.begin(),
                                      m_texturesPerSecondHistory.end());
    std::sort(sortedHistory.begin(), sortedHistory.end());

    estimatedTexturesPerSecondGlobal = sortedHistory[sortedHistory.size() * 2 / 3];
    TRACE_COUNTER1("cc", "estimatedTexturesPerSecond", estimatedTexturesPerSecondGlobal);
    return estimatedTexturesPerSecondGlobal;
}

void ThrottledTextureUploader::beginQuery()
{
    if (m_availableQueries.isEmpty())
      m_availableQueries.append(Query::create(m_context));

    m_availableQueries.first()->begin();
}

void ThrottledTextureUploader::endQuery()
{
    m_availableQueries.first()->end();
    m_pendingQueries.append(m_availableQueries.takeFirst());
    m_numBlockingTextureUploads++;
}

void ThrottledTextureUploader::uploadTexture(CCResourceProvider* resourceProvider, Parameters upload)
{
    bool isFullUpload = upload.geometry.destOffset.isZero() &&
                        upload.geometry.sourceRect.size() == upload.texture->size();

    if (isFullUpload)
        beginQuery();

    if (upload.bitmap) {
        upload.bitmap->lockPixels();
        upload.texture->upload(
            resourceProvider,
            static_cast<const uint8_t*>(upload.bitmap->getPixels()),
            upload.geometry.contentRect,
            upload.geometry.sourceRect,
            upload.geometry.destOffset);
        upload.bitmap->unlockPixels();
    }

    // TODO(reveman): Move this logic to CCTextureUpdateController after
    // removing Parameters struct.
    if (upload.picture) {
        CCPrioritizedTexture* texture = upload.texture;
        IntRect pictureRect = upload.geometry.contentRect;
        IntRect sourceRect = upload.geometry.sourceRect;
        IntSize destOffset = upload.geometry.destOffset;

        texture->acquireBackingTexture(resourceProvider);
        ASSERT(texture->haveBackingTexture());

        ASSERT(resourceProvider->resourceType(texture->resourceId()) ==
               CCResourceProvider::GLTexture);

        WebGraphicsContext3D* paintContext = CCProxy::hasImplThread() ?
            WebSharedGraphicsContext3D::compositorThreadContext() :
            WebSharedGraphicsContext3D::mainThreadContext();
        GrContext* paintGrContext = CCProxy::hasImplThread() ?
            WebSharedGraphicsContext3D::compositorThreadGrContext() :
            WebSharedGraphicsContext3D::mainThreadGrContext();

        // Flush the context in which the backing texture is created so that it
        // is available in other shared contexts. It is important to do here
        // because the backing texture is created in one context while it is
        // being written to in another.
        resourceProvider->flush();
        CCResourceProvider::ScopedWriteLockGL lock(
            resourceProvider, texture->resourceId());

        // Make sure ganesh uses the correct GL context.
        paintContext->makeContextCurrent();

        // Create an accelerated canvas to draw on.
        OwnPtr<SkCanvas> canvas = createAcceleratedCanvas(
            paintGrContext, texture->size(), lock.textureId());

        // The compositor expects the textures to be upside-down so it can flip
        // the final composited image. Ganesh renders the image upright so we
        // need to do a y-flip.
        canvas->translate(0.0, texture->size().height());
        canvas->scale(1.0, -1.0);
        // Clip to the destination on the texture that must be updated.
        canvas->clipRect(SkRect::MakeXYWH(destOffset.width(),
                                          destOffset.height(),
                                          sourceRect.width(),
                                          sourceRect.height()));
        // Translate the origin of pictureRect to destOffset.
        // Note that destOffset is defined relative to sourceRect.
        canvas->translate(
            pictureRect.x() - sourceRect.x() + destOffset.width(),
            pictureRect.y() - sourceRect.y() + destOffset.height());
        canvas->drawPicture(*upload.picture);

        // Flush ganesh context so that all the rendered stuff appears on the
        // texture.
        paintGrContext->flush();

        // Flush the GL context so rendering results from this context are
        // visible in the compositor's context.
        paintContext->flush();
    }

    if (isFullUpload)
        endQuery();
}

void ThrottledTextureUploader::processQueries()
{
    while (!m_pendingQueries.isEmpty()) {
        if (m_pendingQueries.first()->isPending())
            break;

        unsigned usElapsed = m_pendingQueries.first()->value();
        WebKit::Platform::current()->histogramCustomCounts("Renderer4.TextureGpuUploadTimeUS", usElapsed, 0, 100000, 50);

        if (!m_pendingQueries.first()->isNonBlocking())
            m_numBlockingTextureUploads--;

        // Remove the oldest values from our history and insert the new one
        double texturesPerSecond = 1.0 / (usElapsed * 1e-6);
        m_texturesPerSecondHistory.pop_back();
        m_texturesPerSecondHistory.push_front(texturesPerSecond);

        m_availableQueries.append(m_pendingQueries.takeFirst());
    }
}

}
