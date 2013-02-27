// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_manager.h"

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::Pointee;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace gpu {
namespace gles2 {

class TextureManagerTest : public testing::Test {
 public:
  static const GLint kMaxTextureSize = 16;
  static const GLint kMaxCubeMapTextureSize = 8;
  static const GLint kMaxExternalTextureSize = 16;
  static const GLint kMax2dLevels = 5;
  static const GLint kMaxCubeMapLevels = 4;
  static const GLint kMaxExternalLevels = 1;

  TextureManagerTest()
      : feature_info_(new FeatureInfo()) {
  }

  virtual ~TextureManagerTest() {
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());

    manager_.reset(new TextureManager(
        NULL, feature_info_.get(),
        kMaxTextureSize, kMaxCubeMapTextureSize));
    TestHelper::SetupTextureManagerInitExpectations(gl_.get(), "");
    manager_->Initialize();
  }

  virtual void TearDown() {
    manager_->Destroy(false);
    manager_.reset();
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  scoped_refptr<FeatureInfo> feature_info_;
  scoped_ptr<TextureManager> manager_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const GLint TextureManagerTest::kMaxTextureSize;
const GLint TextureManagerTest::kMaxCubeMapTextureSize;
const GLint TextureManagerTest::kMaxExternalTextureSize;
const GLint TextureManagerTest::kMax2dLevels;
const GLint TextureManagerTest::kMaxCubeMapLevels;
const GLint TextureManagerTest::kMaxExternalLevels;
#endif

TEST_F(TextureManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  // Check we can create texture.
  manager_->CreateTexture(kClient1Id, kService1Id);
  // Check texture got created.
  Texture* info1 = manager_->GetTexture(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_EQ(kService1Id, info1->service_id());
  GLuint client_id = 0;
  EXPECT_TRUE(manager_->GetClientId(info1->service_id(), &client_id));
  EXPECT_EQ(kClient1Id, client_id);
  // Check we get nothing for a non-existent texture.
  EXPECT_TRUE(manager_->GetTexture(kClient2Id) == NULL);
  // Check trying to a remove non-existent textures does not crash.
  manager_->RemoveTexture(kClient2Id);
  // Check that it gets deleted when the last reference is released.
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  // Check we can't get the texture after we remove it.
  manager_->RemoveTexture(kClient1Id);
  EXPECT_TRUE(manager_->GetTexture(kClient1Id) == NULL);
}

TEST_F(TextureManagerTest, SetParameter) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create texture.
  manager_->CreateTexture(kClient1Id, kService1Id);
  // Check texture got created.
  Texture* info = manager_->GetTexture(kClient1Id);
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), manager_->SetParameter(
      info, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), info->min_filter());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), manager_->SetParameter(
      info, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), info->mag_filter());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), manager_->SetParameter(
      info, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), info->wrap_s());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), manager_->SetParameter(
      info, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), info->wrap_t());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), manager_->SetParameter(
      info, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1));
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), manager_->SetParameter(
      info, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2));
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), manager_->SetParameter(
      info, GL_TEXTURE_MIN_FILTER, GL_CLAMP_TO_EDGE));
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), info->min_filter());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), manager_->SetParameter(
      info, GL_TEXTURE_MAG_FILTER, GL_CLAMP_TO_EDGE));
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), info->min_filter());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), manager_->SetParameter(
      info, GL_TEXTURE_WRAP_S, GL_NEAREST));
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), info->wrap_s());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_ENUM), manager_->SetParameter(
      info, GL_TEXTURE_WRAP_T, GL_NEAREST));
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), info->wrap_t());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_VALUE), manager_->SetParameter(
      info, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0));
}

TEST_F(TextureManagerTest, TextureUsageExt) {
  TestHelper::SetupTextureManagerInitExpectations(gl_.get(),
                                                  "GL_ANGLE_texture_usage");
  TextureManager manager(
      NULL, feature_info_.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.Initialize();
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  // Check we can create texture.
  manager.CreateTexture(kClient1Id, kService1Id);
  // Check texture got created.
  Texture* info = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), manager.SetParameter(
      info, GL_TEXTURE_USAGE_ANGLE, GL_FRAMEBUFFER_ATTACHMENT_ANGLE));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_ATTACHMENT_ANGLE),
            info->usage());
  manager.Destroy(false);
}

