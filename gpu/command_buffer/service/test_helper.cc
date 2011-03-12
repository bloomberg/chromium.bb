// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/test_helper.h"

#include "gpu/command_buffer/common/gl_mock.h"
#include "gpu/command_buffer/common/types.h"
#include "gpu/GLES2/gles2_command_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArrayArgument;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace gpu {
namespace gles2 {

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLuint TestHelper::kServiceBlackTexture2dId;
const GLuint TestHelper::kServiceBlackTextureCubemapId;
const GLuint TestHelper::kServiceDefaultTexture2dId;
const GLuint TestHelper::kServiceDefaultTextureCubemapId;

const GLint TestHelper::kMaxRenderbufferSize;
const GLint TestHelper::kMaxTextureSize;
const GLint TestHelper::kMaxCubeMapTextureSize;
const GLint TestHelper::kNumVertexAttribs;
const GLint TestHelper::kNumTextureUnits;
const GLint TestHelper::kMaxTextureImageUnits;
const GLint TestHelper::kMaxVertexTextureImageUnits;
const GLint TestHelper::kMaxFragmentUniformVectors;
const GLint TestHelper::kMaxFragmentUniformComponents;
const GLint TestHelper::kMaxVaryingVectors;
const GLint TestHelper::kMaxVaryingFloats;
const GLint TestHelper::kMaxVertexUniformVectors;
const GLint TestHelper::kMaxVertexUniformComponents;
#endif

void TestHelper::SetupTextureManagerInitExpectations(
    ::gfx::MockGLInterface* gl) {
  static GLuint texture_ids[] = {
    kServiceBlackTexture2dId,
    kServiceDefaultTexture2dId,
    kServiceBlackTextureCubemapId,
    kServiceDefaultTextureCubemapId,
  };
  EXPECT_CALL(*gl, GenTextures(arraysize(texture_ids), _))
      .WillOnce(SetArrayArgument<1>(texture_ids,
                                    texture_ids + arraysize(texture_ids)))
      .RetiresOnSaturation();
  for (int ii = 0; ii < 2; ++ii) {
    EXPECT_CALL(*gl, BindTexture(GL_TEXTURE_2D, texture_ids[ii]))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                               GL_UNSIGNED_BYTE, _))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl, BindTexture(GL_TEXTURE_CUBE_MAP, texture_ids[2 + ii]))
        .Times(1)
        .RetiresOnSaturation();
    static GLenum faces[] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
    };
    for (size_t ii = 0; ii < arraysize(faces); ++ii) {
      EXPECT_CALL(*gl, TexImage2D(faces[ii], 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                                 GL_UNSIGNED_BYTE, _))
          .Times(1)
          .RetiresOnSaturation();
    }
  }
  EXPECT_CALL(*gl, BindTexture(GL_TEXTURE_2D, 0))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, BindTexture(GL_TEXTURE_CUBE_MAP, 0))
      .Times(1)
      .RetiresOnSaturation();
}

void TestHelper::SetupContextGroupInitExpectations(
      ::gfx::MockGLInterface* gl,
      const DisallowedExtensions& disallowed_extensions,
      const char* extensions) {
  InSequence sequence;

  SetupFeatureInfoInitExpectations(gl, extensions);

  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_RENDERBUFFER_SIZE, _))
      .WillOnce(SetArgumentPointee<1>(kMaxRenderbufferSize))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_VERTEX_ATTRIBS, _))
      .WillOnce(SetArgumentPointee<1>(kNumVertexAttribs))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, _))
      .WillOnce(SetArgumentPointee<1>(kNumTextureUnits))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_TEXTURE_SIZE, _))
      .WillOnce(SetArgumentPointee<1>(kMaxTextureSize))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, _))
      .WillOnce(SetArgumentPointee<1>(kMaxCubeMapTextureSize))
      .RetiresOnSaturation();

  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, _))
      .WillOnce(SetArgumentPointee<1>(kMaxTextureImageUnits))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, _))
      .WillOnce(SetArgumentPointee<1>(kMaxVertexTextureImageUnits))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, _))
      .WillOnce(SetArgumentPointee<1>(kMaxFragmentUniformComponents))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_VARYING_FLOATS, _))
      .WillOnce(SetArgumentPointee<1>(kMaxVaryingFloats))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, _))
      .WillOnce(SetArgumentPointee<1>(kMaxVertexUniformComponents))
      .RetiresOnSaturation();

  SetupTextureManagerInitExpectations(gl);
}

void TestHelper::SetupFeatureInfoInitExpectations(
      ::gfx::MockGLInterface* gl, const char* extensions) {
  InSequence sequence;

  EXPECT_CALL(*gl, GetString(GL_EXTENSIONS))
      .WillOnce(Return(reinterpret_cast<const uint8*>(extensions)))
      .RetiresOnSaturation();
}

}  // namespace gles2
}  // namespace gpu

