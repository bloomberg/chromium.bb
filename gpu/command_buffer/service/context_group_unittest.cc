// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_group.h"
#include "app/gfx/gl/gl_mock.h"
#include "base/scoped_ptr.h"
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

  void SetupInitExpectations(const char* extensions) {
    TestHelper::SetupContextGroupInitExpectations(gl_.get(), extensions);
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
  }

  virtual void TearDown() {
    group_.Destroy(false);
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  ContextGroup group_;
};

TEST_F(ContextGroupTest, Basic) {
  // Test it starts off uninitialized.
  EXPECT_EQ(0u, group_.max_vertex_attribs());
  EXPECT_EQ(0u, group_.max_texture_units());
  EXPECT_EQ(0u, group_.max_texture_image_units());
  EXPECT_EQ(0u, group_.max_vertex_texture_image_units());
  EXPECT_EQ(0u, group_.max_fragment_uniform_vectors());
  EXPECT_EQ(0u, group_.max_varying_vectors());
  EXPECT_EQ(0u, group_.max_vertex_uniform_vectors());
  EXPECT_TRUE(group_.buffer_manager() == NULL);
  EXPECT_TRUE(group_.framebuffer_manager() == NULL);
  EXPECT_TRUE(group_.renderbuffer_manager() == NULL);
  EXPECT_TRUE(group_.texture_manager() == NULL);
  EXPECT_TRUE(group_.program_manager() == NULL);
  EXPECT_TRUE(group_.shader_manager() == NULL);
  EXPECT_FALSE(group_.extension_flags().ext_framebuffer_multisample);
  EXPECT_FALSE(group_.extension_flags().oes_standard_derivatives);
}

TEST_F(ContextGroupTest, InitializeNoExtensions) {
  SetupInitExpectations("");
  group_.Initialize();
  // Check a couple of random extensions that should not be there.
  EXPECT_THAT(group_.extensions(), Not(HasSubstr("GL_OES_texture_npot")));
  EXPECT_THAT(group_.extensions(),
              Not(HasSubstr("GL_EXT_texture_compression_dxt1")));
  EXPECT_FALSE(group_.texture_manager()->npot_ok());
  EXPECT_FALSE(group_.validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT));
  EXPECT_FALSE(group_.validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));
  EXPECT_FALSE(group_.validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT3_EXT));
  EXPECT_FALSE(group_.validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT5_EXT));
  EXPECT_FALSE(group_.validators()->read_pixel_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_FALSE(group_.validators()->texture_parameter.IsValid(
      GL_TEXTURE_MAX_ANISOTROPY_EXT));
  EXPECT_FALSE(group_.validators()->g_l_state.IsValid(
      GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT));
  EXPECT_FALSE(group_.validators()->frame_buffer_target.IsValid(
      GL_READ_FRAMEBUFFER_EXT));
  EXPECT_FALSE(group_.validators()->frame_buffer_target.IsValid(
      GL_DRAW_FRAMEBUFFER_EXT));
  EXPECT_FALSE(group_.validators()->g_l_state.IsValid(
      GL_READ_FRAMEBUFFER_BINDING_EXT));
  EXPECT_FALSE(group_.validators()->render_buffer_parameter.IsValid(
      GL_MAX_SAMPLES_EXT));
  EXPECT_FALSE(group_.validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_FALSE(group_.validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(group_.validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_FALSE(group_.validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
  EXPECT_FALSE(group_.validators()->render_buffer_format.IsValid(
      GL_DEPTH24_STENCIL8));
  EXPECT_FALSE(group_.validators()->texture_internal_format.IsValid(
      GL_DEPTH_STENCIL));
  EXPECT_FALSE(group_.validators()->texture_format.IsValid(
      GL_DEPTH_STENCIL));
  EXPECT_FALSE(group_.validators()->pixel_type.IsValid(
      GL_UNSIGNED_INT_24_8));
  EXPECT_FALSE(group_.validators()->render_buffer_format.IsValid(
      GL_DEPTH_COMPONENT24));
}

TEST_F(ContextGroupTest, InitializeNPOTExtensionGLES) {
  SetupInitExpectations("GL_OES_texture_npot");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(), HasSubstr("GL_OES_texture_npot"));
  EXPECT_TRUE(group_.texture_manager()->npot_ok());
}

TEST_F(ContextGroupTest, InitializeNPOTExtensionGL) {
  SetupInitExpectations("GL_ARB_texture_non_power_of_two");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(), HasSubstr("GL_OES_texture_npot"));
  EXPECT_TRUE(group_.texture_manager()->npot_ok());
}

TEST_F(ContextGroupTest, InitializeDXTExtensionGLES2) {
  SetupInitExpectations("GL_EXT_texture_compression_dxt1");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_EXT_texture_compression_dxt1"));
  EXPECT_TRUE(group_.validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT));
  EXPECT_TRUE(group_.validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));
  EXPECT_FALSE(group_.validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT3_EXT));
  EXPECT_FALSE(group_.validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT5_EXT));
}

