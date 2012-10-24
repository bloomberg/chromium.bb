// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEXTURE_UPLOADER_H_
#define CC_TEXTURE_UPLOADER_H_

#include "IntRect.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/scoped_ptr_deque.h"
#include <set>
#include "third_party/khronos/GLES2/gl2.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {

class TextureUploader {
public:
    static scoped_ptr<TextureUploader> create(
        WebKit::WebGraphicsContext3D* context, bool useMapTexSubImage)
    {
        return make_scoped_ptr(new TextureUploader(context, useMapTexSubImage));
    }
    ~TextureUploader();

    size_t numBlockingUploads();
    void markPendingUploadsAsNonBlocking();
    double estimatedTexturesPerSecond();

    // Let imageRect be a rectangle, and let sourceRect be a sub-rectangle of
    // imageRect, expressed in the same coordinate system as imageRect. Let 
    // image be a buffer for imageRect. This function will copy the region
    // corresponding to sourceRect to destOffset in this sub-image.
    void upload(const uint8_t* image,
                const IntRect& content_rect,
                const IntRect& source_rect,
                const IntSize& dest_offset,
                GLenum format,
                IntSize size);

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

    TextureUploader(WebKit::WebGraphicsContext3D*, bool useMapTexSubImage);

    void beginQuery();
    void endQuery();
    void processQueries();

    void uploadWithTexSubImage(const uint8_t* image,
                               const IntRect& image_rect,
                               const IntRect& source_rect,
                               const IntSize& dest_offset,
                               GLenum format);
    void uploadWithMapTexSubImage(const uint8_t* image,
                                  const IntRect& image_rect,
                                  const IntRect& source_rect,
                                  const IntSize& dest_offset,
                                  GLenum format);

    WebKit::WebGraphicsContext3D* m_context;
    ScopedPtrDeque<Query> m_pendingQueries;
    ScopedPtrDeque<Query> m_availableQueries;
    std::multiset<double> m_texturesPerSecondHistory;
    size_t m_numBlockingTextureUploads;

    bool m_useMapTexSubImage;
    size_t m_subImageSize;
    scoped_array<uint8_t> m_subImage;

    DISALLOW_COPY_AND_ASSIGN(TextureUploader);
};

}  // namespace cc

#endif  // CC_TEXTURE_UPLOADER_H_
