// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_manager.h"

#include "base/memory/scoped_ptr.h"
#include "gpu/GLES2/gles2_command_buffer.h"
#include "gpu/command_buffer/common/gl_mock.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Pointee;
using ::testing::_;

namespace gpu {
namespace gles2 {

class TextureManagerTest : public testing::Test {
 public:
  static const GLint kMaxTextureSize = 16;
  static const GLint kMaxCubeMapTextureSize = 8;
  static const GLint kMax2dLevels = 5;
  static const GLint kMaxCubeMapLevels = 4;

  static const GLuint kServiceBlackTexture2dId = 701;
  static const GLuint kServiceBlackTextureCubemapId = 702;
  static const GLuint kServiceDefaultTexture2dId = 703;
  static const GLuint kServiceDefaultTextureCubemapId = 704;


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

    TestHelper::SetupTextureManagerInitExpectations(gl_.get());
    manager_.Initialize();
  }

  virtual void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  TextureManager manager_;
  FeatureInfo feature_info_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLint TextureManagerTest::kMaxTextureSize;
const GLint TextureManagerTest::kMaxCubeMapTextureSize;
const GLint TextureManagerTest::kMax2dLevels;
const GLint TextureManagerTest::kMaxCubeMapLevels;
const GLuint TextureManagerTest::kServiceBlackTexture2dId;
const GLuint TextureManagerTest::kServiceBlackTextureCubemapId;
const GLuint TextureManagerTest::kServiceDefaultTexture2dId;
const GLuint TextureManagerTest::kServiceDefaultTextureCubemapId;
#endif

TEST_F(TextureManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());
  // Check we can create texture.
  manager_.CreateTextureInfo(&feature_info_, kClient1Id, kService1Id);
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
  manager_.RemoveTextureInfo(&feature_info_, kClient2Id);
  // Check we can't get the texture after we remove it.
  manager_.RemoveTextureInfo(&feature_info_, kClient1Id);
  EXPECT_TRUE(manager_.GetTextureInfo(kClient1Id) == NULL);
}

TEST_F(TextureManagerTest, SetParameter) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());
  // Check we can create texture.
  manager_.CreateTextureInfo(&feature_info_, kClient1Id, kService1Id);
  // Check texture got created.
  TextureManager::TextureInfo* info = manager_.GetTextureInfo(kClient1Id);
  ASSERT_TRUE(info != NULL);
  EXPECT_TRUE(manager_.SetParameter(
      &feature_info_, info, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), info->min_filter());
  EXPECT_TRUE(manager_.SetParameter(
      &feature_info_, info, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), info->mag_filter());
  EXPECT_TRUE(manager_.SetParameter(
      &feature_info_, info, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), info->wrap_s());
  EXPECT_TRUE(manager_.SetParameter(
      &feature_info_, info, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), info->wrap_t());
  EXPECT_FALSE(manager_.SetParameter(
      &feature_info_, info, GL_TEXTURE_MIN_FILTER, GL_CLAMP_TO_EDGE));
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), info->min_filter());
  EXPECT_FALSE(manager_.SetParameter(
      &feature_info_, info, GL_TEXTURE_MAG_FILTER, GL_CLAMP_TO_EDGE));
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), info->min_filter());
  EXPECT_FALSE(manager_.SetParameter(
      &feature_info_, info, GL_TEXTURE_WRAP_S, GL_NEAREST));
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), info->wrap_s());
  EXPECT_FALSE(manager_.SetParameter(
      &feature_info_, info, GL_TEXTURE_WRAP_T, GL_NEAREST));
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), info->wrap_t());
}

TEST_F(TextureManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());
  // Check we can create texture.
  manager_.CreateTextureInfo(&feature_info_, kClient1Id, kService1Id);
  // Check texture got created.
  TextureManager::TextureInfo* info1 = manager_.GetTextureInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_, DeleteTextures(4, _))
      .Times(1)
      .RetiresOnSaturation();
  manager_.Destroy(true);
  // Check that resources got freed.
  info1 = manager_.GetTextureInfo(kClient1Id);
  ASSERT_TRUE(info1 == NULL);
}

