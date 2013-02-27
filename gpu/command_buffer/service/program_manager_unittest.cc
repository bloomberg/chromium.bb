// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_manager.h"

#include <algorithm>

#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

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
  ProgramManagerTest() : manager_(NULL) { }
  virtual ~ProgramManagerTest() {
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
  manager_.CreateProgram(kClient1Id, kService1Id);
  // Check program got created.
  Program* info1 = manager_.GetProgram(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  GLuint client_id = 0;
  EXPECT_TRUE(manager_.GetClientId(info1->service_id(), &client_id));
  EXPECT_EQ(kClient1Id, client_id);
  // Check we get nothing for a non-existent program.
  EXPECT_TRUE(manager_.GetProgram(kClient2Id) == NULL);
}

TEST_F(ProgramManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create program.
  Program* info0 = manager_.CreateProgram(
      kClient1Id, kService1Id);
  ASSERT_TRUE(info0 != NULL);
  // Check program got created.
  Program* info1 = manager_.GetProgram(kClient1Id);
  ASSERT_EQ(info0, info1);
  EXPECT_CALL(*gl_, DeleteProgram(kService1Id))
      .Times(1)
      .RetiresOnSaturation();
  manager_.Destroy(true);
  // Check the resources were released.
  info1 = manager_.GetProgram(kClient1Id);
  ASSERT_TRUE(info1 == NULL);
}

TEST_F(ProgramManagerTest, DeleteBug) {
  ShaderManager shader_manager;
  const GLuint kClient1Id = 1;
  const GLuint kClient2Id = 2;
  const GLuint kService1Id = 11;
  const GLuint kService2Id = 12;
  // Check we can create program.
  scoped_refptr<Program> info1(
      manager_.CreateProgram(kClient1Id, kService1Id));
  scoped_refptr<Program> info2(
      manager_.CreateProgram(kClient2Id, kService2Id));
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

TEST_F(ProgramManagerTest, Program) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create program.
  Program* info1 = manager_.CreateProgram(
      kClient1Id, kService1Id);
  ASSERT_TRUE(info1);
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
      :  manager_(NULL), program_info_(NULL) {
  }

  virtual ~ProgramManagerWithShaderTest() {
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
  static const GLint kUniform1DesiredLocation = -1;
  static const GLint kUniform2DesiredLocation = -1;
  static const GLint kUniform3DesiredLocation = -1;
  static const GLenum kUniform1Type = GL_FLOAT_VEC4;
  static const GLenum kUniform2Type = GL_INT_VEC2;
  static const GLenum kUniform3Type = GL_FLOAT_VEC3;
  static const GLint kInvalidUniformLocation = 30;
  static const GLint kBadUniformIndex = 1000;

  static const size_t kNumAttribs;
  static const size_t kNumUniforms;

 protected:
  typedef TestHelper::AttribInfo AttribInfo;
  typedef TestHelper::UniformInfo UniformInfo;

  virtual void SetUp() {
    gl_.reset(new StrictMock<gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());

    SetupDefaultShaderExpectations();

    Shader* vertex_shader = shader_manager_.CreateShader(
        kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
    Shader* fragment_shader =
        shader_manager_.CreateShader(
            kFragmentShaderClientId, kFragmentShaderServiceId,
            GL_FRAGMENT_SHADER);
    ASSERT_TRUE(vertex_shader != NULL);
    ASSERT_TRUE(fragment_shader != NULL);
    vertex_shader->SetStatus(true, NULL, NULL);
    fragment_shader->SetStatus(true, NULL, NULL);

    program_info_ = manager_.CreateProgram(
        kClientProgramId, kServiceProgramId);
    ASSERT_TRUE(program_info_ != NULL);

    program_info_->AttachShader(&shader_manager_, vertex_shader);
    program_info_->AttachShader(&shader_manager_, fragment_shader);
    program_info_->Link(NULL, NULL, NULL, NULL);
  }

  void SetupShader(AttribInfo* attribs, size_t num_attribs,
                   UniformInfo* uniforms, size_t num_uniforms,
                   GLuint service_id) {
    TestHelper::SetupShader(
        gl_.get(), attribs, num_attribs, uniforms, num_uniforms, service_id);
  }

  void SetupDefaultShaderExpectations() {
    SetupShader(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
                kServiceProgramId);
  }

  void SetupExpectationsForClearingUniforms(
      UniformInfo* uniforms, size_t num_uniforms) {
    TestHelper::SetupExpectationsForClearingUniforms(
        gl_.get(), uniforms, num_uniforms);
  }

  virtual void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
  }

  // Return true if link status matches expected_link_status
  bool LinkAsExpected(Program* program_info,
                      bool expected_link_status) {
    GLuint service_id = program_info->service_id();
    if (expected_link_status) {
      SetupShader(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
                  service_id);
    }
    program_info->Link(NULL, NULL, NULL, NULL);
    GLint link_status;
    program_info->GetProgramiv(GL_LINK_STATUS, &link_status);
    return (static_cast<bool>(link_status) == expected_link_status);
  }

  static AttribInfo kAttribs[];
  static UniformInfo kUniforms[];

  scoped_ptr<StrictMock<gfx::MockGLInterface> > gl_;

  ProgramManager manager_;
  Program* program_info_;
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
const GLint ProgramManagerWithShaderTest::kUniform1DesiredLocation;
const GLint ProgramManagerWithShaderTest::kUniform2DesiredLocation;
const GLint ProgramManagerWithShaderTest::kUniform3DesiredLocation;
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
    kUniform1Size,
    kUniform1Type,
    kUniform1FakeLocation,
    kUniform1RealLocation,
    kUniform1DesiredLocation,
    kUniform1Name,
  },
  { kUniform2Name,
    kUniform2Size,
    kUniform2Type,
    kUniform2FakeLocation,
    kUniform2RealLocation,
    kUniform2DesiredLocation,
    kUniform2Name,
  },
  { kUniform3BadName,
    kUniform3Size,
    kUniform3Type,
    kUniform3FakeLocation,
    kUniform3RealLocation,
    kUniform3DesiredLocation,
    kUniform3GoodName,
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
  const Program* program_info =
      manager_.GetProgram(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  const Program::AttribInfoVector& infos =
      program_info->GetAttribInfos();
  ASSERT_EQ(kNumAttribs, infos.size());
  for (size_t ii = 0; ii < kNumAttribs; ++ii) {
    const Program::VertexAttrib& info = infos[ii];
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
  const Program* program_info =
      manager_.GetProgram(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  const Program::VertexAttrib* info =
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
  const Program* program_info =
      manager_.GetProgram(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_EQ(kAttrib2Location, program_info->GetAttribLocation(kAttrib2Name));
  EXPECT_EQ(-1, program_info->GetAttribLocation(kInvalidName));
}

TEST_F(ProgramManagerWithShaderTest, GetUniformInfo) {
  const GLint kInvalidIndex = 1000;
  const Program* program_info =
      manager_.GetProgram(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  const Program::UniformInfo* info =
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
  Program* program_info = manager_.CreateProgram(
      kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_FALSE(program_info->CanLink());
  const GLuint kVShaderClientId = 2001;
  const GLuint kFShaderClientId = 2002;
  const GLuint kVShaderServiceId = 3001;
  const GLuint kFShaderServiceId = 3002;
  Shader* vshader = shader_manager_.CreateShader(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, "", NULL);
  Shader* fshader = shader_manager_.CreateShader(
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
  const Program* program_info =
      manager_.GetProgram(kClientProgramId);
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
  EXPECT_EQ(ProgramManager::MakeFakeLocation(kUniform2FakeLocation, 1),
            program_info->GetUniformFakeLocation("uniform2[1]"));
  EXPECT_EQ(ProgramManager::MakeFakeLocation(kUniform2FakeLocation, 2),
            program_info->GetUniformFakeLocation("uniform2[2]"));
  EXPECT_EQ(-1, program_info->GetUniformFakeLocation("uniform2[3]"));
  EXPECT_EQ(ProgramManager::MakeFakeLocation(kUniform3FakeLocation, 1),
            program_info->GetUniformFakeLocation("uniform3[1]"));
  EXPECT_EQ(-1, program_info->GetUniformFakeLocation("uniform3[2]"));
}

TEST_F(ProgramManagerWithShaderTest, GetUniformInfoByFakeLocation) {
  const GLint kInvalidLocation = 1234;
  const Program::UniformInfo* info;
  const Program* program_info =
      manager_.GetProgram(kClientProgramId);
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
      kUniform1Size,
      kUniform1Type,
      kUniform1FakeLocation,
      kUniform1RealLocation,
      kUniform1DesiredLocation,
      kUniform1Name,
    },
    { kUniform2Name,
      kUniform2Size,
      kUniform2Type,
      kUniform2FakeLocation,
      kUniform2RealLocation,
      kUniform2DesiredLocation,
      kUniform2Name,
    },
    { kUniform3BadName,
      kUniform3Size,
      kUniform3Type,
      kUniform3FakeLocation,
      kUniform3RealLocation,
      kUniform3DesiredLocation,
      kUniform3GoodName,
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
  Shader* vshader = shader_manager_.CreateShader(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, "", NULL);
  Shader* fshader = shader_manager_.CreateShader(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != NULL);
  fshader->SetStatus(true, "", NULL);
  Program* program_info =
      manager_.CreateProgram(kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, fshader));
  program_info->Link(NULL, NULL, NULL, NULL);
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

// Test the bug comparing similar array names is fixed.
TEST_F(ProgramManagerWithShaderTest, SimilarArrayNames) {
  static const char* kUniform2Name = "u_nameLong[0]";
  static const char* kUniform3Name = "u_name[0]";
  static const GLint kUniform2Size = 2;
  static const GLint kUniform3Size = 2;
  static ProgramManagerWithShaderTest::UniformInfo kUniforms[] = {
    { kUniform1Name,
      kUniform1Size,
      kUniform1Type,
      kUniform1FakeLocation,
      kUniform1RealLocation,
      kUniform1DesiredLocation,
      kUniform1Name,
    },
    { kUniform2Name,
      kUniform2Size,
      kUniform2Type,
      kUniform2FakeLocation,
      kUniform2RealLocation,
      kUniform2DesiredLocation,
      kUniform2Name,
    },
    { kUniform3Name,
      kUniform3Size,
      kUniform3Type,
      kUniform3FakeLocation,
      kUniform3RealLocation,
      kUniform3DesiredLocation,
      kUniform3Name,
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
  Shader* vshader = shader_manager_.CreateShader(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, "", NULL);
  Shader* fshader = shader_manager_.CreateShader(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != NULL);
  fshader->SetStatus(true, "", NULL);
  Program* program_info =
      manager_.CreateProgram(kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, fshader));
  program_info->Link(NULL, NULL, NULL, NULL);

  // Check that we get the correct locations.
  EXPECT_EQ(kUniform2FakeLocation,
            program_info->GetUniformFakeLocation(kUniform2Name));
  EXPECT_EQ(kUniform3FakeLocation,
            program_info->GetUniformFakeLocation(kUniform3Name));
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
  ShaderTranslator::NameMap name_map;
  EXPECT_CALL(shader_translator, name_map())
      .WillRepeatedly(ReturnRef(name_map));
  const GLuint kVShaderClientId = 2001;
  const GLuint kFShaderClientId = 2002;
  const GLuint kVShaderServiceId = 3001;
  const GLuint kFShaderServiceId = 3002;
  Shader* vshader = shader_manager_.CreateShader(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, "", &shader_translator);
  Shader* fshader = shader_manager_.CreateShader(
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
      kUniform1Size,
      kUniform1Type,
      kUniform1FakeLocation,
      kUniform1RealLocation,
      kUniform1DesiredLocation,
      kUniform1Name,
    },
    { kUniform2Name,
      kUniform2Size,
      kUniform2BadType,
      kUniform2FakeLocation,
      kUniform2RealLocation,
      kUniform2DesiredLocation,
      kUniform2Name,
    },
    { kUniform3BadName,
      kUniform3Size,
      kUniform3Type,
      kUniform3FakeLocation,
      kUniform3RealLocation,
      kUniform3DesiredLocation,
      kUniform3GoodName,
    },
  };
  const size_t kNumAttribs= arraysize(kAttribs);
  const size_t kNumUniforms = arraysize(kUniforms);
  static const GLuint kClientProgramId = 1234;
  static const GLuint kServiceProgramId = 5679;
  SetupShader(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
              kServiceProgramId);
  Program* program_info = manager_.CreateProgram(
      kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, fshader));
  program_info->Link(NULL, NULL, NULL, NULL);
  // Check that we got the good type, not the bad.
  // Check Attribs
  for (unsigned index = 0; index < kNumAttribs; ++index) {
    const Program::VertexAttrib* attrib_info =
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
    const Program::UniformInfo* uniform_info =
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
  Program* program_info = manager_.CreateProgram(
      kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_FALSE(program_info->CanLink());
  const GLuint kVShaderClientId = 2001;
  const GLuint kFShaderClientId = 2002;
  const GLuint kVShaderServiceId = 3001;
  const GLuint kFShaderServiceId = 3002;
  Shader* vshader = shader_manager_.CreateShader(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, "", NULL);
  Shader* fshader = shader_manager_.CreateShader(
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
  Program* info2 =
      manager_.GetProgram(kClientProgramId);
  EXPECT_EQ(program_info, info2);
  manager_.UnuseProgram(&shader_manager_, program_info);
  EXPECT_TRUE(program_info->InUse());
  // this should delete the info.
  EXPECT_CALL(*gl_, DeleteProgram(kServiceProgramId))
      .Times(1)
      .RetiresOnSaturation();
  manager_.UnuseProgram(&shader_manager_, program_info);
  info2 = manager_.GetProgram(kClientProgramId);
  EXPECT_TRUE(info2 == NULL);
  EXPECT_FALSE(vshader->InUse());
  EXPECT_FALSE(fshader->InUse());
}

TEST_F(ProgramManagerWithShaderTest, ProgramInfoUseCount2) {
  static const GLuint kClientProgramId = 124;
  static const GLuint kServiceProgramId = 457;
  Program* program_info = manager_.CreateProgram(
      kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_FALSE(program_info->CanLink());
  const GLuint kVShaderClientId = 2001;
  const GLuint kFShaderClientId = 2002;
  const GLuint kVShaderServiceId = 3001;
  const GLuint kFShaderServiceId = 3002;
  Shader* vshader = shader_manager_.CreateShader(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, "", NULL);
  Shader* fshader = shader_manager_.CreateShader(
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
  Program* info2 =
      manager_.GetProgram(kClientProgramId);
  EXPECT_EQ(program_info, info2);
  // this should delete the program.
  EXPECT_CALL(*gl_, DeleteProgram(kServiceProgramId))
      .Times(1)
      .RetiresOnSaturation();
  manager_.MarkAsDeleted(&shader_manager_, program_info);
  info2 = manager_.GetProgram(kClientProgramId);
  EXPECT_TRUE(info2 == NULL);
  EXPECT_FALSE(vshader->InUse());
  EXPECT_FALSE(fshader->InUse());
}

TEST_F(ProgramManagerWithShaderTest, ProgramInfoGetProgramInfo) {
  CommonDecoder::Bucket bucket;
  const Program* program_info =
      manager_.GetProgram(kClientProgramId);
  ASSERT_TRUE(program_info != NULL);
  program_info->GetProgram(&manager_, &bucket);
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
      EXPECT_EQ(
          ProgramManager::MakeFakeLocation(expected.fake_location, jj),
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
  ShaderTranslator::NameMap name_map;
  EXPECT_CALL(shader_translator, name_map())
      .WillRepeatedly(ReturnRef(name_map));
  // Check we can create shader.
  Shader* vshader = shader_manager_.CreateShader(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  Shader* fshader = shader_manager_.CreateShader(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  // Check shader got created.
  ASSERT_TRUE(vshader != NULL && fshader != NULL);
  // Set Status
  vshader->SetStatus(true, "", &shader_translator);
  // Check attrib infos got copied.
  for (ShaderTranslator::VariableMap::const_iterator it = attrib_map.begin();
       it != attrib_map.end(); ++it) {
    const Shader::VariableInfo* variable_info =
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
  Program* program_info =
      manager_.CreateProgram(kClientProgramId, kServiceProgramId);
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
  Shader* vshader = shader_manager_.CreateShader(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, NULL, NULL);
  Shader* fshader = shader_manager_.CreateShader(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != NULL);
  fshader->SetStatus(true, NULL, NULL);
  static const GLuint kClientProgramId = 1234;
  static const GLuint kServiceProgramId = 5679;
  Program* program_info = manager_.CreateProgram(
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
        kUniform1Size,
        kUniform1Type,
        kUniform1FakeLocation,
        kUniform1RealLocation,
        kUniform1DesiredLocation,
        kUniform1Name,
      },
      { kUniform2Name,
        kUniform2Size,
        kSamplerTypes[ii],
        kUniform2FakeLocation,
        kUniform2RealLocation,
        kUniform2DesiredLocation,
        kUniform2Name,
      },
      { kUniform3BadName,
        kUniform3Size,
        kUniform3Type,
        kUniform3FakeLocation,
        kUniform3RealLocation,
        kUniform3DesiredLocation,
        kUniform3GoodName,
      },
    };
    const size_t kNumAttribs = arraysize(kAttribs);
    const size_t kNumUniforms = arraysize(kUniforms);
    SetupShader(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
                kServiceProgramId);
    program_info->Link(NULL, NULL, NULL, NULL);
    SetupExpectationsForClearingUniforms(kUniforms, kNumUniforms);
    manager_.ClearUniforms(program_info);
  }
}

TEST_F(ProgramManagerWithShaderTest, BindUniformLocation) {
  const GLuint kVShaderClientId = 2001;
  const GLuint kFShaderClientId = 2002;
  const GLuint kVShaderServiceId = 3001;
  const GLuint kFShaderServiceId = 3002;

  const GLint kUniform1DesiredLocation = 10;
  const GLint kUniform2DesiredLocation = -1;
  const GLint kUniform3DesiredLocation = 5;

  Shader* vshader = shader_manager_.CreateShader(
      kVShaderClientId, kVShaderServiceId, GL_VERTEX_SHADER);
  ASSERT_TRUE(vshader != NULL);
  vshader->SetStatus(true, NULL, NULL);
  Shader* fshader = shader_manager_.CreateShader(
      kFShaderClientId, kFShaderServiceId, GL_FRAGMENT_SHADER);
  ASSERT_TRUE(fshader != NULL);
  fshader->SetStatus(true, NULL, NULL);
  static const GLuint kClientProgramId = 1234;
  static const GLuint kServiceProgramId = 5679;
  Program* program_info = manager_.CreateProgram(
      kClientProgramId, kServiceProgramId);
  ASSERT_TRUE(program_info != NULL);
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, vshader));
  EXPECT_TRUE(program_info->AttachShader(&shader_manager_, fshader));
  EXPECT_TRUE(program_info->SetUniformLocationBinding(
      kUniform1Name, kUniform1DesiredLocation));
  EXPECT_TRUE(program_info->SetUniformLocationBinding(
      kUniform3BadName, kUniform3DesiredLocation));

  static ProgramManagerWithShaderTest::AttribInfo kAttribs[] = {
    { kAttrib1Name, kAttrib1Size, kAttrib1Type, kAttrib1Location, },
    { kAttrib2Name, kAttrib2Size, kAttrib2Type, kAttrib2Location, },
    { kAttrib3Name, kAttrib3Size, kAttrib3Type, kAttrib3Location, },
  };
  ProgramManagerWithShaderTest::UniformInfo kUniforms[] = {
    { kUniform1Name,
      kUniform1Size,
      kUniform1Type,
      kUniform1FakeLocation,
      kUniform1RealLocation,
      kUniform1DesiredLocation,
      kUniform1Name,
    },
    { kUniform2Name,
      kUniform2Size,
      kUniform2Type,
      kUniform2FakeLocation,
      kUniform2RealLocation,
      kUniform2DesiredLocation,
      kUniform2Name,
    },
    { kUniform3BadName,
      kUniform3Size,
      kUniform3Type,
      kUniform3FakeLocation,
      kUniform3RealLocation,
      kUniform3DesiredLocation,
      kUniform3GoodName,
    },
  };

  const size_t kNumAttribs = arraysize(kAttribs);
  const size_t kNumUniforms = arraysize(kUniforms);
  SetupShader(kAttribs, kNumAttribs, kUniforms, kNumUniforms,
              kServiceProgramId);
  program_info->Link(NULL, NULL, NULL, NULL);

  EXPECT_EQ(kUniform1DesiredLocation,
            program_info->GetUniformFakeLocation(kUniform1Name));
  EXPECT_EQ(kUniform3DesiredLocation,
            program_info->GetUniformFakeLocation(kUniform3BadName));
  EXPECT_EQ(kUniform3DesiredLocation,
            program_info->GetUniformFakeLocation(kUniform3GoodName));
}

class ProgramManagerWithCacheTest : public testing::Test {
 public:
  static const GLuint kClientProgramId = 1;
  static const GLuint kServiceProgramId = 10;
  static const GLuint kVertexShaderClientId = 2;
  static const GLuint kFragmentShaderClientId = 20;
  static const GLuint kVertexShaderServiceId = 3;
  static const GLuint kFragmentShaderServiceId = 30;

  ProgramManagerWithCacheTest()
      : cache_(new MockProgramCache()),
        manager_(cache_.get()),
        vertex_shader_(NULL),
        fragment_shader_(NULL),
        program_info_(NULL) {
  }
  virtual ~ProgramManagerWithCacheTest() {
    manager_.Destroy(false);
    shader_manager_.Destroy(false);
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new StrictMock<gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());

    vertex_shader_ = shader_manager_.CreateShader(
       kVertexShaderClientId, kVertexShaderServiceId, GL_VERTEX_SHADER);
    fragment_shader_ = shader_manager_.CreateShader(
       kFragmentShaderClientId, kFragmentShaderServiceId, GL_FRAGMENT_SHADER);
    ASSERT_TRUE(vertex_shader_ != NULL);
    ASSERT_TRUE(fragment_shader_ != NULL);
    vertex_shader_->UpdateSource("lka asjf bjajsdfj");
    fragment_shader_->UpdateSource("lka asjf a   fasgag 3rdsf3 bjajsdfj");

    program_info_ = manager_.CreateProgram(
        kClientProgramId, kServiceProgramId);
    ASSERT_TRUE(program_info_ != NULL);

    program_info_->AttachShader(&shader_manager_, vertex_shader_);
    program_info_->AttachShader(&shader_manager_, fragment_shader_);
  }

  virtual void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
  }

  void SetShadersCompiled() {
    cache_->ShaderCompilationSucceeded(*vertex_shader_->source());
    cache_->ShaderCompilationSucceeded(*fragment_shader_->source());
    vertex_shader_->SetStatus(true, NULL, NULL);
    fragment_shader_->SetStatus(true, NULL, NULL);
    vertex_shader_->FlagSourceAsCompiled(true);
    fragment_shader_->FlagSourceAsCompiled(true);
  }

  void SetShadersNotCompiledButCached() {
    SetShadersCompiled();
    vertex_shader_->FlagSourceAsCompiled(false);
    fragment_shader_->FlagSourceAsCompiled(false);
  }

  void SetProgramCached() {
    cache_->LinkedProgramCacheSuccess(
        vertex_shader_->source()->c_str(),
        fragment_shader_->source()->c_str(),
        &program_info_->bind_attrib_location_map());
  }

  void SetExpectationsForProgramCached() {
    SetExpectationsForProgramCached(program_info_,
                                    vertex_shader_,
                                    fragment_shader_);
  }

  void SetExpectationsForProgramCached(
      Program* program_info,
      Shader* vertex_shader,
      Shader* fragment_shader) {
    EXPECT_CALL(*cache_.get(), SaveLinkedProgram(
        program_info->service_id(),
        vertex_shader,
        fragment_shader,
        &program_info->bind_attrib_location_map())).Times(1);
  }

  void SetExpectationsForNotCachingProgram() {
    SetExpectationsForNotCachingProgram(program_info_,
                                        vertex_shader_,
                                        fragment_shader_);
  }

  void SetExpectationsForNotCachingProgram(
      Program* program_info,
      Shader* vertex_shader,
      Shader* fragment_shader) {
    EXPECT_CALL(*cache_.get(), SaveLinkedProgram(
        program_info->service_id(),
        vertex_shader,
        fragment_shader,
        &program_info->bind_attrib_location_map())).Times(0);
  }

  void SetExpectationsForProgramLoad(ProgramCache::ProgramLoadResult result) {
    SetExpectationsForProgramLoad(kServiceProgramId,
                                  program_info_,
                                  vertex_shader_,
                                  fragment_shader_,
                                  result);
  }

  void SetExpectationsForProgramLoad(
      GLuint service_program_id,
      Program* program_info,
      Shader* vertex_shader,
      Shader* fragment_shader,
      ProgramCache::ProgramLoadResult result) {
    EXPECT_CALL(*cache_.get(),
                LoadLinkedProgram(service_program_id,
                                  vertex_shader,
                                  fragment_shader,
                                  &program_info->bind_attrib_location_map()))
        .WillOnce(Return(result));
  }

  void SetExpectationsForProgramLoadSuccess() {
    SetExpectationsForProgramLoadSuccess(kServiceProgramId);
  }

  void SetExpectationsForProgramLoadSuccess(GLuint service_program_id) {
    TestHelper::SetupProgramSuccessExpectations(gl_.get(),
                                                NULL,
                                                0,
                                                NULL,
                                                0,
                                                service_program_id);
  }

  void SetExpectationsForProgramLink() {
    SetExpectationsForProgramLink(kServiceProgramId);
  }

  void SetExpectationsForProgramLink(GLuint service_program_id) {
    TestHelper::SetupShader(gl_.get(), NULL, 0, NULL, 0, service_program_id);
    if (gfx::g_driver_gl.ext.b_GL_ARB_get_program_binary) {
      EXPECT_CALL(*gl_.get(),
                  ProgramParameteri(service_program_id,
                                    PROGRAM_BINARY_RETRIEVABLE_HINT,
                                    GL_TRUE)).Times(1);
    }
  }

  void SetExpectationsForSuccessCompile(
      const Shader* shader) {
    const GLuint shader_id = shader->service_id();
    const char* src = shader->source()->c_str();
    EXPECT_CALL(*gl_.get(),
                ShaderSource(shader_id, 1, Pointee(src), NULL)).Times(1);
    EXPECT_CALL(*gl_.get(), CompileShader(shader_id)).Times(1);
    EXPECT_CALL(*gl_.get(), GetShaderiv(shader_id, GL_COMPILE_STATUS, _))
      .WillOnce(SetArgumentPointee<2>(GL_TRUE));
  }

  void SetExpectationsForNoCompile(const Shader* shader) {
    const GLuint shader_id = shader->service_id();
    const char* src = shader->source()->c_str();
    EXPECT_CALL(*gl_.get(),
                ShaderSource(shader_id, 1, Pointee(src), NULL)).Times(0);
    EXPECT_CALL(*gl_.get(), CompileShader(shader_id)).Times(0);
    EXPECT_CALL(*gl_.get(), GetShaderiv(shader_id, GL_COMPILE_STATUS, _))
        .Times(0);
  }

  void SetExpectationsForErrorCompile(const Shader* shader) {
    const GLuint shader_id = shader->service_id();
    const char* src = shader->source()->c_str();
    EXPECT_CALL(*gl_.get(),
                ShaderSource(shader_id, 1, Pointee(src), NULL)).Times(1);
    EXPECT_CALL(*gl_.get(), CompileShader(shader_id)).Times(1);
    EXPECT_CALL(*gl_.get(), GetShaderiv(shader_id, GL_COMPILE_STATUS, _))
      .WillOnce(SetArgumentPointee<2>(GL_FALSE));
    EXPECT_CALL(*gl_.get(), GetShaderiv(shader_id, GL_INFO_LOG_LENGTH, _))
      .WillOnce(SetArgumentPointee<2>(0));
    EXPECT_CALL(*gl_.get(), GetShaderInfoLog(shader_id, 0, _, _))
      .Times(1);
  }

  scoped_ptr<StrictMock<gfx::MockGLInterface> > gl_;

  scoped_ptr<MockProgramCache> cache_;
  ProgramManager manager_;

  Shader* vertex_shader_;
  Shader* fragment_shader_;
  Program* program_info_;
  ShaderManager shader_manager_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLuint ProgramManagerWithCacheTest::kClientProgramId;
const GLuint ProgramManagerWithCacheTest::kServiceProgramId;
const GLuint ProgramManagerWithCacheTest::kVertexShaderClientId;
const GLuint ProgramManagerWithCacheTest::kFragmentShaderClientId;
const GLuint ProgramManagerWithCacheTest::kVertexShaderServiceId;
const GLuint ProgramManagerWithCacheTest::kFragmentShaderServiceId;
#endif

TEST_F(ProgramManagerWithCacheTest, CacheSuccessAfterShaderCompile) {
  SetExpectationsForSuccessCompile(vertex_shader_);
  scoped_refptr<FeatureInfo> info(new FeatureInfo());
  manager_.DoCompileShader(vertex_shader_, NULL, info.get());
  EXPECT_EQ(ProgramCache::COMPILATION_SUCCEEDED,
            cache_->GetShaderCompilationStatus(*vertex_shader_->source()));
}

TEST_F(ProgramManagerWithCacheTest, CacheUnknownAfterShaderError) {
  SetExpectationsForErrorCompile(vertex_shader_);
  scoped_refptr<FeatureInfo> info(new FeatureInfo());
  manager_.DoCompileShader(vertex_shader_, NULL, info.get());
  EXPECT_EQ(ProgramCache::COMPILATION_UNKNOWN,
            cache_->GetShaderCompilationStatus(*vertex_shader_->source()));
}

TEST_F(ProgramManagerWithCacheTest, NoCompileWhenShaderCached) {
  cache_->ShaderCompilationSucceeded(vertex_shader_->source()->c_str());
  SetExpectationsForNoCompile(vertex_shader_);
  scoped_refptr<FeatureInfo> info(new FeatureInfo());
  manager_.DoCompileShader(vertex_shader_, NULL, info.get());
}

TEST_F(ProgramManagerWithCacheTest, CacheProgramOnSuccessfulLink) {
  SetShadersCompiled();
  SetExpectationsForProgramLink();
  SetExpectationsForProgramCached();
  EXPECT_TRUE(program_info_->Link(NULL, NULL, NULL, NULL));
}

TEST_F(ProgramManagerWithCacheTest, CompileShaderOnLinkCacheMiss) {
  SetShadersCompiled();
  vertex_shader_->FlagSourceAsCompiled(false);

  scoped_refptr<FeatureInfo> info(new FeatureInfo());

  SetExpectationsForSuccessCompile(vertex_shader_);
  SetExpectationsForProgramLink();
  SetExpectationsForProgramCached();
  EXPECT_TRUE(program_info_->Link(&shader_manager_, NULL, NULL, info.get()));
}

TEST_F(ProgramManagerWithCacheTest, LoadProgramOnProgramCacheHit) {
  SetShadersNotCompiledButCached();
  SetProgramCached();

  SetExpectationsForNoCompile(vertex_shader_);
  SetExpectationsForNoCompile(fragment_shader_);
  SetExpectationsForProgramLoad(ProgramCache::PROGRAM_LOAD_SUCCESS);
  SetExpectationsForNotCachingProgram();
  SetExpectationsForProgramLoadSuccess();

  EXPECT_TRUE(program_info_->Link(NULL, NULL, NULL, NULL));
}

TEST_F(ProgramManagerWithCacheTest, CompileAndLinkOnProgramCacheError) {
  SetShadersNotCompiledButCached();
  SetProgramCached();

  SetExpectationsForSuccessCompile(vertex_shader_);
  SetExpectationsForSuccessCompile(fragment_shader_);
  SetExpectationsForProgramLoad(ProgramCache::PROGRAM_LOAD_FAILURE);
  SetExpectationsForProgramLink();
  SetExpectationsForProgramCached();

  scoped_refptr<FeatureInfo> info(new FeatureInfo());
  EXPECT_TRUE(program_info_->Link(&shader_manager_, NULL, NULL, info.get()));
}

TEST_F(ProgramManagerWithCacheTest, CorrectCompileOnSourceChangeNoCompile) {
  SetShadersNotCompiledButCached();
  SetProgramCached();

  const GLuint kNewShaderClientId = 4;
  const GLuint kNewShaderServiceId = 40;
  const GLuint kNewProgramClientId = 5;
  const GLuint kNewProgramServiceId = 50;

  Shader* new_vertex_shader =
      shader_manager_.CreateShader(kNewShaderClientId,
                                       kNewShaderServiceId,
                                       GL_VERTEX_SHADER);

  const std::string original_source = *vertex_shader_->source();
  new_vertex_shader->UpdateSource(original_source.c_str());

  Program* program_info = manager_.CreateProgram(
      kNewProgramClientId, kNewProgramServiceId);
  ASSERT_TRUE(program_info != NULL);
  program_info->AttachShader(&shader_manager_, new_vertex_shader);
  program_info->AttachShader(&shader_manager_, fragment_shader_);

  SetExpectationsForNoCompile(new_vertex_shader);

  manager_.DoCompileShader(new_vertex_shader, NULL, NULL);
  EXPECT_EQ(Shader::PENDING_DEFERRED_COMPILE,
            new_vertex_shader->compilation_status());

  new_vertex_shader->UpdateSource("different!");
  EXPECT_EQ(original_source,
            *new_vertex_shader->deferred_compilation_source());

  EXPECT_EQ(Shader::PENDING_DEFERRED_COMPILE,
            new_vertex_shader->compilation_status());
  EXPECT_EQ(Shader::PENDING_DEFERRED_COMPILE,
            fragment_shader_->compilation_status());

  SetExpectationsForNoCompile(fragment_shader_);
  SetExpectationsForNotCachingProgram(program_info,
                                      new_vertex_shader,
                                      fragment_shader_);
  SetExpectationsForProgramLoad(kNewProgramServiceId,
                                program_info,
                                new_vertex_shader,
                                fragment_shader_,
                                ProgramCache::PROGRAM_LOAD_SUCCESS);
  SetExpectationsForProgramLoadSuccess(kNewProgramServiceId);

  scoped_refptr<FeatureInfo> info(new FeatureInfo());
  EXPECT_TRUE(program_info->Link(&shader_manager_, NULL, NULL, info.get()));
}

TEST_F(ProgramManagerWithCacheTest, CorrectCompileOnSourceChangeWithCompile) {
  SetShadersNotCompiledButCached();
  SetProgramCached();

  const GLuint kNewShaderClientId = 4;
  const GLuint kNewShaderServiceId = 40;
  const GLuint kNewProgramClientId = 5;
  const GLuint kNewProgramServiceId = 50;

  Shader* new_vertex_shader =
      shader_manager_.CreateShader(kNewShaderClientId,
                                       kNewShaderServiceId,
                                       GL_VERTEX_SHADER);

  new_vertex_shader->UpdateSource(vertex_shader_->source()->c_str());

  Program* program_info = manager_.CreateProgram(
      kNewProgramClientId, kNewProgramServiceId);
  ASSERT_TRUE(program_info != NULL);
  program_info->AttachShader(&shader_manager_, new_vertex_shader);
  program_info->AttachShader(&shader_manager_, fragment_shader_);

  SetExpectationsForNoCompile(new_vertex_shader);

  manager_.DoCompileShader(new_vertex_shader, NULL, NULL);

  const std::string differentSource = "different!";
  new_vertex_shader->UpdateSource(differentSource.c_str());
  SetExpectationsForSuccessCompile(new_vertex_shader);

  scoped_refptr<FeatureInfo> info(new FeatureInfo());
  manager_.DoCompileShader(new_vertex_shader, NULL, info.get());
  EXPECT_EQ(differentSource,
            *new_vertex_shader->deferred_compilation_source());

  EXPECT_EQ(Shader::COMPILED,
            new_vertex_shader->compilation_status());
  EXPECT_EQ(Shader::PENDING_DEFERRED_COMPILE,
            fragment_shader_->compilation_status());

  // so we don't recompile because we were pending originally
  SetExpectationsForNoCompile(new_vertex_shader);
  SetExpectationsForSuccessCompile(fragment_shader_);
  SetExpectationsForProgramCached(program_info,
                                  new_vertex_shader,
                                  fragment_shader_);
  SetExpectationsForProgramLink(kNewProgramServiceId);

  EXPECT_TRUE(program_info->Link(&shader_manager_, NULL, NULL, info.get()));
}

}  // namespace gles2
}  // namespace gpu
