// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_manager.h"

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::Return;
using ::testing::ReturnRef;

namespace gpu {
namespace gles2 {

class ShaderManagerTest : public GpuServiceTest {
 public:
  ShaderManagerTest() {
  }

  virtual ~ShaderManagerTest() {
    manager_.Destroy(false);
  }

 protected:
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
  Shader* shader1 = manager_.GetShader(kClient1Id);
  ASSERT_EQ(info0, shader1);
  // Check we get nothing for a non-existent shader.
  EXPECT_TRUE(manager_.GetShader(kClient2Id) == NULL);
  // Check we can't get the shader after we remove it.
  manager_.MarkAsDeleted(shader1);
  EXPECT_TRUE(manager_.GetShader(kClient1Id) == NULL);
}

TEST_F(ShaderManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kShader1Type = GL_VERTEX_SHADER;
  // Check we can create shader.
  Shader* shader1 = manager_.CreateShader(
      kClient1Id, kService1Id, kShader1Type);
  // Check shader got created.
  ASSERT_TRUE(shader1 != NULL);
  EXPECT_CALL(*gl_, DeleteShader(kService1Id))
      .Times(1)
      .RetiresOnSaturation();
  manager_.Destroy(true);
  // Check that resources got freed.
  shader1 = manager_.GetShader(kClient1Id);
  ASSERT_TRUE(shader1 == NULL);
}

TEST_F(ShaderManagerTest, DeleteBug) {
  const GLuint kClient1Id = 1;
  const GLuint kClient2Id = 2;
  const GLuint kService1Id = 11;
  const GLuint kService2Id = 12;
  const GLenum kShaderType = GL_VERTEX_SHADER;
  // Check we can create shader.
  scoped_refptr<Shader> shader1(
      manager_.CreateShader(kClient1Id, kService1Id, kShaderType));
  scoped_refptr<Shader> shader2(
      manager_.CreateShader(kClient2Id, kService2Id, kShaderType));
  ASSERT_TRUE(shader1.get());
  ASSERT_TRUE(shader2.get());
  manager_.UseShader(shader1.get());
  manager_.MarkAsDeleted(shader1.get());
  manager_.MarkAsDeleted(shader2.get());
  EXPECT_TRUE(manager_.IsOwned(shader1.get()));
  EXPECT_FALSE(manager_.IsOwned(shader2.get()));
}

TEST_F(ShaderManagerTest, DoCompile) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kShader1Type = GL_VERTEX_SHADER;
  const char* kClient1Source = "hello world";
  const GLenum kAttrib1Type = GL_FLOAT_VEC2;
  const GLsizei kAttrib1Size = 2;
  const int kAttrib1Precision = SH_PRECISION_MEDIUMP;
  const char* kAttrib1Name = "attr1";
  const GLenum kAttrib2Type = GL_FLOAT_VEC3;
  const GLsizei kAttrib2Size = 4;
  const int kAttrib2Precision = SH_PRECISION_HIGHP;
  const char* kAttrib2Name = "attr2";
  const int kAttribStaticUse = 0;
  const GLenum kUniform1Type = GL_FLOAT_MAT2;
  const GLsizei kUniform1Size = 3;
  const int kUniform1Precision = SH_PRECISION_LOWP;
  const int kUniform1StaticUse = 1;
  const char* kUniform1Name = "uni1";
  const GLenum kUniform2Type = GL_FLOAT_MAT3;
  const GLsizei kUniform2Size = 5;
  const int kUniform2Precision = SH_PRECISION_MEDIUMP;
  const int kUniform2StaticUse = 0;
  const char* kUniform2Name = "uni2";
  const GLenum kVarying1Type = GL_FLOAT_VEC4;
  const GLsizei kVarying1Size = 1;
  const int kVarying1Precision = SH_PRECISION_HIGHP;
  const int kVarying1StaticUse = 0;
  const char* kVarying1Name = "varying1";

