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

void ThrottledTextureUploader::Query::end(size_t texturesUploaded)
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

size_t ThrottledTextureUploader::Query::texturesUploaded()
{
    return m_texturesUploaded;
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
    , m_texturesUploaded(0)
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

        m_numBlockingTextureUploads -= it->get()->texturesUploaded();
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

    estimatedTexturesPerSecondGlobal = sortedHistory[sortedHistory.size() / 2];
    return estimatedTexturesPerSecondGlobal;
}

void ThrottledTextureUploader::beginUploads()
{
    m_texturesUploaded = 0;

    processQueries();

    if (m_availableQueries.isEmpty())
      m_availableQueries.append(Query::create(m_context));

    m_availableQueries.first()->begin();
}

void ThrottledTextureUploader::endUploads()
{
    m_availableQueries.first()->end(m_texturesUploaded);
    m_pendingQueries.append(m_availableQueries.takeFirst());
    m_numBlockingTextureUploads += m_texturesUploaded;
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
        size_t texturesUploaded = m_pendingQueries.first()->texturesUploaded();
        double texturesPerSecond = static_cast<double>(texturesUploaded) /
            (usElapsed * 1e-6);

        if (!m_pendingQueries.first()->isNonBlocking())
            m_numBlockingTextureUploads -= texturesUploaded;

        // Remove the oldest values from our history and insert the new one
        m_texturesPerSecondHistory.pop_back();
        m_texturesPerSecondHistory.push_front(texturesPerSecond);

        m_availableQueries.append(m_pendingQueries.takeFirst());
    }
}

}
