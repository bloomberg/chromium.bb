// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_group.h"

#include "base/scoped_ptr.h"
#include "gpu/command_buffer/common/gl_mock.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/GLES2/gles2_command_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::gfx::MockGLInterface;
using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace gpu {
namespace gles2 {

class ContextGroupTest : public testing::Test {
 public:
  ContextGroupTest() {
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
    group_ = ContextGroup::Ref(new ContextGroup());
  }

  virtual void TearDown() {
    // we must release the ContextGroup before we clear out the GL interface.
    // since its destructor uses GL.
    group_->set_have_context(false);
    group_ = NULL;
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  ContextGroup::Ref group_;
};

TEST_F(ContextGroupTest, Basic) {
  // Test it starts off uninitialized.
  EXPECT_EQ(0u, group_->max_vertex_attribs());
  EXPECT_EQ(0u, group_->max_texture_units());
  EXPECT_EQ(0u, group_->max_texture_image_units());
  EXPECT_EQ(0u, group_->max_vertex_texture_image_units());
  EXPECT_EQ(0u, group_->max_fragment_uniform_vectors());
  EXPECT_EQ(0u, group_->max_varying_vectors());
  EXPECT_EQ(0u, group_->max_vertex_uniform_vectors());
  EXPECT_TRUE(group_->buffer_manager() == NULL);
  EXPECT_TRUE(group_->framebuffer_manager() == NULL);
  EXPECT_TRUE(group_->renderbuffer_manager() == NULL);
  EXPECT_TRUE(group_->texture_manager() == NULL);
  EXPECT_TRUE(group_->program_manager() == NULL);
  EXPECT_TRUE(group_->shader_manager() == NULL);
}

TEST_F(ContextGroupTest, InitializeNoExtensions) {
  TestHelper::SetupContextGroupInitExpectations(gl_.get(),
      DisallowedExtensions(), "");
  group_->Initialize(DisallowedExtensions(), "");
  EXPECT_EQ(static_cast<uint32>(TestHelper::kNumVertexAttribs),
            group_->max_vertex_attribs());
  EXPECT_EQ(static_cast<uint32>(TestHelper::kNumTextureUnits),
            group_->max_texture_units());
  EXPECT_EQ(static_cast<uint32>(TestHelper::kMaxTextureImageUnits),
            group_->max_texture_image_units());
  EXPECT_EQ(static_cast<uint32>(TestHelper::kMaxVertexTextureImageUnits),
             group_->max_vertex_texture_image_units());
  EXPECT_EQ(static_cast<uint32>(TestHelper::kMaxFragmentUniformVectors),
            group_->max_fragment_uniform_vectors());
  EXPECT_EQ(static_cast<uint32>(TestHelper::kMaxVaryingVectors),
            group_->max_varying_vectors());
  EXPECT_EQ(static_cast<uint32>(TestHelper::kMaxVertexUniformVectors),
            group_->max_vertex_uniform_vectors());
  EXPECT_TRUE(group_->buffer_manager() != NULL);
  EXPECT_TRUE(group_->framebuffer_manager() != NULL);
  EXPECT_TRUE(group_->renderbuffer_manager() != NULL);
  EXPECT_TRUE(group_->texture_manager() != NULL);
  EXPECT_TRUE(group_->program_manager() != NULL);
  EXPECT_TRUE(group_->shader_manager() != NULL);
}

}  // namespace gles2
}  // namespace gpu


