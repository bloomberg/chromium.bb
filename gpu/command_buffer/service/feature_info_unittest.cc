// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/feature_info.h"

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/common/gl_mock.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/texture_manager.h"
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

class FeatureInfoTest : public testing::Test {
 public:
  FeatureInfoTest() {
  }

  void SetupInitExpectations(const char* extensions) {
    SetupInitExpectationsWithVendor(extensions, "", "");
  }

  void SetupInitExpectationsWithVendor(
      const char* extensions, const char* vendor, const char* renderer) {
    TestHelper::SetupFeatureInfoInitExpectationsWithVendor(
        gl_.get(), extensions, vendor, renderer);
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
    info_ = new FeatureInfo();
  }

  virtual void TearDown() {
    info_ = NULL;
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  FeatureInfo::Ref info_;
};

TEST_F(FeatureInfoTest, Basic) {
  // Test it starts off uninitialized.
  EXPECT_FALSE(info_->feature_flags().chromium_framebuffer_multisample);
  EXPECT_FALSE(info_->feature_flags().oes_standard_derivatives);
  EXPECT_FALSE(info_->feature_flags().npot_ok);
  EXPECT_FALSE(info_->feature_flags().enable_texture_float_linear);
  EXPECT_FALSE(info_->feature_flags().enable_texture_half_float_linear);
  EXPECT_FALSE(info_->feature_flags().chromium_webglsl);
  EXPECT_FALSE(info_->feature_flags().oes_egl_image_external);
  EXPECT_FALSE(info_->feature_flags().chromium_stream_texture);
  EXPECT_FALSE(info_->feature_flags().angle_translated_shader_source);
  EXPECT_FALSE(info_->feature_flags().angle_pack_reverse_row_order);
  EXPECT_FALSE(info_->feature_flags().arb_texture_rectangle);
  EXPECT_FALSE(info_->feature_flags().angle_instanced_arrays);
  EXPECT_FALSE(info_->feature_flags().occlusion_query_boolean);
  EXPECT_FALSE(info_->feature_flags(
      ).use_arb_occlusion_query2_for_occlusion_query_boolean);
  EXPECT_FALSE(info_->feature_flags(
      ).use_arb_occlusion_query_for_occlusion_query_boolean);
  EXPECT_FALSE(info_->feature_flags().is_intel);
  EXPECT_FALSE(info_->feature_flags().is_nvidia);
  EXPECT_FALSE(info_->feature_flags().is_amd);
}

TEST_F(FeatureInfoTest, InitializeNoExtensions) {
  SetupInitExpectations("");
  info_->Initialize(NULL);
  // Check default extensions are there
  EXPECT_THAT(info_->extensions(), HasSubstr("GL_CHROMIUM_resource_safe"));
  EXPECT_THAT(info_->extensions(), HasSubstr("GL_CHROMIUM_strict_attribs"));
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_ANGLE_translated_shader_source"));

  // Check a couple of random extensions that should not be there.
  EXPECT_THAT(info_->extensions(), Not(HasSubstr("GL_CHROMIUM_webglsl")));
  EXPECT_THAT(info_->extensions(), Not(HasSubstr("GL_OES_texture_npot")));
  EXPECT_THAT(info_->extensions(),
              Not(HasSubstr("GL_EXT_texture_compression_dxt1")));
  EXPECT_THAT(info_->extensions(),
              Not(HasSubstr("GL_CHROMIUM_texture_compression_dxt3")));
  EXPECT_THAT(info_->extensions(),
              Not(HasSubstr("GL_CHROMIUM_texture_compression_dxt5")));
  EXPECT_THAT(info_->extensions(),
              Not(HasSubstr("GL_ANGLE_texture_usage")));
  EXPECT_THAT(info_->extensions(),
              Not(HasSubstr("GL_EXT_texture_storage")));
  EXPECT_FALSE(info_->feature_flags().npot_ok);
  EXPECT_FALSE(info_->feature_flags().chromium_webglsl);
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT3_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT5_EXT));
  EXPECT_FALSE(info_->validators()->read_pixel_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_FALSE(info_->validators()->texture_parameter.IsValid(
      GL_TEXTURE_MAX_ANISOTROPY_EXT));
  EXPECT_FALSE(info_->validators()->g_l_state.IsValid(
      GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT));
  EXPECT_FALSE(info_->validators()->frame_buffer_target.IsValid(
      GL_READ_FRAMEBUFFER_EXT));
  EXPECT_FALSE(info_->validators()->frame_buffer_target.IsValid(
      GL_DRAW_FRAMEBUFFER_EXT));
  EXPECT_FALSE(info_->validators()->g_l_state.IsValid(
      GL_READ_FRAMEBUFFER_BINDING_EXT));
  EXPECT_FALSE(info_->validators()->render_buffer_parameter.IsValid(
      GL_MAX_SAMPLES_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
  EXPECT_FALSE(info_->validators()->render_buffer_format.IsValid(
      GL_DEPTH24_STENCIL8));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_STENCIL));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(
      GL_DEPTH_STENCIL));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(
      GL_UNSIGNED_INT_24_8));
  EXPECT_FALSE(info_->validators()->render_buffer_format.IsValid(
      GL_DEPTH_COMPONENT24));
  EXPECT_FALSE(info_->validators()->texture_parameter.IsValid(
      GL_TEXTURE_USAGE_ANGLE));
}