TEST_F(TextureManagerTest, DestroyUnowned) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());
  // Check we can create texture.
  TextureManager::TextureInfo* created_info =
      manager_.CreateTextureInfo(&feature_info_, kClient1Id, kService1Id);
  created_info->SetNotOwned();

  // Check texture got created.
  TextureManager::TextureInfo* info1 = manager_.GetTextureInfo(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_CALL(*gl_, DeleteTextures(4, _))
      .Times(1)
      .RetiresOnSaturation();

  // Check that it is not freed if it is not owned.
  manager_.Destroy(true);
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

TEST_F(TextureManagerTest, ValidForTarget) {
  // check 2d
  EXPECT_TRUE(manager_.ValidForTarget(
      &feature_info_, GL_TEXTURE_2D, 0,
      kMaxTextureSize, kMaxTextureSize, 1));
  EXPECT_TRUE(manager_.ValidForTarget(
      &feature_info_, GL_TEXTURE_2D, kMax2dLevels - 1,
      kMaxTextureSize, kMaxTextureSize, 1));
  EXPECT_TRUE(manager_.ValidForTarget(
      &feature_info_, GL_TEXTURE_2D, kMax2dLevels - 1,
      1, kMaxTextureSize, 1));
  EXPECT_TRUE(manager_.ValidForTarget(
      &feature_info_, GL_TEXTURE_2D, kMax2dLevels - 1,
      kMaxTextureSize, 1, 1));
  // check level out of range.
  EXPECT_FALSE(manager_.ValidForTarget(
      &feature_info_, GL_TEXTURE_2D, kMax2dLevels,
      kMaxTextureSize, 1, 1));
  // check has depth.
  EXPECT_FALSE(manager_.ValidForTarget(
      &feature_info_, GL_TEXTURE_2D, kMax2dLevels,
      kMaxTextureSize, 1, 2));
  // Check NPOT width on level 0
  EXPECT_TRUE(manager_.ValidForTarget(
      &feature_info_, GL_TEXTURE_2D, 0, 5, 2, 1));
  // Check NPOT height on level 0
  EXPECT_TRUE(manager_.ValidForTarget(
      &feature_info_, GL_TEXTURE_2D, 0, 2, 5, 1));
  // Check NPOT width on level 1
  EXPECT_FALSE(manager_.ValidForTarget(
      &feature_info_, GL_TEXTURE_2D, 1, 5, 2, 1));
  // Check NPOT height on level 1
  EXPECT_FALSE(manager_.ValidForTarget(
      &feature_info_, GL_TEXTURE_2D, 1, 2, 5, 1));

  // check cube
  EXPECT_TRUE(manager_.ValidForTarget(
      &feature_info_, GL_TEXTURE_CUBE_MAP, 0,
      kMaxCubeMapTextureSize, kMaxCubeMapTextureSize, 1));
  EXPECT_TRUE(manager_.ValidForTarget(
      &feature_info_, GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels - 1,
      kMaxCubeMapTextureSize, kMaxCubeMapTextureSize, 1));
  // check level out of range.
  EXPECT_FALSE(manager_.ValidForTarget(
      &feature_info_, GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels,
      kMaxCubeMapTextureSize, 1, 1));
  // check not square.
  EXPECT_FALSE(manager_.ValidForTarget(
      &feature_info_, GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels,
      kMaxCubeMapTextureSize, 1, 1));
  // check has depth.
  EXPECT_FALSE(manager_.ValidForTarget(
      &feature_info_, GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels,
      kMaxCubeMapTextureSize, 1, 2));
}

TEST_F(TextureManagerTest, ValidForTargetNPOT) {
  TextureManager manager(kMaxTextureSize, kMaxCubeMapTextureSize);
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_npot");
  FeatureInfo feature_info;
  feature_info.Initialize(NULL);
  // Check NPOT width on level 0
  EXPECT_TRUE(manager.ValidForTarget(&feature_info, GL_TEXTURE_2D, 0, 5, 2, 1));
  // Check NPOT height on level 0
  EXPECT_TRUE(manager.ValidForTarget(&feature_info, GL_TEXTURE_2D, 0, 2, 5, 1));
  // Check NPOT width on level 1
  EXPECT_TRUE(manager.ValidForTarget(&feature_info, GL_TEXTURE_2D, 1, 5, 2, 1));
  // Check NPOT height on level 1
  EXPECT_TRUE(manager.ValidForTarget(&feature_info, GL_TEXTURE_2D, 1, 2, 5, 1));
  manager.Destroy(false);
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
    manager_.CreateTextureInfo(&feature_info_, kClient1Id, kService1Id);
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
  FeatureInfo feature_info_;
};

TEST_F(TextureInfoTest, Basic) {
  EXPECT_EQ(0u, info_->target());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps(&feature_info_));
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->CanRender(&feature_info_));
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST_MIPMAP_LINEAR), info_->min_filter());
  EXPECT_EQ(static_cast<GLenum>(GL_LINEAR), info_->mag_filter());
  EXPECT_EQ(static_cast<GLenum>(GL_REPEAT), info_->wrap_s());
  EXPECT_EQ(static_cast<GLenum>(GL_REPEAT), info_->wrap_t());
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
}