TEST_F(TextureManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  TestHelper::SetupTextureManagerInitExpectations(gl_.get(), "");
  TextureManager manager(
      NULL, feature_info_.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.Initialize();
  // Check we can create texture.
  manager.CreateTexture(kClient1Id, kService1Id);
  // Check texture got created.
  Texture* info1 = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(info1 != NULL);
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  TestHelper::SetupTextureManagerDestructionExpectations(gl_.get(), "");
  manager.Destroy(true);
  // Check that resources got freed.
  info1 = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(info1 == NULL);
}

TEST_F(TextureManagerTest, DestroyUnowned) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  TestHelper::SetupTextureManagerInitExpectations(gl_.get(), "");
  TextureManager manager(
      NULL, feature_info_.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.Initialize();
  // Check we can create texture.
  Texture* created_info =
      manager.CreateTexture(kClient1Id, kService1Id);
  created_info->SetNotOwned();

  // Check texture got created.
  Texture* info1 = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(info1 != NULL);

  // Check that it is not freed if it is not owned.
  TestHelper::SetupTextureManagerDestructionExpectations(gl_.get(), "");
  manager.Destroy(true);
  info1 = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(info1 == NULL);
}

TEST_F(TextureManagerTest, MaxValues) {
  // Check we get the right values for the max sizes.
  EXPECT_EQ(kMax2dLevels, manager_->MaxLevelsForTarget(GL_TEXTURE_2D));
  EXPECT_EQ(kMaxCubeMapLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_CUBE_MAP));
  EXPECT_EQ(kMaxCubeMapLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_CUBE_MAP_POSITIVE_X));
  EXPECT_EQ(kMaxCubeMapLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_CUBE_MAP_NEGATIVE_X));
  EXPECT_EQ(kMaxCubeMapLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_CUBE_MAP_POSITIVE_Y));
  EXPECT_EQ(kMaxCubeMapLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y));
  EXPECT_EQ(kMaxCubeMapLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_CUBE_MAP_POSITIVE_Z));
  EXPECT_EQ(kMaxCubeMapLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z));
  EXPECT_EQ(kMaxExternalLevels,
            manager_->MaxLevelsForTarget(GL_TEXTURE_EXTERNAL_OES));
  EXPECT_EQ(kMaxTextureSize, manager_->MaxSizeForTarget(GL_TEXTURE_2D));
  EXPECT_EQ(kMaxCubeMapTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP));
  EXPECT_EQ(kMaxCubeMapTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP_POSITIVE_X));
  EXPECT_EQ(kMaxCubeMapTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP_NEGATIVE_X));
  EXPECT_EQ(kMaxCubeMapTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP_POSITIVE_Y));
  EXPECT_EQ(kMaxCubeMapTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y));
  EXPECT_EQ(kMaxCubeMapTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP_POSITIVE_Z));
  EXPECT_EQ(kMaxCubeMapTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z));
  EXPECT_EQ(kMaxExternalTextureSize,
            manager_->MaxSizeForTarget(GL_TEXTURE_EXTERNAL_OES));
}

