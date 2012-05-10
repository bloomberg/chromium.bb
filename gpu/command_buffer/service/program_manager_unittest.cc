// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_manager.h"

#include <algorithm>

#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "gpu/command_buffer/common/gl_mock.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/mocks.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::gfx::MockGLInterface;
using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArrayArgument;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace gpu {
namespace gles2 {

class ProgramManagerTest : public testing::Test {
 public:
  ProgramManagerTest() { }
  ~ProgramManagerTest() {
    manager_.Destroy(false);
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
  }

  virtual void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  ProgramManager manager_;
};

TEST_F(ProgramManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;
  // Check we can create program.
  manager_.CreateProgramInfo(kClient1Id, kService1Id);
  // Check program got created.
  ProgramManager::ProgramInfo* info1 = manager_.GetProgramInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  GLuint client_id = 0;
  EXPECT_TRUE(manager_.GetClientId(info1->service_id(), &client_id));
  EXPECT_EQ(kClient1Id, client_id);
  // Check we get nothing for a non-existent program.
  EXPECT_TRUE(manager_.GetProgramInfo(kClient2Id) == NULL);
}

TEST_F(ProgramManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create program.
  ProgramManager::ProgramInfo* info0 = manager_.CreateProgramInfo(
      kClient1Id, kService1Id);
  ASSERT_TRUE(info0 != NULL);
  // Check program got created.
  ProgramManager::ProgramInfo* info1 = manager_.GetProgramInfo(kClient1Id);
  ASSERT_EQ(info0, info1);
  EXPECT_CALL(*gl_, DeleteProgram(kService1Id))
      .Times(1)
      .RetiresOnSaturation();
  manager_.Destroy(true);
  // Check the resources were released.
  info1 = manager_.GetProgramInfo(kClient1Id);
  ASSERT_TRUE(info1 == NULL);
}

TEST_F(ProgramManagerTest, DeleteBug) {
  ShaderManager shader_manager;
  const GLuint kClient1Id = 1;
  const GLuint kClient2Id = 2;
  const GLuint kService1Id = 11;
  const GLuint kService2Id = 12;
  // Check we can create program.
  ProgramManager::ProgramInfo::Ref info1(
      manager_.CreateProgramInfo(kClient1Id, kService1Id));
  ProgramManager::ProgramInfo::Ref info2(
      manager_.CreateProgramInfo(kClient2Id, kService2Id));
  // Check program got created.
  ASSERT_TRUE(info1);
  ASSERT_TRUE(info2);
  manager_.UseProgram(info1);
  manager_.MarkAsDeleted(&shader_manager, info1);
  //  Program will be deleted when last ref is released.
  EXPECT_CALL(*gl_, DeleteProgram(kService2Id))
      .Times(1)
      .RetiresOnSaturation();
  manager_.MarkAsDeleted(&shader_manager, info2);
  EXPECT_TRUE(manager_.IsOwned(info1));
  EXPECT_FALSE(manager_.IsOwned(info2));
}

TEST_F(ProgramManagerTest, ProgramInfo) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create program.
  ProgramManager::ProgramInfo* info1 = manager_.CreateProgramInfo(
      kClient1Id, kService1Id);
  ASSERT_TRUE(info1);
  EXPECT_EQ(kService1Id, info1->service_id());
  EXPECT_FALSE(info1->InUse());
  EXPECT_FALSE(info1->IsValid());
  EXPECT_FALSE(info1->IsDeleted());
  EXPECT_FALSE(info1->CanLink());
  EXPECT_TRUE(info1->log_info() == NULL);
}

TEST_F(ProgramManagerTest, SwizzleLocation) {
  GLint power = 1;
  for (GLint p = 0; p < 5; ++p, power *= 10) {
    GLint limit = power * 20 + 1;
    for (GLint ii = -limit; ii < limit; ii += power) {
      GLint s = manager_.SwizzleLocation(ii);
      EXPECT_EQ(ii, manager_.UnswizzleLocation(s));
    }
  }
}

class ProgramManagerWithShaderTest : public testing::Test {
 public:
  ProgramManagerWithShaderTest()
      : program_info_(NULL) {
  }

  ~ProgramManagerWithShaderTest() {
    manager_.Destroy(false);
    shader_manager_.Destroy(false);
  }

  static const GLint kNumVertexAttribs = 16;

  static const GLuint kClientProgramId = 123;
  static const GLuint kServiceProgramId = 456;
  static const GLuint kVertexShaderClientId = 201;
  static const GLuint kFragmentShaderClientId = 202;
  static const GLuint kVertexShaderServiceId = 301;
  static const GLuint kFragmentShaderServiceId = 302;

  static const char* kAttrib1Name;
  static const char* kAttrib2Name;
  static const char* kAttrib3Name;
  static const GLint kAttrib1Size = 1;
  static const GLint kAttrib2Size = 1;
  static const GLint kAttrib3Size = 1;
  static const GLint kAttrib1Location = 0;
  static const GLint kAttrib2Location = 1;
  static const GLint kAttrib3Location = 2;
  static const GLenum kAttrib1Type = GL_FLOAT_VEC4;
  static const GLenum kAttrib2Type = GL_FLOAT_VEC2;
  static const GLenum kAttrib3Type = GL_FLOAT_VEC3;
  static const GLint kInvalidAttribLocation = 30;
  static const GLint kBadAttribIndex = kNumVertexAttribs;

  static const char* kUniform1Name;
  static const char* kUniform2Name;
  static const char* kUniform3BadName;
  static const char* kUniform3GoodName;
  static const GLint kUniform1Size = 1;
  static const GLint kUniform2Size = 3;
  static const GLint kUniform3Size = 2;
  static const GLint kUniform1FakeLocation = 0;  // These are hard coded
  static const GLint kUniform2FakeLocation = 1;  // to match
  static const GLint kUniform3FakeLocation = 2;  // ProgramManager.
  static const GLint kUniform1RealLocation = 11;
  static const GLint kUniform2RealLocation = 22;
  static const GLint kUniform3RealLocation = 33;
  static const GLenum kUniform1Type = GL_FLOAT_VEC4;
  static const GLenum kUniform2Type = GL_INT_VEC2;
  static const GLenum kUniform3Type = GL_FLOAT_VEC3;
  static const GLint kInvalidUniformLocation = 30;
  static const GLint kBadUniformIndex = 1000;

  static const size_t kNumAttribs;
  static const size_t kNumUniforms;

 protected:
  struct AttribInfo {
    const char* name;
    GLint size;
    GLenum type;
    GLint location;
  };

  struct UniformInfo {
    const char* name;
    const char* good_name;
    GLint size;
    GLenum type;
    GLint fake_location;
    GLint real_location;
  };

  virtual void SetUp() {
    gl_.reset(new StrictMock<gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());

    SetupDefaultShaderExpectations();

    ShaderManager::ShaderInfo* vertex_shader = shader_manager_.CreateShaderInfo(
        kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
    ShaderManager::ShaderInfo* fragment_shader =
        shader_manager_.CreateShaderInfo(
            kFragmentShaderClientId, kFragmentShaderServiceId,
            GL_FRAGMENT_SHADER);
    ASSERT_TRUE(vertex_shader != NULL);
    ASSERT_TRUE(fragment_shader != NULL);
    vertex_shader->SetStatus(true, NULL, NULL);
    fragment_shader->SetStatus(true, NULL, NULL);

    program_info_ = manager_.CreateProgramInfo(
        kClientProgramId, kServiceProgramId);
    ASSERT_TRUE(program_info_ != NULL);

    program_info_->AttachShader(&shader_manager_, vertex_shader);
    program_info_->AttachShader(&shader_manager_, fragment_shader);
    program_info_->Link();
  }

  void SetupShader(AttribInfo* attribs, size_t num_attribs,
                   UniformInfo* uniforms, size_t num_uniforms,
                   GLuint service_id) {
    InSequence s;

    EXPECT_CALL(*gl_,
        LinkProgram(service_id))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetProgramiv(service_id, GL_LINK_STATUS, _))
        .WillOnce(SetArgumentPointee<2>(1))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetProgramiv(service_id, GL_INFO_LOG_LENGTH, _))
        .WillOnce(SetArgumentPointee<2>(0))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetProgramiv(service_id, GL_ACTIVE_ATTRIBUTES, _))
        .WillOnce(SetArgumentPointee<2>(num_attribs))
        .RetiresOnSaturation();
    size_t max_attrib_len = 0;
    for (size_t ii = 0; ii < num_attribs; ++ii) {
      size_t len = strlen(attribs[ii].name) + 1;
      max_attrib_len = std::max(max_attrib_len, len);
    }
    EXPECT_CALL(*gl_,
        GetProgramiv(service_id, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, _))
        .WillOnce(SetArgumentPointee<2>(max_attrib_len))
        .RetiresOnSaturation();
    for (size_t ii = 0; ii < num_attribs; ++ii) {
      const AttribInfo& info = attribs[ii];
      EXPECT_CALL(*gl_,
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
        EXPECT_CALL(*gl_, GetAttribLocation(service_id,
                                            StrEq(info.name)))
            .WillOnce(Return(info.location))
            .RetiresOnSaturation();
      }
    }
    EXPECT_CALL(*gl_,
        GetProgramiv(service_id, GL_ACTIVE_UNIFORMS, _))
        .WillOnce(SetArgumentPointee<2>(num_uniforms))
        .RetiresOnSaturation();
    size_t max_uniform_len = 0;
    for (size_t ii = 0; ii < num_uniforms; ++ii) {
      size_t len = strlen(uniforms[ii].name) + 1;
      max_uniform_len = std::max(max_uniform_len, len);
    }
    EXPECT_CALL(*gl_,
        GetProgramiv(service_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, _))
        .WillOnce(SetArgumentPointee<2>(max_uniform_len))
        .RetiresOnSaturation();
    for (size_t ii = 0; ii < num_uniforms; ++ii) {
      const UniformInfo& info = uniforms[ii];
      EXPECT_CALL(*gl_,
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
        EXPECT_CALL(*gl_, GetUniformLocation(service_id,
                                             StrEq(info.name)))
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
            EXPECT_CALL(*gl_, GetUniformLocation(service_id,
                                                 StrEq(element_name)))
                .WillOnce(Return(info.real_location + jj * 2))
                .RetiresOnSaturation();
          }
        }
      }
    }
  }

  void SetupDefaultShaderExpectations() {
    SetupShader(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
                kServiceProgramId);
  }

  void SetupExpectationsForClearingUniforms(
      UniformInfo* uniforms, size_t num_uniforms) {
    for (size_t ii = 0; ii < num_uniforms; ++ii) {
      const UniformInfo& info = uniforms[ii];
      switch (info.type) {
      case GL_FLOAT:
        EXPECT_CALL(*gl_, Uniform1fv(info.real_location, info.size, _))
            .Times(1)
            .RetiresOnSaturation();
        break;
      case GL_FLOAT_VEC2:
        EXPECT_CALL(*gl_, Uniform2fv(info.real_location, info.size, _))
            .Times(1)
            .RetiresOnSaturation();
        break;
      case GL_FLOAT_VEC3:
        EXPECT_CALL(*gl_, Uniform3fv(info.real_location, info.size, _))
            .Times(1)
            .RetiresOnSaturation();
        break;
      case GL_FLOAT_VEC4:
        EXPECT_CALL(*gl_, Uniform4fv(info.real_location, info.size, _))
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
        EXPECT_CALL(*gl_, Uniform1iv(info.real_location, info.size, _))
            .Times(1)
            .RetiresOnSaturation();
        break;
      case GL_INT_VEC2:
      case GL_BOOL_VEC2:
        EXPECT_CALL(*gl_, Uniform2iv(info.real_location, info.size, _))
            .Times(1)
            .RetiresOnSaturation();
        break;
      case GL_INT_VEC3:
      case GL_BOOL_VEC3:
        EXPECT_CALL(*gl_, Uniform3iv(info.real_location, info.size, _))
            .Times(1)
            .RetiresOnSaturation();
        break;
      case GL_INT_VEC4:
      case GL_BOOL_VEC4:
        EXPECT_CALL(*gl_, Uniform4iv(info.real_location, info.size, _))
            .Times(1)
            .RetiresOnSaturation();
        break;
      case GL_FLOAT_MAT2:
        EXPECT_CALL(*gl_, UniformMatrix2fv(
            info.real_location, info.size, false, _))
            .Times(1)
            .RetiresOnSaturation();
        break;
      case GL_FLOAT_MAT3:
        EXPECT_CALL(*gl_, UniformMatrix3fv(
            info.real_location, info.size, false, _))
            .Times(1)
            .RetiresOnSaturation();
        break;
      case GL_FLOAT_MAT4:
        EXPECT_CALL(*gl_, UniformMatrix4fv(
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

  virtual void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
  }

  // Return true if link status matches expected_link_status
  bool LinkAsExpected(ProgramManager::ProgramInfo* program_info,
                      bool expected_link_status) {
    GLuint service_id = program_info->service_id();
    if (expected_link_status) {
      SetupShader(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
                  service_id);
    }
    program_info->Link();
    GLint link_status;
    program_info->GetProgramiv(GL_LINK_STATUS, &link_status);
    return (static_cast<bool>(link_status) == expected_link_status);
  }

  static AttribInfo kAttribs[];
  static UniformInfo kUniforms[];

  scoped_ptr<StrictMock<gfx::MockGLInterface> > gl_;

  ProgramManager manager_;
  ProgramManager::ProgramInfo* program_info_;
  ShaderManager shader_manager_;
};

ProgramManagerWithShaderTest::AttribInfo
    ProgramManagerWithShaderTest::kAttribs[] = {
  { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
  { kAttrib2Name, kAttrib2Size, kAttrib2Type, kAttrib2Location, },
  { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLint ProgramManagerWithShaderTest::kNumVertexAttribs;
const GLuint ProgramManagerWithShaderTest::kClientProgramId;
const GLuint ProgramManagerWithShaderTest::kServiceProgramId;
const GLuint ProgramManagerWithShaderTest::kVertexShaderClientId;
const GLuint ProgramManagerWithShaderTest::kFragmentShaderClientId;
const GLuint ProgramManagerWithShaderTest::kVertexShaderServiceId;
const GLuint ProgramManagerWithShaderTest::kFragmentShaderServiceId;
const GLint ProgramManagerWithShaderTest::kAttrib1Size;
const GLint ProgramManagerWithShaderTest::kAttrib2Size;
const GLint ProgramManagerWithShaderTest::kAttrib3Size;
const GLint ProgramManagerWithShaderTest::kAttrib1Location;
const GLint ProgramManagerWithShaderTest::kAttrib2Location;
const GLint ProgramManagerWithShaderTest::kAttrib3Location;
const GLenum ProgramManagerWithShaderTest::kAttrib1Type;
const GLenum ProgramManagerWithShaderTest::kAttrib2Type;
const GLenum ProgramManagerWithShaderTest::kAttrib3Type;
const GLint ProgramManagerWithShaderTest::kInvalidAttribLocation;
const GLint ProgramManagerWithShaderTest::kBadAttribIndex;
const GLint ProgramManagerWithShaderTest::kUniform1Size;
const GLint ProgramManagerWithShaderTest::kUniform2Size;
const GLint ProgramManagerWithShaderTest::kUniform3Size;
const GLint ProgramManagerWithShaderTest::kUniform1FakeLocation;
const GLint ProgramManagerWithShaderTest::kUniform2FakeLocation;
const GLint ProgramManagerWithShaderTest::kUniform3FakeLocation;
const GLint ProgramManagerWithShaderTest::kUniform1RealLocation;
const GLint ProgramManagerWithShaderTest::kUniform2RealLocation;
const GLint ProgramManagerWithShaderTest::kUniform3RealLocation;
const GLenum ProgramManagerWithShaderTest::kUniform1Type;
const GLenum ProgramManagerWithShaderTest::kUniform2Type;
const GLenum ProgramManagerWithShaderTest::kUniform3Type;
const GLint ProgramManagerWithShaderTest::kInvalidUniformLocation;
const GLint ProgramManagerWithShaderTest::kBadUniformIndex;
#endif

const size_t ProgramManagerWithShaderTest::kNumAttribs =
    arraysize(ProgramManagerWithShaderTest::kAttribs);

ProgramManagerWithShaderTest::UniformInfo
    ProgramManagerWithShaderTest::kUniforms[] = {
  { kUniform1Name,
    kUniform1Name,
    kUniform1Size,
    kUniform1Type,
    kUniform1FakeLocation,
    kUniform1RealLocation,
  },
  { kUniform2Name,
    kUniform2Name,
    kUniform2Size,
    kUniform2Type,
    kUniform2FakeLocation,
    kUniform2RealLocation,
  },
  { kUniform3BadName,
    kUniform3GoodName,
    kUniform3Size,
    kUniform3Type,
    kUniform3FakeLocation,
    kUniform3RealLocation,
  },
};

const size_t ProgramManagerWithShaderTest::kNumUniforms =
    arraysize(ProgramManagerWithShaderTest::kUniforms);

const char* ProgramManagerWithShaderTest::kAttrib1Name = "attrib1";
const char* ProgramManagerWithShaderTest::kAttrib2Name = "attrib2";
const char* ProgramManagerWithShaderTest::kAttrib3Name = "attrib3";
const char* ProgramManagerWithShaderTest::kUniform1Name = "uniform1";
// Correctly has array spec.
const char* ProgramManagerWithShaderTest::kUniform2Name = "uniform2[0]";
// Incorrectly missing array spec.
const char* ProgramManagerWithShaderTest::kUniform3BadName = "uniform3";
const char* ProgramManagerWithShaderTest::kUniform3GoodName = "uniform3[0]";

TEST_F(ProgramManagerWithShaderTest, GetAttribInfos) {
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  const ProgramManager::ProgramInfo::AttribInfoVector& infos =
      program_info->GetAttribInfos();
  ASSERT_EQ(kNumAttribs, infos.size());
  for (size_t ii = 0; ii < kNumAttribs; ++ii) {
    const ProgramManager::ProgramInfo::VertexAttribInfo& info = infos[ii];
    const AttribInfo& expected = kAttribs[ii];
    EXPECT_EQ(expected.size, info.size);
    EXPECT_EQ(expected.type, info.type);
    EXPECT_EQ(expected.location, info.location);
    EXPECT_STREQ(expected.name, info.name.c_str());
  }
}

TEST_F(ProgramManagerWithShaderTest, GetAttribInfo) {
  const GLint kValidIndex = 1;
  const GLint kInvalidIndex = 1000;
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  const ProgramManager::ProgramInfo::VertexAttribInfo* info =
      program_info->GetAttribInfo(kValidIndex);
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(kAttrib2Size, info->size);
  EXPECT_EQ(kAttrib2Type, info->type);
  EXPECT_EQ(kAttrib2Location, info->location);
  EXPECT_STREQ(kAttrib2Name, info->name.c_str());
  EXPECT_TRUE(program_info->GetAttribInfo(kInvalidIndex) == NULL);
}

TEST_F(ProgramManagerWithShaderTest, GetAttribLocation) {
  const char* kInvalidName = "foo";
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_EQ(kAttrib2Location, program_info->GetAttribLocation(kAttrib2Name));
  EXPECT_EQ(-1, program_info->GetAttribLocation(kInvalidName));
}

TEST_F(ProgramManagerWithShaderTest, GetUniformInfo) {
  const GLint kInvalidIndex = 1000;
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  const ProgramManager::ProgramInfo::UniformInfo* info =
      program_info->GetUniformInfo(0);
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(kUniform1Size, info->size);
  EXPECT_EQ(kUniform1Type, info->type);
  EXPECT_EQ(kUniform1RealLocation, info->element_locations[0]);
  EXPECT_STREQ(kUniform1Name, info->name.c_str());
  info = program_info->GetUniformInfo(1);
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(kUniform2Size, info->size);
  EXPECT_EQ(kUniform2Type, info->type);
  EXPECT_EQ(kUniform2RealLocation, info->element_locations[0]);
  EXPECT_STREQ(kUniform2Name, info->name.c_str());
  info = program_info->GetUniformInfo(2);
  // We emulate certain OpenGL drivers by supplying the name without
  // the array spec. Our implementation should correctly add the required spec.
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(kUniform3Size, info->size);
  EXPECT_EQ(kUniform3Type, info->type);
  EXPECT_EQ(kUniform3RealLocation, info->element_locations[0]);
  EXPECT_STREQ(kUniform3GoodName, info->name.c_str());
  EXPECT_TRUE(program_info->GetUniformInfo(kInvalidIndex) == NULL);
}

TEST_F(ProgramManagerWithShaderTest, AttachDetachShader) {
  static const GLuint kClientProgramId = 124;
  static const GLuint kServiceProgramId = 457;
  ProgramManager::ProgramInfo* program_info = manager_.CreateProgramInfo(
      kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_FALSE(program_info->CanLink());
  const GLuint kVShaderClientId = 2001;
  const GLuint kFShaderClientId = 2002;
  const GLuint kVShaderServiceId = 3001;
  const GLuint kFShaderServiceId = 3002;
  ShaderManager::ShaderInfo* vshader = shader_manager_.CreateShaderInfo(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, "", NULL);
  ShaderManager::ShaderInfo* fshader = shader_manager_.CreateShaderInfo(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != NULL);
  fshader->SetStatus(true, "", NULL);
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, vshader));
  EXPECT_FALSE(program_info->CanLink());
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, fshader));
  EXPECT_TRUE(program_info->CanLink());
  program_info->DetachShader(&shader_manager_, vshader);
  EXPECT_FALSE(program_info->CanLink());
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program_info->CanLink());
  program_info->DetachShader(&shader_manager_, fshader);
  EXPECT_FALSE(program_info->CanLink());
  EXPECT_FALSE(program_info->AttachShader(&shader_manager_, vshader));
  EXPECT_FALSE(program_info->CanLink());
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, fshader));
  EXPECT_TRUE(program_info->CanLink());
  vshader->SetStatus(false, "", NULL);
  EXPECT_FALSE(program_info->CanLink());
  vshader->SetStatus(true, "", NULL);
  EXPECT_TRUE(program_info->CanLink());
  fshader->SetStatus(false, "", NULL);
  EXPECT_FALSE(program_info->CanLink());
  fshader->SetStatus(true, "", NULL);
  EXPECT_TRUE(program_info->CanLink());
  EXPECT_TRUE(program_info->DetachShader(&shader_manager_, fshader));
  EXPECT_FALSE(program_info->DetachShader(&shader_manager_, fshader));
}