TEST_F(TextureInfoTest, POT2D) {
  manager_.SetInfoTarget(info_, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info_->target());
  // Check Setting level 0 to POT
  manager_.SetLevelInfo(&feature_info_, info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->CanRender(&feature_info_));
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  // Set filters to something that will work with a single mip.
  manager_.SetParameter(
      &feature_info_, info_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  EXPECT_TRUE(info_->CanRender(&feature_info_));
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());
  // Set them back.
  manager_.SetParameter(
      &feature_info_, info_, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());

  EXPECT_TRUE(info_->CanGenerateMipmaps(&feature_info_));
  // Make mips.
  EXPECT_TRUE(manager_.MarkMipmapsGenerated(&feature_info_, info_));
  EXPECT_TRUE(info_->texture_complete());
  EXPECT_TRUE(info_->CanRender(&feature_info_));
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());
  // Change a mip.
  manager_.SetLevelInfo(&feature_info_, info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->CanGenerateMipmaps(&feature_info_));
  EXPECT_FALSE(info_->CanRender(&feature_info_));
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  // Set a level past the number of mips that would get generated.
  manager_.SetLevelInfo(&feature_info_, info_,
      GL_TEXTURE_2D, 3, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_TRUE(info_->CanGenerateMipmaps(&feature_info_));
  // Make mips.
  EXPECT_TRUE(manager_.MarkMipmapsGenerated(&feature_info_, info_));
  EXPECT_FALSE(info_->CanRender(&feature_info_));
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
}

