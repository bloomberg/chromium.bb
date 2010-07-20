// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_group.h"
#include "app/gfx/gl/gl_mock.h"
#include "base/scoped_ptr.h"
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

  void SetupInitExpectations(const char* extensions);

 protected:
  static const GLint kMaxTextureSize = 2048;
  static const GLint kMaxCubeMapTextureSize = 256;
  static const GLint kNumVertexAttribs = 16;
  static const GLint kNumTextureUnits = 8;
  static const GLint kMaxTextureImageUnits = 8;
  static const GLint kMaxVertexTextureImageUnits = 2;
  static const GLint kMaxFragmentUniformVectors = 16;
  static const GLint kMaxVaryingVectors = 8;
  static const GLint kMaxVertexUniformVectors = 128;

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

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLint ContextGroupTest::kMaxTextureSize;
const GLint ContextGroupTest::kMaxCubeMapTextureSize;
const GLint ContextGroupTest::kNumVertexAttribs;
const GLint ContextGroupTest::kNumTextureUnits;
const GLint ContextGroupTest::kMaxTextureImageUnits;
const GLint ContextGroupTest::kMaxVertexTextureImageUnits;
const GLint ContextGroupTest::kMaxFragmentUniformVectors;
const GLint ContextGroupTest::kMaxVaryingVectors;
const GLint ContextGroupTest::kMaxVertexUniformVectors;
#endif

void ContextGroupTest::SetupInitExpectations(const char* extensions) {
  InSequence sequence;

  EXPECT_CALL(*gl_, GetString(GL_EXTENSIONS))
      .WillOnce(Return(reinterpret_cast<const uint8*>(extensions)))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_VERTEX_ATTRIBS, _))
      .WillOnce(SetArgumentPointee<1>(kNumVertexAttribs))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, _))
      .WillOnce(SetArgumentPointee<1>(kNumTextureUnits))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_TEXTURE_SIZE, _))
      .WillOnce(SetArgumentPointee<1>(kMaxTextureSize))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, _))
      .WillOnce(SetArgumentPointee<1>(kMaxCubeMapTextureSize))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, _))
      .WillOnce(SetArgumentPointee<1>(kMaxTextureImageUnits))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, _))
      .WillOnce(SetArgumentPointee<1>(kMaxVertexTextureImageUnits))
      .RetiresOnSaturation();

#if defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)

  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, _))
      .WillOnce(SetArgumentPointee<1>(kMaxFragmentUniformVectors * 4))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_VARYING_VECTORS, _))
      .WillOnce(SetArgumentPointee<1>(kMaxVaryingVectors * 4))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, _))
      .WillOnce(SetArgumentPointee<1>(kMaxVertexUniformVectors * 4))
      .RetiresOnSaturation();

#else  // !defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)

  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, _))
      .WillOnce(SetArgumentPointee<1>(kMaxFragmentUniformVectors))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_VARYING_FLOATS, _))
      .WillOnce(SetArgumentPointee<1>(kMaxVaryingVectors))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, GetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, _))
      .WillOnce(SetArgumentPointee<1>(kMaxVertexUniformVectors))
      .RetiresOnSaturation();

#endif  // !defined(GLES2_GPU_SERVICE_BACKEND_NATIVE_GLES2)
}

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
}

TEST_F(ContextGroupTest, InitializeDXTExtensionGL) {
  SetupInitExpectations("GL_EXT_texture_compression_s3tc");
  group_.Initialize();
  EXPECT_THAT(group_.extensions(),
              HasSubstr("GL_EXT_texture_compression_dxt1"));
  EXPECT_TRUE(group_.validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT));
  EXPECT_TRUE(group_.validators()->compressed_texture_format.IsValid(
      GL_COMPRESSED_RGBA_S3TC_DXT1_EXT));
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
  EXPECT_TRUE(group_.validators()->texture_format.IsValid(
      GL_BGRA_EXT));
  EXPECT_TRUE(group_.validators()->texture_internal_format.IsValid(
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

}  // namespace gles2
}  // namespace gpu


