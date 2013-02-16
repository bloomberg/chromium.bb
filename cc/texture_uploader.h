// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEXTURE_UPLOADER_H_
#define CC_TEXTURE_UPLOADER_H_

#include <set>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"
#include "cc/scoped_ptr_deque.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace gfx {
class Rect;
class Size;
class Vector2d;
}

namespace cc {

class CC_EXPORT TextureUploader {
public:
    static scoped_ptr<TextureUploader> create(
        WebKit::WebGraphicsContext3D* context,
        bool useMapTexSubImage,
        bool useShallowFlush)
    {
        return make_scoped_ptr(
            new TextureUploader(context, useMapTexSubImage, useShallowFlush));
    }
    ~TextureUploader();

    size_t numBlockingUploads();
    void markPendingUploadsAsNonBlocking();
    double estimatedTexturesPerSecond();

    // Let imageRect be a rectangle, and let sourceRect be a sub-rectangle of
    // imageRect, expressed in the same coordinate system as imageRect. Let 
    // image be a buffer for imageRect. This function will copy the region
    // corresponding to sourceRect to destOffset in this sub-image.
    void upload(const uint8* image,
                const gfx::Rect& content_rect,
                const gfx::Rect& source_rect,
                const gfx::Vector2d& dest_offset,
                GLenum format,
                const gfx::Size& size);

    void flush();
    void releaseCachedQueries();

private:
    class Query {
    public:
        static scoped_ptr<Query> create(WebKit::WebGraphicsContext3D* context) { return make_scoped_ptr(new Query(context)); }

        virtual ~Query();

        void begin();
        void end();
        bool isPending();
        unsigned value();
        size_t texturesUploaded();
        void markAsNonBlocking();
        bool isNonBlocking();

    private:
        explicit Query(WebKit::WebGraphicsContext3D*);

        WebKit::WebGraphicsContext3D* m_context;
        unsigned m_queryId;
        unsigned m_value;
        bool m_hasValue;
        bool m_isNonBlocking;
    };

    TextureUploader(WebKit::WebGraphicsContext3D*,
                    bool useMapTexSubImage,
                    bool useShallowFlush);

    void beginQuery();
    void endQuery();
    void processQueries();

    void uploadWithTexSubImage(const uint8* image,
                               const gfx::Rect& image_rect,
                               const gfx::Rect& source_rect,
                               const gfx::Vector2d& dest_offset,
                               GLenum format);
    void uploadWithMapTexSubImage(const uint8* image,
                                  const gfx::Rect& image_rect,
                                  const gfx::Rect& source_rect,
                                  const gfx::Vector2d& dest_offset,
                                  GLenum format);

    WebKit::WebGraphicsContext3D* m_context;
    ScopedPtrDeque<Query> m_pendingQueries;
    ScopedPtrDeque<Query> m_availableQueries;
    std::multiset<double> m_texturesPerSecondHistory;
    size_t m_numBlockingTextureUploads;

    bool m_useMapTexSubImage;
    size_t m_subImageSize;
    scoped_array<uint8> m_subImage;

    bool m_useShallowFlush;
    size_t m_numTextureUploadsSinceLastFlush;

    DISALLOW_COPY_AND_ASSIGN(TextureUploader);
};

}  // namespace cc

#endif  // CC_TEXTURE_UPLOADER_H_
