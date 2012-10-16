// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "ThrottledTextureUploader.h"

#include "CCPrioritizedTexture.h"
#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "cc/test/fake_web_graphics_context_3d.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <wtf/RefPtr.h>

using namespace cc;
using namespace WebKit;

namespace {

class FakeWebGraphicsContext3DWithQueryTesting : public FakeWebGraphicsContext3D {
public:
    FakeWebGraphicsContext3DWithQueryTesting() : m_resultAvailable(0)
    {
    }

    virtual void getQueryObjectuivEXT(WebGLId, GC3Denum type, GC3Duint* value)
    {
        switch (type) {
        case Extensions3DChromium::QUERY_RESULT_AVAILABLE_EXT:
            *value = m_resultAvailable;
            break;
        default:
            *value = 0;
            break;
        }
    }

    void setResultAvailable(unsigned resultAvailable) { m_resultAvailable = resultAvailable; }

private:
    unsigned m_resultAvailable;
};


TEST(ThrottledTextureUploaderTest, NumBlockingUploads)
{
    OwnPtr<FakeWebGraphicsContext3DWithQueryTesting> fakeContext(adoptPtr(new FakeWebGraphicsContext3DWithQueryTesting));
    OwnPtr<ThrottledTextureUploader> uploader = ThrottledTextureUploader::create(fakeContext.get());
    scoped_ptr<CCPrioritizedTexture> texture =
        CCPrioritizedTexture::create(NULL, IntSize(256, 256), GL_RGBA);
    TextureUploader::Parameters upload;
    upload.texture = texture.get();
    upload.bitmap = NULL;
    upload.picture = NULL;
    upload.geometry.contentRect = IntRect(IntPoint(0,0), texture->size());
    upload.geometry.sourceRect = IntRect(IntPoint(0,0), texture->size());
    upload.geometry.destOffset = IntSize();

    fakeContext->setResultAvailable(0);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploader->uploadTexture(NULL, upload);
    EXPECT_EQ(1, uploader->numBlockingUploads());
    uploader->uploadTexture(NULL, upload);
    EXPECT_EQ(2, uploader->numBlockingUploads());

    fakeContext->setResultAvailable(1);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploader->uploadTexture(NULL, upload);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploader->uploadTexture(NULL, upload);
    uploader->uploadTexture(NULL, upload);
    EXPECT_EQ(0, uploader->numBlockingUploads());
}

TEST(ThrottledTextureUploaderTest, MarkPendingUploadsAsNonBlocking)
{
    OwnPtr<FakeWebGraphicsContext3DWithQueryTesting> fakeContext(adoptPtr(new FakeWebGraphicsContext3DWithQueryTesting));
    OwnPtr<ThrottledTextureUploader> uploader = ThrottledTextureUploader::create(fakeContext.get());
    scoped_ptr<CCPrioritizedTexture> texture =
        CCPrioritizedTexture::create(NULL, IntSize(256, 256), GL_RGBA);
    TextureUploader::Parameters upload;
    upload.texture = texture.get();
    upload.bitmap = NULL;
    upload.picture = NULL;
    upload.geometry.contentRect = IntRect(IntPoint(0,0), texture->size());
    upload.geometry.sourceRect = IntRect(IntPoint(0,0), texture->size());
    upload.geometry.destOffset = IntSize();

    fakeContext->setResultAvailable(0);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploader->uploadTexture(NULL, upload);
    uploader->uploadTexture(NULL, upload);
    EXPECT_EQ(2, uploader->numBlockingUploads());

    uploader->markPendingUploadsAsNonBlocking();
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploader->uploadTexture(NULL, upload);
    EXPECT_EQ(1, uploader->numBlockingUploads());

    fakeContext->setResultAvailable(1);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploader->uploadTexture(NULL, upload);
    uploader->markPendingUploadsAsNonBlocking();
    EXPECT_EQ(0, uploader->numBlockingUploads());
}

} // namespace
