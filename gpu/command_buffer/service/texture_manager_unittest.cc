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
#include "gpu/command_buffer/service/texture_definition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_mock.h"

using ::testing::AtLeast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::_;

namespace gpu {
namespace gles2 {

class TextureTestHelper {
 public:
  static bool IsNPOT(const Texture* texture) {
    return texture->npot();
  }
  static bool IsTextureComplete(const Texture* texture) {
    return texture->texture_complete();
  }
  static bool IsCubeComplete(const Texture* texture) {
    return texture->cube_complete();
  }
};

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
    decoder_.reset(new ::testing::StrictMock<gles2::MockGLES2Decoder>());
  }

  virtual void TearDown() {
    manager_->Destroy(false);
    manager_.reset();
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  void SetParameter(
      Texture* texture, GLenum pname, GLint value, GLenum error) {
    TestHelper::SetTexParameterWithExpectations(
        gl_.get(), decoder_.get(), manager_.get(),
        texture, pname, value, error);
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  scoped_refptr<FeatureInfo> feature_info_;
  scoped_ptr<TextureManager> manager_;
  scoped_ptr<MockGLES2Decoder> decoder_;
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
  Texture* texture = manager_->GetTexture(kClient1Id);
  ASSERT_TRUE(texture != NULL);
  EXPECT_EQ(kService1Id, texture->service_id());
  GLuint client_id = 0;
  EXPECT_TRUE(manager_->GetClientId(texture->service_id(), &client_id));
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
  Texture* texture = manager_->GetTexture(kClient1Id);
  manager_->SetTarget(texture, GL_TEXTURE_2D);
  ASSERT_TRUE(texture != NULL);
  SetParameter(texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST, GL_NO_ERROR);
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), texture->min_filter());
  SetParameter(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST, GL_NO_ERROR);
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), texture->mag_filter());
  SetParameter(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE, GL_NO_ERROR);
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), texture->wrap_s());
  SetParameter(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE, GL_NO_ERROR);
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), texture->wrap_t());
  SetParameter(texture, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1, GL_NO_ERROR);
  SetParameter(texture, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2, GL_NO_ERROR);
  SetParameter(
      texture, GL_TEXTURE_MIN_FILTER, GL_CLAMP_TO_EDGE, GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), texture->min_filter());
  SetParameter(
      texture, GL_TEXTURE_MAG_FILTER, GL_CLAMP_TO_EDGE, GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST), texture->min_filter());
  SetParameter(texture, GL_TEXTURE_WRAP_S, GL_NEAREST, GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), texture->wrap_s());
  SetParameter(texture, GL_TEXTURE_WRAP_T, GL_NEAREST, GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_CLAMP_TO_EDGE), texture->wrap_t());
  SetParameter(texture, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0, GL_INVALID_VALUE);
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
  Texture* texture = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture != NULL);
  TestHelper::SetTexParameterWithExpectations(
      gl_.get(), decoder_.get(), &manager, texture,
      GL_TEXTURE_USAGE_ANGLE, GL_FRAMEBUFFER_ATTACHMENT_ANGLE,GL_NO_ERROR);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_ATTACHMENT_ANGLE),
            texture->usage());
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
  Texture* texture = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture != NULL);
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  TestHelper::SetupTextureManagerDestructionExpectations(gl_.get(), "");
  manager.Destroy(true);
  // Check that resources got freed.
  texture = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture == NULL);
}

TEST_F(TextureManagerTest, DestroyUnowned) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  TestHelper::SetupTextureManagerInitExpectations(gl_.get(), "");
  TextureManager manager(
      NULL, feature_info_.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.Initialize();
  // Check we can create texture.
  Texture* created_texture =
      manager.CreateTexture(kClient1Id, kService1Id);
  created_texture->SetNotOwned();

  // Check texture got created.
  Texture* texture = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture != NULL);

  // Check that it is not freed if it is not owned.
  TestHelper::SetupTextureManagerDestructionExpectations(gl_.get(), "");
  manager.Destroy(true);
  texture = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture == NULL);
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

class TextureTestBase : public testing::Test {
 public:
  static const GLint kMaxTextureSize = 16;
  static const GLint kMaxCubeMapTextureSize = 8;
  static const GLint kMax2dLevels = 5;
  static const GLint kMaxCubeMapLevels = 4;
  static const GLuint kClient1Id = 1;
  static const GLuint kService1Id = 11;

  TextureTestBase()
      : feature_info_(new FeatureInfo()) {
  }
  virtual ~TextureTestBase() {
    texture_ = NULL;
  }

 protected:
  void SetUpBase(MemoryTracker* memory_tracker) {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
    manager_.reset(new TextureManager(
        memory_tracker, feature_info_.get(),
        kMaxTextureSize, kMaxCubeMapTextureSize));
    decoder_.reset(new ::testing::StrictMock<gles2::MockGLES2Decoder>());
    manager_->CreateTexture(kClient1Id, kService1Id);
    texture_ = manager_->GetTexture(kClient1Id);
    ASSERT_TRUE(texture_.get() != NULL);
  }

