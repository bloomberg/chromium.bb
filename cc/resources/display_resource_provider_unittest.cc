// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/display_resource_provider.h"
#include "cc/resources/layer_tree_resource_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/common/quads/shared_bitmap.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/shared_bitmap_manager.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "components/viz/test/test_texture.h"
#include "components/viz/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace cc {
namespace {

MATCHER_P(MatchesSyncToken, sync_token, "") {
  gpu::SyncToken other;
  memcpy(&other, arg, sizeof(other));
  return other == sync_token;
}

static void ReleaseSharedBitmapCallback(viz::SharedBitmapId shared_bitmap_id,
                                        bool* release_called,
                                        gpu::SyncToken* release_sync_token,
                                        bool* lost_resource_result,
                                        const gpu::SyncToken& sync_token,
                                        bool lost_resource) {
  *release_called = true;
  *release_sync_token = sync_token;
  *lost_resource_result = lost_resource;
}

static viz::SharedBitmapId CreateAndFillSharedBitmap(
    viz::SharedBitmapManager* manager,
    const gfx::Size& size,
    viz::ResourceFormat format,
    uint32_t value) {
  viz::SharedBitmapId shared_bitmap_id = viz::SharedBitmap::GenerateId();

  std::unique_ptr<base::SharedMemory> shm =
      viz::bitmap_allocation::AllocateMappedBitmap(size, viz::RGBA_8888);
  manager->ChildAllocatedSharedBitmap(
      viz::bitmap_allocation::DuplicateAndCloseMappedBitmap(shm.get(), size,
                                                            viz::RGBA_8888),
      shared_bitmap_id);

  std::fill_n(static_cast<uint32_t*>(shm->memory()), size.GetArea(), value);
  return shared_bitmap_id;
}

// Shared data between multiple ResourceProviderContext. This contains mailbox
// contents as well as information about sync points.
class ContextSharedData {
 public:
  static std::unique_ptr<ContextSharedData> Create() {
    return base::WrapUnique(new ContextSharedData());
  }

  uint32_t InsertFenceSync() { return next_fence_sync_++; }

  void GenMailbox(GLbyte* mailbox) {
    memset(mailbox, 0, GL_MAILBOX_SIZE_CHROMIUM);
    memcpy(mailbox, &next_mailbox_, sizeof(next_mailbox_));
    ++next_mailbox_;
  }

  void ProduceTexture(const GLbyte* mailbox_name,
                      const gpu::SyncToken& sync_token,
                      scoped_refptr<viz::TestTexture> texture) {
    uint32_t sync_point = static_cast<uint32_t>(sync_token.release_count());

    unsigned mailbox = 0;
    memcpy(&mailbox, mailbox_name, sizeof(mailbox));
    ASSERT_TRUE(mailbox && mailbox < next_mailbox_);
    textures_[mailbox] = texture;
    ASSERT_LT(sync_point_for_mailbox_[mailbox], sync_point);
    sync_point_for_mailbox_[mailbox] = sync_point;
  }

  scoped_refptr<viz::TestTexture> ConsumeTexture(
      const GLbyte* mailbox_name,
      const gpu::SyncToken& sync_token) {
    unsigned mailbox = 0;
    memcpy(&mailbox, mailbox_name, sizeof(mailbox));
    DCHECK(mailbox && mailbox < next_mailbox_);

    // If the latest sync point the context has waited on is before the sync
    // point for when the mailbox was set, pretend we never saw that
    // ProduceTexture.
    if (sync_point_for_mailbox_[mailbox] > sync_token.release_count()) {
      NOTREACHED();
      return scoped_refptr<viz::TestTexture>();
    }
    return textures_[mailbox];
  }

 private:
  ContextSharedData() : next_fence_sync_(1), next_mailbox_(1) {}

