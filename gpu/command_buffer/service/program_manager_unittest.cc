// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_manager.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "gpu/command_buffer/service/gl_mock.h"

using ::gles2::MockGLInterface;
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

class ProgramManagerTest : public testing::Test {
 public:
  ProgramManagerTest() { }

 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  ProgramManager manager_;
};

TEST_F(ProgramManagerTest, Basic) {
  const GLuint kProgram1Id = 1;
  const GLuint kProgram2Id = 2;
  // Check we can create program.
  manager_.CreateProgramInfo(kProgram1Id);
  // Check program got created.
  ProgramManager::ProgramInfo* info1 = manager_.GetProgramInfo(kProgram1Id);
  ASSERT_TRUE(info1 != NULL);
  // Check we get nothing for a non-existent program.
  EXPECT_TRUE(manager_.GetProgramInfo(kProgram2Id) == NULL);
  // Check trying to a remove non-existent programs does not crash.
  manager_.RemoveProgramInfo(kProgram2Id);
  // Check we can't get the program after we remove it.
  manager_.RemoveProgramInfo(kProgram1Id);
  EXPECT_TRUE(manager_.GetProgramInfo(kProgram1Id) == NULL);
}

class ProgramManagerWithShaderTest : public testing::Test {
 public:
  ProgramManagerWithShaderTest()
      : program_info_(NULL) {
  }

  static const GLint kNumVertexAttribs = 16;

  static const GLuint kProgramId = 123;

  static const GLint kMaxAttribLength = 10;
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

  static const GLint kMaxUniformLength = 10;
  static const char* kUniform1Name;
  static const char* kUniform2Name;
  static const char* kUniform3Name;
  static const GLint kUniform1Size = 1;
  static const GLint kUniform2Size = 3;
  static const GLint kUniform3Size = 2;
  static const GLint kUniform1Location = 3;
  static const GLint kUniform2Location = 10;
  static const GLint kUniform2ElementLocation = 12;
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
    gl_.reset(new StrictMock<MockGLInterface>());
    ::gles2::GLInterface::SetGLInterface(gl_.get());

    SetupDefaultShaderExpectations();

    manager_.CreateProgramInfo(kProgramId);
    program_info_ = manager_.GetProgramInfo(kProgramId);
    program_info_->Update();
  }

  void SetupShader(AttribInfo* attribs, size_t num_attribs,
                   UniformInfo* uniforms, size_t num_uniforms,
                   GLuint service_id) {
    InSequence s;
    EXPECT_CALL(*gl_,
        GetProgramiv(service_id, GL_ACTIVE_ATTRIBUTES, _))
        .WillOnce(SetArgumentPointee<2>(num_attribs))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetProgramiv(service_id, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, _))
        .WillOnce(SetArgumentPointee<2>(kMaxAttribLength))
        .RetiresOnSaturation();
    for (size_t ii = 0; ii < num_attribs; ++ii) {
      const AttribInfo& info = attribs[ii];
      EXPECT_CALL(*gl_,
          GetActiveAttrib(service_id, ii,
                          kMaxAttribLength, _, _, _, _))
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
    EXPECT_CALL(*gl_,
        GetProgramiv(service_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, _))
        .WillOnce(SetArgumentPointee<2>(kMaxUniformLength))
        .RetiresOnSaturation();
    for (size_t ii = 0; ii < num_uniforms; ++ii) {
      const UniformInfo& info = uniforms[ii];
      EXPECT_CALL(*gl_,
          GetActiveUniform(service_id, ii,
                           kMaxUniformLength, _, _, _, _))
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
          for (GLsizei jj = 1; jj < info.size; ++jj) {
            std::string element_name(
                std::string(info.name) + "[" + IntToString(jj) + "]");
            EXPECT_CALL(*gl_, GetUniformLocation(service_id,
                                                 StrEq(element_name)))
                .WillOnce(Return(info.location + jj))
                .RetiresOnSaturation();
          }
        }
      }
    }
  }


  void SetupDefaultShaderExpectations() {
    SetupShader(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
                kProgramId);
  }

  virtual void TearDown() {
  }

  static AttribInfo kAttribs[];
  static UniformInfo kUniforms[];

  scoped_ptr<StrictMock<MockGLInterface> > gl_;

  ProgramManager manager_;

  ProgramManager::ProgramInfo* program_info_;
};

ProgramManagerWithShaderTest::AttribInfo
    ProgramManagerWithShaderTest::kAttribs[] = {
  { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
  { kAttrib2Name, kAttrib2Size, kAttrib2Type, kAttrib2Location, },
  { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
};

const size_t ProgramManagerWithShaderTest::kNumAttribs =
    arraysize(ProgramManagerWithShaderTest::kAttribs);

ProgramManagerWithShaderTest::UniformInfo
    ProgramManagerWithShaderTest::kUniforms[] = {
  { kUniform1Name, kUniform1Size, kUniform1Type, kUniform1Location, },
  { kUniform2Name, kUniform2Size, kUniform2Type, kUniform2Location, },
  { kUniform3Name, kUniform3Size, kUniform3Type, kUniform3Location, },
};

const size_t ProgramManagerWithShaderTest::kNumUniforms =
    arraysize(ProgramManagerWithShaderTest::kUniforms);

const char* ProgramManagerWithShaderTest::kAttrib1Name = "attrib1";
const char* ProgramManagerWithShaderTest::kAttrib2Name = "attrib2";
const char* ProgramManagerWithShaderTest::kAttrib3Name = "attrib3";
const char* ProgramManagerWithShaderTest::kUniform1Name = "uniform1";
const char* ProgramManagerWithShaderTest::kUniform2Name = "uniform2";
const char* ProgramManagerWithShaderTest::kUniform3Name = "uniform3";

TEST_F(ProgramManagerWithShaderTest, GetAttribInfos) {
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kProgramId);
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
      manager_.GetProgramInfo(kProgramId);
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
      manager_.GetProgramInfo(kProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_EQ(kAttrib2Location, program_info->GetAttribLocation(kAttrib2Name));
  EXPECT_EQ(-1, program_info->GetAttribLocation(kInvalidName));
}

TEST_F(ProgramManagerWithShaderTest, GetUniformLocation) {
  const GLint kValidIndex = 1;
  const GLint kInvalidIndex = 1000;
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kProgramId);
  ASSERT_TRUE(program_info != NULL);
  const ProgramManager::ProgramInfo::UniformInfo* info =
      program_info->GetUniformInfo(kValidIndex);
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(kUniform2Size, info->size);
  EXPECT_EQ(kUniform2Type, info->type);
  EXPECT_EQ(kUniform2Location, info->element_locations[0]);
  EXPECT_STREQ(kUniform2Name, info->name.c_str());
  EXPECT_TRUE(program_info->GetUniformInfo(kInvalidIndex) == NULL);
}

TEST_F(ProgramManagerWithShaderTest, GetUniformTypeByLocation) {
  const GLint kInvalidLocation = 1234;
  GLenum type = 0u;
  const ProgramManager::ProgramInfo* program_info =
      manager_.GetProgramInfo(kProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_TRUE(program_info->GetUniformTypeByLocation(kUniform2Location, &type));
  EXPECT_EQ(kUniform2Type, type);
  type = 0u;
  EXPECT_FALSE(program_info->GetUniformTypeByLocation(
      kInvalidLocation, &type));
  EXPECT_EQ(0u, type);
}

}  // namespace gles2
}  // namespace gpu


