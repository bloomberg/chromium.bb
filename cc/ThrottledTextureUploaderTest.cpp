// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "ThrottledTextureUploader.h"

#include "Extensions3DChromium.h"
#include "FakeWebGraphicsContext3D.h"
#include "GraphicsContext3D.h"

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

class FakeTexture : public cc::LayerTextureUpdater::Texture {
public:
  FakeTexture() : LayerTextureUpdater::Texture(
      PassOwnPtr<cc::CCPrioritizedTexture>()) { }

    virtual void updateRect(cc::CCResourceProvider* , const cc::IntRect&, const cc::IntSize&) OVERRIDE { }
};


TEST(ThrottledTextureUploaderTest, NumBlockingUploads)
{
    OwnPtr<FakeWebGraphicsContext3DWithQueryTesting> fakeContext(adoptPtr(new FakeWebGraphicsContext3DWithQueryTesting));
    OwnPtr<ThrottledTextureUploader> uploader = ThrottledTextureUploader::create(fakeContext.get());
    OwnPtr<FakeTexture> texture = adoptPtr(new FakeTexture);
    TextureUploader::Parameters upload;
    upload.texture = texture.get();
    upload.sourceRect = IntRect();
    upload.destOffset = IntSize();

    fakeContext->setResultAvailable(0);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploader->beginUploads();
    uploader->uploadTexture(NULL, upload);
    uploader->endUploads();
    EXPECT_EQ(1, uploader->numBlockingUploads());
    uploader->beginUploads();
    uploader->uploadTexture(NULL, upload);
    uploader->endUploads();
    EXPECT_EQ(2, uploader->numBlockingUploads());

    fakeContext->setResultAvailable(1);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploader->beginUploads();
    uploader->uploadTexture(NULL, upload);
    uploader->endUploads();
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploader->beginUploads();
    uploader->uploadTexture(NULL, upload);
    uploader->uploadTexture(NULL, upload);
    uploader->endUploads();
    EXPECT_EQ(0, uploader->numBlockingUploads());
}

TEST(ThrottledTextureUploaderTest, MarkPendingUploadsAsNonBlocking)
{
    OwnPtr<FakeWebGraphicsContext3DWithQueryTesting> fakeContext(adoptPtr(new FakeWebGraphicsContext3DWithQueryTesting));
    OwnPtr<ThrottledTextureUploader> uploader = ThrottledTextureUploader::create(fakeContext.get());
    OwnPtr<FakeTexture> texture = adoptPtr(new FakeTexture);
    TextureUploader::Parameters upload;
    upload.texture = texture.get();
    upload.sourceRect = IntRect();
    upload.destOffset = IntSize();

    fakeContext->setResultAvailable(0);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploader->beginUploads();
    uploader->uploadTexture(NULL, upload);
    uploader->uploadTexture(NULL, upload);
    uploader->endUploads();
    EXPECT_EQ(2, uploader->numBlockingUploads());

    uploader->markPendingUploadsAsNonBlocking();
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploader->beginUploads();
    uploader->uploadTexture(NULL, upload);
    uploader->endUploads();
    EXPECT_EQ(1, uploader->numBlockingUploads());

    fakeContext->setResultAvailable(1);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploader->beginUploads();
    uploader->uploadTexture(NULL, upload);
    uploader->endUploads();
    uploader->markPendingUploadsAsNonBlocking();
    EXPECT_EQ(0, uploader->numBlockingUploads());
}

} // namespace