TEST_F(FeatureInfoTest, InitializeNPOTExtensionGLES) {
  SetupInitExpectations("GL_OES_texture_npot");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(), HasSubstr("GL_OES_texture_npot"));
  EXPECT_TRUE(info_->feature_flags().npot_ok);
}

TEST_F(FeatureInfoTest, InitializeNPOTExtensionGL) {
  SetupInitExpectations("GL_ARB_texture_non_power_of_two");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(), HasSubstr("GL_OES_texture_npot"));
  EXPECT_TRUE(info_->feature_flags().npot_ok);
}

TEST_F(FeatureInfoTest, InitializeDXTExtensionGLES2) {
  SetupInitExpectations("GL_EXT_texture_compression_dxt1");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_EXT_texture_compression_dxt1"));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT3_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT5_EXT));
}

TEST_F(FeatureInfoTest, InitializeDXTExtensionGL) {
  SetupInitExpectations("GL_EXT_texture_compression_s3tc");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_EXT_texture_compression_dxt1"));
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_CHROMIUM_texture_compression_dxt3"));
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_CHROMIUM_texture_compression_dxt5"));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT3_EXT));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT5_EXT));
}

TEST_F(FeatureInfoTest, InitializeEXT_texture_format_BGRA8888GLES2) {
  SetupInitExpectations("GL_EXT_texture_format_BGRA8888");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_EXT_texture_format_BGRA8888"));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_BGRA_EXT));
}

TEST_F(FeatureInfoTest, InitializeEXT_texture_format_BGRA8888GL) {
  SetupInitExpectations("GL_EXT_bgra");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_EXT_texture_format_BGRA8888"));
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_EXT_read_format_bgra"));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_TRUE(info_->validators()->read_pixel_format.IsValid(
      GL_BGRA_EXT));
}

TEST_F(FeatureInfoTest, InitializeEXT_texture_format_BGRA8888Apple) {
  SetupInitExpectations("GL_APPLE_texture_format_BGRA8888");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_EXT_texture_format_BGRA8888"));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_BGRA_EXT));
}

TEST_F(FeatureInfoTest, InitializeEXT_read_format_bgra) {
  SetupInitExpectations("GL_EXT_read_format_bgra");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_EXT_read_format_bgra"));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_TRUE(info_->validators()->read_pixel_format.IsValid(
      GL_BGRA_EXT));
}

TEST_F(FeatureInfoTest, InitializeOES_texture_floatGLES2) {
  SetupInitExpectations("GL_OES_texture_float");
  info_->Initialize(NULL);
  EXPECT_FALSE(info_->feature_flags().enable_texture_float_linear);
  EXPECT_FALSE(info_->feature_flags().enable_texture_half_float_linear);
  EXPECT_THAT(info_->extensions(), HasSubstr("GL_OES_texture_float"));
  EXPECT_THAT(info_->extensions(), Not(HasSubstr("GL_OES_texture_half_float")));
  EXPECT_THAT(info_->extensions(),
              Not(HasSubstr("GL_OES_texture_float_linear")));
  EXPECT_THAT(info_->extensions(),
              Not(HasSubstr("GL_OES_texture_half_float_linear")));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_FLOAT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_HALF_FLOAT_OES));
}

