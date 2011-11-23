// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/test_helper.h"

#include "base/string_tokenizer.h"
#include "gpu/command_buffer/common/gl_mock.h"
#include "gpu/command_buffer/common/types.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <string.h>

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
const GLuint TestHelper::kServiceDefaultTexture2dId;
const GLuint TestHelper::kServiceBlackTextureCubemapId;
const GLuint TestHelper::kServiceDefaultTextureCubemapId;
const GLuint TestHelper::kServiceBlackExternalTextureId;
const GLuint TestHelper::kServiceDefaultExternalTextureId;
const GLuint TestHelper::kServiceBlackRectangleTextureId;
const GLuint TestHelper::kServiceDefaultRectangleTextureId;

const GLint TestHelper::kMaxSamples;
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

void TestHelper::SetupTextureInitializationExpectations(
    ::gfx::MockGLInterface* gl, GLenum target) {
  InSequence sequence;

  bool needs_initialization = (target != GL_TEXTURE_EXTERNAL_OES);
  bool needs_faces = (target == GL_TEXTURE_CUBE_MAP);

  static GLuint texture_2d_ids[] = {
    kServiceBlackTexture2dId,
    kServiceDefaultTexture2dId };
  static GLuint texture_cube_map_ids[] = {
    kServiceBlackTextureCubemapId,
    kServiceDefaultTextureCubemapId };
  static GLuint texture_external_oes_ids[] = {
    kServiceBlackExternalTextureId,
    kServiceDefaultExternalTextureId };
  static GLuint texture_rectangle_arb_ids[] = {
    kServiceBlackRectangleTextureId,
    kServiceDefaultRectangleTextureId };

  const GLuint* texture_ids = NULL;
  switch (target) {
    case GL_TEXTURE_2D:
      texture_ids = &texture_2d_ids[0];
      break;
    case GL_TEXTURE_CUBE_MAP:
      texture_ids = &texture_cube_map_ids[0];
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      texture_ids = &texture_external_oes_ids[0];
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      texture_ids = &texture_rectangle_arb_ids[0];
      break;
    default:
      NOTREACHED();
  }

  int array_size = 2;

  EXPECT_CALL(*gl, GenTextures(array_size, _))
      .WillOnce(SetArrayArgument<1>(texture_ids,
                                    texture_ids + array_size))
          .RetiresOnSaturation();
  for (int ii = 0; ii < array_size; ++ii) {
    EXPECT_CALL(*gl, BindTexture(target, texture_ids[ii]))
        .Times(1)
        .RetiresOnSaturation();
    if (needs_initialization) {
      if (needs_faces) {
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
      } else {
        EXPECT_CALL(*gl, TexImage2D(target, 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                                    GL_UNSIGNED_BYTE, _))
            .Times(1)
            .RetiresOnSaturation();
      }
    }
  }
  EXPECT_CALL(*gl, BindTexture(target, 0))
      .Times(1)
      .RetiresOnSaturation();
}

void TestHelper::SetupTextureManagerInitExpectations(
    ::gfx::MockGLInterface* gl,
    const char* extensions) {
  InSequence sequence;

  SetupTextureInitializationExpectations(gl, GL_TEXTURE_2D);
  SetupTextureInitializationExpectations(gl, GL_TEXTURE_CUBE_MAP);

  bool ext_image_external = false;
  bool arb_texture_rectangle = false;
  CStringTokenizer t(extensions, extensions + strlen(extensions), " ");
  while (t.GetNext()) {
    if (t.token() == "GL_OES_EGL_image_external") {
      ext_image_external = true;
      break;
    }
    if (t.token() == "GL_ARB_texture_rectangle") {
      arb_texture_rectangle = true;
      break;
    }
  }

  if (ext_image_external) {
    SetupTextureInitializationExpectations(gl, GL_TEXTURE_EXTERNAL_OES);
  }
  if (arb_texture_rectangle) {
    SetupTextureInitializationExpectations(gl, GL_TEXTURE_RECTANGLE_ARB);
  }
}

void TestHelper::SetupContextGroupInitExpectations(
      ::gfx::MockGLInterface* gl,
      const DisallowedFeatures& disallowed_features,
      const char* extensions) {
  InSequence sequence;

  SetupFeatureInfoInitExpectations(gl, extensions);

  EXPECT_CALL(*gl, GetIntegerv(GL_MAX_RENDERBUFFER_SIZE, _))
      .WillOnce(SetArgumentPointee<1>(kMaxRenderbufferSize))
      .RetiresOnSaturation();
  if (strstr(extensions, "GL_EXT_framebuffer_multisample")) {
    EXPECT_CALL(*gl, GetIntegerv(GL_MAX_SAMPLES, _))
        .WillOnce(SetArgumentPointee<1>(kMaxSamples))
        .RetiresOnSaturation();
  }
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

#if defined(OS_MACOSX)
  EXPECT_CALL(*gl, GetString(GL_VENDOR))
      .WillOnce(Return(reinterpret_cast<const uint8*>("")))
      .RetiresOnSaturation();
#endif

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

  SetupTextureManagerInitExpectations(gl, extensions);
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

