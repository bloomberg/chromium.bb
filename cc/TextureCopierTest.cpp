// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "TextureCopier.h"

#include "FakeWebGraphicsContext3D.h"
#include "GraphicsContext3D.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wtf/RefPtr.h>

using namespace cc;
using namespace WebKit;
using testing::InSequence;
using testing::Test;
using testing::_;

class MockContext : public FakeWebGraphicsContext3D {
public:
    MOCK_METHOD2(bindFramebuffer, void(WGC3Denum, WebGLId));
    MOCK_METHOD3(texParameteri, void(WGC3Denum target, WGC3Denum pname, WGC3Dint param));

    MOCK_METHOD3(drawArrays, void(WGC3Denum mode, WGC3Dint first, WGC3Dsizei count));
};

TEST(TextureCopierTest, testDrawArraysCopy)
{
    OwnPtr<MockContext> mockContext = adoptPtr(new MockContext);

    {
        InSequence sequence;

        // Here we check just some essential properties of copyTexture() to avoid mirroring the full implementation.
        EXPECT_CALL(*mockContext, bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, _));

        // Make sure linear filtering is disabled during the copy.
        EXPECT_CALL(*mockContext, texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::NEAREST));
        EXPECT_CALL(*mockContext, texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::NEAREST));

        EXPECT_CALL(*mockContext, drawArrays(_, _, _));

        // Linear filtering should be restored.
        EXPECT_CALL(*mockContext, texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR));
        EXPECT_CALL(*mockContext, texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR));

        // Default framebuffer should be restored
        EXPECT_CALL(*mockContext, bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, 0));
    }

    int sourceTextureId = 1;
    int destTextureId = 2;
    IntSize size(256, 128);
    OwnPtr<AcceleratedTextureCopier> copier(AcceleratedTextureCopier::create(mockContext.get(), false));
    TextureCopier::Parameters copy = { sourceTextureId, destTextureId, size };
    copier->copyTexture(copy);
}