TEST_F(TextureInfoTest, NPOT2D) {
  manager_.SetInfoTarget(info_, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info_->target());
  // Check Setting level 0 to NPOT
  manager_.SetLevelInfo(&feature_info_, info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_TRUE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps(&feature_info_));
  EXPECT_FALSE(info_->CanRender(&feature_info_));
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetParameter(
      &feature_info_, info_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  EXPECT_FALSE(info_->CanRender(&feature_info_));
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetParameter(
      &feature_info_, info_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  EXPECT_FALSE(info_->CanRender(&feature_info_));
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetParameter(
      &feature_info_, info_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(info_->CanRender(&feature_info_));
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());
  // Change it to POT.
  manager_.SetLevelInfo(&feature_info_, info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->CanGenerateMipmaps(&feature_info_));
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());
}

TEST_F(TextureInfoTest, NPOT2DNPOTOK) {
  TextureManager manager(kMaxTextureSize, kMaxCubeMapTextureSize);
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_npot");
  FeatureInfo feature_info;
  feature_info.Initialize(NULL);
  manager.CreateTextureInfo(&feature_info, kClient1Id, kService1Id);
  TextureManager::TextureInfo* info = manager_.GetTextureInfo(kClient1Id);
  ASSERT_TRUE(info_ != NULL);

  manager.SetInfoTarget(info, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info->target());
  // Check Setting level 0 to NPOT
  manager.SetLevelInfo(&feature_info, info,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_TRUE(info->npot());
  EXPECT_FALSE(info->texture_complete());
  EXPECT_TRUE(info->CanGenerateMipmaps(&feature_info));
  EXPECT_FALSE(info->CanRender(&feature_info));
  EXPECT_TRUE(manager.HaveUnrenderableTextures());
  EXPECT_TRUE(manager.MarkMipmapsGenerated(&feature_info, info));
  EXPECT_TRUE(info->texture_complete());
  EXPECT_TRUE(info->CanRender(&feature_info));
  EXPECT_FALSE(manager.HaveUnrenderableTextures());
  manager.Destroy(false);
}

TEST_F(TextureInfoTest, POTCubeMap) {
  manager_.SetInfoTarget(info_, GL_TEXTURE_CUBE_MAP);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP), info_->target());
  // Check Setting level 0 each face to POT
  manager_.SetLevelInfo(&feature_info_, info_,
      GL_TEXTURE_CUBE_MAP_POSITIVE_X,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps(&feature_info_));
  EXPECT_FALSE(info_->CanRender(&feature_info_));
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetLevelInfo(&feature_info_, info_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps(&feature_info_));
  EXPECT_FALSE(info_->CanRender(&feature_info_));
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetLevelInfo(&feature_info_, info_,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps(&feature_info_));
  EXPECT_FALSE(info_->CanRender(&feature_info_));
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetLevelInfo(&feature_info_, info_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanRender(&feature_info_));
  EXPECT_FALSE(info_->CanGenerateMipmaps(&feature_info_));
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetLevelInfo(&feature_info_, info_,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(info_->CanGenerateMipmaps(&feature_info_));
  EXPECT_FALSE(info_->CanRender(&feature_info_));
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());
  manager_.SetLevelInfo(&feature_info_, info_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());
  EXPECT_TRUE(info_->CanGenerateMipmaps(&feature_info_));
  EXPECT_FALSE(info_->CanRender(&feature_info_));
  EXPECT_TRUE(manager_.HaveUnrenderableTextures());

  // Make mips.
  EXPECT_TRUE(manager_.MarkMipmapsGenerated(&feature_info_, info_));
  EXPECT_TRUE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());
  EXPECT_TRUE(info_->CanRender(&feature_info_));
  EXPECT_FALSE(manager_.HaveUnrenderableTextures());

  // Change a mip.
  manager_.SetLevelInfo(&feature_info_, info_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      1, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());
  EXPECT_TRUE(info_->CanGenerateMipmaps(&feature_info_));
  // Set a level past the number of mips that would get generated.
  manager_.SetLevelInfo(&feature_info_, info_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      3, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  EXPECT_TRUE(info_->CanGenerateMipmaps(&feature_info_));
  // Make mips.
  EXPECT_TRUE(manager_.MarkMipmapsGenerated(&feature_info_, info_));
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());
}

TEST_F(TextureInfoTest, GetLevelSize) {
  manager_.SetInfoTarget(info_, GL_TEXTURE_2D);
  manager_.SetLevelInfo(&feature_info_, info_,
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
  manager_.RemoveTextureInfo(&feature_info_, kClient1Id);
  EXPECT_FALSE(info_->GetLevelSize(GL_TEXTURE_2D, 1, &width, &height));
}

TEST_F(TextureInfoTest, GetLevelType) {
  manager_.SetInfoTarget(info_, GL_TEXTURE_2D);
  manager_.SetLevelInfo(&feature_info_, info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  GLenum type = -1;
  GLenum format = -1;
  EXPECT_FALSE(info_->GetLevelType(GL_TEXTURE_2D, -1, &type, &format));
  EXPECT_FALSE(info_->GetLevelType(GL_TEXTURE_2D, 1000, &type, &format));
  EXPECT_TRUE(info_->GetLevelType(GL_TEXTURE_2D, 0, &type, &format));
  EXPECT_EQ(0u, type);
  EXPECT_EQ(0u, format);
  EXPECT_TRUE(info_->GetLevelType(GL_TEXTURE_2D, 1, &type, &format));
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), format);
  manager_.RemoveTextureInfo(&feature_info_, kClient1Id);
  EXPECT_FALSE(info_->GetLevelType(GL_TEXTURE_2D, 1, &type, &format));
}

