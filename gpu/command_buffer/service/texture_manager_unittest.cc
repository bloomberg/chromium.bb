// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class TextureManagerTest : public testing::Test {
 public:
  static const GLint kMaxTextureSize = 16;
  static const GLint kMaxCubeMapTextureSize = 8;

  TextureManagerTest()
      : manager_(kMaxTextureSize, kMaxCubeMapTextureSize) {
  }

 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  TextureManager manager_;
};

TEST_F(TextureManagerTest, Basic) {
  const GLuint kTexture1Id = 1;
  const GLsizeiptr kTexture1Size = 123;
  const GLuint kTexture2Id = 2;
  // Check we can create texture.
  manager_.CreateTextureInfo(kTexture1Id);
  // Check texture got created.
  TextureManager::TextureInfo* info1 = manager_.GetTextureInfo(kTexture1Id);
  ASSERT_TRUE(info1 != NULL);
  // Check we get nothing for a non-existent texture.
  EXPECT_TRUE(manager_.GetTextureInfo(kTexture2Id) == NULL);
  // Check trying to a remove non-existent textures does not crash.
  manager_.RemoveTextureInfo(kTexture2Id);
  // Check we can't get the texture after we remove it.
  manager_.RemoveTextureInfo(kTexture1Id);
  EXPECT_TRUE(manager_.GetTextureInfo(kTexture1Id) == NULL);
}

// TODO(gman): Test TextureInfo setting.

}  // namespace gles2
}  // namespace gpu


