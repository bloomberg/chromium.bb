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
  static const GLint kMax2dLevels = 5;
  static const GLint kMaxCubeMapLevels = 4;

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

TEST_F(TextureManagerTest, MaxValues) {
  // Check we get the right values for the max sizes.
  EXPECT_EQ(kMax2dLevels, manager_.MaxLevelsForTarget(GL_TEXTURE_2D));
  EXPECT_EQ(kMaxCubeMapLevels,
            manager_.MaxLevelsForTarget(GL_TEXTURE_CUBE_MAP));
  EXPECT_EQ(kMaxTextureSize, manager_.MaxSizeForTarget(GL_TEXTURE_2D));
  EXPECT_EQ(kMaxCubeMapTextureSize,
            manager_.MaxSizeForTarget(GL_TEXTURE_CUBE_MAP));
}

TEST_F(TextureManagerTest, ValidForTexture) {
  // check 2d
  EXPECT_TRUE(manager_.ValidForTarget(
      GL_TEXTURE_2D, 0,
      kMaxTextureSize, kMaxTextureSize, 1));
  EXPECT_TRUE(manager_.ValidForTarget(
      GL_TEXTURE_2D, kMax2dLevels - 1,
      kMaxTextureSize, kMaxTextureSize, 1));
  EXPECT_TRUE(manager_.ValidForTarget(
      GL_TEXTURE_2D, kMax2dLevels - 1,
      1, kMaxTextureSize, 1));
  EXPECT_TRUE(manager_.ValidForTarget(
      GL_TEXTURE_2D, kMax2dLevels - 1,
      kMaxTextureSize, 1, 1));
  // check level out of range.
  EXPECT_FALSE(manager_.ValidForTarget(
      GL_TEXTURE_2D, kMax2dLevels,
      kMaxTextureSize, 1, 1));
  // check has depth.
  EXPECT_FALSE(manager_.ValidForTarget(
      GL_TEXTURE_2D, kMax2dLevels,
      kMaxTextureSize, 1, 2));

  // check cube
  EXPECT_TRUE(manager_.ValidForTarget(
      GL_TEXTURE_CUBE_MAP, 0,
      kMaxCubeMapTextureSize, kMaxCubeMapTextureSize, 1));
  EXPECT_TRUE(manager_.ValidForTarget(
      GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels - 1,
      kMaxCubeMapTextureSize, kMaxCubeMapTextureSize, 1));
  // check level out of range.
  EXPECT_FALSE(manager_.ValidForTarget(
      GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels,
      kMaxCubeMapTextureSize, 1, 1));
  // check not square.
  EXPECT_FALSE(manager_.ValidForTarget(
      GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels,
      kMaxCubeMapTextureSize, 1, 1));
  // check has depth.
  EXPECT_FALSE(manager_.ValidForTarget(
      GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels,
      kMaxCubeMapTextureSize, 1, 2));
}

class TextureInfoTest : public testing::Test {
 public:
  static const GLint kMaxTextureSize = 16;
  static const GLint kMaxCubeMapTextureSize = 8;
  static const GLint kMax2dLevels = 5;
  static const GLint kMaxCubeMapLevels = 4;
  static const GLuint kTexture1Id = 1;

  TextureInfoTest()
      : manager_(kMaxTextureSize, kMaxCubeMapTextureSize) {
  }

 protected:
  virtual void SetUp() {
    manager_.CreateTextureInfo(kTexture1Id);
    info_ = manager_.GetTextureInfo(kTexture1Id);
    ASSERT_TRUE(info_ != NULL);
  }

  virtual void TearDown() {
  }

  TextureManager manager_;
  TextureManager::TextureInfo* info_;
};

TEST_F(TextureInfoTest, Basic) {
  EXPECT_EQ(0, info_->target());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->CanRender());
}

