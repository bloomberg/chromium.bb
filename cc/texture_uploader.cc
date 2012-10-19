// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "cc/texture_uploader.h"

#include "CCPrioritizedTexture.h"
#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include <algorithm>
#include <public/WebGraphicsContext3D.h>
#include <vector>

namespace {

// How many previous uploads to use when predicting future throughput.
static const size_t uploadHistorySize = 100;

// Global estimated number of textures per second to maintain estimates across
// subsequent instances of TextureUploader.
// More than one thread will not access this variable, so we do not need to synchronize access.
static double estimatedTexturesPerSecondGlobal = 48.0 * 60.0;

} // anonymous namespace

namespace cc {

TextureUploader::Query::Query(WebKit::WebGraphicsContext3D* context)
    : m_context(context)
    , m_queryId(0)
    , m_value(0)
    , m_hasValue(false)
    , m_isNonBlocking(false)
{
    m_queryId = m_context->createQueryEXT();
}

TextureUploader::Query::~Query()
{
    m_context->deleteQueryEXT(m_queryId);
}

void TextureUploader::Query::begin()
{
    m_hasValue = false;
    m_isNonBlocking = false;
    m_context->beginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, m_queryId);
}

void TextureUploader::Query::end()
{
    m_context->endQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
}

bool TextureUploader::Query::isPending()
{
    unsigned available = 1;
    m_context->getQueryObjectuivEXT(m_queryId, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
    return !available;
}

unsigned TextureUploader::Query::value()
{
    if (!m_hasValue) {
        m_context->getQueryObjectuivEXT(m_queryId, GL_QUERY_RESULT_EXT, &m_value);
        m_hasValue = true;
    }
    return m_value;
}

void TextureUploader::Query::markAsNonBlocking()
{
    m_isNonBlocking = true;
}

bool TextureUploader::Query::isNonBlocking()
{
    return m_isNonBlocking;
}

TextureUploader::TextureUploader(
    WebKit::WebGraphicsContext3D* context, bool useMapTexSubImage)
    : m_context(context)
    , m_texturesPerSecondHistory(uploadHistorySize,
                                 estimatedTexturesPerSecondGlobal)
    , m_numBlockingTextureUploads(0)
    , m_useMapTexSubImage(useMapTexSubImage)
    , m_subImageSize(0)
{
}

TextureUploader::~TextureUploader()
{
}

size_t TextureUploader::numBlockingUploads()
{
    processQueries();
    return m_numBlockingTextureUploads;
}

void TextureUploader::markPendingUploadsAsNonBlocking()
{
    for (ScopedPtrDeque<Query>::iterator it = m_pendingQueries.begin();
         it != m_pendingQueries.end(); ++it) {
        if ((*it)->isNonBlocking())
            continue;

        m_numBlockingTextureUploads--;
        (*it)->markAsNonBlocking();
    }

    DCHECK(!m_numBlockingTextureUploads);
}

double TextureUploader::estimatedTexturesPerSecond()
{
    processQueries();

    // The history should never be empty because we initialize all elements with an estimate.
    DCHECK(m_texturesPerSecondHistory.size() == uploadHistorySize);

    // Sort the history and use the median as our estimate.
    std::vector<double> sortedHistory(m_texturesPerSecondHistory.begin(),
                                      m_texturesPerSecondHistory.end());
    std::sort(sortedHistory.begin(), sortedHistory.end());

    estimatedTexturesPerSecondGlobal = sortedHistory[sortedHistory.size() * 2 / 3];
    TRACE_COUNTER1("cc", "estimatedTexturesPerSecond", estimatedTexturesPerSecondGlobal);
    return estimatedTexturesPerSecondGlobal;
}

void TextureUploader::beginQuery()
{
    if (m_availableQueries.isEmpty())
      m_availableQueries.append(Query::create(m_context));

    m_availableQueries.first()->begin();
}

void TextureUploader::endQuery()
{
    m_availableQueries.first()->end();
    m_pendingQueries.append(m_availableQueries.takeFirst());
    m_numBlockingTextureUploads++;
}

void TextureUploader::upload(const uint8_t* image,
                             const IntRect& image_rect,
                             const IntRect& source_rect,
                             const IntSize& dest_offset,
                             GLenum format,
                             IntSize size)
{
    bool isFullUpload = dest_offset.isZero() && source_rect.size() == size;

    if (isFullUpload)
        beginQuery();

    if (m_useMapTexSubImage) {
        uploadWithMapTexSubImage(
            image, image_rect, source_rect, dest_offset, format);
    } else {
        uploadWithTexSubImage(
            image, image_rect, source_rect, dest_offset, format);
    }

    if (isFullUpload)
        endQuery();
}

void TextureUploader::uploadWithTexSubImage(const uint8_t* image,
                                            const IntRect& image_rect,
                                            const IntRect& source_rect,
                                            const IntSize& dest_offset,
                                            GLenum format)
{
    TRACE_EVENT0("cc", "TextureUploader::uploadWithTexSubImage");

    // Offset from image-rect to source-rect.
    IntPoint offset(source_rect.x() - image_rect.x(),
                    source_rect.y() - image_rect.y());

    const uint8_t* pixel_source;
    if (image_rect.width() == source_rect.width() && !offset.x()) {
        pixel_source = &image[4 * offset.y() * image_rect.width()];
    } else {
        size_t needed_size = source_rect.width() * source_rect.height() * 4;
        if (m_subImageSize < needed_size) {
            m_subImage.reset(new uint8_t[needed_size]);
            m_subImageSize = needed_size;
        }
        // Strides not equal, so do a row-by-row memcpy from the
        // paint results into a temp buffer for uploading.
        for (int row = 0; row < source_rect.height(); ++row)
            memcpy(&m_subImage[source_rect.width() * 4 * row],
                   &image[4 * (offset.x() +
                               (offset.y() + row) * image_rect.width())],
                   source_rect.width() * 4);

        pixel_source = &m_subImage[0];
    }

    m_context->texSubImage2D(GL_TEXTURE_2D,
                             0,
                             dest_offset.width(),
                             dest_offset.height(),
                             source_rect.width(),
                             source_rect.height(),
                             format,
                             GL_UNSIGNED_BYTE,
                             pixel_source);
}

void TextureUploader::uploadWithMapTexSubImage(const uint8_t* image,
                                               const IntRect& image_rect,
                                               const IntRect& source_rect,
                                               const IntSize& dest_offset,
                                               GLenum format)
{
    TRACE_EVENT0("cc", "TextureUploader::uploadWithMapTexSubImage");

    // Offset from image-rect to source-rect.
    IntPoint offset(source_rect.x() - image_rect.x(),
                    source_rect.y() - image_rect.y());

    // Upload tile data via a mapped transfer buffer
    uint8_t* pixel_dest = static_cast<uint8_t*>(
        m_context->mapTexSubImage2DCHROMIUM(GL_TEXTURE_2D,
                                            0,
                                            dest_offset.width(),
                                            dest_offset.height(),
                                            source_rect.width(),
                                            source_rect.height(),
                                            format,
                                            GL_UNSIGNED_BYTE,
                                            GL_WRITE_ONLY));

    if (!pixel_dest) {
        uploadWithTexSubImage(
            image, image_rect, source_rect, dest_offset, format);
        return;
    }

    unsigned int components_per_pixel = 0;
    switch (format) {
    case GL_RGBA:
    case GL_BGRA_EXT:
        components_per_pixel = 4;
        break;
    case GL_LUMINANCE:
        components_per_pixel = 1;
        break;
    default:
        NOTREACHED();
    }
    unsigned int bytes_per_component = 1;
    unsigned int bytes_per_pixel = components_per_pixel * bytes_per_component;

    if (image_rect.width() == source_rect.width() && !offset.x()) {
        memcpy(pixel_dest,
               &image[offset.y() * image_rect.width() * bytes_per_pixel],
               image_rect.width() * source_rect.height() * bytes_per_pixel);
    } else {
        // Strides not equal, so do a row-by-row memcpy from the
        // paint results into the pixelDest
        for (int row = 0; row < source_rect.height(); ++row)
            memcpy(&pixel_dest[source_rect.width() * row * bytes_per_pixel],
                   &image[4 * (offset.x() +
                               (offset.y() + row) * image_rect.width())],
                   source_rect.width() * bytes_per_pixel);
    }

    m_context->unmapTexSubImage2DCHROMIUM(pixel_dest);
}

void TextureUploader::processQueries()
{
    while (!m_pendingQueries.isEmpty()) {
        if (m_pendingQueries.first()->isPending())
            break;

        unsigned usElapsed = m_pendingQueries.first()->value();
        HISTOGRAM_CUSTOM_COUNTS("Renderer4.TextureGpuUploadTimeUS", usElapsed, 0, 100000, 50);

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