TEST_F(TextureManagerTest, ValidForTarget) {
  // check 2d
  EXPECT_TRUE(manager_->ValidForTarget(
      GL_TEXTURE_2D, 0, kMaxTextureSize, kMaxTextureSize, 1));
  EXPECT_TRUE(manager_->ValidForTarget(
      GL_TEXTURE_2D, kMax2dLevels - 1, 1, 1, 1));
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_2D, kMax2dLevels - 1, 1, 2, 1));
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_2D, kMax2dLevels - 1, 2, 1, 1));
  // check level out of range.
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_2D, kMax2dLevels, kMaxTextureSize, 1, 1));
  // check has depth.
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_2D, kMax2dLevels, kMaxTextureSize, 1, 2));
  // Check NPOT width on level 0
  EXPECT_TRUE(manager_->ValidForTarget(GL_TEXTURE_2D, 0, 5, 2, 1));
  // Check NPOT height on level 0
  EXPECT_TRUE(manager_->ValidForTarget(GL_TEXTURE_2D, 0, 2, 5, 1));
  // Check NPOT width on level 1
  EXPECT_FALSE(manager_->ValidForTarget(GL_TEXTURE_2D, 1, 5, 2, 1));
  // Check NPOT height on level 1
  EXPECT_FALSE(manager_->ValidForTarget(GL_TEXTURE_2D, 1, 2, 5, 1));

  // check cube
  EXPECT_TRUE(manager_->ValidForTarget(
      GL_TEXTURE_CUBE_MAP, 0,
      kMaxCubeMapTextureSize, kMaxCubeMapTextureSize, 1));
  EXPECT_TRUE(manager_->ValidForTarget(
      GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels - 1, 1, 1, 1));
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels - 1, 2, 2, 1));
  // check level out of range.
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels,
      kMaxCubeMapTextureSize, 1, 1));
  // check not square.
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels,
      kMaxCubeMapTextureSize, 1, 1));
  // check has depth.
  EXPECT_FALSE(manager_->ValidForTarget(
      GL_TEXTURE_CUBE_MAP, kMaxCubeMapLevels,
      kMaxCubeMapTextureSize, 1, 2));

  for (GLint level = 0; level < kMax2dLevels; ++level) {
    EXPECT_TRUE(manager_->ValidForTarget(
        GL_TEXTURE_2D, level, kMaxTextureSize >> level, 1, 1));
    EXPECT_TRUE(manager_->ValidForTarget(
        GL_TEXTURE_2D, level, 1, kMaxTextureSize >> level, 1));
    EXPECT_FALSE(manager_->ValidForTarget(
        GL_TEXTURE_2D, level, (kMaxTextureSize >> level) + 1, 1, 1));
    EXPECT_FALSE(manager_->ValidForTarget(
        GL_TEXTURE_2D, level, 1, (kMaxTextureSize >> level) + 1, 1));
  }

  for (GLint level = 0; level < kMaxCubeMapLevels; ++level) {
    EXPECT_TRUE(manager_->ValidForTarget(
        GL_TEXTURE_CUBE_MAP, level,
        kMaxCubeMapTextureSize >> level,
        kMaxCubeMapTextureSize >> level,
        1));
    EXPECT_FALSE(manager_->ValidForTarget(
        GL_TEXTURE_CUBE_MAP, level,
        (kMaxCubeMapTextureSize >> level) * 2,
        (kMaxCubeMapTextureSize >> level) * 2,
        1));
  }
}

TEST_F(TextureManagerTest, ValidForTargetNPOT) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_npot");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  TextureManager manager(
     NULL, feature_info.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  // Check NPOT width on level 0
  EXPECT_TRUE(manager.ValidForTarget(GL_TEXTURE_2D, 0, 5, 2, 1));
  // Check NPOT height on level 0
  EXPECT_TRUE(manager.ValidForTarget(GL_TEXTURE_2D, 0, 2, 5, 1));
  // Check NPOT width on level 1
  EXPECT_TRUE(manager.ValidForTarget(GL_TEXTURE_2D, 1, 5, 2, 1));
  // Check NPOT height on level 1
  EXPECT_TRUE(manager.ValidForTarget(GL_TEXTURE_2D, 1, 2, 5, 1));
  manager.Destroy(false);
}

class TextureInfoTestBase : public testing::Test {
 public:
  static const GLint kMaxTextureSize = 16;
  static const GLint kMaxCubeMapTextureSize = 8;
  static const GLint kMax2dLevels = 5;
  static const GLint kMaxCubeMapLevels = 4;
  static const GLuint kClient1Id = 1;
  static const GLuint kService1Id = 11;

  TextureInfoTestBase()
      : feature_info_(new FeatureInfo()) {
  }
  virtual ~TextureInfoTestBase() {
    info_ = NULL;
  }

 protected:
  void SetUpBase(MemoryTracker* memory_tracker) {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
    manager_.reset(new TextureManager(
        memory_tracker, feature_info_.get(),
        kMaxTextureSize, kMaxCubeMapTextureSize));
    manager_->CreateTexture(kClient1Id, kService1Id);
    info_ = manager_->GetTexture(kClient1Id);
    ASSERT_TRUE(info_.get() != NULL);
  }

  virtual void TearDown() {
    if (info_.get()) {
      GLuint client_id = 0;
      // If it's not in the manager then setting info_ to NULL will
      // delete the texture.
      if (!manager_->GetClientId(info_->service_id(), &client_id)) {
        // Check that it gets deleted when the last reference is released.
        EXPECT_CALL(*gl_,
            DeleteTextures(1, ::testing::Pointee(info_->service_id())))
            .Times(1)
            .RetiresOnSaturation();
      }
      info_ = NULL;
    }
    manager_->Destroy(false);
    manager_.reset();
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  scoped_refptr<FeatureInfo> feature_info_;
  scoped_ptr<TextureManager> manager_;
  scoped_refptr<Texture> info_;
};

class TextureInfoTest : public TextureInfoTestBase {
 protected:
  virtual void SetUp() {
    SetUpBase(NULL);
  }
};

class TextureInfoMemoryTrackerTest : public TextureInfoTestBase {
 protected:
  virtual void SetUp() {
    mock_memory_tracker_ = new StrictMock<MockMemoryTracker>();
    SetUpBase(mock_memory_tracker_.get());
  }

