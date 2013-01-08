// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/feature_info.h"

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"

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

namespace {

struct FormatInfo {
   GLenum format;
   const GLenum* types;
   size_t count;
};

}  // anonymous namespace.

TEST_F(FeatureInfoTest, Basic) {
  // Test it starts off uninitialized.
  EXPECT_FALSE(info_->feature_flags().chromium_framebuffer_multisample);
  EXPECT_FALSE(info_->feature_flags().oes_standard_derivatives);
  EXPECT_FALSE(info_->feature_flags().npot_ok);
  EXPECT_FALSE(info_->feature_flags().enable_texture_float_linear);
  EXPECT_FALSE(info_->feature_flags().enable_texture_half_float_linear);
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
  EXPECT_FALSE(info_->feature_flags().native_vertex_array_object);
  EXPECT_FALSE(info_->workarounds().reverse_point_sprite_coord_origin);
  EXPECT_FALSE(
      info_->workarounds().set_texture_filter_before_generating_mipmap);
  EXPECT_FALSE(info_->workarounds().clear_alpha_in_readpixels);
  EXPECT_EQ(0, info_->workarounds().max_texture_size);
  EXPECT_EQ(0, info_->workarounds().max_cube_map_texture_size);

  // Test good types.
  {
    static const GLenum kAlphaTypes[] = {
        GL_UNSIGNED_BYTE,
    };
    static const GLenum kRGBTypes[] = {
        GL_UNSIGNED_BYTE,
        GL_UNSIGNED_SHORT_5_6_5,
    };
    static const GLenum kRGBATypes[] = {
        GL_UNSIGNED_BYTE,
        GL_UNSIGNED_SHORT_4_4_4_4,
        GL_UNSIGNED_SHORT_5_5_5_1,
    };
    static const GLenum kLuminanceTypes[] = {
        GL_UNSIGNED_BYTE,
    };
    static const GLenum kLuminanceAlphaTypes[] = {
        GL_UNSIGNED_BYTE,
    };
    static const FormatInfo kFormatTypes[] = {
      { GL_ALPHA, kAlphaTypes, arraysize(kAlphaTypes), },
      { GL_RGB, kRGBTypes, arraysize(kRGBTypes), },
      { GL_RGBA, kRGBATypes, arraysize(kRGBATypes), },
      { GL_LUMINANCE, kLuminanceTypes, arraysize(kLuminanceTypes), },
      { GL_LUMINANCE_ALPHA, kLuminanceAlphaTypes,
        arraysize(kLuminanceAlphaTypes), } ,
    };
    for (size_t ii = 0; ii < arraysize(kFormatTypes); ++ii) {
      const FormatInfo& info = kFormatTypes[ii];
      const ValueValidator<GLenum>& validator =
          info_->GetTextureFormatValidator(info.format);
      for (size_t jj = 0; jj < info.count; ++jj) {
        EXPECT_TRUE(validator.IsValid(info.types[jj]));
      }
    }
  }

  // Test some bad types
  {
    static const GLenum kAlphaTypes[] = {
        GL_UNSIGNED_SHORT_5_5_5_1,
        GL_FLOAT,
    };
    static const GLenum kRGBTypes[] = {
        GL_UNSIGNED_SHORT_4_4_4_4,
        GL_FLOAT,
    };
    static const GLenum kRGBATypes[] = {
        GL_UNSIGNED_SHORT_5_6_5,
        GL_FLOAT,
    };
    static const GLenum kLuminanceTypes[] = {
        GL_UNSIGNED_SHORT_4_4_4_4,
        GL_FLOAT,
    };
    static const GLenum kLuminanceAlphaTypes[] = {
        GL_UNSIGNED_SHORT_5_5_5_1,
        GL_FLOAT,
    };
    static const GLenum kBGRATypes[] = {
        GL_UNSIGNED_BYTE,
        GL_UNSIGNED_SHORT_5_6_5,
        GL_FLOAT,
    };
    static const GLenum kDepthTypes[] = {
        GL_UNSIGNED_BYTE,
        GL_UNSIGNED_SHORT,
        GL_UNSIGNED_INT,
        GL_FLOAT,
    };
    static const FormatInfo kFormatTypes[] = {
      { GL_ALPHA, kAlphaTypes, arraysize(kAlphaTypes), },
      { GL_RGB, kRGBTypes, arraysize(kRGBTypes), },
      { GL_RGBA, kRGBATypes, arraysize(kRGBATypes), },
      { GL_LUMINANCE, kLuminanceTypes, arraysize(kLuminanceTypes), },
      { GL_LUMINANCE_ALPHA, kLuminanceAlphaTypes,
        arraysize(kLuminanceAlphaTypes), } ,
      { GL_BGRA_EXT, kBGRATypes, arraysize(kBGRATypes), },
      { GL_DEPTH_COMPONENT, kDepthTypes, arraysize(kDepthTypes), },
    };
    for (size_t ii = 0; ii < arraysize(kFormatTypes); ++ii) {
      const FormatInfo& info = kFormatTypes[ii];
      const ValueValidator<GLenum>& validator =
          info_->GetTextureFormatValidator(info.format);
      for (size_t jj = 0; jj < info.count; ++jj) {
        EXPECT_FALSE(validator.IsValid(info.types[jj]));
      }
    }
  }
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
  EXPECT_THAT(info_->extensions(),
              Not(HasSubstr("GL_OES_compressed_ETC1_RGB8_texture")));
  EXPECT_FALSE(info_->feature_flags().npot_ok);
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT3_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT5_EXT));
  EXPECT_FALSE(info_->validators()->compressed_texture_format.IsValid(
      GL_ETC1_RGB8_OES));
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
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_DEPTH_COMPONENT16));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_DEPTH_COMPONENT32_OES));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_DEPTH24_STENCIL8_OES));
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
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_BGRA_EXT).IsValid(
      GL_UNSIGNED_BYTE));
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
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_BGRA_EXT).IsValid(
      GL_UNSIGNED_BYTE));
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
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_BGRA_EXT).IsValid(
      GL_UNSIGNED_BYTE));
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
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_ALPHA).IsValid(
      GL_FLOAT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_RGB).IsValid(
      GL_FLOAT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_RGBA).IsValid(
      GL_FLOAT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_LUMINANCE).IsValid(
      GL_FLOAT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_LUMINANCE_ALPHA).IsValid(
      GL_FLOAT));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_ALPHA).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_RGB).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_RGBA).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_LUMINANCE).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_LUMINANCE_ALPHA).IsValid(
      GL_HALF_FLOAT_OES));
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
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_ALPHA).IsValid(
      GL_FLOAT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_RGB).IsValid(
      GL_FLOAT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_RGBA).IsValid(
      GL_FLOAT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_LUMINANCE).IsValid(
      GL_FLOAT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_LUMINANCE_ALPHA).IsValid(
      GL_FLOAT));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_ALPHA).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_RGB).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_RGBA).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_LUMINANCE).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_LUMINANCE_ALPHA).IsValid(
      GL_HALF_FLOAT_OES));
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
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_ALPHA).IsValid(
      GL_FLOAT));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_RGB).IsValid(
      GL_FLOAT));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_RGBA).IsValid(
      GL_FLOAT));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_LUMINANCE).IsValid(
      GL_FLOAT));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_LUMINANCE_ALPHA).IsValid(
      GL_FLOAT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_ALPHA).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_RGB).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_RGBA).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_LUMINANCE).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_LUMINANCE_ALPHA).IsValid(
      GL_HALF_FLOAT_OES));
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
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_ALPHA).IsValid(
      GL_FLOAT));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_RGB).IsValid(
      GL_FLOAT));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_RGBA).IsValid(
      GL_FLOAT));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_LUMINANCE).IsValid(
      GL_FLOAT));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_LUMINANCE_ALPHA).IsValid(
      GL_FLOAT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_ALPHA).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_RGB).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_RGBA).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_LUMINANCE).IsValid(
      GL_HALF_FLOAT_OES));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_LUMINANCE_ALPHA).IsValid(
      GL_HALF_FLOAT_OES));
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
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_CHROMIUM_depth_texture"));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_DEPTH_STENCIL));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_DEPTH_COMPONENT).IsValid(
      GL_UNSIGNED_SHORT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_DEPTH_COMPONENT).IsValid(
      GL_UNSIGNED_INT));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_DEPTH_STENCIL).IsValid(
      GL_UNSIGNED_INT_24_8));
}

