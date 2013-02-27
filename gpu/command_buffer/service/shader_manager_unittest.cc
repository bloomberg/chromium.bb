// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_manager.h"

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/mocks.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::Return;
using ::testing::ReturnRef;

namespace gpu {
namespace gles2 {

class ShaderManagerTest : public testing::Test {
 public:
  ShaderManagerTest() {
  }

  virtual ~ShaderManagerTest() {
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
  ShaderManager manager_;
};

TEST_F(ShaderManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kShader1Type = GL_VERTEX_SHADER;
  const GLuint kClient2Id = 2;
  // Check we can create shader.
  Shader* info0 = manager_.CreateShader(
      kClient1Id, kService1Id, kShader1Type);
  // Check shader got created.
  ASSERT_TRUE(info0 != NULL);
  Shader* info1 = manager_.GetShader(kClient1Id);
  ASSERT_EQ(info0, info1);
  // Check we get nothing for a non-existent shader.
  EXPECT_TRUE(manager_.GetShader(kClient2Id) == NULL);
  // Check we can't get the shader after we remove it.
  manager_.MarkAsDeleted(info1);
  EXPECT_TRUE(manager_.GetShader(kClient1Id) == NULL);
}

TEST_F(ShaderManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kShader1Type = GL_VERTEX_SHADER;
  // Check we can create shader.
  Shader* info1 = manager_.CreateShader(
      kClient1Id, kService1Id, kShader1Type);
  // Check shader got created.
  ASSERT_TRUE(info1 != NULL);
  EXPECT_CALL(*gl_, DeleteShader(kService1Id))
      .Times(1)
      .RetiresOnSaturation();
  manager_.Destroy(true);
  // Check that resources got freed.
  info1 = manager_.GetShader(kClient1Id);
  ASSERT_TRUE(info1 == NULL);
}

TEST_F(ShaderManagerTest, DeleteBug) {
  const GLuint kClient1Id = 1;
  const GLuint kClient2Id = 2;
  const GLuint kService1Id = 11;
  const GLuint kService2Id = 12;
  const GLenum kShaderType = GL_VERTEX_SHADER;
  // Check we can create shader.
  scoped_refptr<Shader> info1(
      manager_.CreateShader(kClient1Id, kService1Id, kShaderType));
  scoped_refptr<Shader> info2(
      manager_.CreateShader(kClient2Id, kService2Id, kShaderType));
  ASSERT_TRUE(info1);
  ASSERT_TRUE(info2);
  manager_.UseShader(info1);
  manager_.MarkAsDeleted(info1);
  manager_.MarkAsDeleted(info2);
  EXPECT_TRUE(manager_.IsOwned(info1));
  EXPECT_FALSE(manager_.IsOwned(info2));
}

TEST_F(ShaderManagerTest, Shader) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kShader1Type = GL_VERTEX_SHADER;
  const char* kClient1Source = "hello world";
  // Check we can create shader.
  Shader* info1 = manager_.CreateShader(
      kClient1Id, kService1Id, kShader1Type);
  // Check shader got created.
  ASSERT_TRUE(info1 != NULL);
  EXPECT_EQ(kService1Id, info1->service_id());
  // Check if the shader has correct type.
  EXPECT_EQ(kShader1Type, info1->shader_type());
  EXPECT_FALSE(info1->IsValid());
  EXPECT_FALSE(info1->InUse());
  EXPECT_TRUE(info1->source() == NULL);
  EXPECT_TRUE(info1->log_info() == NULL);
  const char* kLog = "foo";
  info1->SetStatus(true, kLog, NULL);
  EXPECT_TRUE(info1->IsValid());
  EXPECT_STREQ(kLog, info1->log_info()->c_str());
  // Check we can set its source.
  info1->UpdateSource(kClient1Source);
  EXPECT_STREQ(kClient1Source, info1->source()->c_str());
  EXPECT_EQ(NULL, info1->translated_source());
  // Check we can set its translated source.
  info1->UpdateTranslatedSource(kClient1Source);
  EXPECT_STREQ(kClient1Source,
               info1->translated_source()->c_str());
}

TEST_F(ShaderManagerTest, GetInfo) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kShader1Type = GL_VERTEX_SHADER;
  const GLenum kAttrib1Type = GL_FLOAT_VEC2;
  const GLsizei kAttrib1Size = 2;
  const char* kAttrib1Name = "attr1";
  const GLenum kAttrib2Type = GL_FLOAT_VEC3;
  const GLsizei kAttrib2Size = 4;
  const char* kAttrib2Name = "attr2";
  const GLenum kUniform1Type = GL_FLOAT_MAT2;
  const GLsizei kUniform1Size = 3;
  const char* kUniform1Name = "uni1";
  const GLenum kUniform2Type = GL_FLOAT_MAT3;
  const GLsizei kUniform2Size = 5;
  const char* kUniform2Name = "uni2";
  MockShaderTranslator shader_translator;
  ShaderTranslator::VariableMap attrib_map;
  attrib_map[kAttrib1Name] = ShaderTranslatorInterface::VariableInfo(
      kAttrib1Type, kAttrib1Size, kAttrib1Name);
  attrib_map[kAttrib2Name] = ShaderTranslatorInterface::VariableInfo(
      kAttrib2Type, kAttrib2Size, kAttrib2Name);
  ShaderTranslator::VariableMap uniform_map;
  uniform_map[kUniform1Name] = ShaderTranslatorInterface::VariableInfo(
      kUniform1Type, kUniform1Size, kUniform1Name);
  uniform_map[kUniform2Name] = ShaderTranslatorInterface::VariableInfo(
      kUniform2Type, kUniform2Size, kUniform2Name);
  EXPECT_CALL(shader_translator, attrib_map())
      .WillRepeatedly(ReturnRef(attrib_map));
  EXPECT_CALL(shader_translator, uniform_map())
      .WillRepeatedly(ReturnRef(uniform_map));
  ShaderTranslator::NameMap name_map;
  EXPECT_CALL(shader_translator, name_map())
      .WillRepeatedly(ReturnRef(name_map));
  // Check we can create shader.
  Shader* info1 = manager_.CreateShader(
      kClient1Id, kService1Id, kShader1Type);
  // Check shader got created.
  ASSERT_TRUE(info1 != NULL);
  // Set Status
  info1->SetStatus(true, "", &shader_translator);
  // Check attrib and uniform infos got copied.
  for (ShaderTranslator::VariableMap::const_iterator it = attrib_map.begin();
       it != attrib_map.end(); ++it) {
    const Shader::VariableInfo* variable_info =
        info1->GetAttribInfo(it->first);
    ASSERT_TRUE(variable_info != NULL);
    EXPECT_EQ(it->second.type, variable_info->type);
    EXPECT_EQ(it->second.size, variable_info->size);
    EXPECT_EQ(it->second.name, variable_info->name);
  }
  for (ShaderTranslator::VariableMap::const_iterator it = uniform_map.begin();
       it != uniform_map.end(); ++it) {
    const Shader::VariableInfo* variable_info =
        info1->GetUniformInfo(it->first);
    ASSERT_TRUE(variable_info != NULL);
    EXPECT_EQ(it->second.type, variable_info->type);
    EXPECT_EQ(it->second.size, variable_info->size);
    EXPECT_EQ(it->second.name, variable_info->name);
  }
  // Check attrib and uniform get cleared.
  info1->SetStatus(true, NULL, NULL);
  EXPECT_TRUE(info1->log_info() == NULL);
  for (ShaderTranslator::VariableMap::const_iterator it = attrib_map.begin();
       it != attrib_map.end(); ++it) {
    const Shader::VariableInfo* variable_info =
        info1->GetAttribInfo(it->first);
    EXPECT_TRUE(variable_info == NULL);
  }
  for (ShaderTranslator::VariableMap::const_iterator it = uniform_map.begin();
       it != uniform_map.end(); ++it) {
    const Shader::VariableInfo* variable_info =
        info1->GetUniformInfo(it->first);
    ASSERT_TRUE(variable_info == NULL);
  }
}

TEST_F(ShaderManagerTest, ShaderInfoUseCount) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kShader1Type = GL_VERTEX_SHADER;
  // Check we can create shader.
  Shader* info1 = manager_.CreateShader(
      kClient1Id, kService1Id, kShader1Type);
  // Check shader got created.
  ASSERT_TRUE(info1 != NULL);
  EXPECT_FALSE(info1->InUse());
  EXPECT_FALSE(info1->IsDeleted());
  manager_.UseShader(info1);
  EXPECT_TRUE(info1->InUse());
  manager_.UseShader(info1);
  EXPECT_TRUE(info1->InUse());
  manager_.MarkAsDeleted(info1);
  EXPECT_TRUE(info1->IsDeleted());
  Shader* info2 = manager_.GetShader(kClient1Id);
  EXPECT_EQ(info1, info2);
  manager_.UnuseShader(info1);
  EXPECT_TRUE(info1->InUse());
  manager_.UnuseShader(info1);  // this should delete the info.
  info2 = manager_.GetShader(kClient1Id);
  EXPECT_TRUE(info2 == NULL);

