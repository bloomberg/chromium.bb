// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_manager.h"
#include "base/scoped_ptr.h"
#include "app/gfx/gl/gl_mock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class ShaderManagerTest : public testing::Test {
 public:
  ShaderManagerTest() {
  }

  ~ShaderManagerTest() {
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
  const std::string kClient1Source("hello world");
  const GLuint kClient2Id = 2;
  // Check we can create shader.
  manager_.CreateShaderInfo(kClient1Id, kService1Id, kShader1Type);
  // Check shader got created.
  ShaderManager::ShaderInfo* info1 = manager_.GetShaderInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_EQ(kService1Id, info1->service_id());
  // Check if the shader has correct type.
  EXPECT_EQ(kShader1Type, info1->shader_type());
  EXPECT_FALSE(info1->IsValid());
  EXPECT_STREQ("", info1->log_info().c_str());
  const char* kLog = "foo";
  info1->SetStatus(true, kLog);
  EXPECT_TRUE(info1->IsValid());
  EXPECT_STREQ(kLog, info1->log_info().c_str());
  // Check we can set its source.
  info1->Update(kClient1Source);
  EXPECT_STREQ(kClient1Source.c_str(), info1->source().c_str());
  // Check we get nothing for a non-existent shader.
  EXPECT_TRUE(manager_.GetShaderInfo(kClient2Id) == NULL);
  // Check trying to a remove non-existent shaders does not crash.
  manager_.RemoveShaderInfo(kClient2Id);
  // Check we can't get the shader after we remove it.
  manager_.RemoveShaderInfo(kClient1Id);
  EXPECT_TRUE(manager_.GetShaderInfo(kClient1Id) == NULL);
}

TEST_F(ShaderManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kShader1Type = GL_VERTEX_SHADER;
  // Check we can create shader.
  manager_.CreateShaderInfo(kClient1Id, kService1Id, kShader1Type);
  // Check shader got created.
  ShaderManager::ShaderInfo* info1 = manager_.GetShaderInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_CALL(*gl_, DeleteShader(kService1Id))
      .Times(1)
      .RetiresOnSaturation();
  manager_.Destroy(true);
  // Check that resources got freed.
  info1 = manager_.GetShaderInfo(kClient1Id);
  ASSERT_TRUE(info1 == NULL);
}

}  // namespace gles2
}  // namespace gpu