  uint64_t next_fence_sync_;
  unsigned next_mailbox_;
  using TextureMap =
      std::unordered_map<unsigned, scoped_refptr<viz::TestTexture>>;
  TextureMap textures_;
  std::unordered_map<unsigned, uint32_t> sync_point_for_mailbox_;
};

class ResourceProviderContext : public viz::TestWebGraphicsContext3D {
 public:
  static std::unique_ptr<ResourceProviderContext> Create(
      ContextSharedData* shared_data) {
    return base::WrapUnique(new ResourceProviderContext(shared_data));
  }

  void genSyncToken(GLbyte* sync_token) override {
    uint64_t fence_sync = shared_data_->InsertFenceSync();
    gpu::SyncToken sync_token_data(gpu::CommandBufferNamespace::GPU_IO,
                                   gpu::CommandBufferId::FromUnsafeValue(0x123),
                                   fence_sync);
    sync_token_data.SetVerifyFlush();
    // Commit the ProduceTextureDirectCHROMIUM calls at this point, so that
    // they're associated with the sync point.
    for (const std::unique_ptr<PendingProduceTexture>& pending_texture :
         pending_produce_textures_) {
      shared_data_->ProduceTexture(pending_texture->mailbox, sync_token_data,
                                   pending_texture->texture);
    }
    pending_produce_textures_.clear();
    memcpy(sync_token, &sync_token_data, sizeof(sync_token_data));
  }

  void waitSyncToken(const GLbyte* sync_token) override {
    gpu::SyncToken sync_token_data;
    if (sync_token)
      memcpy(&sync_token_data, sync_token, sizeof(sync_token_data));

    if (sync_token_data.release_count() >
        last_waited_sync_token_.release_count()) {
      last_waited_sync_token_ = sync_token_data;
    }
  }

  const gpu::SyncToken& last_waited_sync_token() const {
    return last_waited_sync_token_;
  }

  void texStorage2DEXT(GLenum target,
                       GLint levels,
                       GLuint internalformat,
                       GLint width,
                       GLint height) override {
    CheckTextureIsBound(target);
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    ASSERT_EQ(1, levels);
    GLenum format = GL_RGBA;
    switch (internalformat) {
      case GL_RGBA8_OES:
        break;
      case GL_BGRA8_EXT:
        format = GL_BGRA_EXT;
        break;
      default:
        NOTREACHED();
    }
    AllocateTexture(gfx::Size(width, height), format);
  }

  void texImage2D(GLenum target,
                  GLint level,
                  GLenum internalformat,
                  GLsizei width,
                  GLsizei height,
                  GLint border,
                  GLenum format,
                  GLenum type,
                  const void* pixels) override {
    CheckTextureIsBound(target);
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    ASSERT_FALSE(level);
    ASSERT_EQ(internalformat, format);
    ASSERT_FALSE(border);
    ASSERT_EQ(static_cast<unsigned>(GL_UNSIGNED_BYTE), type);
    AllocateTexture(gfx::Size(width, height), format);
    if (pixels)
      SetPixels(0, 0, width, height, pixels);
  }

  void texSubImage2D(GLenum target,
                     GLint level,
                     GLint xoffset,
                     GLint yoffset,
                     GLsizei width,
                     GLsizei height,
                     GLenum format,
                     GLenum type,
                     const void* pixels) override {
    CheckTextureIsBound(target);
    ASSERT_EQ(static_cast<unsigned>(GL_TEXTURE_2D), target);
    ASSERT_FALSE(level);
    ASSERT_EQ(static_cast<unsigned>(GL_UNSIGNED_BYTE), type);
    {
      base::AutoLock lock_for_texture_access(namespace_->lock);
      ASSERT_EQ(GLDataFormat(BoundTexture(target)->format), format);
    }
    ASSERT_TRUE(pixels);
    SetPixels(xoffset, yoffset, width, height, pixels);
  }

  void genMailboxCHROMIUM(GLbyte* mailbox) override {
    return shared_data_->GenMailbox(mailbox);
  }

  void produceTextureDirectCHROMIUM(GLuint texture,
                                    const GLbyte* mailbox) override {
    // Delay moving the texture into the mailbox until the next
    // sync token, so that it is not visible to other contexts that
    // haven't waited on that sync point.
    std::unique_ptr<PendingProduceTexture> pending(new PendingProduceTexture);
    memcpy(pending->mailbox, mailbox, sizeof(pending->mailbox));
    base::AutoLock lock_for_texture_access(namespace_->lock);
    pending->texture = UnboundTexture(texture);
    pending_produce_textures_.push_back(std::move(pending));
  }