  scoped_refptr<MockMemoryTracker> mock_memory_tracker_;
};

#define EXPECT_MEMORY_ALLOCATION_CHANGE(old_size, new_size, pool) \
    EXPECT_CALL(*mock_memory_tracker_, \
                TrackMemoryAllocatedChange(old_size, new_size, pool)) \
        .Times(1) \
        .RetiresOnSaturation() \

TEST_F(TextureInfoTest, Basic) {
  EXPECT_EQ(0u, info_->target());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(manager_->CanGenerateMipmaps(info_));
  EXPECT_FALSE(info_->npot());
  EXPECT_EQ(0, info_->num_uncleared_mips());
  EXPECT_FALSE(manager_->CanRender(info_));
  EXPECT_TRUE(info_->SafeToRenderFrom());
  EXPECT_FALSE(info_->IsImmutable());
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST_MIPMAP_LINEAR), info_->min_filter());
  EXPECT_EQ(static_cast<GLenum>(GL_LINEAR), info_->mag_filter());
  EXPECT_EQ(static_cast<GLenum>(GL_REPEAT), info_->wrap_s());
  EXPECT_EQ(static_cast<GLenum>(GL_REPEAT), info_->wrap_t());
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_EQ(0u, info_->estimated_size());
}

TEST_F(TextureInfoTest, EstimatedSize) {
  manager_->SetInfoTarget(info_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_EQ(8u * 4u * 4u, info_->estimated_size());
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 2, GL_RGBA, 8, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_EQ(8u * 4u * 4u * 2u, info_->estimated_size());
}

TEST_F(TextureInfoMemoryTrackerTest, EstimatedSize) {
  manager_->SetInfoTarget(info_, GL_TEXTURE_2D);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 128, MemoryTracker::kUnmanaged);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_MEMORY_ALLOCATION_CHANGE(128, 0, MemoryTracker::kUnmanaged);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 256, MemoryTracker::kUnmanaged);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 2, GL_RGBA, 8, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  // Add expectation for texture deletion.
  EXPECT_MEMORY_ALLOCATION_CHANGE(256, 0, MemoryTracker::kUnmanaged);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 0, MemoryTracker::kUnmanaged);
}

TEST_F(TextureInfoMemoryTrackerTest, SetParameterPool) {
  manager_->SetInfoTarget(info_, GL_TEXTURE_2D);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 128, MemoryTracker::kUnmanaged);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_MEMORY_ALLOCATION_CHANGE(128, 0, MemoryTracker::kUnmanaged);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 128, MemoryTracker::kManaged);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), manager_->SetParameter(
      info_, GL_TEXTURE_POOL_CHROMIUM, GL_TEXTURE_POOL_MANAGED_CHROMIUM));
  // Add expectation for texture deletion.
  EXPECT_MEMORY_ALLOCATION_CHANGE(128, 0, MemoryTracker::kManaged);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 0, MemoryTracker::kUnmanaged);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 0, MemoryTracker::kManaged);
}