TEST_F(ProgramManagerWithShaderTest, GetUniformFakeLocation) {
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_EQ(kUniform1FakeLocation,
            program_info->GetUniformFakeLocation(kUniform1Name));
  EXPECT_EQ(kUniform2FakeLocation,
            program_info->GetUniformFakeLocation(kUniform2Name));
  EXPECT_EQ(kUniform3FakeLocation,
             program_info->GetUniformFakeLocation(kUniform3BadName));
  // Check we can get uniform2 as "uniform2" even though the name is
  // "uniform2[0]"
  EXPECT_EQ(kUniform2FakeLocation,
            program_info->GetUniformFakeLocation("uniform2"));
  // Check we can get uniform3 as "uniform3[0]" even though we simulated GL
  // returning "uniform3"
  EXPECT_EQ(kUniform3FakeLocation,
            program_info->GetUniformFakeLocation(kUniform3GoodName));
  // Check that we can get the locations of the array elements > 1
  EXPECT_EQ(ProgramManager::ProgramInfo::GetFakeLocation(
              kUniform2FakeLocation, 1),
            program_info->GetUniformFakeLocation("uniform2[1]"));
  EXPECT_EQ(ProgramManager::ProgramInfo::GetFakeLocation(
                kUniform2FakeLocation, 2),
            program_info->GetUniformFakeLocation("uniform2[2]"));
  EXPECT_EQ(-1, program_info->GetUniformFakeLocation("uniform2[3]"));
  EXPECT_EQ(ProgramManager::ProgramInfo::GetFakeLocation(
                kUniform3FakeLocation, 1),
            program_info->GetUniformFakeLocation("uniform3[1]"));
  EXPECT_EQ(-1, program_info->GetUniformFakeLocation("uniform3[2]"));
}

