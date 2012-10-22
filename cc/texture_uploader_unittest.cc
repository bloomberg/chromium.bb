// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/texture_uploader.h"

#include "cc/prioritized_texture.h"
#include "cc/test/fake_web_graphics_context_3d.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using namespace cc;
using namespace WebKit;

namespace {

class FakeWebGraphicsContext3DWithQueryTesting : public FakeWebGraphicsContext3D {
public:
    FakeWebGraphicsContext3DWithQueryTesting() : m_resultAvailable(0)
    {
    }

    virtual void getQueryObjectuivEXT(WebGLId, WGC3Denum type, WGC3Duint* value)
    {
        switch (type) {
        case GL_QUERY_RESULT_AVAILABLE_EXT:
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

void uploadTexture(TextureUploader* uploader)
{
    IntSize size(256, 256);
    uploader->upload(NULL,
                     IntRect(IntPoint(0, 0), size),
                     IntRect(IntPoint(0, 0), size),
                     IntSize(),
                     GL_RGBA,
                     size);
}

TEST(TextureUploaderTest, NumBlockingUploads)
{
    scoped_ptr<FakeWebGraphicsContext3DWithQueryTesting> fakeContext(new FakeWebGraphicsContext3DWithQueryTesting);
    scoped_ptr<TextureUploader> uploader = TextureUploader::create(fakeContext.get(), false);

    fakeContext->setResultAvailable(0);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get());
    EXPECT_EQ(1, uploader->numBlockingUploads());
    uploadTexture(uploader.get());
    EXPECT_EQ(2, uploader->numBlockingUploads());

    fakeContext->setResultAvailable(1);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get());
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get());
    uploadTexture(uploader.get());
    EXPECT_EQ(0, uploader->numBlockingUploads());
}

TEST(TextureUploaderTest, MarkPendingUploadsAsNonBlocking)
{
    scoped_ptr<FakeWebGraphicsContext3DWithQueryTesting> fakeContext(new FakeWebGraphicsContext3DWithQueryTesting);
    scoped_ptr<TextureUploader> uploader = TextureUploader::create(fakeContext.get(), false);

    fakeContext->setResultAvailable(0);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get());
    uploadTexture(uploader.get());
    EXPECT_EQ(2, uploader->numBlockingUploads());

    uploader->markPendingUploadsAsNonBlocking();
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get());
    EXPECT_EQ(1, uploader->numBlockingUploads());

    fakeContext->setResultAvailable(1);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get());
    uploader->markPendingUploadsAsNonBlocking();
    EXPECT_EQ(0, uploader->numBlockingUploads());
}

} // namespace