  virtual void TearDown() {
    if (texture_.get()) {
      GLuint client_id = 0;
      // If it's not in the manager then setting texture_ to NULL will
      // delete the texture.
      if (!manager_->GetClientId(texture_->service_id(), &client_id)) {
        // Check that it gets deleted when the last reference is released.
        EXPECT_CALL(*gl_,
            DeleteTextures(1, ::testing::Pointee(texture_->service_id())))
            .Times(1)
            .RetiresOnSaturation();
      }
      texture_ = NULL;
    }
    manager_->Destroy(false);
    manager_.reset();
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  void SetParameter(
      Texture* texture, GLenum pname, GLint value, GLenum error) {
    TestHelper::SetTexParameterWithExpectations(
        gl_.get(), decoder_.get(), manager_.get(),
        texture, pname, value, error);
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  scoped_refptr<FeatureInfo> feature_info_;
  scoped_ptr<TextureManager> manager_;
  scoped_ptr<MockGLES2Decoder> decoder_;
  scoped_refptr<Texture> texture_;
};

class TextureTest : public TextureTestBase {
 protected:
  virtual void SetUp() {
    SetUpBase(NULL);
  }
};

class TextureMemoryTrackerTest : public TextureTestBase {
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

TEST_F(TextureTest, Basic) {
  EXPECT_EQ(0u, texture_->target());
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture_));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_EQ(0, texture_->num_uncleared_mips());
  EXPECT_FALSE(manager_->CanRender(texture_));
  EXPECT_TRUE(texture_->SafeToRenderFrom());
  EXPECT_FALSE(texture_->IsImmutable());
  EXPECT_EQ(static_cast<GLenum>(GL_NEAREST_MIPMAP_LINEAR),
            texture_->min_filter());
  EXPECT_EQ(static_cast<GLenum>(GL_LINEAR), texture_->mag_filter());
  EXPECT_EQ(static_cast<GLenum>(GL_REPEAT), texture_->wrap_s());
  EXPECT_EQ(static_cast<GLenum>(GL_REPEAT), texture_->wrap_t());
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_EQ(0u, texture_->estimated_size());
}

TEST_F(TextureTest, SetTargetTexture2D) {
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture_));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_FALSE(manager_->CanRender(texture_));
  EXPECT_TRUE(texture_->SafeToRenderFrom());
  EXPECT_FALSE(texture_->IsImmutable());
}

TEST_F(TextureTest, SetTargetTextureExternalOES) {
  manager_->SetTarget(texture_, GL_TEXTURE_EXTERNAL_OES);
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture_));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_TRUE(manager_->CanRender(texture_));
  EXPECT_TRUE(texture_->SafeToRenderFrom());
  EXPECT_TRUE(texture_->IsImmutable());
}

TEST_F(TextureTest, EstimatedSize) {
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_EQ(8u * 4u * 4u, texture_->estimated_size());
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 2, GL_RGBA, 8, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_EQ(8u * 4u * 4u * 2u, texture_->estimated_size());
}

TEST_F(TextureMemoryTrackerTest, EstimatedSize) {
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 128, MemoryTracker::kUnmanaged);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_MEMORY_ALLOCATION_CHANGE(128, 0, MemoryTracker::kUnmanaged);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 256, MemoryTracker::kUnmanaged);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 2, GL_RGBA, 8, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  // Add expectation for texture deletion.
  EXPECT_MEMORY_ALLOCATION_CHANGE(256, 0, MemoryTracker::kUnmanaged);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 0, MemoryTracker::kUnmanaged);
}

TEST_F(TextureMemoryTrackerTest, SetParameterPool) {
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 128, MemoryTracker::kUnmanaged);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_MEMORY_ALLOCATION_CHANGE(128, 0, MemoryTracker::kUnmanaged);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 128, MemoryTracker::kManaged);
  SetParameter(
      texture_, GL_TEXTURE_POOL_CHROMIUM, GL_TEXTURE_POOL_MANAGED_CHROMIUM,
      GL_NO_ERROR);
  // Add expectation for texture deletion.
  EXPECT_MEMORY_ALLOCATION_CHANGE(128, 0, MemoryTracker::kManaged);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 0, MemoryTracker::kUnmanaged);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 0, MemoryTracker::kManaged);
}

TEST_F(TextureTest, POT2D) {
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture_->target());
  // Check Setting level 0 to POT
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_FALSE(manager_->CanRender(texture_));
  EXPECT_EQ(0, texture_->num_uncleared_mips());
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  // Set filters to something that will work with a single mip.
  SetParameter(texture_, GL_TEXTURE_MIN_FILTER, GL_LINEAR, GL_NO_ERROR);
  EXPECT_TRUE(manager_->CanRender(texture_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
  // Set them back.
  SetParameter(
      texture_, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR, GL_NO_ERROR);
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());

  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_));
  // Make mips.
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(texture_));
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_TRUE(manager_->CanRender(texture_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
  // Change a mip.
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_));
  EXPECT_FALSE(manager_->CanRender(texture_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  // Set a level past the number of mips that would get generated.
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 3, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_));
  // Make mips.
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(texture_));
  EXPECT_TRUE(manager_->CanRender(texture_));
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
}

