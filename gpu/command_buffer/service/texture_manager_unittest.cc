// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_manager.h"
#include "base/scoped_ptr.h"
#include "app/gfx/gl/gl_mock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Pointee;

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

  ~TextureManagerTest() {
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
  TextureManager manager_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLint TextureManagerTest::kMaxTextureSize;
const GLint TextureManagerTest::kMaxCubeMapTextureSize;
const GLint TextureManagerTest::kMax2dLevels;
const GLint TextureManagerTest::kMaxCubeMapLevels;
#endif

TEST_F(TextureManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());
  // Check we can create texture.
  manager_.CreateTextureInfo(kClient1Id, kService1Id);
  // Check texture got created.
  TextureManager::TextureInfo* info1 = manager_.GetTextureInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_EQ(kService1Id, info1->service_id());
  GLuint client_id = 0;
  EXPECT_TRUE(manager_.GetClientId(info1->service_id(), &client_id));
  EXPECT_EQ(kClient1Id, client_id);
  // Check we get nothing for a non-existent texture.
  EXPECT_TRUE(manager_.GetTextureInfo(kClient2Id) == NULL);
  // Check trying to a remove non-existent textures does not crash.
  manager_.RemoveTextureInfo(kClient2Id);
  // Check we can't get the texture after we remove it.
  manager_.RemoveTextureInfo(kClient1Id);
  EXPECT_TRUE(manager_.GetTextureInfo(kClient1Id) == NULL);
}

TEST_F(TextureManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());
  // Check we can create texture.
  manager_.CreateTextureInfo(kClient1Id, kService1Id);
  // Check texture got created.
  TextureManager::TextureInfo* info1 = manager_.GetTextureInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  manager_.Destroy(true);
  // Check that resources got freed.
  info1 = manager_.GetTextureInfo(kClient1Id);
  ASSERT_TRUE(info1 == NULL);
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
  static const GLuint kClient1Id = 1;
  static const GLuint kService1Id = 11;

  TextureInfoTest()
      : manager_(kMaxTextureSize, kMaxCubeMapTextureSize) {
  }
  ~TextureInfoTest() {
    manager_.Destroy(false);
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
    manager_.CreateTextureInfo(kClient1Id, kService1Id);
    info_ = manager_.GetTextureInfo(kClient1Id);
    ASSERT_TRUE(info_ != NULL);
  }

  virtual void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  TextureManager manager_;
  TextureManager::TextureInfo* info_;
};

TEST_F(TextureInfoTest, Basic) {
  EXPECT_EQ(0u, info_->target());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->CanRender());
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
}

TEST_F(TextureInfoTest, POT2D) {
  manager_.SetInfoTarget(info_, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info_->target());
  // Check Setting level 0 to POT
  manager_.SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->CanRender());
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  // Set filters to something that will work with a single mip.
  manager_.SetParameter(info_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  EXPECT_TRUE(info_->CanRender());
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());
  // Set them back.
  manager_.SetParameter(info_, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());

  EXPECT_TRUE(info_->CanGenerateMipmaps());
  // Make mips.
  EXPECT_TRUE(manager_.MarkMipmapsGenerated(info_));
  EXPECT_TRUE(info_->texture_complete());
  EXPECT_TRUE(info_->CanRender());
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());
  // Change a mip.
  manager_.SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->CanRender());
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  // Set a level past the number of mips that would get generated.
  manager_.SetLevelInfo(info_,
      GL_TEXTURE_2D, 3, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_TRUE(info_->CanGenerateMipmaps());
  // Make mips.
  EXPECT_TRUE(manager_.MarkMipmapsGenerated(info_));
  EXPECT_FALSE(info_->CanRender());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
}

TEST_F(TextureInfoTest, NPOT2D) {
  manager_.SetInfoTarget(info_, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info_->target());
  // Check Setting level 0 to NPOT
  manager_.SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_TRUE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->CanRender());
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetParameter(info_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  EXPECT_FALSE(info_->CanRender());
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetParameter(info_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  EXPECT_FALSE(info_->CanRender());
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetParameter(info_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(info_->CanRender());
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());
  // Change it to POT.
  manager_.SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());
}

TEST_F(TextureInfoTest, POTCubeMap) {
  manager_.SetInfoTarget(info_, GL_TEXTURE_CUBE_MAP);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP), info_->target());
  // Check Setting level 0 each face to POT
  manager_.SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_POSITIVE_X,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->CanRender());
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->CanRender());
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->CanRender());
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanRender());
  EXPECT_FALSE(info_->CanGenerateMipmaps());
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->CanRender());
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());
  EXPECT_TRUE(info_->CanGenerateMipmaps());
  EXPECT_FALSE(info_->CanRender());
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());

  // Make mips.
  EXPECT_TRUE(manager_.MarkMipmapsGenerated(info_));
  EXPECT_TRUE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());
  EXPECT_TRUE(info_->CanRender());
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());

  // Change a mip.
  manager_.SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      1, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());
  EXPECT_TRUE(info_->CanGenerateMipmaps());
  // Set a level past the number of mips that would get generated.
  manager_.SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      3, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_TRUE(info_->CanGenerateMipmaps());
  // Make mips.
  EXPECT_TRUE(manager_.MarkMipmapsGenerated(info_));
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());
}

TEST_F(TextureInfoTest, GetLevelSize) {
  manager_.SetInfoTarget(info_, GL_TEXTURE_2D);
  manager_.SetLevelInfo(info_,
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
  manager_.RemoveTextureInfo(kClient1Id);
  EXPECT_FALSE(info_->GetLevelSize(GL_TEXTURE_2D, 1, &width, &height));
}

}  // namespace gles2
}  // namespace gpu


