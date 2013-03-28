// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/texture_copier.h"

#include "cc/test/test_web_graphics_context_3d.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"

using testing::InSequence;
using testing::Test;
using testing::_;
using WebKit::WebGLId;
using WebKit::WGC3Denum;
using WebKit::WGC3Dint;
using WebKit::WGC3Dsizei;

namespace cc {
namespace {

class MockContext : public TestWebGraphicsContext3D {
 public:
  MOCK_METHOD2(bindFramebuffer, void(WGC3Denum, WebGLId));
  MOCK_METHOD3(texParameteri,
               void(WGC3Denum target, WGC3Denum pname, WGC3Dint param));
  MOCK_METHOD1(disable, void(WGC3Denum cap));
  MOCK_METHOD1(enable, void(WGC3Denum cap));

  MOCK_METHOD3(drawArrays,
               void(WGC3Denum mode, WGC3Dint first, WGC3Dsizei count));
};

TEST(TextureCopierTest, TestDrawArraysCopy) {
  scoped_ptr<MockContext> mock_context(new MockContext);
  {
    InSequence sequence;

    EXPECT_CALL(*mock_context, disable(GL_SCISSOR_TEST));

    // Here we check just some essential properties of copyTexture() to avoid
    // mirroring the full implementation.
    EXPECT_CALL(*mock_context, bindFramebuffer(GL_FRAMEBUFFER, _));

    // Make sure linear filtering is disabled during the copy.
    EXPECT_CALL(
        *mock_context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    EXPECT_CALL(
        *mock_context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

    EXPECT_CALL(*mock_context, disable(GL_BLEND));

    EXPECT_CALL(*mock_context, drawArrays(_, _, _));

    // Linear filtering, default framebuffer and scissor test should be
    // restored.
    EXPECT_CALL(*mock_context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    EXPECT_CALL(*mock_context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    EXPECT_CALL(*mock_context, bindFramebuffer(GL_FRAMEBUFFER, 0));
    EXPECT_CALL(*mock_context, enable(GL_SCISSOR_TEST));
  }

  int source_texture_id = mock_context->createTexture();
  int dest_texture_id = mock_context->createTexture();
  gfx::Size size(256, 128);
  scoped_ptr<AcceleratedTextureCopier> copier(
      AcceleratedTextureCopier::Create(mock_context.get(), false));
  TextureCopier::Parameters copy = { source_texture_id, dest_texture_id, size };
  copier->CopyTexture(copy);
}

}  // namespace
}  // namespace cc
