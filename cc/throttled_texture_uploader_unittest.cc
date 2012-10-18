// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/throttled_texture_uploader.h"

#include "CCPrioritizedTexture.h"
#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "cc/test/fake_web_graphics_context_3d.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

void uploadTexture(
    ThrottledTextureUploader* uploader, CCPrioritizedTexture* texture)
{
    uploader->uploadTexture(NULL,
                            texture,
                            NULL,
                            IntRect(IntPoint(0,0), texture->size()),
                            IntRect(IntPoint(0,0), texture->size()),
                            IntSize());
}

TEST(ThrottledTextureUploaderTest, NumBlockingUploads)
{
    scoped_ptr<FakeWebGraphicsContext3DWithQueryTesting> fakeContext(new FakeWebGraphicsContext3DWithQueryTesting);
    scoped_ptr<ThrottledTextureUploader> uploader = ThrottledTextureUploader::create(fakeContext.get());
    scoped_ptr<CCPrioritizedTexture> texture =
        CCPrioritizedTexture::create(NULL, IntSize(256, 256), GL_RGBA);

    fakeContext->setResultAvailable(0);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get(), texture.get());
    EXPECT_EQ(1, uploader->numBlockingUploads());
    uploadTexture(uploader.get(), texture.get());
    EXPECT_EQ(2, uploader->numBlockingUploads());

    fakeContext->setResultAvailable(1);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get(), texture.get());
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get(), texture.get());
    uploadTexture(uploader.get(), texture.get());
    EXPECT_EQ(0, uploader->numBlockingUploads());
}

TEST(ThrottledTextureUploaderTest, MarkPendingUploadsAsNonBlocking)
{
    scoped_ptr<FakeWebGraphicsContext3DWithQueryTesting> fakeContext(new FakeWebGraphicsContext3DWithQueryTesting);
    scoped_ptr<ThrottledTextureUploader> uploader = ThrottledTextureUploader::create(fakeContext.get());
    scoped_ptr<CCPrioritizedTexture> texture =
        CCPrioritizedTexture::create(NULL, IntSize(256, 256), GL_RGBA);

    fakeContext->setResultAvailable(0);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get(), texture.get());
    uploadTexture(uploader.get(), texture.get());
    EXPECT_EQ(2, uploader->numBlockingUploads());

    uploader->markPendingUploadsAsNonBlocking();
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get(), texture.get());
    EXPECT_EQ(1, uploader->numBlockingUploads());

    fakeContext->setResultAvailable(1);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get(), texture.get());
    uploader->markPendingUploadsAsNonBlocking();
    EXPECT_EQ(0, uploader->numBlockingUploads());
}

} // namespace