  // Check we can create shader.
  Shader* shader1 = manager_.CreateShader(
      kClient1Id, kService1Id, kShader1Type);
  // Check shader got created.
  ASSERT_TRUE(shader1 != NULL);
  EXPECT_EQ(kService1Id, shader1->service_id());
  // Check if the shader has correct type.
  EXPECT_EQ(kShader1Type, shader1->shader_type());
  EXPECT_FALSE(shader1->valid());
  EXPECT_FALSE(shader1->InUse());
  EXPECT_TRUE(shader1->source().empty());
  EXPECT_TRUE(shader1->log_info().empty());
  EXPECT_TRUE(shader1->signature_source().empty());
  EXPECT_TRUE(shader1->translated_source().empty());
  EXPECT_EQ(0u, shader1->attrib_map().size());
  EXPECT_EQ(0u, shader1->uniform_map().size());
  EXPECT_EQ(0u, shader1->varying_map().size());

  // Check we can set its source.
  shader1->set_source(kClient1Source);
  EXPECT_STREQ(kClient1Source, shader1->source().c_str());
  EXPECT_TRUE(shader1->signature_source().empty());

  // Check DoCompile() will set compilation states, log, translated source,
  // shader variables, and name mapping.
  const std::string kLog = "foo";
  const std::string kTranslatedSource = "poo";

  ShaderTranslator::VariableMap attrib_map;
  attrib_map[kAttrib1Name] = ShaderTranslatorInterface::VariableInfo(
      kAttrib1Type, kAttrib1Size, kAttrib1Precision,
      kAttribStaticUse, kAttrib1Name);
  attrib_map[kAttrib2Name] = ShaderTranslatorInterface::VariableInfo(
      kAttrib2Type, kAttrib2Size, kAttrib2Precision,
      kAttribStaticUse, kAttrib2Name);
  ShaderTranslator::VariableMap uniform_map;
  uniform_map[kUniform1Name] = ShaderTranslatorInterface::VariableInfo(
      kUniform1Type, kUniform1Size, kUniform1Precision,
      kUniform1StaticUse, kUniform1Name);
  uniform_map[kUniform2Name] = ShaderTranslatorInterface::VariableInfo(
      kUniform2Type, kUniform2Size, kUniform2Precision,
      kUniform2StaticUse, kUniform2Name);
  ShaderTranslator::VariableMap varying_map;
  varying_map[kVarying1Name] = ShaderTranslatorInterface::VariableInfo(
      kVarying1Type, kVarying1Size, kVarying1Precision,
      kVarying1StaticUse, kVarying1Name);

  TestHelper::SetShaderStates(
      gl_.get(), shader1, true, &kLog, &kTranslatedSource,
      &attrib_map, &uniform_map, &varying_map, NULL);
  EXPECT_TRUE(shader1->valid());
  // When compilation succeeds, no log is recorded.
  EXPECT_STREQ("", shader1->log_info().c_str());
  EXPECT_STREQ(kClient1Source, shader1->signature_source().c_str());
  EXPECT_STREQ(kTranslatedSource.c_str(), shader1->translated_source().c_str());