TEST_F(TextureInfoTest, POT2D) {
  manager_->SetInfoTarget(info_, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info_->target());
  // Check Setting level 0 to POT
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(manager_->CanRender(info_));
  EXPECT_EQ(0, info_->num_uncleared_mips());
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  // Set filters to something that will work with a single mip.
  manager_->SetParameter(info_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  EXPECT_TRUE(manager_->CanRender(info_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
  // Set them back.
  manager_->SetParameter(info_, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());

  EXPECT_TRUE(manager_->CanGenerateMipmaps(info_));
  // Make mips.
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(info_));
  EXPECT_TRUE(info_->texture_complete());
  EXPECT_TRUE(manager_->CanRender(info_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
  // Change a mip.
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(manager_->CanGenerateMipmaps(info_));
  EXPECT_FALSE(manager_->CanRender(info_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  // Set a level past the number of mips that would get generated.
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 3, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(manager_->CanGenerateMipmaps(info_));
  // Make mips.
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(info_));
  EXPECT_TRUE(manager_->CanRender(info_));
  EXPECT_TRUE(info_->texture_complete());
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
}

TEST_F(TextureInfoMemoryTrackerTest, MarkMipmapsGenerated) {
  manager_->SetInfoTarget(info_, GL_TEXTURE_2D);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 64, MemoryTracker::kUnmanaged);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_MEMORY_ALLOCATION_CHANGE(64, 0, MemoryTracker::kUnmanaged);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 84, MemoryTracker::kUnmanaged);
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(info_));
  EXPECT_MEMORY_ALLOCATION_CHANGE(84, 0, MemoryTracker::kUnmanaged);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 0, MemoryTracker::kUnmanaged);
}

TEST_F(TextureInfoTest, UnusedMips) {
  manager_->SetInfoTarget(info_, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info_->target());
  // Set level zero to large size.
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(info_));
  EXPECT_FALSE(info_->npot());
  EXPECT_TRUE(info_->texture_complete());
  EXPECT_TRUE(manager_->CanRender(info_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
  // Set level zero to large smaller (levels unused mips)
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(info_));
  EXPECT_FALSE(info_->npot());
  EXPECT_TRUE(info_->texture_complete());
  EXPECT_TRUE(manager_->CanRender(info_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
  // Set an unused level to some size
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 4, GL_RGBA, 16, 16, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(info_->npot());
  EXPECT_TRUE(info_->texture_complete());
  EXPECT_TRUE(manager_->CanRender(info_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
}

TEST_F(TextureInfoTest, NPOT2D) {
  manager_->SetInfoTarget(info_, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info_->target());
  // Check Setting level 0 to NPOT
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(manager_->CanGenerateMipmaps(info_));
  EXPECT_FALSE(manager_->CanRender(info_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  manager_->SetParameter(info_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  EXPECT_FALSE(manager_->CanRender(info_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  manager_->SetParameter(info_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  EXPECT_FALSE(manager_->CanRender(info_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  manager_->SetParameter(info_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  EXPECT_TRUE(manager_->CanRender(info_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
  // Change it to POT.
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(manager_->CanGenerateMipmaps(info_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
}

TEST_F(TextureInfoTest, NPOT2DNPOTOK) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_npot");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  TextureManager manager(
      NULL, feature_info.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.CreateTexture(kClient1Id, kService1Id);
  Texture* info = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(info_ != NULL);

  manager.SetInfoTarget(info, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info->target());
  // Check Setting level 0 to NPOT
  manager.SetLevelInfo(info,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(info->npot());
  EXPECT_FALSE(info->texture_complete());
  EXPECT_TRUE(manager.CanGenerateMipmaps(info));
  EXPECT_FALSE(manager.CanRender(info));
  EXPECT_TRUE(manager.HaveUnrenderableTextures());
  EXPECT_TRUE(manager.MarkMipmapsGenerated(info));
  EXPECT_TRUE(info->texture_complete());
  EXPECT_TRUE(manager.CanRender(info));
  EXPECT_FALSE(manager.HaveUnrenderableTextures());
  manager.Destroy(false);
}

TEST_F(TextureInfoTest, POTCubeMap) {
  manager_->SetInfoTarget(info_, GL_TEXTURE_CUBE_MAP);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP), info_->target());
  // Check Setting level 0 each face to POT
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_POSITIVE_X,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(manager_->CanGenerateMipmaps(info_));
  EXPECT_FALSE(manager_->CanRender(info_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(manager_->CanGenerateMipmaps(info_));
  EXPECT_FALSE(manager_->CanRender(info_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(manager_->CanGenerateMipmaps(info_));
  EXPECT_FALSE(manager_->CanRender(info_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(manager_->CanRender(info_));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(info_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_FALSE(info_->cube_complete());
  EXPECT_FALSE(manager_->CanGenerateMipmaps(info_));
  EXPECT_FALSE(manager_->CanRender(info_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());
  EXPECT_TRUE(manager_->CanGenerateMipmaps(info_));
  EXPECT_FALSE(manager_->CanRender(info_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());

  // Make mips.
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(info_));
  EXPECT_TRUE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());
  EXPECT_TRUE(manager_->CanRender(info_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());

  // Change a mip.
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      1, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(info_->npot());
  EXPECT_FALSE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());
  EXPECT_TRUE(manager_->CanGenerateMipmaps(info_));
  // Set a level past the number of mips that would get generated.
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      3, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(manager_->CanGenerateMipmaps(info_));
  // Make mips.
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(info_));
  EXPECT_TRUE(info_->texture_complete());
  EXPECT_TRUE(info_->cube_complete());
}

TEST_F(TextureInfoTest, GetLevelSize) {
  manager_->SetInfoTarget(info_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  GLsizei width = -1;
  GLsizei height = -1;
  EXPECT_FALSE(info_->GetLevelSize(GL_TEXTURE_2D, -1, &width, &height));
  EXPECT_FALSE(info_->GetLevelSize(GL_TEXTURE_2D, 1000, &width, &height));
  EXPECT_FALSE(info_->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height));
  EXPECT_TRUE(info_->GetLevelSize(GL_TEXTURE_2D, 1, &width, &height));
  EXPECT_EQ(4, width);
  EXPECT_EQ(5, height);
  manager_->RemoveTexture(kClient1Id);
  EXPECT_TRUE(info_->GetLevelSize(GL_TEXTURE_2D, 1, &width, &height));
  EXPECT_EQ(4, width);
  EXPECT_EQ(5, height);
}

TEST_F(TextureInfoTest, GetLevelType) {
  manager_->SetInfoTarget(info_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  GLenum type = -1;
  GLenum format = -1;
  EXPECT_FALSE(info_->GetLevelType(GL_TEXTURE_2D, -1, &type, &format));
  EXPECT_FALSE(info_->GetLevelType(GL_TEXTURE_2D, 1000, &type, &format));
  EXPECT_FALSE(info_->GetLevelType(GL_TEXTURE_2D, 0, &type, &format));
  EXPECT_TRUE(info_->GetLevelType(GL_TEXTURE_2D, 1, &type, &format));
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), format);
  manager_->RemoveTexture(kClient1Id);
  EXPECT_TRUE(info_->GetLevelType(GL_TEXTURE_2D, 1, &type, &format));
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), format);
}

TEST_F(TextureInfoTest, ValidForTexture) {
  manager_->SetInfoTarget(info_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
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
  manager_->RemoveTexture(kClient1Id);
  EXPECT_TRUE(info_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, 0, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
}

TEST_F(TextureInfoTest, FloatNotLinear) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_float");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  TextureManager manager(
      NULL, feature_info.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.CreateTexture(kClient1Id, kService1Id);
  Texture* info = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(info != NULL);
  manager.SetInfoTarget(info, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info->target());
  manager.SetLevelInfo(info,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_FLOAT, true);
  EXPECT_FALSE(info->texture_complete());
  manager.SetParameter(info, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_FALSE(info->texture_complete());
  manager.SetParameter(info, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
  EXPECT_TRUE(info->texture_complete());
  manager.Destroy(false);
}

TEST_F(TextureInfoTest, FloatLinear) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_float GL_OES_texture_float_linear");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  TextureManager manager(
      NULL, feature_info.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.CreateTexture(kClient1Id, kService1Id);
  Texture* info = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(info != NULL);
  manager.SetInfoTarget(info, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info->target());
  manager.SetLevelInfo(info,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_FLOAT, true);
  EXPECT_TRUE(info->texture_complete());
  manager.Destroy(false);
}

TEST_F(TextureInfoTest, HalfFloatNotLinear) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_half_float");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  TextureManager manager(
      NULL, feature_info.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.CreateTexture(kClient1Id, kService1Id);
  Texture* info = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(info != NULL);
  manager.SetInfoTarget(info, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info->target());
  manager.SetLevelInfo(info,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_HALF_FLOAT_OES, true);
  EXPECT_FALSE(info->texture_complete());
  manager.SetParameter(info, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  EXPECT_FALSE(info->texture_complete());
  manager.SetParameter(info, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
  EXPECT_TRUE(info->texture_complete());
  manager.Destroy(false);
}

TEST_F(TextureInfoTest, HalfFloatLinear) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_half_float GL_OES_texture_half_float_linear");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  TextureManager manager(
      NULL, feature_info.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.CreateTexture(kClient1Id, kService1Id);
  Texture* info = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(info != NULL);
  manager.SetInfoTarget(info, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), info->target());
  manager.SetLevelInfo(info,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_HALF_FLOAT_OES, true);
  EXPECT_TRUE(info->texture_complete());
  manager.Destroy(false);
}

TEST_F(TextureInfoTest, EGLImageExternal) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_EGL_image_external");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  TextureManager manager(
      NULL, feature_info.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.CreateTexture(kClient1Id, kService1Id);
  Texture* info = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(info != NULL);
  manager.SetInfoTarget(info, GL_TEXTURE_EXTERNAL_OES);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_EXTERNAL_OES), info->target());
  EXPECT_FALSE(manager.CanGenerateMipmaps(info));
  manager.Destroy(false);
}

TEST_F(TextureInfoTest, DepthTexture) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_ANGLE_depth_texture");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  TextureManager manager(
      NULL, feature_info.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.CreateTexture(kClient1Id, kService1Id);
  Texture* info = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(info != NULL);
  manager.SetInfoTarget(info, GL_TEXTURE_2D);
  manager.SetLevelInfo(
      info, GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 4, 4, 1, 0,
      GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, false);
  EXPECT_FALSE(manager.CanGenerateMipmaps(info));
  manager.Destroy(false);
}

TEST_F(TextureInfoTest, SafeUnsafe) {
  static const GLuint kClient2Id = 2;
  static const GLuint kService2Id = 12;
  static const GLuint kClient3Id = 3;
  static const GLuint kService3Id = 13;
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, info_->num_uncleared_mips());
  manager_->SetInfoTarget(info_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_FALSE(info_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, info_->num_uncleared_mips());
  manager_->SetLevelCleared(info_, GL_TEXTURE_2D, 0, true);
  EXPECT_TRUE(info_->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, info_->num_uncleared_mips());
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_FALSE(info_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, info_->num_uncleared_mips());
  manager_->SetLevelCleared(info_, GL_TEXTURE_2D, 1, true);
  EXPECT_TRUE(info_->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, info_->num_uncleared_mips());
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_FALSE(info_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(2, info_->num_uncleared_mips());
  manager_->SetLevelCleared(info_, GL_TEXTURE_2D, 0, true);
  EXPECT_FALSE(info_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, info_->num_uncleared_mips());
  manager_->SetLevelCleared(info_, GL_TEXTURE_2D, 1, true);
  EXPECT_TRUE(info_->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, info_->num_uncleared_mips());
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_FALSE(info_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, info_->num_uncleared_mips());
  manager_->MarkMipmapsGenerated(info_);
  EXPECT_TRUE(info_->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, info_->num_uncleared_mips());

  manager_->CreateTexture(kClient2Id, kService2Id);
  scoped_refptr<Texture> info2(
      manager_->GetTexture(kClient2Id));
  ASSERT_TRUE(info2.get() != NULL);
  manager_->SetInfoTarget(info2, GL_TEXTURE_2D);
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, info2->num_uncleared_mips());
  manager_->SetLevelInfo(info2,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, info2->num_uncleared_mips());
  manager_->SetLevelInfo(info2,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, info2->num_uncleared_mips());

  manager_->CreateTexture(kClient3Id, kService3Id);
  scoped_refptr<Texture> info3(
      manager_->GetTexture(kClient3Id));
  ASSERT_TRUE(info3.get() != NULL);
  manager_->SetInfoTarget(info3, GL_TEXTURE_2D);
  manager_->SetLevelInfo(info3,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, info3->num_uncleared_mips());
  manager_->SetLevelCleared(info2, GL_TEXTURE_2D, 0, true);
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, info2->num_uncleared_mips());
  manager_->SetLevelCleared(info3, GL_TEXTURE_2D, 0, true);
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, info3->num_uncleared_mips());

  manager_->SetLevelInfo(info2,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  manager_->SetLevelInfo(info3,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, info2->num_uncleared_mips());
  EXPECT_EQ(1, info3->num_uncleared_mips());
  manager_->RemoveTexture(kClient3Id);
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  manager_->RemoveTexture(kClient2Id);
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService2Id)))
      .Times(1)
      .RetiresOnSaturation();
  info2 = NULL;
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService3Id)))
      .Times(1)
      .RetiresOnSaturation();
  info3 = NULL;
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
}

TEST_F(TextureInfoTest, ClearTexture) {
  scoped_ptr<MockGLES2Decoder> decoder(new gles2::MockGLES2Decoder());
  EXPECT_CALL(*decoder, ClearLevel(_, _, _, _, _, _, _, _, _))
      .WillRepeatedly(Return(true));
  manager_->SetInfoTarget(info_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_FALSE(info_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(2, info_->num_uncleared_mips());
  manager_->ClearRenderableLevels(decoder.get(), info_);
  EXPECT_TRUE(info_->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, info_->num_uncleared_mips());
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_FALSE(info_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(2, info_->num_uncleared_mips());
  manager_->ClearTextureLevel(decoder.get(), info_, GL_TEXTURE_2D, 0);
  EXPECT_FALSE(info_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, info_->num_uncleared_mips());
  manager_->ClearTextureLevel(decoder.get(), info_, GL_TEXTURE_2D, 1);
  EXPECT_TRUE(info_->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, info_->num_uncleared_mips());
}

TEST_F(TextureInfoTest, UseDeletedTexture) {
  static const GLuint kClient2Id = 2;
  static const GLuint kService2Id = 12;
  // Make the default texture renderable
  manager_->SetInfoTarget(info_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
  // Make a new texture
  manager_->CreateTexture(kClient2Id, kService2Id);
  scoped_refptr<Texture> info(
      manager_->GetTexture(kClient2Id));
  manager_->SetInfoTarget(info, GL_TEXTURE_2D);
  EXPECT_FALSE(manager_->CanRender(info));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  // Remove it.
  manager_->RemoveTexture(kClient2Id);
  EXPECT_FALSE(manager_->CanRender(info));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  // Check that we can still manipulate it and it effects the manager.
  manager_->SetLevelInfo(info,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_TRUE(manager_->CanRender(info));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService2Id)))
      .Times(1)
      .RetiresOnSaturation();
  info = NULL;
}

TEST_F(TextureInfoTest, GetLevelImage) {
  manager_->SetInfoTarget(info_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(info_->GetLevelImage(GL_TEXTURE_2D, 1) == NULL);
  // Set image.
  manager_->SetLevelImage(info_,
      GL_TEXTURE_2D, 1, gfx::GLImage::CreateGLImage(0));
  EXPECT_FALSE(info_->GetLevelImage(GL_TEXTURE_2D, 1) == NULL);
  // Remove it.
  manager_->SetLevelImage(info_, GL_TEXTURE_2D, 1, NULL);
  EXPECT_TRUE(info_->GetLevelImage(GL_TEXTURE_2D, 1) == NULL);
  manager_->SetLevelImage(info_,
      GL_TEXTURE_2D, 1, gfx::GLImage::CreateGLImage(0));
  // Image should be reset when SetLevelInfo is called.
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(info_->GetLevelImage(GL_TEXTURE_2D, 1) == NULL);
}

namespace {

bool InSet(std::set<std::string>* string_set, const std::string& str) {
  std::pair<std::set<std::string>::iterator, bool> result =
      string_set->insert(str);
  return !result.second;
}

}  // anonymous namespace

TEST_F(TextureInfoTest, AddToSignature) {
  manager_->SetInfoTarget(info_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  std::string signature1;
  std::string signature2;
  manager_->AddToSignature(info_, GL_TEXTURE_2D, 1, &signature1);

  std::set<std::string> string_set;
  EXPECT_FALSE(InSet(&string_set, signature1));

  // check changing 1 thing makes a different signature.
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  manager_->AddToSignature(info_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  // check putting it back makes the same signature.
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  signature2.clear();
  manager_->AddToSignature(info_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_EQ(signature1, signature2);

  // Check setting cleared status does not change signature.
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  signature2.clear();
  manager_->AddToSignature(info_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_EQ(signature1, signature2);

  // Check changing other settings changes signature.
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  signature2.clear();
  manager_->AddToSignature(info_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  signature2.clear();
  manager_->AddToSignature(info_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, false);
  signature2.clear();
  manager_->AddToSignature(info_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, false);
  signature2.clear();
  manager_->AddToSignature(info_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_FLOAT,
      false);
  signature2.clear();
  manager_->AddToSignature(info_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  // put it back
  manager_->SetLevelInfo(info_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      false);
  signature2.clear();
  manager_->AddToSignature(info_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_EQ(signature1, signature2);

  // check changing parameters changes signature.
  manager_->SetParameter(info_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  signature2.clear();
  manager_->AddToSignature(info_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  manager_->SetParameter(
      info_, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
  manager_->SetParameter(info_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  signature2.clear();
  manager_->AddToSignature(info_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  manager_->SetParameter(info_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  manager_->SetParameter(info_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  signature2.clear();
  manager_->AddToSignature(info_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  manager_->SetParameter(info_, GL_TEXTURE_WRAP_S, GL_REPEAT);
  manager_->SetParameter(info_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  signature2.clear();
  manager_->AddToSignature(info_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  // Check putting it back genenerates the same signature
  manager_->SetParameter(info_, GL_TEXTURE_WRAP_T, GL_REPEAT);
  signature2.clear();
  manager_->AddToSignature(info_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_EQ(signature1, signature2);

  // Check the set was acutally getting different signatures.
  EXPECT_EQ(11u, string_set.size());
}


}  // namespace gles2
}  // namespace gpu