TEST_F(TextureInfoTest, POT2D) {
  manager_.SetInfoTarget(info_, GL_TEXTURE_2D);
  EXPECT_EQ(GL_TEXTURE_2D, info_->target());
  // Check Setting level 0 to POT
  info_->SetLevelInfo(
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->CanRender());
  // Set filters to something that will work with a single mip.
  info_->SetParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  EXPECT_TRUE(info_->CanRender());
  // Set them back.
  info_->SetParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

  EXPECT_TRUE(info_->CanGenerateMipmaps());
  // Make mips.
  EXPECT_TRUE(info_->MarkMipmapsGenerated());
  EXPECT_TRUE(info_->texture_complete());
  EXPECT_TRUE(info_->CanRender());
  // Change a mip.
  info_->SetLevelInfo(
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->CanRender());
  // Set a level past the number of mips that would get generated.
  info_->SetLevelInfo(
      GL_TEXTURE_2D, 3, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_TRUE(info_->CanGenerateMipmaps());
  // Make mips.
  EXPECT_TRUE(info_->MarkMipmapsGenerated());
  EXPECT_FALSE(info_->CanRender());
  EXPECT_FALSE(info_->texture_complete());
}

TEST_F(TextureInfoTest, NPOT2D) {
  manager_.SetInfoTarget(info_, GL_TEXTURE_2D);
  EXPECT_EQ(GL_TEXTURE_2D, info_->target());
  // Check Setting level 0 to NPOT
  info_->SetLevelInfo(
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_TRUE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->CanRender());
  info_->SetParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  EXPECT_FALSE(info_->CanRender());
  info_->SetParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  EXPECT_FALSE(info_->CanRender());
  info_->SetParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(info_->CanRender());
  // Change it to POT.
  info_->SetLevelInfo(
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->CanGenerateMipmaps());
}

TEST_F(TextureInfoTest, POTCubeMap) {
  manager_.SetInfoTarget(info_, GL_TEXTURE_CUBE_MAP);
  EXPECT_EQ(GL_TEXTURE_CUBE_MAP, info_->target());
  // Check Setting level 0 each face to POT
  info_->SetLevelInfo(
      GL_TEXTURE_CUBE_MAP_POSITIVE_X,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->CanRender());
  info_->SetLevelInfo(
      GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->CanRender());
  info_->SetLevelInfo(
      GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->CanRender());
  info_->SetLevelInfo(
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanRender());
  EXPECT_FALSE(info_->CanGenerateMipmaps());
  info_->SetLevelInfo(
      GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->CanRender());
  info_->SetLevelInfo(
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());
  EXPECT_TRUE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->CanRender());

  // Make mips.
  EXPECT_TRUE(info_->MarkMipmapsGenerated());
  EXPECT_TRUE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());

  // Change a mip.
  info_->SetLevelInfo(
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      1, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());
  EXPECT_TRUE(info_->CanGenerateMipmaps());
  // Set a level past the number of mips that would get generated.
  info_->SetLevelInfo(
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      3, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_TRUE(info_->CanGenerateMipmaps());
  // Make mips.
  EXPECT_TRUE(info_->MarkMipmapsGenerated());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());
}

TEST_F(TextureInfoTest, GetLevelSize) {
  manager_.SetInfoTarget(info_, GL_TEXTURE_2D);
  info_->SetLevelInfo(
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  GLsizei width = -1;
  GLsizei height = -1;
  EXPECT_FALSE(info_->GetLevelSize(GL_TEXTURE_2D, -1, &width, &height));
  EXPECT_FALSE(info_->GetLevelSize(GL_TEXTURE_2D, 1000, &width, &height));
  EXPECT_TRUE(info_->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height));
  EXPECT_EQ(0, width);
  EXPECT_EQ(0, height);
  EXPECT_TRUE(info_->GetLevelSize(GL_TEXTURE_2D, 1, &width, &height));
  EXPECT_EQ(4, width);
  EXPECT_EQ(5, height);
  manager_.RemoveTextureInfo(info_->texture_id());
  EXPECT_FALSE(info_->GetLevelSize(GL_TEXTURE_2D, 1, &width, &height));
}

}  // namespace gles2
}  // namespace gpu


