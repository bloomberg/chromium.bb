// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/texture_uploader.h"

#include "cc/prioritized_resource.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using namespace WebKit;

namespace cc {
namespace {

unsigned int RoundUp(unsigned int n, unsigned int mul)
{
  return (((n - 1) / mul) * mul) + mul;
}

class TestWebGraphicsContext3DTextureUpload : public TestWebGraphicsContext3D {
public:
    TestWebGraphicsContext3DTextureUpload()
        : m_resultAvailable(0)
        , m_unpackAlignment(4)
    {
    }

    virtual void pixelStorei(WGC3Denum pname, WGC3Dint param)
    {
        switch (pname) {
        case GL_UNPACK_ALIGNMENT:
            // param should be a power of two <= 8
            EXPECT_EQ(0, param & (param - 1));
            EXPECT_GE(8, param);
            switch (param) {
            case 1:
            case 2:
            case 4:
            case 8:
                m_unpackAlignment = param;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
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

    virtual void texSubImage2D(WGC3Denum target, WGC3Dint level, WGC3Dint xoffset, WGC3Dint yoffset, WGC3Dsizei width, WGC3Dsizei height, WGC3Denum format, WGC3Denum type, const void* pixels)
    {
        EXPECT_EQ(GL_TEXTURE_2D, target);
        EXPECT_EQ(0, level);
        EXPECT_LE(0, width);
        EXPECT_LE(0, height);
        EXPECT_LE(0, xoffset);
        EXPECT_LE(0, yoffset);
        EXPECT_LE(0, width);
        EXPECT_LE(0, height);

        // Check for allowed format/type combination.
        unsigned int bytesPerPixel = 0;
        switch (format) {
        case GL_ALPHA:
            EXPECT_EQ(GL_UNSIGNED_BYTE, type);
            bytesPerPixel = 1;
            break;
        case GL_RGB:
            EXPECT_NE(GL_UNSIGNED_SHORT_4_4_4_4, type);
            EXPECT_NE(GL_UNSIGNED_SHORT_5_5_5_1, type);
            switch (type) {
            case GL_UNSIGNED_BYTE:
                bytesPerPixel = 3;
                break;
            case GL_UNSIGNED_SHORT_5_6_5:
                bytesPerPixel = 2;
                break;
            }
            break;
        case GL_RGBA:
            EXPECT_NE(GL_UNSIGNED_SHORT_5_6_5, type);
            switch (type) {
            case GL_UNSIGNED_BYTE:
                bytesPerPixel = 4;
                break;
            case GL_UNSIGNED_SHORT_4_4_4_4:
                bytesPerPixel = 2;
                break;
            case GL_UNSIGNED_SHORT_5_5_5_1:
                bytesPerPixel = 2;
                break;
            }
            break;
        case GL_LUMINANCE:
            EXPECT_EQ(GL_UNSIGNED_BYTE, type);
            bytesPerPixel = 1;
            break;
        case GL_LUMINANCE_ALPHA:
            EXPECT_EQ(GL_UNSIGNED_BYTE, type);
            bytesPerPixel = 2;
            break;
        }

        // If NULL, we aren't checking texture contents.
        if (pixels == NULL)
            return;

        const uint8* bytes = static_cast<const uint8*>(pixels);
        // We'll expect the first byte of every row to be 0x1, and the last byte to be 0x2
        const unsigned int stride =  RoundUp(bytesPerPixel * width, m_unpackAlignment);
        for (WGC3Dsizei row = 0; row < height; ++row) {
            const uint8* rowBytes = bytes + (xoffset * bytesPerPixel + (yoffset + row) * stride);
            EXPECT_EQ(0x1, rowBytes[0]);
            EXPECT_EQ(0x2, rowBytes[width * bytesPerPixel - 1]);
        }
    }

    void setResultAvailable(unsigned resultAvailable) { m_resultAvailable = resultAvailable; }

private:
    unsigned m_unpackAlignment;
    unsigned m_resultAvailable;
};

void uploadTexture(TextureUploader* uploader, WGC3Denum format, const gfx::Size& size, const uint8* data)
{
    uploader->upload(data,
                     gfx::Rect(gfx::Point(0, 0), size),
                     gfx::Rect(gfx::Point(0, 0), size),
                     gfx::Vector2d(),
                     format,
                     size);
}

TEST(TextureUploaderTest, NumBlockingUploads)
{
    scoped_ptr<TestWebGraphicsContext3DTextureUpload> fakeContext(new TestWebGraphicsContext3DTextureUpload);
    scoped_ptr<TextureUploader> uploader = TextureUploader::create(fakeContext.get(), false, false);

    fakeContext->setResultAvailable(0);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get(), GL_RGBA, gfx::Size(0, 0), NULL);
    EXPECT_EQ(1, uploader->numBlockingUploads());
    uploadTexture(uploader.get(), GL_RGBA, gfx::Size(0, 0), NULL);
    EXPECT_EQ(2, uploader->numBlockingUploads());

    fakeContext->setResultAvailable(1);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get(), GL_RGBA, gfx::Size(0, 0), NULL);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get(), GL_RGBA, gfx::Size(0, 0), NULL);
    uploadTexture(uploader.get(), GL_RGBA, gfx::Size(0, 0), NULL);
    EXPECT_EQ(0, uploader->numBlockingUploads());
}

TEST(TextureUploaderTest, MarkPendingUploadsAsNonBlocking)
{
    scoped_ptr<TestWebGraphicsContext3DTextureUpload> fakeContext(new TestWebGraphicsContext3DTextureUpload);
    scoped_ptr<TextureUploader> uploader = TextureUploader::create(fakeContext.get(), false, false);

    fakeContext->setResultAvailable(0);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get(), GL_RGBA, gfx::Size(0, 0), NULL);
    uploadTexture(uploader.get(), GL_RGBA, gfx::Size(0, 0), NULL);
    EXPECT_EQ(2, uploader->numBlockingUploads());