TEST_F(TextureMemoryTrackerTest, MarkMipmapsGenerated) {
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 64, MemoryTracker::kUnmanaged);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_MEMORY_ALLOCATION_CHANGE(64, 0, MemoryTracker::kUnmanaged);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 84, MemoryTracker::kUnmanaged);
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(texture_));
  EXPECT_MEMORY_ALLOCATION_CHANGE(84, 0, MemoryTracker::kUnmanaged);
  EXPECT_MEMORY_ALLOCATION_CHANGE(0, 0, MemoryTracker::kUnmanaged);
}

TEST_F(TextureTest, UnusedMips) {
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture_->target());
  // Set level zero to large size.
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(texture_));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_TRUE(manager_->CanRender(texture_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
  // Set level zero to large smaller (levels unused mips)
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(texture_));
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_TRUE(manager_->CanRender(texture_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
  // Set an unused level to some size
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 4, GL_RGBA, 16, 16, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_TRUE(manager_->CanRender(texture_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
}

TEST_F(TextureTest, NPOT2D) {
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture_->target());
  // Check Setting level 0 to NPOT
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_));
  EXPECT_FALSE(manager_->CanRender(texture_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  SetParameter(texture_, GL_TEXTURE_MIN_FILTER, GL_LINEAR, GL_NO_ERROR);
  EXPECT_FALSE(manager_->CanRender(texture_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  SetParameter(texture_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE, GL_NO_ERROR);
  EXPECT_FALSE(manager_->CanRender(texture_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  SetParameter(texture_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE, GL_NO_ERROR);
  EXPECT_TRUE(manager_->CanRender(texture_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
  // Change it to POT.
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
}

TEST_F(TextureTest, NPOT2DNPOTOK) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_npot");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  TextureManager manager(
      NULL, feature_info.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.CreateTexture(kClient1Id, kService1Id);
  Texture* texture = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture != NULL);

  manager.SetTarget(texture, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  // Check Setting level 0 to NPOT
  manager.SetLevelInfo(texture,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(TextureTestHelper::IsNPOT(texture));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_TRUE(manager.CanGenerateMipmaps(texture));
  EXPECT_FALSE(manager.CanRender(texture));
  EXPECT_TRUE(manager.HaveUnrenderableTextures());
  EXPECT_TRUE(manager.MarkMipmapsGenerated(texture));
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  EXPECT_TRUE(manager.CanRender(texture));
  EXPECT_FALSE(manager.HaveUnrenderableTextures());
  manager.Destroy(false);
}

TEST_F(TextureTest, POTCubeMap) {
  manager_->SetTarget(texture_, GL_TEXTURE_CUBE_MAP);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP), texture_->target());
  // Check Setting level 0 each face to POT
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_CUBE_MAP_POSITIVE_X,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture_));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_));
  EXPECT_FALSE(manager_->CanRender(texture_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture_));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_));
  EXPECT_FALSE(manager_->CanRender(texture_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture_));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_));
  EXPECT_FALSE(manager_->CanRender(texture_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture_));
  EXPECT_FALSE(manager_->CanRender(texture_));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_FALSE(TextureTestHelper::IsCubeComplete(texture_));
  EXPECT_FALSE(manager_->CanGenerateMipmaps(texture_));
  EXPECT_FALSE(manager_->CanRender(texture_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_TRUE(TextureTestHelper::IsCubeComplete(texture_));
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_));
  EXPECT_FALSE(manager_->CanRender(texture_));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());

  // Make mips.
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(texture_));
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_TRUE(TextureTestHelper::IsCubeComplete(texture_));
  EXPECT_TRUE(manager_->CanRender(texture_));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());

  // Change a mip.
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      1, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(TextureTestHelper::IsNPOT(texture_));
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_TRUE(TextureTestHelper::IsCubeComplete(texture_));
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_));
  // Set a level past the number of mips that would get generated.
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      3, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(manager_->CanGenerateMipmaps(texture_));
  // Make mips.
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(texture_));
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture_));
  EXPECT_TRUE(TextureTestHelper::IsCubeComplete(texture_));
}

TEST_F(TextureTest, GetLevelSize) {
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  GLsizei width = -1;
  GLsizei height = -1;
  EXPECT_FALSE(texture_->GetLevelSize(GL_TEXTURE_2D, -1, &width, &height));
  EXPECT_FALSE(texture_->GetLevelSize(GL_TEXTURE_2D, 1000, &width, &height));
  EXPECT_FALSE(texture_->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height));
  EXPECT_TRUE(texture_->GetLevelSize(GL_TEXTURE_2D, 1, &width, &height));
  EXPECT_EQ(4, width);
  EXPECT_EQ(5, height);
  manager_->RemoveTexture(kClient1Id);
  EXPECT_TRUE(texture_->GetLevelSize(GL_TEXTURE_2D, 1, &width, &height));
  EXPECT_EQ(4, width);
  EXPECT_EQ(5, height);
}