TEST_F(ProgramManagerWithShaderTest, GetUniformInfoByFakeLocation) {
  const GLint kInvalidLocation = 1234;
  const ProgramManager::ProgramInfo::UniformInfo* info;
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  GLint real_location = -1;
  GLint array_index = -1;
  ASSERT_TRUE(program_info != NULL);
  info = program_info->GetUniformInfoByFakeLocation(
      kUniform2FakeLocation, &real_location, &array_index);
  EXPECT_EQ(kUniform2RealLocation, real_location);
  EXPECT_EQ(0, array_index);
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(kUniform2Type, info->type);
  real_location = -1;
  array_index = -1;
  info = program_info->GetUniformInfoByFakeLocation(
      kInvalidLocation, &real_location, &array_index);
  EXPECT_TRUE(info == NULL);
  EXPECT_EQ(-1, real_location);
  EXPECT_EQ(-1, array_index);
  GLint loc = program_info->GetUniformFakeLocation("uniform2[2]");
  info = program_info->GetUniformInfoByFakeLocation(
      loc, &real_location, &array_index);
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(kUniform2RealLocation + 2 * 2, real_location);
  EXPECT_EQ(2, array_index);
}

// Some GL drivers incorrectly return gl_DepthRange and possibly other uniforms
// that start with "gl_". Our implementation catches these and does not allow
// them back to client.
TEST_F(ProgramManagerWithShaderTest, GLDriverReturnsGLUnderscoreUniform) {
  static const char* kUniform2Name = "gl_longNameWeCanCheckFor";
  static ProgramManagerWithShaderTest::UniformInfo kUniforms[] = {
    { kUniform1Name,
      kUniform1Name,
      kUniform1Size,
      kUniform1Type,
      kUniform1FakeLocation,
      kUniform1RealLocation,
    },
    { kUniform2Name,
      kUniform2Name,
      kUniform2Size,
      kUniform2Type,
      kUniform2FakeLocation,
      kUniform2RealLocation,
    },
    { kUniform3BadName,
      kUniform3GoodName,
      kUniform3Size,
      kUniform3Type,
      kUniform3FakeLocation,
      kUniform3RealLocation,
    },
  };
  const size_t kNumUniforms = arraysize(kUniforms);
  static const GLuint kClientProgramId = 1234;
  static const GLuint kServiceProgramId = 5679;
  const GLuint kVShaderClientId = 2001;
  const GLuint kFShaderClientId = 2002;
  const GLuint kVShaderServiceId = 3001;
  const GLuint kFShaderServiceId = 3002;
  SetupShader(
      kAttribs, kNumAttribs, kUniforms, kNumUniforms, kServiceProgramId);
  ShaderManager::ShaderInfo* vshader = shader_manager_.CreateShaderInfo(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, "", NULL);
  ShaderManager::ShaderInfo* fshader = shader_manager_.CreateShaderInfo(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != NULL);
  fshader->SetStatus(true, "", NULL);
  ProgramManager::ProgramInfo* program_info =
      manager_.CreateProgramInfo(kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, fshader));
  program_info->Link();
  GLint value = 0;
  program_info->GetProgramiv(GL_ACTIVE_ATTRIBUTES, &value);
  EXPECT_EQ(3, value);
  // Check that we skipped the "gl_" uniform.
  program_info->GetProgramiv(GL_ACTIVE_UNIFORMS, &value);
  EXPECT_EQ(2, value);
  // Check that our max length adds room for the array spec and is not as long
  // as the "gl_" uniform we skipped.
  // +4u is to account for "gl_" and NULL terminator.
  program_info->GetProgramiv(GL_ACTIVE_UNIFORM_MAX_LENGTH, &value);
  EXPECT_EQ(strlen(kUniform3BadName) + 4u, static_cast<size_t>(value));
}

