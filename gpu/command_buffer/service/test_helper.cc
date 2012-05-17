// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/test_helper.h"

#include "base/string_number_conversions.h"
#include "base/string_tokenizer.h"
#include "gpu/command_buffer/common/gl_mock.h"
#include "gpu/command_buffer/common/types.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/program_manager.h"
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
  SetupFeatureInfoInitExpectationsWithVendor(gl, extensions, "", "");
}

void TestHelper::SetupFeatureInfoInitExpectationsWithVendor(
     ::gfx::MockGLInterface* gl,
     const char* extensions,
     const char* vendor,
     const char* renderer) {
  InSequence sequence;

  EXPECT_CALL(*gl, GetString(GL_EXTENSIONS))
      .WillOnce(Return(reinterpret_cast<const uint8*>(extensions)))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetString(GL_VENDOR))
      .WillOnce(Return(reinterpret_cast<const uint8*>(vendor)))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl, GetString(GL_RENDERER))
      .WillOnce(Return(reinterpret_cast<const uint8*>(renderer)))
      .RetiresOnSaturation();
}

void TestHelper::SetupExpectationsForClearingUniforms(
    ::gfx::MockGLInterface* gl, UniformInfo* uniforms, size_t num_uniforms) {
  for (size_t ii = 0; ii < num_uniforms; ++ii) {
    const UniformInfo& info = uniforms[ii];
    switch (info.type) {
    case GL_FLOAT:
      EXPECT_CALL(*gl, Uniform1fv(info.real_location, info.size, _))
          .Times(1)
          .RetiresOnSaturation();
      break;
    case GL_FLOAT_VEC2:
      EXPECT_CALL(*gl, Uniform2fv(info.real_location, info.size, _))
          .Times(1)
          .RetiresOnSaturation();
      break;
    case GL_FLOAT_VEC3:
      EXPECT_CALL(*gl, Uniform3fv(info.real_location, info.size, _))
          .Times(1)
          .RetiresOnSaturation();
      break;
    case GL_FLOAT_VEC4:
      EXPECT_CALL(*gl, Uniform4fv(info.real_location, info.size, _))
          .Times(1)
          .RetiresOnSaturation();
      break;
    case GL_INT:
    case GL_BOOL:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_EXTERNAL_OES:
    case GL_SAMPLER_3D_OES:
    case GL_SAMPLER_2D_RECT_ARB:
      EXPECT_CALL(*gl, Uniform1iv(info.real_location, info.size, _))
          .Times(1)
          .RetiresOnSaturation();
      break;
    case GL_INT_VEC2:
    case GL_BOOL_VEC2:
      EXPECT_CALL(*gl, Uniform2iv(info.real_location, info.size, _))
          .Times(1)
          .RetiresOnSaturation();
      break;
    case GL_INT_VEC3:
    case GL_BOOL_VEC3:
      EXPECT_CALL(*gl, Uniform3iv(info.real_location, info.size, _))
          .Times(1)
          .RetiresOnSaturation();
      break;
    case GL_INT_VEC4:
    case GL_BOOL_VEC4:
      EXPECT_CALL(*gl, Uniform4iv(info.real_location, info.size, _))
          .Times(1)
          .RetiresOnSaturation();
      break;
    case GL_FLOAT_MAT2:
      EXPECT_CALL(*gl, UniformMatrix2fv(
          info.real_location, info.size, false, _))
          .Times(1)
          .RetiresOnSaturation();
      break;
    case GL_FLOAT_MAT3:
      EXPECT_CALL(*gl, UniformMatrix3fv(
          info.real_location, info.size, false, _))
          .Times(1)
          .RetiresOnSaturation();
      break;
    case GL_FLOAT_MAT4:
      EXPECT_CALL(*gl, UniformMatrix4fv(
          info.real_location, info.size, false, _))
          .Times(1)
          .RetiresOnSaturation();
      break;
    default:
      NOTREACHED();
      break;
    }
  }
}

