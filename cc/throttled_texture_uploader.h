// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThrottledTextureUploader_h
#define ThrottledTextureUploader_h

#include "cc/texture_uploader.h"

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/scoped_ptr_deque.h"
#include <deque>

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {

class ThrottledTextureUploader : public TextureUploader {
public:
    static scoped_ptr<ThrottledTextureUploader> create(WebKit::WebGraphicsContext3D* context)
    {
        return make_scoped_ptr(new ThrottledTextureUploader(context));
    }
    virtual ~ThrottledTextureUploader();

    virtual size_t numBlockingUploads() OVERRIDE;
    virtual void markPendingUploadsAsNonBlocking() OVERRIDE;
    virtual double estimatedTexturesPerSecond() OVERRIDE;
    virtual void uploadTexture(CCResourceProvider*,
                               CCPrioritizedTexture*,
                               const SkBitmap*,
                               IntRect content_rect,
                               IntRect source_rect,
                               IntSize dest_offset) OVERRIDE;

private:
    class Query {
    public:
        static scoped_ptr<Query> create(WebKit::WebGraphicsContext3D* context) { return make_scoped_ptr(new Query(context)); }

        virtual ~Query();

        void begin();
        void end();
        bool isPending();
        void wait();
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

    ThrottledTextureUploader(WebKit::WebGraphicsContext3D*);

    void beginQuery();
    void endQuery();
    void processQueries();

    WebKit::WebGraphicsContext3D* m_context;
    ScopedPtrDeque<Query> m_pendingQueries;
    ScopedPtrDeque<Query> m_availableQueries;
    std::deque<double> m_texturesPerSecondHistory;
    size_t m_numBlockingTextureUploads;

    DISALLOW_COPY_AND_ASSIGN(ThrottledTextureUploader);
};

}

#endif
