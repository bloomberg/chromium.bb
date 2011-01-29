// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_manager.h"

#include <algorithm>

#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "gpu/command_buffer/common/gl_mock.h"
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
  manager_.CreateProgramInfo(kClient1Id, kService1Id);
  // Check program got created.
  ProgramManager::ProgramInfo* info1 =
      manager_.GetProgramInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_CALL(*gl_, DeleteProgram(kService1Id))
      .Times(1)
      .RetiresOnSaturation();
  manager_.Destroy(true);
  // Check the resources were released.
  info1 = manager_.GetProgramInfo(kClient1Id);
  ASSERT_TRUE(info1 == NULL);
}

TEST_F(ProgramManagerTest, ProgramInfo) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create program.
  manager_.CreateProgramInfo(kClient1Id, kService1Id);
  // Check program got created.
  ProgramManager::ProgramInfo* info1 = manager_.GetProgramInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_EQ(kService1Id, info1->service_id());
  EXPECT_FALSE(info1->InUse());
  EXPECT_FALSE(info1->IsValid());
  EXPECT_FALSE(info1->IsDeleted());
  EXPECT_FALSE(info1->CanLink());
  EXPECT_TRUE(info1->log_info() == NULL);
}

class ProgramManagerWithShaderTest : public testing::Test {
 public:
  ProgramManagerWithShaderTest()
      : program_info_(NULL) {
  }

  ~ProgramManagerWithShaderTest() {
    manager_.Destroy(false);
  }

  static const GLint kNumVertexAttribs = 16;

  static const GLuint kClientProgramId = 123;
  static const GLuint kServiceProgramId = 456;

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
  static const GLint kUniform1Location = 3;
  static const GLint kUniform2Location = 10;
  static const GLint kUniform3Location = 20;
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
    GLint size;
    GLenum type;
    GLint location;
  };

  virtual void SetUp() {
    gl_.reset(new StrictMock<gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());

    SetupDefaultShaderExpectations();

    manager_.CreateProgramInfo(kClientProgramId, kServiceProgramId);
    program_info_ = manager_.GetProgramInfo(kClientProgramId);
    program_info_->Update();
  }

  void SetupShader(AttribInfo* attribs, size_t num_attribs,
                   UniformInfo* uniforms, size_t num_uniforms,
                   GLuint service_id) {
    InSequence s;
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
            .WillOnce(Return(info.location))
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
                .WillOnce(Return(info.location + jj * 2))
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

  virtual void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
  }

  static AttribInfo kAttribs[];
  static UniformInfo kUniforms[];

  scoped_ptr<StrictMock<gfx::MockGLInterface> > gl_;

  ProgramManager manager_;

  ProgramManager::ProgramInfo* program_info_;
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
const GLint ProgramManagerWithShaderTest::kUniform1Location;
const GLint ProgramManagerWithShaderTest::kUniform2Location;
const GLint ProgramManagerWithShaderTest::kUniform3Location;
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
  { kUniform1Name, kUniform1Size, kUniform1Type, kUniform1Location, },
  { kUniform2Name, kUniform2Size, kUniform2Type, kUniform2Location, },
  { kUniform3BadName, kUniform3Size, kUniform3Type, kUniform3Location, },
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
  EXPECT_EQ(kUniform1Location, info->element_locations[0]);
  EXPECT_STREQ(kUniform1Name, info->name.c_str());
  info = program_info->GetUniformInfo(1);
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(kUniform2Size, info->size);
  EXPECT_EQ(kUniform2Type, info->type);
  EXPECT_EQ(kUniform2Location, info->element_locations[0]);
  EXPECT_STREQ(kUniform2Name, info->name.c_str());
  info = program_info->GetUniformInfo(2);
  // We emulate certain OpenGL drivers by supplying the name without
  // the array spec. Our implementation should correctly add the required spec.
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(kUniform3Size, info->size);
  EXPECT_EQ(kUniform3Type, info->type);
  EXPECT_EQ(kUniform3Location, info->element_locations[0]);
  EXPECT_STREQ(kUniform3GoodName, info->name.c_str());
  EXPECT_TRUE(program_info->GetUniformInfo(kInvalidIndex) == NULL);
}