// Some GL drivers incorrectly return the wrong type. For example they return
// GL_FLOAT_VEC2 when they should return GL_FLOAT_MAT2. Check we handle this.
TEST_F(ProgramManagerWithShaderTest, GLDriverReturnsWrongTypeInfo) {
  static GLenum kAttrib2BadType = GL_FLOAT_VEC2;
  static GLenum kAttrib2GoodType = GL_FLOAT_MAT2;
  static GLenum kUniform2BadType = GL_FLOAT_VEC3;
  static GLenum kUniform2GoodType = GL_FLOAT_MAT3;
  MockShaderTranslator shader_translator;
  ShaderTranslator::VariableMap attrib_map;
  ShaderTranslator::VariableMap uniform_map;
  attrib_map[kAttrib1Name] = ShaderTranslatorInterface::VariableInfo(
      kAttrib1Type, kAttrib1Size, kAttrib1Name);
  attrib_map[kAttrib2Name] = ShaderTranslatorInterface::VariableInfo(
      kAttrib2GoodType, kAttrib2Size, kAttrib2Name);
  attrib_map[kAttrib3Name] = ShaderTranslatorInterface::VariableInfo(
      kAttrib3Type, kAttrib3Size, kAttrib3Name);
  uniform_map[kUniform1Name] = ShaderTranslatorInterface::VariableInfo(
      kUniform1Type, kUniform1Size, kUniform1Name);
  uniform_map[kUniform2Name] = ShaderTranslatorInterface::VariableInfo(
      kUniform2GoodType, kUniform2Size, kUniform2Name);
  uniform_map[kUniform3GoodName] = ShaderTranslatorInterface::VariableInfo(
      kUniform3Type, kUniform3Size, kUniform3GoodName);
  EXPECT_CALL(shader_translator, attrib_map())
      .WillRepeatedly(ReturnRef(attrib_map));
  EXPECT_CALL(shader_translator, uniform_map())
      .WillRepeatedly(ReturnRef(uniform_map));
  const GLuint kVShaderClientId = 2001;
  const GLuint kFShaderClientId = 2002;
  const GLuint kVShaderServiceId = 3001;
  const GLuint kFShaderServiceId = 3002;
  ShaderManager::ShaderInfo* vshader = shader_manager_.CreateShaderInfo(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, "", &shader_translator);
  ShaderManager::ShaderInfo* fshader = shader_manager_.CreateShaderInfo(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != NULL);
  fshader->SetStatus(true, "", &shader_translator);
  static ProgramManagerWithShaderTest::AttribInfo kAttribs[] = {
    { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
    { kAttrib2Name, kAttrib2Size, kAttrib2BadType, kAttrib2Location, },
    { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
  };
  static ProgramManagerWithShaderTest::UniformInfo kUniforms[] = {
    { kUniform1Name,
      kUniform1Name,
      kUniform1Size,
      kUniform1Type,
      kUniform1FakeLocation,
      kUniform1RealLocation,
    },
    { kUniform2Name,
      kUniform2Name,
      kUniform2Size,
      kUniform2BadType,
      kUniform2FakeLocation,
      kUniform2RealLocation,
    },
    { kUniform3BadName,
      kUniform3GoodName,
      kUniform3Size,
      kUniform3Type,
      kUniform3FakeLocation,
      kUniform3RealLocation,
    },
  };
  const size_t kNumAttribs= arraysize(kAttribs);
  const size_t kNumUniforms = arraysize(kUniforms);
  static const GLuint kClientProgramId = 1234;
  static const GLuint kServiceProgramId = 5679;
  SetupShader(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
              kServiceProgramId);
  ProgramManager::ProgramInfo* program_info = manager_.CreateProgramInfo(
      kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, fshader));
  program_info->Link();
  // Check that we got the good type, not the bad.
  // Check Attribs
  for (unsigned index = 0; index < kNumAttribs; ++index) {
    const ProgramManager::ProgramInfo::VertexAttribInfo* attrib_info =
        program_info->GetAttribInfo(index);
    ASSERT_TRUE(attrib_info != NULL);
    ShaderTranslator::VariableMap::const_iterator it = attrib_map.find(
        attrib_info->name);
    ASSERT_TRUE(it != attrib_map.end());
    EXPECT_EQ(it->first, attrib_info->name);
    EXPECT_EQ(static_cast<GLenum>(it->second.type), attrib_info->type);
    EXPECT_EQ(it->second.size, attrib_info->size);
    EXPECT_EQ(it->second.name, attrib_info->name);
  }
  // Check Uniforms
  for (unsigned index = 0; index < kNumUniforms; ++index) {
    const ProgramManager::ProgramInfo::UniformInfo* uniform_info =
        program_info->GetUniformInfo(index);
    ASSERT_TRUE(uniform_info != NULL);
    ShaderTranslator::VariableMap::const_iterator it = uniform_map.find(
        uniform_info->name);
    ASSERT_TRUE(it != uniform_map.end());
    EXPECT_EQ(it->first, uniform_info->name);
    EXPECT_EQ(static_cast<GLenum>(it->second.type), uniform_info->type);
    EXPECT_EQ(it->second.size, uniform_info->size);
    EXPECT_EQ(it->second.name, uniform_info->name);
  }
}

TEST_F(ProgramManagerWithShaderTest, ProgramInfoUseCount) {
  static const GLuint kClientProgramId = 124;
  static const GLuint kServiceProgramId = 457;
  ProgramManager::ProgramInfo* program_info = manager_.CreateProgramInfo(
      kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_FALSE(program_info->CanLink());
  const GLuint kVShaderClientId = 2001;
  const GLuint kFShaderClientId = 2002;
  const GLuint kVShaderServiceId = 3001;
  const GLuint kFShaderServiceId = 3002;
  ShaderManager::ShaderInfo* vshader = shader_manager_.CreateShaderInfo(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, "", NULL);
  ShaderManager::ShaderInfo* fshader = shader_manager_.CreateShaderInfo(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != NULL);
  fshader->SetStatus(true, "", NULL);
  EXPECT_FALSE(vshader->InUse());
  EXPECT_FALSE(fshader->InUse());
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(vshader->InUse());
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, fshader));
  EXPECT_TRUE(fshader->InUse());
  EXPECT_TRUE(program_info->CanLink());
  EXPECT_FALSE(program_info->InUse());
  EXPECT_FALSE(program_info->IsDeleted());
  manager_.UseProgram(program_info);
  EXPECT_TRUE(program_info->InUse());
  manager_.UseProgram(program_info);
  EXPECT_TRUE(program_info->InUse());
  manager_.MarkAsDeleted(&shader_manager_, program_info);
  EXPECT_TRUE(program_info->IsDeleted());
  ProgramManager::ProgramInfo* info2 =
      manager_.GetProgramInfo(kClientProgramId);
  EXPECT_EQ(program_info, info2);
  manager_.UnuseProgram(&shader_manager_, program_info);
  EXPECT_TRUE(program_info->InUse());
  // this should delete the info.
  EXPECT_CALL(*gl_, DeleteProgram(kServiceProgramId))
      .Times(1)
      .RetiresOnSaturation();
  manager_.UnuseProgram(&shader_manager_, program_info);
  info2 = manager_.GetProgramInfo(kClientProgramId);
  EXPECT_TRUE(info2 == NULL);
  EXPECT_FALSE(vshader->InUse());
  EXPECT_FALSE(fshader->InUse());
}