TEST_F(FeatureInfoTest, InitializeOES_ARB_depth_texture) {
  SetupInitExpectations("GL_OES_depth_texture");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_GOOGLE_depth_texture"));
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_CHROMIUM_depth_texture"));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_DEPTH_STENCIL));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_DEPTH_COMPONENT).IsValid(
      GL_UNSIGNED_SHORT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_DEPTH_COMPONENT).IsValid(
      GL_UNSIGNED_INT));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_DEPTH_STENCIL).IsValid(
      GL_UNSIGNED_INT_24_8));
}

TEST_F(FeatureInfoTest, InitializeANGLE_depth_texture) {
  SetupInitExpectations("GL_ANGLE_depth_texture");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_GOOGLE_depth_texture"));
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_CHROMIUM_depth_texture"));
  EXPECT_THAT(info_->extensions(),
              Not(HasSubstr("GL_ANGLE_depth_texture")));
  EXPECT_TRUE(info_->validators()->texture_internal_format.IsValid(
      GL_DEPTH_COMPONENT));
  EXPECT_TRUE(info_->validators()->texture_format.IsValid(GL_DEPTH_COMPONENT));
  EXPECT_FALSE(info_->validators()->texture_format.IsValid(GL_DEPTH_STENCIL));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_SHORT));
  EXPECT_TRUE(info_->validators()->pixel_type.IsValid(GL_UNSIGNED_INT));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_DEPTH_COMPONENT16));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_DEPTH_COMPONENT32_OES));
  EXPECT_FALSE(info_->validators()->texture_internal_format_storage.IsValid(
      GL_DEPTH24_STENCIL8_OES));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_DEPTH_COMPONENT).IsValid(
      GL_UNSIGNED_SHORT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_DEPTH_COMPONENT).IsValid(
      GL_UNSIGNED_INT));
  EXPECT_FALSE(info_->GetTextureFormatValidator(GL_DEPTH_STENCIL).IsValid(
      GL_UNSIGNED_INT_24_8));
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
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_DEPTH_COMPONENT).IsValid(
      GL_UNSIGNED_SHORT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_DEPTH_COMPONENT).IsValid(
      GL_UNSIGNED_INT));
  EXPECT_TRUE(info_->GetTextureFormatValidator(GL_DEPTH_STENCIL).IsValid(
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

TEST_F(FeatureInfoTest, InitializeOES_compressed_ETC1_RGB8_texture) {
  SetupInitExpectations("GL_OES_compressed_ETC1_RGB8_texture");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_OES_compressed_ETC1_RGB8_texture"));
  EXPECT_TRUE(info_->validators()->compressed_texture_format.IsValid(
      GL_ETC1_RGB8_OES));
  EXPECT_FALSE(info_->validators()->texture_internal_format.IsValid(
      GL_ETC1_RGB8_OES));
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

TEST_F(FeatureInfoTest, InitializeOES_vertex_array_object) {
  SetupInitExpectations("GL_OES_vertex_array_object");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
      HasSubstr("GL_OES_vertex_array_object"));
  EXPECT_TRUE(info_->feature_flags().native_vertex_array_object);
}

TEST_F(FeatureInfoTest, InitializeARB_vertex_array_object) {
  SetupInitExpectations("GL_ARB_vertex_array_object");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
      HasSubstr("GL_OES_vertex_array_object"));
  EXPECT_TRUE(info_->feature_flags().native_vertex_array_object);
}

TEST_F(FeatureInfoTest, InitializeAPPLE_vertex_array_object) {
  SetupInitExpectations("GL_APPLE_vertex_array_object");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
      HasSubstr("GL_OES_vertex_array_object"));
  EXPECT_TRUE(info_->feature_flags().native_vertex_array_object);
}

TEST_F(FeatureInfoTest, InitializeNo_vertex_array_object) {
  SetupInitExpectations("");
  info_->Initialize(NULL);
  // Even if the native extensions are not available the implementation
  // may still emulate the GL_OES_vertex_array_object functionality. In this
  // scenario native_vertex_array_object must be false.
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_OES_vertex_array_object"));
  EXPECT_FALSE(info_->feature_flags().native_vertex_array_object);
}

TEST_F(FeatureInfoTest, InitializeOES_element_index_uint) {
  SetupInitExpectations("GL_OES_element_index_uint");
  info_->Initialize(NULL);
  EXPECT_THAT(info_->extensions(),
              HasSubstr("GL_OES_element_index_uint"));
  EXPECT_TRUE(info_->validators()->index_type.IsValid(GL_UNSIGNED_INT));
}

}  // namespace gles2
}  // namespace gpu
