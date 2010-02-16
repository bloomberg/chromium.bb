// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shader_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class ShaderManagerTest : public testing::Test {
 public:
  ShaderManagerTest() {
  }

 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  ShaderManager manager_;
};

TEST_F(ShaderManagerTest, Basic) {
  const GLuint kShader1Id = 1;
  const std::string kShader1Source("hello world");
  const GLuint kShader2Id = 2;
  // Check we can create shader.
  manager_.CreateShaderInfo(kShader1Id);
  // Check shader got created.
  ShaderManager::ShaderInfo* info1 = manager_.GetShaderInfo(kShader1Id);
  ASSERT_TRUE(info1 != NULL);
  // Check we and set its source.
  info1->Update(kShader1Source);
  EXPECT_STREQ(kShader1Source.c_str(), info1->source().c_str());
  // Check we get nothing for a non-existent shader.
  EXPECT_TRUE(manager_.GetShaderInfo(kShader2Id) == NULL);
  // Check trying to a remove non-existent shaders does not crash.
  manager_.RemoveShaderInfo(kShader2Id);
  // Check we can't get the shader after we remove it.
  manager_.RemoveShaderInfo(kShader1Id);
  EXPECT_TRUE(manager_.GetShaderInfo(kShader1Id) == NULL);
}

}  // namespace gles2
}  // namespace gpu


