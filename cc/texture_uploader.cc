// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/texture_uploader.h"

#include <algorithm>
#include <vector>

#include "base/debug/alias.h"
#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "cc/prioritized_resource.h"
#include "cc/resource.h"
#include "cc/util.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d.h"

namespace {

// How many previous uploads to use when predicting future throughput.
static const size_t uploadHistorySizeMax = 1000;
static const size_t uploadHistorySizeInitial = 100;

// Global estimated number of textures per second to maintain estimates across
// subsequent instances of TextureUploader.
// More than one thread will not access this variable, so we do not need to synchronize access.
static const double defaultEstimatedTexturesPerSecond = 48.0 * 60.0;

// Flush interval when performing texture uploads.
const int textureUploadFlushPeriod = 4;

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
    WebKit::WebGraphicsContext3D* context,
    bool useMapTexSubImage,
    bool useShallowFlush)
    : m_context(context)
    , m_numBlockingTextureUploads(0)
    , m_useMapTexSubImage(useMapTexSubImage)
    , m_subImageSize(0)
    , m_useShallowFlush(useShallowFlush)
    , m_numTextureUploadsSinceLastFlush(0)
{
    for (size_t i = uploadHistorySizeInitial; i > 0; i--)
        m_texturesPerSecondHistory.insert(defaultEstimatedTexturesPerSecond);
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

    // Use the median as our estimate.
    std::multiset<double>::iterator median = m_texturesPerSecondHistory.begin();
    std::advance(median, m_texturesPerSecondHistory.size() / 2);
    TRACE_COUNTER_ID1("cc", "EstimatedTexturesPerSecond", m_context, *median);
    return *median;
}

void TextureUploader::beginQuery()
{
    if (m_availableQueries.empty())
      m_availableQueries.push_back(Query::create(m_context));

    m_availableQueries.front()->begin();
}

void TextureUploader::endQuery()
{
    m_availableQueries.front()->end();
    m_pendingQueries.push_back(m_availableQueries.take_front());
    m_numBlockingTextureUploads++;
}

void TextureUploader::upload(const uint8* image,
                             const gfx::Rect& image_rect,
                             const gfx::Rect& source_rect,
                             const gfx::Vector2d& dest_offset,
                             GLenum format,
                             const gfx::Size& size)
{
    CHECK(image_rect.Contains(source_rect));

    bool isFullUpload = dest_offset.IsZero() && source_rect.size() == size;

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

    m_numTextureUploadsSinceLastFlush++;
    if (m_numTextureUploadsSinceLastFlush >= textureUploadFlushPeriod)
      flush();
}

void TextureUploader::flush() {
  if (!m_numTextureUploadsSinceLastFlush)
    return;

  if (m_useShallowFlush)
    m_context->shallowFlushCHROMIUM();

  m_numTextureUploadsSinceLastFlush = 0;
}

void TextureUploader::uploadWithTexSubImage(const uint8* image,
                                            const gfx::Rect& image_rect,
                                            const gfx::Rect& source_rect,
                                            const gfx::Vector2d& dest_offset,
                                            GLenum format)
{
    // Instrumentation to debug issue 156107
    int source_rect_x = source_rect.x();
    int source_rect_y = source_rect.y();
    int source_rect_width = source_rect.width();
    int source_rect_height = source_rect.height();
    int image_rect_x = image_rect.x();
    int image_rect_y = image_rect.y();
    int image_rect_width = image_rect.width();
    int image_rect_height = image_rect.height();
    int dest_offset_x = dest_offset.x();
    int dest_offset_y = dest_offset.y();
    base::debug::Alias(&image);
    base::debug::Alias(&source_rect_x);
    base::debug::Alias(&source_rect_y);
    base::debug::Alias(&source_rect_width);
    base::debug::Alias(&source_rect_height);
    base::debug::Alias(&image_rect_x);
    base::debug::Alias(&image_rect_y);
    base::debug::Alias(&image_rect_width);
    base::debug::Alias(&image_rect_height);
    base::debug::Alias(&dest_offset_x);
    base::debug::Alias(&dest_offset_y);
    TRACE_EVENT0("cc", "TextureUploader::uploadWithTexSubImage");

    // Offset from image-rect to source-rect.
    gfx::Vector2d offset(source_rect.origin() - image_rect.origin());

    const uint8* pixel_source;
    unsigned int bytes_per_pixel = Resource::BytesPerPixel(format);
    // Use 4-byte row alignment (OpenGL default) for upload performance.
    // Assuming that GL_UNPACK_ALIGNMENT has not changed from default.
    unsigned int upload_image_stride =
        RoundUp(bytes_per_pixel * source_rect.width(), 4u);

    if (upload_image_stride == image_rect.width() * bytes_per_pixel && !offset.x()) {
        pixel_source = &image[image_rect.width() * bytes_per_pixel * offset.y()];
    } else {
        size_t needed_size = upload_image_stride * source_rect.height();
        if (m_subImageSize < needed_size) {
            m_subImage.reset(new uint8[needed_size]);
            m_subImageSize = needed_size;
        }
        // Strides not equal, so do a row-by-row memcpy from the
        // paint results into a temp buffer for uploading.
        for (int row = 0; row < source_rect.height(); ++row)
            memcpy(&m_subImage[upload_image_stride * row],
                   &image[bytes_per_pixel * (offset.x() +
                          (offset.y() + row) * image_rect.width())],
                   source_rect.width() * bytes_per_pixel);

        pixel_source = &m_subImage[0];
    }

    m_context->texSubImage2D(GL_TEXTURE_2D,
                             0,
                             dest_offset.x(),
                             dest_offset.y(),
                             source_rect.width(),
                             source_rect.height(),
                             format,
                             GL_UNSIGNED_BYTE,
                             pixel_source);
}