  // Check varying infos got copied.
  EXPECT_EQ(attrib_map.size(), shader1->attrib_map().size());
  for (ShaderTranslator::VariableMap::const_iterator it = attrib_map.begin();
       it != attrib_map.end(); ++it) {
    const Shader::VariableInfo* variable_info =
        shader1->GetAttribInfo(it->first);
    ASSERT_TRUE(variable_info != NULL);
    EXPECT_EQ(it->second.type, variable_info->type);
    EXPECT_EQ(it->second.size, variable_info->size);
    EXPECT_EQ(it->second.precision, variable_info->precision);
    EXPECT_EQ(it->second.static_use, variable_info->static_use);
    EXPECT_STREQ(it->second.name.c_str(), variable_info->name.c_str());
  }
  // Check uniform infos got copied.
  EXPECT_EQ(uniform_map.size(), shader1->uniform_map().size());
  for (ShaderTranslator::VariableMap::const_iterator it = uniform_map.begin();
       it != uniform_map.end(); ++it) {
    const Shader::VariableInfo* variable_info =
        shader1->GetUniformInfo(it->first);
    ASSERT_TRUE(variable_info != NULL);
    EXPECT_EQ(it->second.type, variable_info->type);
    EXPECT_EQ(it->second.size, variable_info->size);
    EXPECT_EQ(it->second.precision, variable_info->precision);
    EXPECT_EQ(it->second.static_use, variable_info->static_use);
    EXPECT_STREQ(it->second.name.c_str(), variable_info->name.c_str());
  }
  // Check varying infos got copied.
  EXPECT_EQ(varying_map.size(), shader1->varying_map().size());
  for (ShaderTranslator::VariableMap::const_iterator it = varying_map.begin();
       it != varying_map.end(); ++it) {
    const Shader::VariableInfo* variable_info =
        shader1->GetVaryingInfo(it->first);
    ASSERT_TRUE(variable_info != NULL);
    EXPECT_EQ(it->second.type, variable_info->type);
    EXPECT_EQ(it->second.size, variable_info->size);
    EXPECT_EQ(it->second.precision, variable_info->precision);
    EXPECT_EQ(it->second.static_use, variable_info->static_use);
    EXPECT_STREQ(it->second.name.c_str(), variable_info->name.c_str());
  }

  // Compile failure case.
  TestHelper::SetShaderStates(
      gl_.get(), shader1, false, &kLog, &kTranslatedSource,
      &attrib_map, &uniform_map, &varying_map, NULL);
  EXPECT_FALSE(shader1->valid());
  EXPECT_STREQ(kLog.c_str(), shader1->log_info().c_str());
  EXPECT_STREQ("", shader1->translated_source().c_str());
  EXPECT_TRUE(shader1->attrib_map().empty());
  EXPECT_TRUE(shader1->uniform_map().empty());
  EXPECT_TRUE(shader1->varying_map().empty());
}

TEST_F(ShaderManagerTest, ShaderInfoUseCount) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kShader1Type = GL_VERTEX_SHADER;
  // Check we can create shader.
  Shader* shader1 = manager_.CreateShader(
      kClient1Id, kService1Id, kShader1Type);
  // Check shader got created.
  ASSERT_TRUE(shader1 != NULL);
  EXPECT_FALSE(shader1->InUse());
  EXPECT_FALSE(shader1->IsDeleted());
  manager_.UseShader(shader1);
  EXPECT_TRUE(shader1->InUse());
  manager_.UseShader(shader1);
  EXPECT_TRUE(shader1->InUse());
  manager_.MarkAsDeleted(shader1);
  EXPECT_TRUE(shader1->IsDeleted());
  Shader* shader2 = manager_.GetShader(kClient1Id);
  EXPECT_EQ(shader1, shader2);
  manager_.UnuseShader(shader1);
  EXPECT_TRUE(shader1->InUse());
  manager_.UnuseShader(shader1);  // this should delete the info.
  shader2 = manager_.GetShader(kClient1Id);
  EXPECT_TRUE(shader2 == NULL);

  shader1 = manager_.CreateShader(kClient1Id, kService1Id, kShader1Type);
  ASSERT_TRUE(shader1 != NULL);
  EXPECT_FALSE(shader1->InUse());
  manager_.UseShader(shader1);
  EXPECT_TRUE(shader1->InUse());
  manager_.UseShader(shader1);
  EXPECT_TRUE(shader1->InUse());
  manager_.UnuseShader(shader1);
  EXPECT_TRUE(shader1->InUse());
  manager_.UnuseShader(shader1);
  EXPECT_FALSE(shader1->InUse());
  shader2 = manager_.GetShader(kClient1Id);
  EXPECT_EQ(shader1, shader2);
  manager_.MarkAsDeleted(shader1);  // this should delete the shader.
  shader2 = manager_.GetShader(kClient1Id);
  EXPECT_TRUE(shader2 == NULL);
}

}  // namespace gles2
}  // namespace gpu