void TestHelper::SetupShader(
    ::gfx::MockGLInterface* gl,
    AttribInfo* attribs, size_t num_attribs,
    UniformInfo* uniforms, size_t num_uniforms,
    GLuint service_id) {
  InSequence s;

  EXPECT_CALL(*gl,
      LinkProgram(service_id))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl,
      GetProgramiv(service_id, GL_LINK_STATUS, _))
      .WillOnce(SetArgumentPointee<2>(1))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl,
      GetProgramiv(service_id, GL_INFO_LOG_LENGTH, _))
      .WillOnce(SetArgumentPointee<2>(0))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl,
      GetProgramiv(service_id, GL_ACTIVE_ATTRIBUTES, _))
      .WillOnce(SetArgumentPointee<2>(num_attribs))
      .RetiresOnSaturation();
  size_t max_attrib_len = 0;
  for (size_t ii = 0; ii < num_attribs; ++ii) {
    size_t len = strlen(attribs[ii].name) + 1;
    max_attrib_len = std::max(max_attrib_len, len);
  }
  EXPECT_CALL(*gl,
      GetProgramiv(service_id, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, _))
      .WillOnce(SetArgumentPointee<2>(max_attrib_len))
      .RetiresOnSaturation();
  for (size_t ii = 0; ii < num_attribs; ++ii) {
    const AttribInfo& info = attribs[ii];
    EXPECT_CALL(*gl,
        GetActiveAttrib(service_id, ii,
                        max_attrib_len, _, _, _, _))
        .WillOnce(DoAll(
            SetArgumentPointee<3>(strlen(info.name)),
            SetArgumentPointee<4>(info.size),
            SetArgumentPointee<5>(info.type),
            SetArrayArgument<6>(info.name,
                                info.name + strlen(info.name) + 1)))
        .RetiresOnSaturation();
    if (!ProgramManager::IsInvalidPrefix(info.name, strlen(info.name))) {
      EXPECT_CALL(*gl, GetAttribLocation(service_id, StrEq(info.name)))
          .WillOnce(Return(info.location))
          .RetiresOnSaturation();
    }
  }
  EXPECT_CALL(*gl,
      GetProgramiv(service_id, GL_ACTIVE_UNIFORMS, _))
      .WillOnce(SetArgumentPointee<2>(num_uniforms))
      .RetiresOnSaturation();
  size_t max_uniform_len = 0;
  for (size_t ii = 0; ii < num_uniforms; ++ii) {
    size_t len = strlen(uniforms[ii].name) + 1;
    max_uniform_len = std::max(max_uniform_len, len);
  }
  EXPECT_CALL(*gl,
      GetProgramiv(service_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, _))
      .WillOnce(SetArgumentPointee<2>(max_uniform_len))
      .RetiresOnSaturation();
  for (size_t ii = 0; ii < num_uniforms; ++ii) {
    const UniformInfo& info = uniforms[ii];
    EXPECT_CALL(*gl,
        GetActiveUniform(service_id, ii,
                         max_uniform_len, _, _, _, _))
        .WillOnce(DoAll(
            SetArgumentPointee<3>(strlen(info.name)),
            SetArgumentPointee<4>(info.size),
            SetArgumentPointee<5>(info.type),
            SetArrayArgument<6>(info.name,
                                info.name + strlen(info.name) + 1)))
        .RetiresOnSaturation();
    if (!ProgramManager::IsInvalidPrefix(info.name, strlen(info.name))) {
      EXPECT_CALL(*gl, GetUniformLocation(service_id, StrEq(info.name)))
          .WillOnce(Return(info.real_location))
          .RetiresOnSaturation();
      if (info.size > 1) {
        std::string base_name = info.name;
        size_t array_pos = base_name.rfind("[0]");
        if (base_name.size() > 3 && array_pos == base_name.size() - 3) {
          base_name = base_name.substr(0, base_name.size() - 3);
        }
        for (GLsizei jj = 1; jj < info.size; ++jj) {
          std::string element_name(
              std::string(base_name) + "[" + base::IntToString(jj) + "]");
          EXPECT_CALL(*gl, GetUniformLocation(service_id, StrEq(element_name)))
              .WillOnce(Return(info.real_location + jj * 2))
              .RetiresOnSaturation();
        }
      }
    }
  }
}


}  // namespace gles2
}  // namespace gpu

