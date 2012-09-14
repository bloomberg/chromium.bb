// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThrottledTextureUploader_h
#define ThrottledTextureUploader_h

#include "TextureUploader.h"

#include <wtf/Deque.h>

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {

class ThrottledTextureUploader : public TextureUploader {
    WTF_MAKE_NONCOPYABLE(ThrottledTextureUploader);
public:
    static PassOwnPtr<ThrottledTextureUploader> create(WebKit::WebGraphicsContext3D* context)
    {
        return adoptPtr(new ThrottledTextureUploader(context));
    }
    static PassOwnPtr<ThrottledTextureUploader> create(WebKit::WebGraphicsContext3D* context, size_t pendingUploadLimit)
    {
        return adoptPtr(new ThrottledTextureUploader(context, pendingUploadLimit));
    }
    virtual ~ThrottledTextureUploader();

    virtual bool isBusy() OVERRIDE;
    virtual void beginUploads() OVERRIDE;
    virtual void endUploads() OVERRIDE;
    virtual void uploadTexture(CCResourceProvider*, Parameters) OVERRIDE;

private:
    class Query {
    public:
        static PassOwnPtr<Query> create(WebKit::WebGraphicsContext3D* context) { return adoptPtr(new Query(context)); }

        virtual ~Query();

        void begin();
        void end();
        bool isPending();
        void wait();

    private:
        explicit Query(WebKit::WebGraphicsContext3D*);

        WebKit::WebGraphicsContext3D* m_context;
        unsigned m_queryId;
    };

    ThrottledTextureUploader(WebKit::WebGraphicsContext3D*);
    ThrottledTextureUploader(WebKit::WebGraphicsContext3D*, size_t pendingUploadLimit);

    void processQueries();

    WebKit::WebGraphicsContext3D* m_context;
    size_t m_maxPendingQueries;
    Deque<OwnPtr<Query> > m_pendingQueries;
    Deque<OwnPtr<Query> > m_availableQueries;
};

}

#endif
