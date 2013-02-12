// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/texture_copier.h"

#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"

using namespace WebKit;
using testing::InSequence;
using testing::Test;
using testing::_;

namespace cc {
namespace {

class MockContext : public TestWebGraphicsContext3D {
public:
    MOCK_METHOD2(bindFramebuffer, void(WGC3Denum, WebGLId));
    MOCK_METHOD3(texParameteri, void(WGC3Denum target, WGC3Denum pname, WGC3Dint param));
    MOCK_METHOD1(disable, void(WGC3Denum cap));
    MOCK_METHOD1(enable, void(WGC3Denum cap));

    MOCK_METHOD3(drawArrays, void(WGC3Denum mode, WGC3Dint first, WGC3Dsizei count));
};

TEST(TextureCopierTest, testDrawArraysCopy)
{
    scoped_ptr<MockContext> mockContext(new MockContext);

    {
        InSequence sequence;

        EXPECT_CALL(*mockContext, disable(GL_SCISSOR_TEST));

        // Here we check just some essential properties of copyTexture() to avoid mirroring the full implementation.
        EXPECT_CALL(*mockContext, bindFramebuffer(GL_FRAMEBUFFER, _));

        // Make sure linear filtering is disabled during the copy.
        EXPECT_CALL(*mockContext, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        EXPECT_CALL(*mockContext, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

        EXPECT_CALL(*mockContext, disable(GL_BLEND));

        EXPECT_CALL(*mockContext, drawArrays(_, _, _));

        // Linear filtering, default framebuffer and scissor test should be restored.
        EXPECT_CALL(*mockContext, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        EXPECT_CALL(*mockContext, texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        EXPECT_CALL(*mockContext, bindFramebuffer(GL_FRAMEBUFFER, 0));
        EXPECT_CALL(*mockContext, enable(GL_SCISSOR_TEST));
    }

    int sourceTextureId = mockContext->createTexture();
    int destTextureId = mockContext->createTexture();
    gfx::Size size(256, 128);
    scoped_ptr<AcceleratedTextureCopier> copier(AcceleratedTextureCopier::create(mockContext.get(), false));
    TextureCopier::Parameters copy = { sourceTextureId, destTextureId, size };
    copier->copyTexture(copy);
}

}  // namespace
}  // namespace cc