  GLuint createAndConsumeTextureCHROMIUM(const GLbyte* mailbox) override {
    GLuint texture_id = createTexture();
    base::AutoLock lock_for_texture_access(namespace_->lock);
    scoped_refptr<viz::TestTexture> texture =
        shared_data_->ConsumeTexture(mailbox, last_waited_sync_token_);
    namespace_->textures.Replace(texture_id, texture);
    return texture_id;
  }

  void GetPixels(const gfx::Size& size,
                 viz::ResourceFormat format,
                 uint8_t* pixels) {
    CheckTextureIsBound(GL_TEXTURE_2D);
    base::AutoLock lock_for_texture_access(namespace_->lock);
    scoped_refptr<viz::TestTexture> texture = BoundTexture(GL_TEXTURE_2D);
    ASSERT_EQ(texture->size, size);
    ASSERT_EQ(texture->format, format);
    memcpy(pixels, texture->data.get(), TextureSizeBytes(size, format));
  }

 protected:
  explicit ResourceProviderContext(ContextSharedData* shared_data)
      : shared_data_(shared_data) {}

 private:
  void AllocateTexture(const gfx::Size& size, GLenum format) {
    CheckTextureIsBound(GL_TEXTURE_2D);
    viz::ResourceFormat texture_format = viz::RGBA_8888;
    switch (format) {
      case GL_RGBA:
        texture_format = viz::RGBA_8888;
        break;
      case GL_BGRA_EXT:
        texture_format = viz::BGRA_8888;
        break;
    }
    base::AutoLock lock_for_texture_access(namespace_->lock);
    BoundTexture(GL_TEXTURE_2D)->Reallocate(size, texture_format);
  }

  void SetPixels(int xoffset,
                 int yoffset,
                 int width,
                 int height,
                 const void* pixels) {
    CheckTextureIsBound(GL_TEXTURE_2D);
    base::AutoLock lock_for_texture_access(namespace_->lock);
    scoped_refptr<viz::TestTexture> texture = BoundTexture(GL_TEXTURE_2D);
    ASSERT_TRUE(texture->data.get());
    ASSERT_TRUE(xoffset >= 0 && xoffset + width <= texture->size.width());
    ASSERT_TRUE(yoffset >= 0 && yoffset + height <= texture->size.height());
    ASSERT_TRUE(pixels);
    size_t in_pitch = TextureSizeBytes(gfx::Size(width, 1), texture->format);
    size_t out_pitch =
        TextureSizeBytes(gfx::Size(texture->size.width(), 1), texture->format);
    uint8_t* dest = texture->data.get() + yoffset * out_pitch +
                    TextureSizeBytes(gfx::Size(xoffset, 1), texture->format);
    const uint8_t* src = static_cast<const uint8_t*>(pixels);
    for (int i = 0; i < height; ++i) {
      memcpy(dest, src, in_pitch);
      dest += out_pitch;
      src += in_pitch;
    }
  }