TEST_F(TextureInfoTest, ValidForTexture) {
  manager_.SetInfoTarget(info_, GL_TEXTURE_2D);
  manager_.SetLevelInfo(&feature_info_, info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE);
  // Check bad face.
  EXPECT_FALSE(info_->ValidForTexture(
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      1, 0, 0, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad level.
  EXPECT_FALSE(info_->ValidForTexture(
      GL_TEXTURE_2D, 0, 0, 0, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad xoffset.
  EXPECT_FALSE(info_->ValidForTexture(
      GL_TEXTURE_2D, 1, -1, 0, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad xoffset + width > width.
  EXPECT_FALSE(info_->ValidForTexture(
      GL_TEXTURE_2D, 1, 1, 0, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad yoffset.
  EXPECT_FALSE(info_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, -1, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad yoffset + height > height.
  EXPECT_FALSE(info_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, 1, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad width.
  EXPECT_FALSE(info_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, 0, 5, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad height.
  EXPECT_FALSE(info_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, 0, 4, 6, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad format.
  EXPECT_FALSE(info_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, 0, 4, 5, GL_RGB, GL_UNSIGNED_BYTE));
  // Check bad type.
  EXPECT_FALSE(info_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, 0, 4, 5, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4));
  // Check valid full size
  EXPECT_TRUE(info_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, 0, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check valid particial size.
  EXPECT_TRUE(info_->ValidForTexture(
      GL_TEXTURE_2D, 1, 1, 1, 2, 3, GL_RGBA, GL_UNSIGNED_BYTE));
  manager_.RemoveTextureInfo(&feature_info_, kClient1Id);
  EXPECT_FALSE(info_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, 0, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
}

TEST_F(TextureInfoTest, FloatNotLinear) {
  TextureManager manager(kMaxTextureSize, kMaxCubeMapTextureSize);
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_float");
  FeatureInfo feature_info;
  feature_info.Initialize(NULL);
  manager.CreateTextureInfo(&feature_info, kClient1Id, kService1Id);
  TextureManager::TextureInfo* info = manager_.GetTextureInfo(kClient1Id);
  ASSERT_TRUE(info != NULL);
  manager.SetInfoTarget(info, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info->target());
  manager.SetLevelInfo(&feature_info, info,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_FLOAT);
  EXPECT_FALSE(info->texture_complete());
  manager.SetParameter(&feature_info, info, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_FALSE(info->texture_complete());
  manager.SetParameter(
      &feature_info, info, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
  EXPECT_TRUE(info->texture_complete());
  manager.Destroy(false);
}

TEST_F(TextureInfoTest, FloatLinear) {
  TextureManager manager(kMaxTextureSize, kMaxCubeMapTextureSize);
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_float GL_OES_texture_float_linear");
  FeatureInfo feature_info;
  feature_info.Initialize(NULL);
  manager.CreateTextureInfo(&feature_info, kClient1Id, kService1Id);
  TextureManager::TextureInfo* info = manager_.GetTextureInfo(kClient1Id);
  ASSERT_TRUE(info != NULL);
  manager.SetInfoTarget(info, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info->target());
  manager.SetLevelInfo(&feature_info, info,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_FLOAT);
  EXPECT_TRUE(info->texture_complete());
  manager.Destroy(false);
}

TEST_F(TextureInfoTest, HalfFloatNotLinear) {
  TextureManager manager(kMaxTextureSize, kMaxCubeMapTextureSize);
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_half_float");
  FeatureInfo feature_info;
  feature_info.Initialize(NULL);
  manager.CreateTextureInfo(&feature_info, kClient1Id, kService1Id);
  TextureManager::TextureInfo* info = manager_.GetTextureInfo(kClient1Id);
  ASSERT_TRUE(info != NULL);
  manager.SetInfoTarget(info, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info->target());
  manager.SetLevelInfo(&feature_info, info,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_HALF_FLOAT_OES);
  EXPECT_FALSE(info->texture_complete());
  manager.SetParameter(&feature_info, info, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_FALSE(info->texture_complete());
  manager.SetParameter(
      &feature_info, info, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
  EXPECT_TRUE(info->texture_complete());
  manager.Destroy(false);
}

TEST_F(TextureInfoTest, HalfFloatLinear) {
  TextureManager manager(kMaxTextureSize, kMaxCubeMapTextureSize);
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_half_float GL_OES_texture_half_float_linear");
  FeatureInfo feature_info;
  feature_info.Initialize(NULL);
  manager.CreateTextureInfo(&feature_info, kClient1Id, kService1Id);
  TextureManager::TextureInfo* info = manager_.GetTextureInfo(kClient1Id);
  ASSERT_TRUE(info != NULL);
  manager.SetInfoTarget(info, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info->target());
  manager.SetLevelInfo(&feature_info, info,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_HALF_FLOAT_OES);
  EXPECT_TRUE(info->texture_complete());
  manager.Destroy(false);
}

}  // namespace gles2
}  // namespace gpu