TEST_F(FeatureInfoTest, InitializeOES_texture_float_linearGLES2) {
  SetupInitExpectations("GL_OES_texture_float GL_OES_texture_float_linear");
  info_->Initialize(NULL);
  EXPECT_TRUE(info_->feature_flags().enable_texture_float_linear);
  EXPECT_FALSE(info_->feature_flags().enable_texture_half_float_linear);
  EXPECT_THAT(info_->extensions(), HasSubstr("GL_OES_texture_float"));
  EXPECT_THAT(info_->extensions(), Not(HasSubstr("GL_OES_texture_half_float")));
  EXPECT_THAT(info_->extensions(), HasSubstr("GL_OES_texture_float_linear"));
  EXPECT_THAT(info_->extensions(),
              Not(HasSubstr("GL_OES_texture_half_float_linear")));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_FLOAT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_HALF_FLOAT_OES));
}

TEST_F(FeatureInfoTest, InitializeOES_texture_half_floatGLES2) {
  SetupInitExpectations("GL_OES_texture_half_float");
  info_->Initialize(NULL);
  EXPECT_FALSE(info_->feature_flags().enable_texture_float_linear);
  EXPECT_FALSE(info_->feature_flags().enable_texture_half_float_linear);
  EXPECT_THAT(info_->extensions(), Not(HasSubstr("GL_OES_texture_float")));
  EXPECT_THAT(info_->extensions(), HasSubstr("GL_OES_texture_half_float"));
  EXPECT_THAT(info_->extensions(),
              Not(HasSubstr("GL_OES_texture_float_linear")));
  EXPECT_THAT(info_->extensions(),
              Not(HasSubstr("GL_OES_texture_half_float_linear")));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_FLOAT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_HALF_FLOAT_OES));
}

TEST_F(FeatureInfoTest, InitializeOES_texture_half_float_linearGLES2) {
  SetupInitExpectations(
      "GL_OES_texture_half_float GL_OES_texture_half_float_linear");
  info_->Initialize(NULL);
  EXPECT_FALSE(info_->feature_flags().enable_texture_float_linear);
  EXPECT_TRUE(info_->feature_flags().enable_texture_half_float_linear);
  EXPECT_THAT(info_->extensions(), Not(HasSubstr("GL_OES_texture_float")));
  EXPECT_THAT(info_->extensions(), HasSubstr("GL_OES_texture_half_float"));
  EXPECT_THAT(info_->extensions(),
              Not(HasSubstr("GL_OES_texture_float_linear")));
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_OES_texture_half_float_linear"));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_FLOAT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_HALF_FLOAT_OES));
}

TEST_F(FeatureInfoTest, InitializeEXT_framebuffer_multisample) {
  SetupInitExpectations("GL_EXT_framebuffer_multisample");
  info_->Initialize(NULL);
  EXPECT_TRUE(info_->feature_flags().chromium_framebuffer_multisample);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_CHROMIUM_framebuffer_multisample"));
  EXPECT_TRUE(info_->validators()->frame_buffer_target.IsValid(
      GL_READ_FRAMEBUFFER_EXT));
  EXPECT_TRUE(info_->validators()->frame_buffer_target.IsValid(
      GL_DRAW_FRAMEBUFFER_EXT));
  EXPECT_TRUE(info_->validators()->g_l_state.IsValid(
      GL_READ_FRAMEBUFFER_BINDING_EXT));
  EXPECT_TRUE(info_->validators()->g_l_state.IsValid(
      GL_MAX_SAMPLES_EXT));
  EXPECT_TRUE(info_->validators()->render_buffer_parameter.IsValid(
      GL_RENDERBUFFER_SAMPLES_EXT));
}

TEST_F(FeatureInfoTest, InitializeEXT_texture_filter_anisotropic) {
  SetupInitExpectations("GL_EXT_texture_filter_anisotropic");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_EXT_texture_filter_anisotropic"));
  EXPECT_TRUE(info_->validators()->texture_parameter.IsValid(
      GL_TEXTURE_MAX_ANISOTROPY_EXT));
  EXPECT_TRUE(info_->validators()->g_l_state.IsValid(
      GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT));
}