TEST_F(TextureTest, GetLevelType) {
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  GLenum type = -1;
  GLenum format = -1;
  EXPECT_FALSE(texture_->GetLevelType(GL_TEXTURE_2D, -1, &type, &format));
  EXPECT_FALSE(texture_->GetLevelType(GL_TEXTURE_2D, 1000, &type, &format));
  EXPECT_FALSE(texture_->GetLevelType(GL_TEXTURE_2D, 0, &type, &format));
  EXPECT_TRUE(texture_->GetLevelType(GL_TEXTURE_2D, 1, &type, &format));
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), format);
  manager_->RemoveTexture(kClient1Id);
  EXPECT_TRUE(texture_->GetLevelType(GL_TEXTURE_2D, 1, &type, &format));
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE), type);
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA), format);
}

TEST_F(TextureTest, ValidForTexture) {
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 5, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  // Check bad face.
  EXPECT_FALSE(texture_->ValidForTexture(
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
      1, 0, 0, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad level.
  EXPECT_FALSE(texture_->ValidForTexture(
      GL_TEXTURE_2D, 0, 0, 0, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad xoffset.
  EXPECT_FALSE(texture_->ValidForTexture(
      GL_TEXTURE_2D, 1, -1, 0, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad xoffset + width > width.
  EXPECT_FALSE(texture_->ValidForTexture(
      GL_TEXTURE_2D, 1, 1, 0, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad yoffset.
  EXPECT_FALSE(texture_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, -1, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad yoffset + height > height.
  EXPECT_FALSE(texture_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, 1, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad width.
  EXPECT_FALSE(texture_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, 0, 5, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad height.
  EXPECT_FALSE(texture_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, 0, 4, 6, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check bad format.
  EXPECT_FALSE(texture_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, 0, 4, 5, GL_RGB, GL_UNSIGNED_BYTE));
  // Check bad type.
  EXPECT_FALSE(texture_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, 0, 4, 5, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4));
  // Check valid full size
  EXPECT_TRUE(texture_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, 0, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
  // Check valid particial size.
  EXPECT_TRUE(texture_->ValidForTexture(
      GL_TEXTURE_2D, 1, 1, 1, 2, 3, GL_RGBA, GL_UNSIGNED_BYTE));
  manager_->RemoveTexture(kClient1Id);
  EXPECT_TRUE(texture_->ValidForTexture(
      GL_TEXTURE_2D, 1, 0, 0, 4, 5, GL_RGBA, GL_UNSIGNED_BYTE));
}

TEST_F(TextureTest, FloatNotLinear) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_float");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  TextureManager manager(
      NULL, feature_info.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.CreateTexture(kClient1Id, kService1Id);
  Texture* texture = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture != NULL);
  manager.SetTarget(texture, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  manager.SetLevelInfo(texture,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_FLOAT, true);
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  TestHelper::SetTexParameterWithExpectations(
      gl_.get(), decoder_.get(), &manager,
      texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST, GL_NO_ERROR);
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  TestHelper::SetTexParameterWithExpectations(
      gl_.get(), decoder_.get(), &manager,
      texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST, GL_NO_ERROR);
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  manager.Destroy(false);
}

TEST_F(TextureTest, FloatLinear) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_float GL_OES_texture_float_linear");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  TextureManager manager(
      NULL, feature_info.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.CreateTexture(kClient1Id, kService1Id);
  Texture* texture = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture != NULL);
  manager.SetTarget(texture, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  manager.SetLevelInfo(texture,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_FLOAT, true);
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  manager.Destroy(false);
}

TEST_F(TextureTest, HalfFloatNotLinear) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_half_float");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  TextureManager manager(
      NULL, feature_info.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.CreateTexture(kClient1Id, kService1Id);
  Texture* texture = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture != NULL);
  manager.SetTarget(texture, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  manager.SetLevelInfo(texture,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_HALF_FLOAT_OES, true);
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  TestHelper::SetTexParameterWithExpectations(
      gl_.get(), decoder_.get(), &manager,
      texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST, GL_NO_ERROR);
  EXPECT_FALSE(TextureTestHelper::IsTextureComplete(texture));
  TestHelper::SetTexParameterWithExpectations(
      gl_.get(), decoder_.get(), &manager,
      texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST, GL_NO_ERROR);
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  manager.Destroy(false);
}

TEST_F(TextureTest, HalfFloatLinear) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_texture_half_float GL_OES_texture_half_float_linear");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  TextureManager manager(
      NULL, feature_info.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.CreateTexture(kClient1Id, kService1Id);
  Texture* texture = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture != NULL);
  manager.SetTarget(texture, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture->target());
  manager.SetLevelInfo(texture,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_HALF_FLOAT_OES, true);
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture));
  manager.Destroy(false);
}

TEST_F(TextureTest, EGLImageExternal) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_OES_EGL_image_external");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  TextureManager manager(
      NULL, feature_info.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.CreateTexture(kClient1Id, kService1Id);
  Texture* texture = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture != NULL);
  manager.SetTarget(texture, GL_TEXTURE_EXTERNAL_OES);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_EXTERNAL_OES), texture->target());
  EXPECT_FALSE(manager.CanGenerateMipmaps(texture));
  manager.Destroy(false);
}

