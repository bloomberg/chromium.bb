// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/texture_pool.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "media/gpu/android/mock_command_buffer_stub_wrapper.h"
#include "media/gpu/android/texture_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using testing::NiceMock;
using testing::Return;

// SupportsWeakPtr so it's easy to tell when it has been destroyed.
class MockTextureWrapper : public NiceMock<TextureWrapper>,
                           public base::SupportsWeakPtr<MockTextureWrapper> {
 public:
  MockTextureWrapper() {}
  ~MockTextureWrapper() override {}

  MOCK_METHOD0(ForceContextLost, void());
};

class TexturePoolTest : public testing::Test {
 public:
  TexturePoolTest() = default;

  void SetUp() override {
    std::unique_ptr<MockCommandBufferStubWrapper> stub =
        std::make_unique<MockCommandBufferStubWrapper>();
    stub_ = stub.get();
    SetContextCanBeCurrent(true);
    texture_pool_ = new TexturePool(std::move(stub));
  }

  using WeakTexture = base::WeakPtr<MockTextureWrapper>;

  // Set whether or not |stub_| will report that MakeCurrent worked.
  void SetContextCanBeCurrent(bool allow) {
    ON_CALL(*stub_, MakeCurrent()).WillByDefault(Return(allow));
  }

  WeakTexture CreateAndAddTexture() {
    std::unique_ptr<MockTextureWrapper> texture =
        std::make_unique<MockTextureWrapper>();
    WeakTexture texture_weak = texture->AsWeakPtr();

    texture_pool_->AddTexture(std::move(texture));

    return texture_weak;
  }

  scoped_refptr<TexturePool> texture_pool_;
  MockCommandBufferStubWrapper* stub_ = nullptr;
};

TEST_F(TexturePoolTest, AddAndReleaseTexturesWithContext) {
  // Test that adding then deleting a texture destroys it.
  WeakTexture texture = CreateAndAddTexture();
  // The texture should not be notified that the context was lost.
  EXPECT_CALL(*texture.get(), ForceContextLost()).Times(0);
  EXPECT_CALL(*stub_, MakeCurrent()).Times(1);
  texture_pool_->ReleaseTexture(texture.get());
  ASSERT_FALSE(texture);
}

TEST_F(TexturePoolTest, AddAndReleaseTexturesWithoutContext) {
  // Test that adding then deleting a texture destroys it, and marks that the
  // context is lost.
  WeakTexture texture = CreateAndAddTexture();
  SetContextCanBeCurrent(false);
  EXPECT_CALL(*texture, ForceContextLost()).Times(1);
  EXPECT_CALL(*stub_, MakeCurrent()).Times(1);
  texture_pool_->ReleaseTexture(texture.get());
  ASSERT_FALSE(texture);
}

TEST_F(TexturePoolTest, TexturesAreReleasedOnStubDestructionWithContext) {
  // Add multiple textures, and test that they're all destroyed when the stub
  // says that it's destroyed.
  std::vector<TextureWrapper*> raw_textures;
  std::vector<WeakTexture> textures;

  for (int i = 0; i < 3; i++) {
    textures.push_back(CreateAndAddTexture());
    raw_textures.push_back(textures.back().get());
    // The context should not be lost.
    EXPECT_CALL(*textures.back(), ForceContextLost()).Times(0);
  }

  EXPECT_CALL(*stub_, MakeCurrent()).Times(1);

  stub_->NotifyDestruction();

  // TextureWrappers should be destroyed.
  for (auto& texture : textures)
    ASSERT_FALSE(texture);

  // It should be okay to release the textures after they're destroyed, and
  // nothing should crash.
  for (auto* raw_texture : raw_textures)
    texture_pool_->ReleaseTexture(raw_texture);
}

TEST_F(TexturePoolTest, TexturesAreReleasedOnStubDestructionWithoutContext) {
  std::vector<TextureWrapper*> raw_textures;
  std::vector<WeakTexture> textures;

  for (int i = 0; i < 3; i++) {
    textures.push_back(CreateAndAddTexture());
    raw_textures.push_back(textures.back().get());
    EXPECT_CALL(*textures.back(), ForceContextLost()).Times(1);
  }

  SetContextCanBeCurrent(false);
  EXPECT_CALL(*stub_, MakeCurrent()).Times(1);

  stub_->NotifyDestruction();

  for (auto& texture : textures)
    ASSERT_FALSE(texture);

  // It should be okay to release the textures after they're destroyed, and
  // nothing should crash.
  for (auto* raw_texture : raw_textures)
    texture_pool_->ReleaseTexture(raw_texture);
}

TEST_F(TexturePoolTest, NonEmptyPoolAfterStubDestructionDoesntCrash) {
  // Make sure that we can delete the stub, and verify that pool teardown still
  // works (doesn't crash) even though the pool is not empty.
  CreateAndAddTexture();

  stub_->NotifyDestruction();
}

TEST_F(TexturePoolTest,
       NonEmptyPoolAfterStubWithoutContextDestructionDoesntCrash) {
  // Make sure that we can delete the stub, and verify that pool teardown still
  // works (doesn't crash) even though the pool is not empty.
  CreateAndAddTexture();

  SetContextCanBeCurrent(false);
  stub_->NotifyDestruction();
}

}  // namespace media