TEST_F(ProgramManagerWithShaderTest, ProgramInfoUseCount2) {
  static const GLuint kClientProgramId = 124;
  static const GLuint kServiceProgramId = 457;
  ProgramManager::ProgramInfo* program_info = manager_.CreateProgramInfo(
      kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_FALSE(program_info->CanLink());
  const GLuint kVShaderClientId = 2001;
  const GLuint kFShaderClientId = 2002;
  const GLuint kVShaderServiceId = 3001;
  const GLuint kFShaderServiceId = 3002;
  ShaderManager::ShaderInfo* vshader = shader_manager_.CreateShaderInfo(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, "", NULL);
  ShaderManager::ShaderInfo* fshader = shader_manager_.CreateShaderInfo(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != NULL);
  fshader->SetStatus(true, "", NULL);
  EXPECT_FALSE(vshader->InUse());
  EXPECT_FALSE(fshader->InUse());
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(vshader->InUse());
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, fshader));
  EXPECT_TRUE(fshader->InUse());
  EXPECT_TRUE(program_info->CanLink());
  EXPECT_FALSE(program_info->InUse());
  EXPECT_FALSE(program_info->IsDeleted());
  manager_.UseProgram(program_info);
  EXPECT_TRUE(program_info->InUse());
  manager_.UseProgram(program_info);
  EXPECT_TRUE(program_info->InUse());
  manager_.UnuseProgram(&shader_manager_, program_info);
  EXPECT_TRUE(program_info->InUse());
  manager_.UnuseProgram(&shader_manager_, program_info);
  EXPECT_FALSE(program_info->InUse());
  ProgramManager::ProgramInfo* info2 =
      manager_.GetProgramInfo(kClientProgramId);
  EXPECT_EQ(program_info, info2);
  // this should delete the program.
  EXPECT_CALL(*gl_, DeleteProgram(kServiceProgramId))
      .Times(1)
      .RetiresOnSaturation();
  manager_.MarkAsDeleted(&shader_manager_, program_info);
  info2 = manager_.GetProgramInfo(kClientProgramId);
  EXPECT_TRUE(info2 == NULL);
  EXPECT_FALSE(vshader->InUse());
  EXPECT_FALSE(fshader->InUse());
}