TEST_F(TextureTest, DepthTexture) {
  TestHelper::SetupFeatureInfoInitExpectations(
      gl_.get(), "GL_ANGLE_depth_texture");
  scoped_refptr<FeatureInfo> feature_info(new FeatureInfo());
  feature_info->Initialize(NULL);
  TextureManager manager(
      NULL, feature_info.get(), kMaxTextureSize, kMaxCubeMapTextureSize);
  manager.CreateTexture(kClient1Id, kService1Id);
  Texture* texture = manager.GetTexture(kClient1Id);
  ASSERT_TRUE(texture != NULL);
  manager.SetTarget(texture, GL_TEXTURE_2D);
  manager.SetLevelInfo(
      texture, GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 4, 4, 1, 0,
      GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, false);
  EXPECT_FALSE(manager.CanGenerateMipmaps(texture));
  manager.Destroy(false);
}

TEST_F(TextureTest, SafeUnsafe) {
  static const GLuint kClient2Id = 2;
  static const GLuint kService2Id = 12;
  static const GLuint kClient3Id = 3;
  static const GLuint kService3Id = 13;
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture_->num_uncleared_mips());
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_FALSE(texture_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, texture_->num_uncleared_mips());
  manager_->SetLevelCleared(texture_, GL_TEXTURE_2D, 0, true);
  EXPECT_TRUE(texture_->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture_->num_uncleared_mips());
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_FALSE(texture_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, texture_->num_uncleared_mips());
  manager_->SetLevelCleared(texture_, GL_TEXTURE_2D, 1, true);
  EXPECT_TRUE(texture_->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture_->num_uncleared_mips());
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_FALSE(texture_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(2, texture_->num_uncleared_mips());
  manager_->SetLevelCleared(texture_, GL_TEXTURE_2D, 0, true);
  EXPECT_FALSE(texture_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, texture_->num_uncleared_mips());
  manager_->SetLevelCleared(texture_, GL_TEXTURE_2D, 1, true);
  EXPECT_TRUE(texture_->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture_->num_uncleared_mips());
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_FALSE(texture_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, texture_->num_uncleared_mips());
  manager_->MarkMipmapsGenerated(texture_);
  EXPECT_TRUE(texture_->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture_->num_uncleared_mips());

  manager_->CreateTexture(kClient2Id, kService2Id);
  scoped_refptr<Texture> texture2(
      manager_->GetTexture(kClient2Id));
  ASSERT_TRUE(texture2.get() != NULL);
  manager_->SetTarget(texture2, GL_TEXTURE_2D);
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture2->num_uncleared_mips());
  manager_->SetLevelInfo(texture2,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture2->num_uncleared_mips());
  manager_->SetLevelInfo(texture2,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, texture2->num_uncleared_mips());

  manager_->CreateTexture(kClient3Id, kService3Id);
  scoped_refptr<Texture> texture3(
      manager_->GetTexture(kClient3Id));
  ASSERT_TRUE(texture3.get() != NULL);
  manager_->SetTarget(texture3, GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture3,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, texture3->num_uncleared_mips());
  manager_->SetLevelCleared(texture2, GL_TEXTURE_2D, 0, true);
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture2->num_uncleared_mips());
  manager_->SetLevelCleared(texture3, GL_TEXTURE_2D, 0, true);
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture3->num_uncleared_mips());

  manager_->SetLevelInfo(texture2,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  manager_->SetLevelInfo(texture3,
      GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, texture2->num_uncleared_mips());
  EXPECT_EQ(1, texture3->num_uncleared_mips());
  manager_->RemoveTexture(kClient3Id);
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  manager_->RemoveTexture(kClient2Id);
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService2Id)))
      .Times(1)
      .RetiresOnSaturation();
  texture2 = NULL;
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService3Id)))
      .Times(1)
      .RetiresOnSaturation();
  texture3 = NULL;
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
}