    uploader->markPendingUploadsAsNonBlocking();
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get(), GL_RGBA, gfx::Size(0, 0), NULL);
    EXPECT_EQ(1, uploader->numBlockingUploads());

    fakeContext->setResultAvailable(1);
    EXPECT_EQ(0, uploader->numBlockingUploads());
    uploadTexture(uploader.get(), GL_RGBA, gfx::Size(0, 0), NULL);
    uploader->markPendingUploadsAsNonBlocking();
    EXPECT_EQ(0, uploader->numBlockingUploads());
}

TEST(TextureUploaderTest, UploadContentsTest)
{
    scoped_ptr<TestWebGraphicsContext3DTextureUpload> fakeContext(new TestWebGraphicsContext3DTextureUpload);
    scoped_ptr<TextureUploader> uploader = TextureUploader::create(fakeContext.get(), false, false);
    uint8 buffer[256 * 256 * 4];

    // Upload a tightly packed 256x256 RGBA texture.
    memset(buffer, 0, sizeof(buffer));
    for (int i = 0; i < 256; ++i) {
        // Mark the beginning and end of each row, for the test.
        buffer[i * 4 * 256] = 0x1;
        buffer[(i + 1) * 4 * 256 - 1] = 0x2;
    }
    uploadTexture(uploader.get(), GL_RGBA, gfx::Size(256, 256), buffer);

    // Upload a tightly packed 41x43 RGBA texture.
    memset(buffer, 0, sizeof(buffer));
    for (int i = 0; i < 43; ++i) {
        // Mark the beginning and end of each row, for the test.
        buffer[i * 4 * 41] = 0x1;
        buffer[(i + 1) * 4 * 41 - 1] = 0x2;
    }
    uploadTexture(uploader.get(), GL_RGBA, gfx::Size(41, 43), buffer);

    // Upload a tightly packed 82x86 LUMINANCE texture.
    memset(buffer, 0, sizeof(buffer));
    for (int i = 0; i < 86; ++i) {
        // Mark the beginning and end of each row, for the test.
        buffer[i * 1 * 82] = 0x1;
        buffer[(i + 1) * 82 - 1] = 0x2;
    }
    uploadTexture(uploader.get(), GL_LUMINANCE, gfx::Size(82, 86), buffer);
}

}  // namespace
}  // namespace cc
