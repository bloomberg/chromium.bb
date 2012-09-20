// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "ThrottledTextureUploader.h"

#include "Extensions3DChromium.h"
#include <algorithm>
#include <public/WebGraphicsContext3D.h>
#include <vector>

namespace {

// Number of pending texture update queries to allow.
static const size_t maxPendingQueries = 2;

// How many previous uploads to use when predicting future throughput.
static const size_t uploadHistorySize = 10;

// Global estimated number of textures per second to maintain estimates across
// subsequent instances of ThrottledTextureUploader.
// More than one thread will not access this variable, so we do not need to synchronize access.
static double estimatedTexturesPerSecondGlobal = 48.0 * 60.0;

} // anonymous namespace

namespace cc {

ThrottledTextureUploader::Query::Query(WebKit::WebGraphicsContext3D* context)
    : m_context(context)
    , m_queryId(0)
    , m_value(0)
    , m_hasValue(false)
    , m_texturesUploaded(0)
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
    m_context->beginQueryEXT(Extensions3DChromium::COMMANDS_ISSUED_CHROMIUM, m_queryId);
}

void ThrottledTextureUploader::Query::end(double texturesUploaded)
{
    m_context->endQueryEXT(Extensions3DChromium::COMMANDS_ISSUED_CHROMIUM);
    m_texturesUploaded = texturesUploaded;
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

double ThrottledTextureUploader::Query::texturesUploaded()
{
    return m_texturesUploaded;
}

ThrottledTextureUploader::ThrottledTextureUploader(WebKit::WebGraphicsContext3D* context)
    : m_context(context)
    , m_maxPendingQueries(maxPendingQueries)
    , m_texturesPerSecondHistory(uploadHistorySize, estimatedTexturesPerSecondGlobal)
    , m_texturesUploaded(0)
{
}

ThrottledTextureUploader::ThrottledTextureUploader(WebKit::WebGraphicsContext3D* context, size_t pendingUploadLimit)
    : m_context(context)
    , m_maxPendingQueries(pendingUploadLimit)
    , m_texturesPerSecondHistory(uploadHistorySize, estimatedTexturesPerSecondGlobal)
    , m_texturesUploaded(0)
{
    ASSERT(m_context);
}

ThrottledTextureUploader::~ThrottledTextureUploader()
{
}

bool ThrottledTextureUploader::isBusy()
{
    processQueries();

    if (!m_availableQueries.isEmpty())
        return false;

    if (m_pendingQueries.size() == m_maxPendingQueries)
        return true;

    m_availableQueries.append(Query::create(m_context));
    return false;
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

    estimatedTexturesPerSecondGlobal = sortedHistory[sortedHistory.size() / 2];
    return estimatedTexturesPerSecondGlobal;
}

void ThrottledTextureUploader::beginUploads()
{
    m_texturesUploaded = 0;

    // Wait for query to become available.
    while (isBusy())
        m_pendingQueries.first()->wait();

    ASSERT(!m_availableQueries.isEmpty());
    m_availableQueries.first()->begin();
}

void ThrottledTextureUploader::endUploads()
{
    m_availableQueries.first()->end(m_texturesUploaded);
    m_pendingQueries.append(m_availableQueries.takeFirst());
}

void ThrottledTextureUploader::uploadTexture(CCResourceProvider* resourceProvider, Parameters upload)
{
    m_texturesUploaded++;
    upload.texture->updateRect(resourceProvider, upload.sourceRect, upload.destOffset);
}

void ThrottledTextureUploader::processQueries()
{
    while (!m_pendingQueries.isEmpty()) {
        if (m_pendingQueries.first()->isPending())
            break;

        unsigned usElapsed = m_pendingQueries.first()->value();
        double texturesPerSecond = m_pendingQueries.first()->texturesUploaded() / (usElapsed * 1e-6);

        // Remove the oldest values from our history and insert the new one
        m_texturesPerSecondHistory.pop_back();
        m_texturesPerSecondHistory.push_front(texturesPerSecond);

        m_availableQueries.append(m_pendingQueries.takeFirst());
    }
}

}