  struct PendingProduceTexture {
    GLbyte mailbox[GL_MAILBOX_SIZE_CHROMIUM];
    scoped_refptr<viz::TestTexture> texture;
  };
  ContextSharedData* shared_data_;
  gpu::SyncToken last_waited_sync_token_;
  std::vector<std::unique_ptr<PendingProduceTexture>> pending_produce_textures_;
};

class DisplayResourceProviderTest : public testing::TestWithParam<bool> {
 public:
  explicit DisplayResourceProviderTest(bool child_needs_sync_token)
      : use_gpu_(GetParam()),
        child_needs_sync_token_(child_needs_sync_token),
        shared_data_(ContextSharedData::Create()) {
    if (use_gpu_) {
      auto context3d(ResourceProviderContext::Create(shared_data_.get()));
      context3d_ = context3d.get();
      context_provider_ =
          viz::TestContextProvider::Create(std::move(context3d));
      context_provider_->UnboundTestContext3d()
          ->set_support_texture_format_bgra8888(true);
      context_provider_->BindToCurrentThread();

      auto child_context_owned =
          ResourceProviderContext::Create(shared_data_.get());
      child_context_ = child_context_owned.get();
      child_context_provider_ =
          viz::TestContextProvider::Create(std::move(child_context_owned));
      child_context_provider_->UnboundTestContext3d()
          ->set_support_texture_format_bgra8888(true);
      child_context_provider_->BindToCurrentThread();
      gpu_memory_buffer_manager_ =
          std::make_unique<viz::TestGpuMemoryBufferManager>();
    } else {
      shared_bitmap_manager_ = std::make_unique<viz::TestSharedBitmapManager>();
    }

    resource_provider_ = std::make_unique<DisplayResourceProvider>(
        context_provider_.get(), shared_bitmap_manager_.get());

    MakeChildResourceProvider();
  }

  DisplayResourceProviderTest() : DisplayResourceProviderTest(true) {}

  bool use_gpu() const { return use_gpu_; }

  void MakeChildResourceProvider() {
    child_resource_provider_ = std::make_unique<LayerTreeResourceProvider>(
        child_context_provider_.get(), child_needs_sync_token_);
  }

  static void CollectResources(
      std::vector<viz::ReturnedResource>* array,
      const std::vector<viz::ReturnedResource>& returned) {
    array->insert(array->end(), returned.begin(), returned.end());
  }

  static ReturnCallback GetReturnCallback(
      std::vector<viz::ReturnedResource>* array) {
    return base::BindRepeating(&DisplayResourceProviderTest::CollectResources,
                               array);
  }

  static void SetResourceFilter(DisplayResourceProvider* resource_provider,
                                viz::ResourceId id,
                                GLenum filter) {
    DisplayResourceProvider::ScopedSamplerGL sampler(resource_provider, id,
                                                     GL_TEXTURE_2D, filter);
  }

  ResourceProviderContext* context() { return context3d_; }

  viz::ResourceId CreateChildMailbox(gpu::SyncToken* release_sync_token,
                                     bool* lost_resource,
                                     bool* release_called,
                                     gpu::SyncToken* sync_token,
                                     viz::ResourceFormat format) {
    if (use_gpu()) {
      unsigned texture = child_context_->createTexture();
      gpu::Mailbox gpu_mailbox;
      child_context_->genMailboxCHROMIUM(gpu_mailbox.name);
      child_context_->produceTextureDirectCHROMIUM(texture, gpu_mailbox.name);
      child_context_->genSyncToken(sync_token->GetData());
      EXPECT_TRUE(sync_token->HasData());

      std::unique_ptr<viz::SingleReleaseCallback> callback =
          viz::SingleReleaseCallback::Create(base::BindRepeating(
              ReleaseSharedBitmapCallback, viz::SharedBitmapId(),
              release_called, release_sync_token, lost_resource));
      viz::TransferableResource gl_resource = viz::TransferableResource::MakeGL(
          gpu_mailbox, GL_LINEAR, GL_TEXTURE_2D, *sync_token);
      gl_resource.format = format;
      return child_resource_provider_->ImportResource(gl_resource,
                                                      std::move(callback));
    } else {
      gfx::Size size(64, 64);
      viz::SharedBitmapId shared_bitmap_id = CreateAndFillSharedBitmap(
          shared_bitmap_manager_.get(), size, format, 0);

      std::unique_ptr<viz::SingleReleaseCallback> callback =
          viz::SingleReleaseCallback::Create(base::BindRepeating(
              ReleaseSharedBitmapCallback, shared_bitmap_id, release_called,
              release_sync_token, lost_resource));
      return child_resource_provider_->ImportResource(
          viz::TransferableResource::MakeSoftware(shared_bitmap_id, size,
                                                  format),
          std::move(callback));
    }
  }