TEST_F(TextureTest, ClearTexture) {
  EXPECT_CALL(*decoder_, ClearLevel(_, _, _, _, _, _, _, _, _))
      .WillRepeatedly(Return(true));
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_FALSE(texture_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(2, texture_->num_uncleared_mips());
  manager_->ClearRenderableLevels(decoder_.get(), texture_);
  EXPECT_TRUE(texture_->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture_->num_uncleared_mips());
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_FALSE(texture_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(2, texture_->num_uncleared_mips());
  manager_->ClearTextureLevel(decoder_.get(), texture_, GL_TEXTURE_2D, 0);
  EXPECT_FALSE(texture_->SafeToRenderFrom());
  EXPECT_TRUE(manager_->HaveUnsafeTextures());
  EXPECT_TRUE(manager_->HaveUnclearedMips());
  EXPECT_EQ(1, texture_->num_uncleared_mips());
  manager_->ClearTextureLevel(decoder_.get(), texture_, GL_TEXTURE_2D, 1);
  EXPECT_TRUE(texture_->SafeToRenderFrom());
  EXPECT_FALSE(manager_->HaveUnsafeTextures());
  EXPECT_FALSE(manager_->HaveUnclearedMips());
  EXPECT_EQ(0, texture_->num_uncleared_mips());
}

TEST_F(TextureTest, UseDeletedTexture) {
  static const GLuint kClient2Id = 2;
  static const GLuint kService2Id = 12;
  // Make the default texture renderable
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
  // Make a new texture
  manager_->CreateTexture(kClient2Id, kService2Id);
  scoped_refptr<Texture> texture(
      manager_->GetTexture(kClient2Id));
  manager_->SetTarget(texture, GL_TEXTURE_2D);
  EXPECT_FALSE(manager_->CanRender(texture));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  // Remove it.
  manager_->RemoveTexture(kClient2Id);
  EXPECT_FALSE(manager_->CanRender(texture));
  EXPECT_TRUE(manager_->HaveUnrenderableTextures());
  // Check that we can still manipulate it and it effects the manager.
  manager_->SetLevelInfo(texture,
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  EXPECT_TRUE(manager_->CanRender(texture));
  EXPECT_FALSE(manager_->HaveUnrenderableTextures());
  EXPECT_CALL(*gl_, DeleteTextures(1, ::testing::Pointee(kService2Id)))
      .Times(1)
      .RetiresOnSaturation();
  texture = NULL;
}

TEST_F(TextureTest, GetLevelImage) {
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(texture_->GetLevelImage(GL_TEXTURE_2D, 1) == NULL);
  // Set image.
  manager_->SetLevelImage(texture_,
      GL_TEXTURE_2D, 1, gfx::GLImage::CreateGLImage(0));
  EXPECT_FALSE(texture_->GetLevelImage(GL_TEXTURE_2D, 1) == NULL);
  // Remove it.
  manager_->SetLevelImage(texture_, GL_TEXTURE_2D, 1, NULL);
  EXPECT_TRUE(texture_->GetLevelImage(GL_TEXTURE_2D, 1) == NULL);
  manager_->SetLevelImage(texture_,
      GL_TEXTURE_2D, 1, gfx::GLImage::CreateGLImage(0));
  // Image should be reset when SetLevelInfo is called.
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  EXPECT_TRUE(texture_->GetLevelImage(GL_TEXTURE_2D, 1) == NULL);
}

namespace {

bool InSet(std::set<std::string>* string_set, const std::string& str) {
  std::pair<std::set<std::string>::iterator, bool> result =
      string_set->insert(str);
  return !result.second;
}

}  // anonymous namespace

TEST_F(TextureTest, AddToSignature) {
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  std::string signature1;
  std::string signature2;
  manager_->AddToSignature(texture_, GL_TEXTURE_2D, 1, &signature1);

  std::set<std::string> string_set;
  EXPECT_FALSE(InSet(&string_set, signature1));

  // check changing 1 thing makes a different signature.
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 4, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  manager_->AddToSignature(texture_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  // check putting it back makes the same signature.
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
  signature2.clear();
  manager_->AddToSignature(texture_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_EQ(signature1, signature2);

  // Check setting cleared status does not change signature.
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  signature2.clear();
  manager_->AddToSignature(texture_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_EQ(signature1, signature2);

  // Check changing other settings changes signature.
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  signature2.clear();
  manager_->AddToSignature(texture_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, false);
  signature2.clear();
  manager_->AddToSignature(texture_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, false);
  signature2.clear();
  manager_->AddToSignature(texture_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, false);
  signature2.clear();
  manager_->AddToSignature(texture_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_FLOAT,
      false);
  signature2.clear();
  manager_->AddToSignature(texture_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  // put it back
  manager_->SetLevelInfo(texture_,
      GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
      false);
  signature2.clear();
  manager_->AddToSignature(texture_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_EQ(signature1, signature2);

  // check changing parameters changes signature.
  SetParameter(texture_, GL_TEXTURE_MIN_FILTER, GL_NEAREST, GL_NO_ERROR);
  signature2.clear();
  manager_->AddToSignature(texture_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  SetParameter(
      texture_, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR, GL_NO_ERROR);
  SetParameter(texture_, GL_TEXTURE_MAG_FILTER, GL_NEAREST, GL_NO_ERROR);
  signature2.clear();
  manager_->AddToSignature(texture_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  SetParameter(texture_, GL_TEXTURE_MAG_FILTER, GL_LINEAR, GL_NO_ERROR);
  SetParameter(texture_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE, GL_NO_ERROR);
  signature2.clear();
  manager_->AddToSignature(texture_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  SetParameter(texture_, GL_TEXTURE_WRAP_S, GL_REPEAT, GL_NO_ERROR);
  SetParameter(texture_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE, GL_NO_ERROR);
  signature2.clear();
  manager_->AddToSignature(texture_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_FALSE(InSet(&string_set, signature2));

  // Check putting it back genenerates the same signature
  SetParameter(texture_, GL_TEXTURE_WRAP_T, GL_REPEAT, GL_NO_ERROR);
  signature2.clear();
  manager_->AddToSignature(texture_, GL_TEXTURE_2D, 1, &signature2);
  EXPECT_EQ(signature1, signature2);

  // Check the set was acutally getting different signatures.
  EXPECT_EQ(11u, string_set.size());
}

class SaveRestoreTextureTest : public TextureTest {
 public:
  virtual void SetUp() {
    TextureTest::SetUp();
    manager_->CreateTexture(kClient2Id, kService2Id);
    texture2_ = manager_->GetTexture(kClient2Id);
  }

  virtual void TearDown() {
    if (texture2_.get()) {
      GLuint client_id = 0;
      // If it's not in the manager then setting texture2_ to NULL will
      // delete the texture.
      if (!manager_->GetClientId(texture2_->service_id(), &client_id)) {
        // Check that it gets deleted when the last reference is released.
        EXPECT_CALL(
            *gl_,
            DeleteTextures(1, ::testing::Pointee(texture2_->service_id())))
            .Times(1).RetiresOnSaturation();
      }
      texture2_ = NULL;
    }
    TextureTest::TearDown();
  }

 protected:
  struct LevelInfo {
    LevelInfo(GLenum target,
              GLenum format,
              GLsizei width,
              GLsizei height,
              GLsizei depth,
              GLint border,
              GLenum type,
              bool cleared)
        : target(target),
          format(format),
          width(width),
          height(height),
          depth(depth),
          border(border),
          type(type),
          cleared(cleared) {}

    LevelInfo()
        : target(0),
          format(0),
          width(-1),
          height(-1),
          depth(1),
          border(0),
          type(0),
          cleared(false) {}

    bool operator==(const LevelInfo& other) const {
      return target == other.target && format == other.format &&
             width == other.width && height == other.height &&
             depth == other.depth && border == other.border &&
             type == other.type && cleared == other.cleared;
    }

    GLenum target;
    GLenum format;
    GLsizei width;
    GLsizei height;
    GLsizei depth;
    GLint border;
    GLenum type;
    bool cleared;
  };

  void SetLevelInfo(Texture* texture, GLint level, const LevelInfo& info) {
    manager_->SetLevelInfo(texture,
                           info.target,
                           level,
                           info.format,
                           info.width,
                           info.height,
                           info.depth,
                           info.border,
                           info.format,
                           info.type,
                           info.cleared);
  }

  static LevelInfo GetLevelInfo(const Texture* texture,
                                GLint target,
                                GLint level) {
    LevelInfo info;
    info.target = target;
    EXPECT_TRUE(texture->GetLevelSize(target, level, &info.width,
                                      &info.height));
    EXPECT_TRUE(texture->GetLevelType(target, level, &info.type,
                                      &info.format));
    info.cleared = texture->IsLevelCleared(target, level);
    return info;
  }

  TextureDefinition* Save(Texture* texture) {
    EXPECT_CALL(*gl_, GenTextures(_, _))
        .WillOnce(SetArgumentPointee<1>(kService2Id));
    TextureDefinition* definition = manager_->Save(texture);
    EXPECT_TRUE(definition != NULL);
    return definition;
  }

  void Restore(Texture* texture, TextureDefinition* definition) {
    EXPECT_CALL(*gl_, DeleteTextures(1, Pointee(texture->service_id())))
        .Times(1).RetiresOnSaturation();
    EXPECT_CALL(*gl_,
                BindTexture(definition->target(), definition->service_id()))
        .Times(1).RetiresOnSaturation();
    EXPECT_CALL(*gl_, TexParameteri(_, _, _)).Times(AtLeast(1));

    EXPECT_TRUE(
        manager_->Restore("TextureTest", decoder_.get(), texture, definition));
  }

  scoped_refptr<Texture> texture2_;

 private:
  static const GLuint kEmptyTextureServiceId;
  static const GLuint kClient2Id;
  static const GLuint kService2Id;
};

const GLuint SaveRestoreTextureTest::kClient2Id = 2;
const GLuint SaveRestoreTextureTest::kService2Id = 12;
const GLuint SaveRestoreTextureTest::kEmptyTextureServiceId = 13;

TEST_F(SaveRestoreTextureTest, SaveRestore2D) {
  manager_->SetTarget(texture_, GL_TEXTURE_2D);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_2D), texture_->target());
  LevelInfo level0(
      GL_TEXTURE_2D, GL_RGBA, 4, 4, 1, 0, GL_UNSIGNED_BYTE, true);
  SetLevelInfo(texture_, 0, level0);
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(texture_));
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture_));
  LevelInfo level1 = GetLevelInfo(texture_.get(), GL_TEXTURE_2D, 1);
  LevelInfo level2 = GetLevelInfo(texture_.get(), GL_TEXTURE_2D, 2);
  scoped_ptr<TextureDefinition> definition(Save(texture_));
  const TextureDefinition::LevelInfos& infos = definition->level_infos();
  EXPECT_EQ(1U, infos.size());
  EXPECT_EQ(3U, infos[0].size());

  // Make this texture bigger with more levels, and make sure they get
  // clobbered correctly during Restore().
  manager_->SetTarget(texture2_, GL_TEXTURE_2D);
  SetLevelInfo(
      texture2_,
      0,
      LevelInfo(GL_TEXTURE_2D, GL_RGBA, 16, 16, 1, 0, GL_UNSIGNED_BYTE, false));
  EXPECT_TRUE(manager_->MarkMipmapsGenerated(texture2_));
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture2_));
  EXPECT_EQ(1024U + 256U + 64U + 16U + 4U, texture2_->estimated_size());
  Restore(texture2_, definition.release());
  EXPECT_EQ(level0, GetLevelInfo(texture2_.get(), GL_TEXTURE_2D, 0));
  EXPECT_EQ(level1, GetLevelInfo(texture2_.get(), GL_TEXTURE_2D, 1));
  EXPECT_EQ(level2, GetLevelInfo(texture2_.get(), GL_TEXTURE_2D, 2));
  EXPECT_EQ(64U + 16U + 4U, texture2_->estimated_size());
  GLint w, h;
  EXPECT_TRUE(texture2_->GetLevelSize(GL_TEXTURE_2D, 3, &w, &h));
  EXPECT_EQ(0, w);
  EXPECT_EQ(0, h);
}

TEST_F(SaveRestoreTextureTest, SaveRestoreClearRectangle) {
  manager_->SetTarget(texture_, GL_TEXTURE_RECTANGLE_ARB);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_RECTANGLE_ARB), texture_->target());
  LevelInfo level0(
      GL_TEXTURE_RECTANGLE_ARB, GL_RGBA, 1, 1, 1, 0, GL_UNSIGNED_BYTE, false);
  SetLevelInfo(texture_, 0, level0);
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture_));
  scoped_ptr<TextureDefinition> definition(Save(texture_));
  const TextureDefinition::LevelInfos& infos = definition->level_infos();
  EXPECT_EQ(1U, infos.size());
  EXPECT_EQ(1U, infos[0].size());
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_RECTANGLE_ARB), infos[0][0].target);
  manager_->SetTarget(texture2_, GL_TEXTURE_RECTANGLE_ARB);
  Restore(texture2_, definition.release());

  // See if we can clear the previously uncleared level now.
  EXPECT_EQ(level0, GetLevelInfo(texture2_.get(), GL_TEXTURE_RECTANGLE_ARB, 0));
  EXPECT_CALL(*decoder_, ClearLevel(_, _, _, _, _, _, _, _, _))
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(manager_->ClearTextureLevel(
      decoder_.get(), texture2_, GL_TEXTURE_RECTANGLE_ARB, 0));
}