TEST_F(ProgramManagerWithShaderTest, ProgramInfoGetProgramInfo) {
  CommonDecoder::Bucket bucket;
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  program_info->GetProgramInfo(&manager_, &bucket);
  ProgramInfoHeader* header =
      bucket.GetDataAs<ProgramInfoHeader*>(0, sizeof(ProgramInfoHeader));
  ASSERT_TRUE(header != NULL);
  EXPECT_EQ(1u, header->link_status);
  EXPECT_EQ(arraysize(kAttribs), header->num_attribs);
  EXPECT_EQ(arraysize(kUniforms), header->num_uniforms);
  const ProgramInput* inputs = bucket.GetDataAs<const ProgramInput*>(
      sizeof(*header),
      sizeof(ProgramInput) * (header->num_attribs + header->num_uniforms));
  ASSERT_TRUE(inputs != NULL);
  const ProgramInput* input = inputs;
  // TODO(gman): Don't assume these are in order.
  for (uint32 ii = 0; ii < header->num_attribs; ++ii) {
    const AttribInfo& expected = kAttribs[ii];
    EXPECT_EQ(expected.size, input->size);
    EXPECT_EQ(expected.type, input->type);
    const int32* location = bucket.GetDataAs<const int32*>(
        input->location_offset, sizeof(int32));
    ASSERT_TRUE(location != NULL);
    EXPECT_EQ(expected.location, *location);
    const char* name_buf = bucket.GetDataAs<const char*>(
        input->name_offset, input->name_length);
    ASSERT_TRUE(name_buf != NULL);
    std::string name(name_buf, input->name_length);
    EXPECT_STREQ(expected.name, name.c_str());
    ++input;
  }
  // TODO(gman): Don't assume these are in order.
  for (uint32 ii = 0; ii < header->num_uniforms; ++ii) {
    const UniformInfo& expected = kUniforms[ii];
    EXPECT_EQ(expected.size, input->size);
    EXPECT_EQ(expected.type, input->type);
    const int32* locations = bucket.GetDataAs<const int32*>(
        input->location_offset, sizeof(int32) * input->size);
    ASSERT_TRUE(locations != NULL);
    for (int32 jj = 0; jj < input->size; ++jj) {
      EXPECT_EQ(manager_.SwizzleLocation(
          ProgramManager::ProgramInfo::GetFakeLocation(
              expected.fake_location, jj)),
          locations[jj]);
    }
    const char* name_buf = bucket.GetDataAs<const char*>(
        input->name_offset, input->name_length);
    ASSERT_TRUE(name_buf != NULL);
    std::string name(name_buf, input->name_length);
    EXPECT_STREQ(expected.good_name, name.c_str());
    ++input;
  }
  EXPECT_EQ(header->num_attribs + header->num_uniforms,
            static_cast<uint32>(input - inputs));
}