TEST_F(FeatureInfoTest, InitializeEXT_ARB_depth_texture) {
  SetupInitExpectations("GL_ARB_depth_texture");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_GOOGLE_depth_texture"));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
}

TEST_F(FeatureInfoTest, InitializeOES_ARB_depth_texture) {
  SetupInitExpectations("GL_OES_depth_texture");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_GOOGLE_depth_texture"));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
}

TEST_F(FeatureInfoTest, InitializeEXT_packed_depth_stencil) {
  SetupInitExpectations("GL_EXT_packed_depth_stencil");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_OES_packed_depth_stencil"));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(
      GL_DEPTH24_STENCIL8));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
}

TEST_F(FeatureInfoTest, InitializeOES_packed_depth_stencil) {
  SetupInitExpectations("GL_OES_packed_depth_stencil");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_OES_packed_depth_stencil"));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(
      GL_DEPTH24_STENCIL8));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_FALSE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
}

TEST_F(FeatureInfoTest,
       InitializeOES_packed_depth_stencil_and_GL_ARB_depth_texture) {
  SetupInitExpectations("GL_OES_packed_depth_stencil GL_ARB_depth_texture");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_OES_packed_depth_stencil"));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(
      GL_DEPTH24_STENCIL8));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_STENCIL));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(
      GL_DEPTH_STENCIL));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(
      GL_UNSIGNED_INT_24_8));
}

TEST_F(FeatureInfoTest, InitializeOES_depth24) {
  SetupInitExpectations("GL_OES_depth24");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(), HasSubstr("GL_OES_depth24"));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(
      GL_DEPTH_COMPONENT24));
}

TEST_F(FeatureInfoTest, InitializeOES_standard_derivatives) {
  SetupInitExpectations("GL_OES_standard_derivatives");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(), HasSubstr("GL_OES_standard_derivatives"));
  EXPECT_TRUE(info_->feature_flags().oes_standard_derivatives);
  EXPECT_TRUE(info_->validators()->hint_target.IsValid(
      GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES));
  EXPECT_TRUE(info_->validators()->g_l_state.IsValid(
      GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES));
}

TEST_F(FeatureInfoTest, InitializeCHROMIUM_webglsl) {
  SetupInitExpectations("");
  info_->Initialize("GL_CHROMIUM_webglsl");
  EXPECT_THAT(info_->extensions(), HasSubstr("GL_CHROMIUM_webglsl"));
  EXPECT_TRUE(info_->feature_flags().chromium_webglsl);
}

TEST_F(FeatureInfoTest, InitializeOES_rgb8_rgba8) {
  SetupInitExpectations("GL_OES_rgb8_rgba8");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_OES_rgb8_rgba8"));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(
      GL_RGB8_OES));
  EXPECT_TRUE(info_->validators()->render_buffer_format.IsValid(
      GL_RGBA8_OES));
}

TEST_F(FeatureInfoTest, InitializeOES_EGL_image_external) {
  SetupInitExpectations("GL_OES_EGL_image_external");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_OES_EGL_image_external"));
  EXPECT_TRUE(info_->feature_flags().oes_egl_image_external);
  EXPECT_TRUE(info_->validators()->texture_bind_target.IsValid(
      GL_TEXTURE_EXTERNAL_OES));
  EXPECT_TRUE(info_->validators()->get_tex_param_target.IsValid(
      GL_TEXTURE_EXTERNAL_OES));
  EXPECT_TRUE(info_->validators()->texture_parameter.IsValid(
      GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES));
  EXPECT_TRUE(info_->validators()->g_l_state.IsValid(
      GL_TEXTURE_BINDING_EXTERNAL_OES));
}

TEST_F(FeatureInfoTest, InitializeCHROMIUM_stream_texture) {
  SetupInitExpectations("GL_CHROMIUM_stream_texture");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_CHROMIUM_stream_texture"));
  EXPECT_TRUE(info_->feature_flags().chromium_stream_texture);
}

TEST_F(FeatureInfoTest, InitializeEXT_occlusion_query_boolean) {
  SetupInitExpectations("GL_EXT_occlusion_query_boolean");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_EXT_occlusion_query_boolean"));
  EXPECT_TRUE(info_->feature_flags().occlusion_query_boolean);
  EXPECT_FALSE(info_->feature_flags(
      ).use_arb_occlusion_query2_for_occlusion_query_boolean);
  EXPECT_FALSE(info_->feature_flags(
      ).use_arb_occlusion_query_for_occlusion_query_boolean);
}

