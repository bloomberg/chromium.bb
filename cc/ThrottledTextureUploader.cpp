// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "ThrottledTextureUploader.h"

#include "Extensions3DChromium.h"
#include <public/WebGraphicsContext3D.h>

namespace {

// Number of pending texture update queries to allow.
static const size_t maxPendingQueries = 2;

} // anonymous namespace

namespace cc {

ThrottledTextureUploader::Query::Query(WebKit::WebGraphicsContext3D* context)
    : m_context(context)
    , m_queryId(0)
{
    m_queryId = m_context->createQueryEXT();
}

ThrottledTextureUploader::Query::~Query()
{
    m_context->deleteQueryEXT(m_queryId);
}

void ThrottledTextureUploader::Query::begin()
{
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
    unsigned result;
    m_context->getQueryObjectuivEXT(m_queryId, Extensions3DChromium::QUERY_RESULT_EXT, &result);
}

ThrottledTextureUploader::ThrottledTextureUploader(WebKit::WebGraphicsContext3D* context)
    : m_context(context)
    , m_maxPendingQueries(maxPendingQueries)
{
}

ThrottledTextureUploader::ThrottledTextureUploader(WebKit::WebGraphicsContext3D* context, size_t pendingUploadLimit)
    : m_context(context)
    , m_maxPendingQueries(pendingUploadLimit)
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

void ThrottledTextureUploader::beginUploads()
{
    // Wait for query to become available.
    while (isBusy())
        m_pendingQueries.first()->wait();

    ASSERT(!m_availableQueries.isEmpty());
    m_availableQueries.first()->begin();
}

void ThrottledTextureUploader::endUploads()
{
    m_availableQueries.first()->end();
    m_pendingQueries.append(m_availableQueries.takeFirst());
}

void ThrottledTextureUploader::uploadTexture(CCResourceProvider* resourceProvider, Parameters upload)
{
    upload.texture->updateRect(resourceProvider, upload.sourceRect, upload.destOffset);
}

void ThrottledTextureUploader::processQueries()
{
    while (!m_pendingQueries.isEmpty()) {
        if (m_pendingQueries.first()->isPending())
            break;

        m_availableQueries.append(m_pendingQueries.takeFirst());
    }
}

}