TEST_F(ContextGroupTest, InitializeDXTExtensionGL) {
  SetupInitExpectations("GL_EXT_texture_compression_s3tc");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_EXT_texture_compression_dxt1"));
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_EXT_texture_compression_s3tc"));
  EXPECT_TRUE(group_.validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT));
  EXPECT_TRUE(group_.validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));
  EXPECT_TRUE(group_.validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT3_EXT));
  EXPECT_TRUE(group_.validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT5_EXT));
}

TEST_F(ContextGroupTest, InitializeEXT_texture_format_BGRA8888GLES2) {
  SetupInitExpectations("GL_EXT_texture_format_BGRA8888");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_EXT_texture_format_BGRA8888"));
  EXPECT_TRUE(group_.validators()->texture_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_TRUE(group_.validators()->texture_internal_format.IsValid(
      GL_BGRA_EXT));
}

TEST_F(ContextGroupTest, InitializeEXT_texture_format_BGRA8888GL) {
  SetupInitExpectations("GL_EXT_bgra");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_EXT_texture_format_BGRA8888"));
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_EXT_read_format_bgra"));
  EXPECT_TRUE(group_.validators()->texture_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_TRUE(group_.validators()->texture_internal_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_TRUE(group_.validators()->read_pixel_format.IsValid(
      GL_BGRA_EXT));
}

TEST_F(ContextGroupTest, InitializeEXT_texture_format_BGRA8888Apple) {
  SetupInitExpectations("GL_APPLE_texture_format_BGRA8888");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_EXT_texture_format_BGRA8888"));
  EXPECT_TRUE(group_.validators()->texture_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_TRUE(group_.validators()->texture_internal_format.IsValid(
      GL_BGRA_EXT));
}

TEST_F(ContextGroupTest, InitializeEXT_read_format_bgra) {
  SetupInitExpectations("GL_EXT_read_format_bgra");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_EXT_read_format_bgra"));
  EXPECT_FALSE(group_.validators()->texture_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_FALSE(group_.validators()->texture_internal_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_TRUE(group_.validators()->read_pixel_format.IsValid(
      GL_BGRA_EXT));
}

TEST_F(ContextGroupTest, InitializeOES_texture_floatGLES2) {
  SetupInitExpectations("GL_OES_texture_float");
  group_.Initialize();
  EXPECT_FALSE(group_.texture_manager()->enable_float_linear());
  EXPECT_FALSE(group_.texture_manager()->enable_half_float_linear());
  EXPECT_THAT(group_.extensions(), HasSubstr("GL_OES_texture_float"));
  EXPECT_THAT(group_.extensions(), Not(HasSubstr("GL_OES_texture_half_float")));
  EXPECT_THAT(group_.extensions(),
              Not(HasSubstr("GL_OES_texture_float_linear")));
  EXPECT_THAT(group_.extensions(),
              Not(HasSubstr("GL_OES_texture_half_float_linear")));
  EXPECT_TRUE(group_.validators()->pixel_type.IsValid(GL_FLOAT));
  EXPECT_FALSE(group_.validators()->pixel_type.IsValid(GL_HALF_FLOAT_OES));
}

TEST_F(ContextGroupTest, InitializeOES_texture_float_linearGLES2) {
  SetupInitExpectations("GL_OES_texture_float GL_OES_texture_float_linear");
  group_.Initialize();
  EXPECT_TRUE(group_.texture_manager()->enable_float_linear());
  EXPECT_FALSE(group_.texture_manager()->enable_half_float_linear());
  EXPECT_THAT(group_.extensions(), HasSubstr("GL_OES_texture_float"));
  EXPECT_THAT(group_.extensions(), Not(HasSubstr("GL_OES_texture_half_float")));
  EXPECT_THAT(group_.extensions(), HasSubstr("GL_OES_texture_float_linear"));
  EXPECT_THAT(group_.extensions(),
              Not(HasSubstr("GL_OES_texture_half_float_linear")));
  EXPECT_TRUE(group_.validators()->pixel_type.IsValid(GL_FLOAT));
  EXPECT_FALSE(group_.validators()->pixel_type.IsValid(GL_HALF_FLOAT_OES));
}

TEST_F(ContextGroupTest, InitializeOES_texture_half_floatGLES2) {
  SetupInitExpectations("GL_OES_texture_half_float");
  group_.Initialize();
  EXPECT_FALSE(group_.texture_manager()->enable_float_linear());
  EXPECT_FALSE(group_.texture_manager()->enable_half_float_linear());
  EXPECT_THAT(group_.extensions(), Not(HasSubstr("GL_OES_texture_float")));
  EXPECT_THAT(group_.extensions(), HasSubstr("GL_OES_texture_half_float"));
  EXPECT_THAT(group_.extensions(),
              Not(HasSubstr("GL_OES_texture_float_linear")));
  EXPECT_THAT(group_.extensions(),
              Not(HasSubstr("GL_OES_texture_half_float_linear")));
  EXPECT_FALSE(group_.validators()->pixel_type.IsValid(GL_FLOAT));
  EXPECT_TRUE(group_.validators()->pixel_type.IsValid(GL_HALF_FLOAT_OES));
}

TEST_F(ContextGroupTest, InitializeOES_texture_half_float_linearGLES2) {
  SetupInitExpectations(
      "GL_OES_texture_half_float GL_OES_texture_half_float_linear");
  group_.Initialize();
  EXPECT_FALSE(group_.texture_manager()->enable_float_linear());
  EXPECT_TRUE(group_.texture_manager()->enable_half_float_linear());
  EXPECT_THAT(group_.extensions(), Not(HasSubstr("GL_OES_texture_float")));
  EXPECT_THAT(group_.extensions(), HasSubstr("GL_OES_texture_half_float"));
  EXPECT_THAT(group_.extensions(),
              Not(HasSubstr("GL_OES_texture_float_linear")));
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_OES_texture_half_float_linear"));
  EXPECT_FALSE(group_.validators()->pixel_type.IsValid(GL_FLOAT));
  EXPECT_TRUE(group_.validators()->pixel_type.IsValid(GL_HALF_FLOAT_OES));
}

TEST_F(ContextGroupTest, InitializeEXT_framebuffer_multisample) {
  SetupInitExpectations("GL_EXT_framebuffer_multisample");
  group_.Initialize();
  EXPECT_TRUE(group_.extension_flags().ext_framebuffer_multisample);
  EXPECT_THAT(group_.extensions(), HasSubstr("GL_EXT_framebuffer_multisample"));
  EXPECT_THAT(group_.extensions(), HasSubstr("GL_EXT_framebuffer_blit"));
  EXPECT_TRUE(group_.validators()->frame_buffer_target.IsValid(
      GL_READ_FRAMEBUFFER_EXT));
  EXPECT_TRUE(group_.validators()->frame_buffer_target.IsValid(
      GL_DRAW_FRAMEBUFFER_EXT));
  EXPECT_TRUE(group_.validators()->g_l_state.IsValid(
      GL_READ_FRAMEBUFFER_BINDING_EXT));
  EXPECT_TRUE(group_.validators()->render_buffer_parameter.IsValid(
      GL_MAX_SAMPLES_EXT));
}

TEST_F(ContextGroupTest, InitializeEXT_texture_filter_anisotropic) {
  SetupInitExpectations("GL_EXT_texture_filter_anisotropic");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_EXT_texture_filter_anisotropic"));
  EXPECT_TRUE(group_.validators()->texture_parameter.IsValid(
      GL_TEXTURE_MAX_ANISOTROPY_EXT));
  EXPECT_TRUE(group_.validators()->g_l_state.IsValid(
      GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT));
}

TEST_F(ContextGroupTest, InitializeEXT_ARB_depth_texture) {
  SetupInitExpectations("GL_ARB_depth_texture");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_GOOGLE_depth_texture"));
  EXPECT_TRUE(group_.validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_TRUE(group_.validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_TRUE(group_.validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_TRUE(group_.validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
}

TEST_F(ContextGroupTest, InitializeOES_ARB_depth_texture) {
  SetupInitExpectations("GL_OES_depth_texture");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_GOOGLE_depth_texture"));
  EXPECT_TRUE(group_.validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_TRUE(group_.validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_TRUE(group_.validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_TRUE(group_.validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
}

TEST_F(ContextGroupTest, InitializeEXT_packed_depth_stencil) {
  SetupInitExpectations("GL_EXT_packed_depth_stencil");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_OES_packed_depth_stencil"));
  EXPECT_TRUE(group_.validators()->render_buffer_format.IsValid(
      GL_DEPTH24_STENCIL8));
  EXPECT_FALSE(group_.validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_FALSE(group_.validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(group_.validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_FALSE(group_.validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
}

TEST_F(ContextGroupTest, InitializeOES_packed_depth_stencil) {
  SetupInitExpectations("GL_OES_packed_depth_stencil");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_OES_packed_depth_stencil"));
  EXPECT_TRUE(group_.validators()->render_buffer_format.IsValid(
      GL_DEPTH24_STENCIL8));
  EXPECT_FALSE(group_.validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_FALSE(group_.validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(group_.validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_FALSE(group_.validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
}

TEST_F(ContextGroupTest,
       InitializeOES_packed_depth_stencil_and_GL_ARB_depth_texture) {
  SetupInitExpectations("GL_OES_packed_depth_stencil GL_ARB_depth_texture");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_OES_packed_depth_stencil"));
  EXPECT_TRUE(group_.validators()->render_buffer_format.IsValid(
      GL_DEPTH24_STENCIL8));
  EXPECT_TRUE(group_.validators()->texture_internal_format.IsValid(
      GL_DEPTH_STENCIL));
  EXPECT_TRUE(group_.validators()->texture_format.IsValid(
      GL_DEPTH_STENCIL));
  EXPECT_TRUE(group_.validators()->pixel_type.IsValid(
      GL_UNSIGNED_INT_24_8));
}

TEST_F(ContextGroupTest, InitializeOES_depth24) {
  SetupInitExpectations("GL_OES_depth24");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(), HasSubstr("GL_OES_depth24"));
  EXPECT_TRUE(group_.validators()->render_buffer_format.IsValid(
      GL_DEPTH_COMPONENT24));
}

TEST_F(ContextGroupTest, InitializeOES_standard_derivatives) {
  SetupInitExpectations("GL_OES_standard_derivatives");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(), HasSubstr("GL_OES_standard_derivatives"));
  EXPECT_TRUE(group_.extension_flags().oes_standard_derivatives);
}

}  // namespace gles2
}  // namespace gpu