TEST_F(ProgramManagerWithShaderTest, BindAttribLocationConflicts) {
  // Set up shader
  const GLuint kVShaderClientId = 1;
  const GLuint kVShaderServiceId = 11;
  const GLuint kFShaderClientId = 2;
  const GLuint kFShaderServiceId = 12;
  MockShaderTranslator shader_translator;
  ShaderTranslator::VariableMap attrib_map;
  for (uint32 ii = 0; ii < kNumAttribs; ++ii) {
    attrib_map[kAttribs[ii].name] = ShaderTranslatorInterface::VariableInfo(
        kAttribs[ii].type, kAttribs[ii].size, kAttribs[ii].name);
  }
  ShaderTranslator::VariableMap uniform_map;
  EXPECT_CALL(shader_translator, attrib_map())
      .WillRepeatedly(ReturnRef(attrib_map));
  EXPECT_CALL(shader_translator, uniform_map())
      .WillRepeatedly(ReturnRef(uniform_map));
  // Check we can create shader.
  ShaderManager::ShaderInfo* vshader = shader_manager_.CreateShaderInfo(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ShaderManager::ShaderInfo* fshader = shader_manager_.CreateShaderInfo(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  // Check shader got created.
  ASSERT_TRUE(vshader != NULL && fshader != NULL);
  // Set Status
  vshader->SetStatus(true, "", &shader_translator);
  // Check attrib infos got copied.
  for (ShaderTranslator::VariableMap::const_iterator it = attrib_map.begin();
       it != attrib_map.end(); ++it) {
    const ShaderManager::ShaderInfo::VariableInfo* variable_info =
        vshader->GetAttribInfo(it->first);
    ASSERT_TRUE(variable_info != NULL);
    EXPECT_EQ(it->second.type, variable_info->type);
    EXPECT_EQ(it->second.size, variable_info->size);
    EXPECT_EQ(it->second.name, variable_info->name);
  }
  fshader->SetStatus(true, "", NULL);

  // Set up program
  const GLuint kClientProgramId = 6666;
  const GLuint kServiceProgramId = 8888;
  ProgramManager::ProgramInfo* program_info =
      manager_.CreateProgramInfo(kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, fshader));

  EXPECT_FALSE(program_info->DetectAttribLocationBindingConflicts());
  EXPECT_TRUE(LinkAsExpected(program_info, true));

  program_info->SetAttribLocationBinding(kAttrib1Name, 0);
  EXPECT_FALSE(program_info->DetectAttribLocationBindingConflicts());
  EXPECT_TRUE(LinkAsExpected(program_info, true));

  program_info->SetAttribLocationBinding("xxx", 0);
  EXPECT_FALSE(program_info->DetectAttribLocationBindingConflicts());
  EXPECT_TRUE(LinkAsExpected(program_info, true));

  program_info->SetAttribLocationBinding(kAttrib2Name, 1);
  EXPECT_FALSE(program_info->DetectAttribLocationBindingConflicts());
  EXPECT_TRUE(LinkAsExpected(program_info, true));

  program_info->SetAttribLocationBinding(kAttrib2Name, 0);
  EXPECT_TRUE(program_info->DetectAttribLocationBindingConflicts());
  EXPECT_TRUE(LinkAsExpected(program_info, false));
}

TEST_F(ProgramManagerWithShaderTest, ClearWithSamplerTypes) {
  const GLuint kVShaderClientId = 2001;
  const GLuint kFShaderClientId = 2002;
  const GLuint kVShaderServiceId = 3001;
  const GLuint kFShaderServiceId = 3002;
  ShaderManager::ShaderInfo* vshader = shader_manager_.CreateShaderInfo(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, NULL, NULL);
  ShaderManager::ShaderInfo* fshader = shader_manager_.CreateShaderInfo(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != NULL);
  fshader->SetStatus(true, NULL, NULL);
  static const GLuint kClientProgramId = 1234;
  static const GLuint kServiceProgramId = 5679;
  ProgramManager::ProgramInfo* program_info = manager_.CreateProgramInfo(
      kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, fshader));

  static const GLenum kSamplerTypes[] = {
    GL_SAMPLER_2D,
    GL_SAMPLER_CUBE,
    GL_SAMPLER_EXTERNAL_OES,
    GL_SAMPLER_3D_OES,
    GL_SAMPLER_2D_RECT_ARB,
  };
  const size_t kNumSamplerTypes = arraysize(kSamplerTypes);
  for (size_t ii = 0; ii < kNumSamplerTypes; ++ii) {
    static ProgramManagerWithShaderTest::AttribInfo kAttribs[] = {
      { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
      { kAttrib2Name, kAttrib2Size, kAttrib2Type, kAttrib2Location, },
      { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
    };
    ProgramManagerWithShaderTest::UniformInfo kUniforms[] = {
      { kUniform1Name,
        kUniform1Name,
        kUniform1Size,
        kUniform1Type,
        kUniform1FakeLocation,
        kUniform1RealLocation,
      },
      { kUniform2Name,
        kUniform2Name,
        kUniform2Size,
        kSamplerTypes[ii],
        kUniform2FakeLocation,
        kUniform2RealLocation,
      },
      { kUniform3BadName,
        kUniform3GoodName,
        kUniform3Size,
        kUniform3Type,
        kUniform3FakeLocation,
        kUniform3RealLocation,
      },
    };
    const size_t kNumAttribs = arraysize(kAttribs);
    const size_t kNumUniforms = arraysize(kUniforms);
    SetupShader(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
                kServiceProgramId);
    program_info->Link();
    SetupExpectationsForClearingUniforms(kUniforms, kNumUniforms);
    manager_.ClearUniforms(program_info);
  }
}

}  // namespace gles2
}  // namespace gpu