  viz::ResourceId MakeGpuResourceAndSendToDisplay(
      char c,
      GLuint filter,
      GLuint target,
      const gpu::SyncToken& sync_token,
      DisplayResourceProvider* resource_provider) {
    ReturnCallback return_callback = base::DoNothing();

    int child = resource_provider->CreateChild(return_callback);

    gpu::Mailbox gpu_mailbox;
    gpu_mailbox.name[0] = c;
    gpu_mailbox.name[1] = 0;
    auto resource = viz::TransferableResource::MakeGL(gpu_mailbox, GL_LINEAR,
                                                      target, sync_token);
    resource.id = 11;
    resource_provider->ReceiveFromChild(child, {resource});
    auto& map = resource_provider->GetChildToParentMap(child);
    return map.find(resource.id)->second;
  }

 protected:
  const bool use_gpu_;
  const bool child_needs_sync_token_;
  const std::unique_ptr<ContextSharedData> shared_data_;
  ResourceProviderContext* context3d_ = nullptr;
  ResourceProviderContext* child_context_ = nullptr;
  scoped_refptr<viz::TestContextProvider> context_provider_;
  scoped_refptr<viz::TestContextProvider> child_context_provider_;
  std::unique_ptr<viz::TestGpuMemoryBufferManager> gpu_memory_buffer_manager_;
  std::unique_ptr<DisplayResourceProvider> resource_provider_;
  std::unique_ptr<LayerTreeResourceProvider> child_resource_provider_;
  std::unique_ptr<viz::TestSharedBitmapManager> shared_bitmap_manager_;
};

INSTANTIATE_TEST_CASE_P(DisplayResourceProviderTests,
                        DisplayResourceProviderTest,
                        ::testing::Values(false, true));

TEST_P(DisplayResourceProviderTest, LockForExternalUse) {
  // TODO(penghuang): consider supporting SW mode.
  if (!use_gpu())
    return;

  gpu::SyncToken sync_token1(gpu::CommandBufferNamespace::GPU_IO,
                             gpu::CommandBufferId::FromUnsafeValue(0x123),
                             0x42);
  auto mailbox = gpu::Mailbox::Generate();
  viz::TransferableResource gl_resource = viz::TransferableResource::MakeGL(
      mailbox, GL_LINEAR, GL_TEXTURE_2D, sync_token1);
  viz::ResourceId id1 = child_resource_provider_->ImportResource(
      gl_resource, viz::SingleReleaseCallback::Create(base::DoNothing()));
  std::vector<viz::ReturnedResource> returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));

  // Transfer some resources to the parent.
  std::vector<viz::ResourceId> resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(id1);

  std::vector<viz::TransferableResource> list;
  child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list,
                                                child_context_provider_.get());
  ASSERT_EQ(1u, list.size());
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));

  resource_provider_->ReceiveFromChild(child_id, list);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<viz::ResourceId, viz::ResourceId> resource_map =
      resource_provider_->GetChildToParentMap(child_id);

  unsigned parent_id = resource_map[list.front().id];

  DisplayResourceProvider::LockSetForExternalUse lock_set(
      resource_provider_.get());

  viz::ResourceMetadata metadata = lock_set.LockResource(parent_id);
  ASSERT_EQ(metadata.mailbox, mailbox);
  ASSERT_TRUE(metadata.sync_token.HasData());

  resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                    viz::ResourceIdSet());
  // The resource should not be returned due to the external use lock.
  EXPECT_EQ(0u, returned_to_child.size());

  gpu::SyncToken sync_token2(gpu::CommandBufferNamespace::GPU_IO,
                             gpu::CommandBufferId::FromUnsafeValue(0x234),
                             0x456);
  sync_token2.SetVerifyFlush();
  lock_set.UnlockResources(sync_token2);
  resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                    viz::ResourceIdSet());
  // The resource should be returned after the lock is released.
  EXPECT_EQ(1u, returned_to_child.size());
  EXPECT_EQ(sync_token2, returned_to_child[0].sync_token);
  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
  child_resource_provider_->RemoveImportedResource(id1);
}

}  // namespace
}  // namespace cc