TEST_F(SaveRestoreTextureTest, SaveRestoreCube) {
  manager_->SetTarget(texture_, GL_TEXTURE_CUBE_MAP);
  EXPECT_EQ(static_cast<GLenum>(GL_TEXTURE_CUBE_MAP), texture_->target());
  LevelInfo face0(GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                  GL_RGBA,
                  1,
                  1,
                  1,
                  0,
                  GL_UNSIGNED_BYTE,
                  true);
  LevelInfo face5(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
                  GL_RGBA,
                  3,
                  3,
                  1,
                  0,
                  GL_UNSIGNED_BYTE,
                  true);
  SetLevelInfo(texture_, 0, face0);
  SetLevelInfo(texture_, 0, face5);
  EXPECT_TRUE(TextureTestHelper::IsTextureComplete(texture_));
  scoped_ptr<TextureDefinition> definition(Save(texture_));
  const TextureDefinition::LevelInfos& infos = definition->level_infos();
  EXPECT_EQ(6U, infos.size());
  EXPECT_EQ(1U, infos[0].size());
  manager_->SetTarget(texture2_, GL_TEXTURE_CUBE_MAP);
  Restore(texture2_, definition.release());
  EXPECT_EQ(face0,
            GetLevelInfo(texture2_.get(), GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0));
  EXPECT_EQ(face5,
            GetLevelInfo(texture2_.get(), GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0));
}

}  // namespace gles2
}  // namespace gpu