void TextureUploader::uploadWithMapTexSubImage(const uint8* image,
                                               const gfx::Rect& image_rect,
                                               const gfx::Rect& source_rect,
                                               const gfx::Vector2d& dest_offset,
                                               GLenum format)
{
    // Instrumentation to debug issue 156107
    int source_rect_x = source_rect.x();
    int source_rect_y = source_rect.y();
    int source_rect_width = source_rect.width();
    int source_rect_height = source_rect.height();
    int image_rect_x = image_rect.x();
    int image_rect_y = image_rect.y();
    int image_rect_width = image_rect.width();
    int image_rect_height = image_rect.height();
    int dest_offset_x = dest_offset.x();
    int dest_offset_y = dest_offset.y();
    base::debug::Alias(&image);
    base::debug::Alias(&source_rect_x);
    base::debug::Alias(&source_rect_y);
    base::debug::Alias(&source_rect_width);
    base::debug::Alias(&source_rect_height);
    base::debug::Alias(&image_rect_x);
    base::debug::Alias(&image_rect_y);
    base::debug::Alias(&image_rect_width);
    base::debug::Alias(&image_rect_height);
    base::debug::Alias(&dest_offset_x);
    base::debug::Alias(&dest_offset_y);

    TRACE_EVENT0("cc", "TextureUploader::uploadWithMapTexSubImage");

    // Offset from image-rect to source-rect.
    gfx::Vector2d offset(source_rect.origin() - image_rect.origin());

    unsigned int bytes_per_pixel = Resource::BytesPerPixel(format);
    // Use 4-byte row alignment (OpenGL default) for upload performance.
    // Assuming that GL_UNPACK_ALIGNMENT has not changed from default.
    unsigned int upload_image_stride =
        RoundUp(bytes_per_pixel * source_rect.width(), 4u);

    // Upload tile data via a mapped transfer buffer
    uint8* pixel_dest = static_cast<uint8*>(
        m_context->mapTexSubImage2DCHROMIUM(GL_TEXTURE_2D,
                                            0,
                                            dest_offset.x(),
                                            dest_offset.y(),
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

    if (upload_image_stride == image_rect.width() * bytes_per_pixel && !offset.x()) {
        memcpy(pixel_dest,
               &image[image_rect.width() * bytes_per_pixel * offset.y()],
               source_rect.height() * image_rect.width() * bytes_per_pixel);
    } else {
        // Strides not equal, so do a row-by-row memcpy from the
        // paint results into the pixelDest
        for (int row = 0; row < source_rect.height(); ++row)
            memcpy(&pixel_dest[upload_image_stride * row],
                   &image[bytes_per_pixel * (offset.x() +
                          (offset.y() + row) * image_rect.width())],
                   source_rect.width() * bytes_per_pixel);
    }

    m_context->unmapTexSubImage2DCHROMIUM(pixel_dest);
}

void TextureUploader::processQueries()
{
    while (!m_pendingQueries.empty()) {
        if (m_pendingQueries.front()->isPending())
            break;

        unsigned usElapsed = m_pendingQueries.front()->value();
        UMA_HISTOGRAM_CUSTOM_COUNTS("Renderer4.TextureGpuUploadTimeUS",
                                    usElapsed, 0, 100000, 50);

        // Clamp the queries to saner values in case the queries fail.
        usElapsed = std::max(1u, usElapsed);
        usElapsed = std::min(15000u, usElapsed);

        if (!m_pendingQueries.front()->isNonBlocking())
            m_numBlockingTextureUploads--;

        // Remove the min and max value from our history and insert the new one.
        double texturesPerSecond = 1.0 / (usElapsed * 1e-6);
        if (m_texturesPerSecondHistory.size() >= uploadHistorySizeMax) {
            m_texturesPerSecondHistory.erase(m_texturesPerSecondHistory.begin());
            m_texturesPerSecondHistory.erase(--m_texturesPerSecondHistory.end());
        }
        m_texturesPerSecondHistory.insert(texturesPerSecond);

        m_availableQueries.push_back(m_pendingQueries.take_front());
    }
}

}  // namespace cc