TEST_F(ProgramManagerWithShaderTest, AttachDetachShader) {
  ShaderManager shader_manager;
  ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_FALSE(program_info->CanLink());
  const GLuint kVShaderClientId = 2001;
  const GLuint kFShaderClientId = 2002;
  const GLuint kVShaderServiceId = 3001;
  const GLuint kFShaderServiceId = 3002;
  shader_manager.CreateShaderInfo(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ShaderManager::ShaderInfo* vshader = shader_manager.GetShaderInfo(
      kVShaderClientId);
  vshader->SetStatus(true, "", NULL);
  shader_manager.CreateShaderInfo(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  ShaderManager::ShaderInfo* fshader = shader_manager.GetShaderInfo(
      kFShaderClientId);
  fshader->SetStatus(true, "", NULL);
  EXPECT_TRUE(program_info->AttachShader(&shader_manager, vshader));
  EXPECT_FALSE(program_info->CanLink());
  EXPECT_TRUE(program_info->AttachShader(&shader_manager, fshader));
  EXPECT_TRUE(program_info->CanLink());
  program_info->DetachShader(&shader_manager, vshader);
  EXPECT_FALSE(program_info->CanLink());
  EXPECT_TRUE(program_info->AttachShader(&shader_manager, vshader));
  EXPECT_TRUE(program_info->CanLink());
  program_info->DetachShader(&shader_manager, fshader);
  EXPECT_FALSE(program_info->CanLink());
  EXPECT_FALSE(program_info->AttachShader(&shader_manager, vshader));
  EXPECT_FALSE(program_info->CanLink());
  EXPECT_TRUE(program_info->AttachShader(&shader_manager, fshader));
  EXPECT_TRUE(program_info->CanLink());
  vshader->SetStatus(false, "", NULL);
  EXPECT_FALSE(program_info->CanLink());
  vshader->SetStatus(true, "", NULL);
  EXPECT_TRUE(program_info->CanLink());
  fshader->SetStatus(false, "", NULL);
  EXPECT_FALSE(program_info->CanLink());
  fshader->SetStatus(true, "", NULL);
  EXPECT_TRUE(program_info->CanLink());
  shader_manager.Destroy(false);
}

TEST_F(ProgramManagerWithShaderTest, GetUniformLocation) {
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_EQ(kUniform1Location, program_info->GetUniformLocation(kUniform1Name));
  EXPECT_EQ(kUniform2Location, program_info->GetUniformLocation(kUniform2Name));
  EXPECT_EQ(kUniform3Location, program_info->GetUniformLocation(
      kUniform3BadName));
  // Check we can get uniform2 as "uniform2" even though the name is
  // "uniform2[0]"
  EXPECT_EQ(kUniform2Location, program_info->GetUniformLocation("uniform2"));
  // Check we can get uniform3 as "uniform3[0]" even though we simulated GL
  // returning "uniform3"
  EXPECT_EQ(kUniform3Location, program_info->GetUniformLocation(
      kUniform3GoodName));
  // Check that we can get the locations of the array elements > 1
  EXPECT_EQ(kUniform2Location + 2,
            program_info->GetUniformLocation("uniform2[1]"));
  EXPECT_EQ(kUniform2Location + 4,
            program_info->GetUniformLocation("uniform2[2]"));
  EXPECT_EQ(-1,
            program_info->GetUniformLocation("uniform2[3]"));
  EXPECT_EQ(kUniform3Location + 2,
            program_info->GetUniformLocation("uniform3[1]"));
  EXPECT_EQ(-1,
            program_info->GetUniformLocation("uniform3[2]"));
}

TEST_F(ProgramManagerWithShaderTest, GetUniformTypeByLocation) {
  const GLint kInvalidLocation = 1234;
  GLenum type = 0u;
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_TRUE(program_info->GetUniformTypeByLocation(kUniform2Location, &type));
  EXPECT_EQ(kUniform2Type, type);
  type = 0u;
  EXPECT_FALSE(program_info->GetUniformTypeByLocation(
      kInvalidLocation, &type));
  EXPECT_EQ(0u, type);
}

// Some GL drivers incorrectly return gl_DepthRange and possibly other uniforms
// that start with "gl_". Our implementation catches these and does not allow
// them back to client.
TEST_F(ProgramManagerWithShaderTest, GLDriverReturnsGLUnderscoreUniform) {
  static const char* kUniform2Name = "gl_longNameWeCanCheckFor";
  static ProgramManagerWithShaderTest::UniformInfo kUniforms[] = {
    { kUniform1Name, kUniform1Size, kUniform1Type, kUniform1Location, },
    { kUniform2Name, kUniform2Size, kUniform2Type, kUniform2Location, },
    { kUniform3BadName, kUniform3Size, kUniform3Type, kUniform3Location, },
  };
  const size_t kNumUniforms = arraysize(kUniforms);
  static const GLuint kClientProgramId = 1234;
  static const GLuint kServiceProgramId = 5679;
  SetupShader(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
              kServiceProgramId);
  manager_.CreateProgramInfo(kClientProgramId, kServiceProgramId);
  ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  program_info->Update();
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
  const GLuint kShaderClientId = 999;
  const GLuint kShaderServiceId = 998;
  static GLenum kAttrib2BadType = GL_FLOAT_VEC2;
  static GLenum kAttrib2GoodType = GL_FLOAT_MAT2;
  static GLenum kUniform2BadType = GL_FLOAT_VEC3;
  static GLenum kUniform2GoodType = GL_FLOAT_MAT3;
  MockShaderTranslator shader_translator;
  ShaderTranslator::VariableMap attrib_map;
  ShaderTranslator::VariableMap uniform_map;
  attrib_map[kAttrib1Name] = ShaderTranslatorInterface::VariableInfo(
      kAttrib1Type, kAttrib1Size);
  attrib_map[kAttrib2Name] = ShaderTranslatorInterface::VariableInfo(
      kAttrib2GoodType, kAttrib2Size);
  attrib_map[kAttrib3Name] = ShaderTranslatorInterface::VariableInfo(
      kAttrib3Type, kAttrib3Size);
  uniform_map[kUniform1Name] = ShaderTranslatorInterface::VariableInfo(
      kUniform1Type, kUniform1Size);
  uniform_map[kUniform2Name] = ShaderTranslatorInterface::VariableInfo(
      kUniform2GoodType, kUniform2Size);
  uniform_map[kUniform3GoodName] = ShaderTranslatorInterface::VariableInfo(
      kUniform3Type, kUniform3Size);
  EXPECT_CALL(shader_translator, attrib_map())
      .WillRepeatedly(ReturnRef(attrib_map));
  EXPECT_CALL(shader_translator, uniform_map())
      .WillRepeatedly(ReturnRef(uniform_map));
  ShaderManager shader_manager;
  shader_manager.CreateShaderInfo(
      kShaderClientId, kShaderServiceId, GL_FRAGMENT_SHADER);
  ShaderManager::ShaderInfo* shader_info =
      shader_manager.GetShaderInfo(kShaderClientId);
  shader_info->SetStatus(true, "", &shader_translator);
  static ProgramManagerWithShaderTest::AttribInfo kAttribs[] = {
    { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
    { kAttrib2Name, kAttrib2Size, kAttrib2BadType, kAttrib2Location, },
    { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
  };
  static ProgramManagerWithShaderTest::UniformInfo kUniforms[] = {
    { kUniform1Name, kUniform1Size, kUniform1Type, kUniform1Location, },
    { kUniform2Name, kUniform2Size, kUniform2BadType, kUniform2Location, },
    { kUniform3BadName, kUniform3Size, kUniform3Type, kUniform3Location, },
  };
  const size_t kNumAttribs= arraysize(kAttribs);
  const size_t kNumUniforms = arraysize(kUniforms);
  static const GLuint kClientProgramId = 1234;
  static const GLuint kServiceProgramId = 5679;
  SetupShader(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
              kServiceProgramId);
  manager_.CreateProgramInfo(kClientProgramId, kServiceProgramId);
  ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_TRUE(program_info->AttachShader(&shader_manager, shader_info));
  program_info->Update();
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
  }
  shader_manager.Destroy(false);
}

TEST_F(ProgramManagerWithShaderTest, ProgramInfoUseCount) {
  ShaderManager shader_manager;
  ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_FALSE(program_info->CanLink());
  const GLuint kVShaderClientId = 2001;
  const GLuint kFShaderClientId = 2002;
  const GLuint kVShaderServiceId = 3001;
  const GLuint kFShaderServiceId = 3002;
  shader_manager.CreateShaderInfo(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ShaderManager::ShaderInfo* vshader = shader_manager.GetShaderInfo(
      kVShaderClientId);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, "", NULL);
  shader_manager.CreateShaderInfo(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  ShaderManager::ShaderInfo* fshader = shader_manager.GetShaderInfo(
      kFShaderClientId);
  ASSERT_TRUE(fshader != NULL);
  fshader->SetStatus(true, "", NULL);
  EXPECT_FALSE(vshader->InUse());
  EXPECT_FALSE(fshader->InUse());
  EXPECT_TRUE(program_info->AttachShader(&shader_manager, vshader));
  EXPECT_TRUE(vshader->InUse());
  EXPECT_TRUE(program_info->AttachShader(&shader_manager, fshader));
  EXPECT_TRUE(fshader->InUse());
  EXPECT_TRUE(program_info->CanLink());
  EXPECT_FALSE(program_info->InUse());
  EXPECT_FALSE(program_info->IsDeleted());
  manager_.UseProgram(program_info);
  EXPECT_TRUE(program_info->InUse());
  manager_.UseProgram(program_info);
  EXPECT_TRUE(program_info->InUse());
  manager_.MarkAsDeleted(&shader_manager, program_info);
  EXPECT_TRUE(program_info->IsDeleted());
  ProgramManager::ProgramInfo* info2 =
      manager_.GetProgramInfo(kClientProgramId);
  EXPECT_EQ(program_info, info2);
  manager_.UnuseProgram(&shader_manager, program_info);
  EXPECT_TRUE(program_info->InUse());
  // this should delete the info.
  manager_.UnuseProgram(&shader_manager, program_info);
  info2 = manager_.GetProgramInfo(kClientProgramId);
  EXPECT_TRUE(info2 == NULL);
  EXPECT_FALSE(vshader->InUse());
  EXPECT_FALSE(fshader->InUse());
  shader_manager.Destroy(false);
}

TEST_F(ProgramManagerWithShaderTest, ProgramInfoUseCount2) {
  ShaderManager shader_manager;
  ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_FALSE(program_info->CanLink());
  const GLuint kVShaderClientId = 2001;
  const GLuint kFShaderClientId = 2002;
  const GLuint kVShaderServiceId = 3001;
  const GLuint kFShaderServiceId = 3002;
  shader_manager.CreateShaderInfo(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ShaderManager::ShaderInfo* vshader = shader_manager.GetShaderInfo(
      kVShaderClientId);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, "", NULL);
  shader_manager.CreateShaderInfo(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  ShaderManager::ShaderInfo* fshader = shader_manager.GetShaderInfo(
      kFShaderClientId);
  ASSERT_TRUE(fshader != NULL);
  fshader->SetStatus(true, "", NULL);
  EXPECT_FALSE(vshader->InUse());
  EXPECT_FALSE(fshader->InUse());
  EXPECT_TRUE(program_info->AttachShader(&shader_manager, vshader));
  EXPECT_TRUE(vshader->InUse());
  EXPECT_TRUE(program_info->AttachShader(&shader_manager, fshader));
  EXPECT_TRUE(fshader->InUse());
  EXPECT_TRUE(program_info->CanLink());
  EXPECT_FALSE(program_info->InUse());
  EXPECT_FALSE(program_info->IsDeleted());
  manager_.UseProgram(program_info);
  EXPECT_TRUE(program_info->InUse());
  manager_.UseProgram(program_info);
  EXPECT_TRUE(program_info->InUse());
  manager_.UnuseProgram(&shader_manager, program_info);
  EXPECT_TRUE(program_info->InUse());
  manager_.UnuseProgram(&shader_manager, program_info);
  EXPECT_FALSE(program_info->InUse());
  ProgramManager::ProgramInfo* info2 =
      manager_.GetProgramInfo(kClientProgramId);
  EXPECT_EQ(program_info, info2);
  // this should delete the program.
  manager_.MarkAsDeleted(&shader_manager, program_info);
  info2 = manager_.GetProgramInfo(kClientProgramId);
  EXPECT_TRUE(info2 == NULL);
  EXPECT_FALSE(vshader->InUse());
  EXPECT_FALSE(fshader->InUse());
  shader_manager.Destroy(false);
}

}  // namespace gles2
}  // namespace gpu