  info1 = manager_.CreateShader(kClient1Id, kService1Id, kShader1Type);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_FALSE(info1->InUse());
  manager_.UseShader(info1);
  EXPECT_TRUE(info1->InUse());
  manager_.UseShader(info1);
  EXPECT_TRUE(info1->InUse());
  manager_.UnuseShader(info1);
  EXPECT_TRUE(info1->InUse());
  manager_.UnuseShader(info1);
  EXPECT_FALSE(info1->InUse());
  info2 = manager_.GetShader(kClient1Id);
  EXPECT_EQ(info1, info2);
  manager_.MarkAsDeleted(info1);  // this should delete the shader.
  info2 = manager_.GetShader(kClient1Id);
  EXPECT_TRUE(info2 == NULL);
}

TEST_F(ShaderManagerTest, ShaderInfoStoreCompilationStatus) {
  const GLuint kClientId = 1;
  const GLuint kServiceId = 11;
  const GLenum kShaderType = GL_VERTEX_SHADER;
  Shader* info = manager_.CreateShader(
      kClientId, kServiceId, kShaderType);
  ASSERT_TRUE(info != NULL);

  EXPECT_EQ(Shader::NOT_COMPILED,
            info->compilation_status());
  info->UpdateSource("original source");
  EXPECT_EQ(Shader::NOT_COMPILED,
            info->compilation_status());
  info->FlagSourceAsCompiled(false);
  EXPECT_EQ(Shader::PENDING_DEFERRED_COMPILE,
            info->compilation_status());
  info->FlagSourceAsCompiled(true);
  EXPECT_EQ(Shader::COMPILED,
            info->compilation_status());
}

TEST_F(ShaderManagerTest, ShaderInfoStoreDeferredSource) {
  const GLuint kClientId = 1;
  const GLuint kServiceId = 11;
  const GLenum kShaderType = GL_VERTEX_SHADER;
  Shader* info = manager_.CreateShader(
      kClientId, kServiceId, kShaderType);
  ASSERT_TRUE(info != NULL);

  info->UpdateSource("original source");
  info->FlagSourceAsCompiled(false);

  EXPECT_EQ("original source", *info->deferred_compilation_source());
  info->UpdateSource("different!");
  EXPECT_EQ("original source", *info->deferred_compilation_source());

  info->FlagSourceAsCompiled(true);
  EXPECT_EQ("different!", *info->deferred_compilation_source());
}

}  // namespace gles2
}  // namespace gpu