TEST_F(FeatureInfoTest, InitializeARB_occlusion_query) {
  SetupInitExpectations("GL_ARB_occlusion_query");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_EXT_occlusion_query_boolean"));
  EXPECT_TRUE(info_->feature_flags().occlusion_query_boolean);
  EXPECT_FALSE(info_->feature_flags(
      ).use_arb_occlusion_query2_for_occlusion_query_boolean);
  EXPECT_TRUE(info_->feature_flags(
      ).use_arb_occlusion_query_for_occlusion_query_boolean);
}

TEST_F(FeatureInfoTest, InitializeARB_occlusion_query2) {
  SetupInitExpectations("GL_ARB_occlusion_query2 GL_ARB_occlusion_query2");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_EXT_occlusion_query_boolean"));
  EXPECT_TRUE(info_->feature_flags().occlusion_query_boolean);
  EXPECT_TRUE(info_->feature_flags(
      ).use_arb_occlusion_query2_for_occlusion_query_boolean);
  EXPECT_FALSE(info_->feature_flags(
      ).use_arb_occlusion_query_for_occlusion_query_boolean);
}

TEST_F(FeatureInfoTest, IsIntel) {
  SetupInitExpectationsWithVendor("", "iNTel", "");
  info_->Initialize(NULL);
  EXPECT_TRUE(info_->feature_flags().is_intel);
  EXPECT_FALSE(info_->feature_flags().is_nvidia);
  EXPECT_FALSE(info_->feature_flags().is_amd);

  SetupInitExpectationsWithVendor("", "", "IntEl");
  FeatureInfo::Ref feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  EXPECT_TRUE(feature_info->feature_flags().is_intel);
  EXPECT_FALSE(feature_info->feature_flags().is_nvidia);
  EXPECT_FALSE(feature_info->feature_flags().is_amd);
}

TEST_F(FeatureInfoTest, IsNvidia) {
  SetupInitExpectationsWithVendor("", "nvIdIa", "");
  info_->Initialize(NULL);
  EXPECT_FALSE(info_->feature_flags().is_intel);
  EXPECT_TRUE(info_->feature_flags().is_nvidia);
  EXPECT_FALSE(info_->feature_flags().is_amd);

  SetupInitExpectationsWithVendor("", "", "NViDiA");
  FeatureInfo::Ref feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  EXPECT_FALSE(feature_info->feature_flags().is_intel);
  EXPECT_TRUE(feature_info->feature_flags().is_nvidia);
  EXPECT_FALSE(feature_info->feature_flags().is_amd);
}

TEST_F(FeatureInfoTest, IsAMD) {
  SetupInitExpectationsWithVendor("", "aMd", "");
  info_->Initialize(NULL);
  EXPECT_FALSE(info_->feature_flags().is_intel);
  EXPECT_FALSE(info_->feature_flags().is_nvidia);
  EXPECT_TRUE(info_->feature_flags().is_amd);

  SetupInitExpectationsWithVendor("", "", "AmD");
  FeatureInfo::Ref feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  EXPECT_FALSE(feature_info->feature_flags().is_intel);
  EXPECT_FALSE(feature_info->feature_flags().is_nvidia);
  EXPECT_TRUE(feature_info->feature_flags().is_amd);
}

TEST_F(FeatureInfoTest, IsAMDATI) {
  SetupInitExpectationsWithVendor("", "aTI", "");
  info_->Initialize(NULL);
  EXPECT_FALSE(info_->feature_flags().is_intel);
  EXPECT_FALSE(info_->feature_flags().is_nvidia);
  EXPECT_TRUE(info_->feature_flags().is_amd);

  SetupInitExpectationsWithVendor("", "", "AtI");
  FeatureInfo::Ref feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  EXPECT_FALSE(feature_info->feature_flags().is_intel);
  EXPECT_FALSE(feature_info->feature_flags().is_nvidia);
  EXPECT_TRUE(feature_info->feature_flags().is_amd);
}

}  // namespace gles2
}  // namespace gpu



