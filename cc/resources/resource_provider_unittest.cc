// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/layer_tree_resource_provider.h"
#include "components/viz/service/display/display_resource_provider.h"

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
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "components/viz/test/test_texture.h"
#include "components/viz/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/geometry/rect.h"

using testing::InSequence;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::_;
using testing::AnyNumber;

namespace cc {
namespace {

static const bool kDelegatedSyncPointsRequired = true;

MATCHER_P(MatchesSyncToken, sync_token, "") {
  gpu::SyncToken other;
  memcpy(&other, arg, sizeof(other));
  return other == sync_token;
}

void ReleaseCallback(gpu::SyncToken* release_sync_token,
                     bool* release_lost_resource,
                     const gpu::SyncToken& sync_token,
                     bool lost_resource) {
  *release_sync_token = sync_token;
  *release_lost_resource = lost_resource;
}

void ReleaseSharedBitmapCallback(const viz::SharedBitmapId& shared_bitmap_id,
                                 bool* release_called,
                                 gpu::SyncToken* release_sync_token,
                                 bool* lost_resource_result,
                                 const gpu::SyncToken& sync_token,
                                 bool lost_resource) {
  *release_called = true;
  *release_sync_token = sync_token;
  *lost_resource_result = lost_resource;
}

void CountingReleaseCallback(int* count,
                             const gpu::SyncToken& sync_token,
                             bool lost_resource) {
  ++*count;
}

viz::SharedBitmapId CreateAndFillSharedBitmap(viz::SharedBitmapManager* manager,
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

class TextureStateTrackingContext : public viz::TestWebGraphicsContext3D {
 public:
  MOCK_METHOD2(bindTexture, void(GLenum target, GLuint texture));
  MOCK_METHOD3(texParameteri, void(GLenum target, GLenum pname, GLint param));
  MOCK_METHOD1(waitSyncToken, void(const GLbyte* sync_token));
  MOCK_METHOD2(produceTextureDirectCHROMIUM,
               void(GLuint texture, const GLbyte* mailbox));
  MOCK_METHOD1(createAndConsumeTextureCHROMIUM,
               unsigned(const GLbyte* mailbox));

  // Force all textures to be consecutive numbers starting at "1",
  // so we easily can test for them.
  GLuint NextTextureId() override {
    base::AutoLock lock(namespace_->lock);
    return namespace_->next_texture_id++;
  }

  void RetireTextureId(GLuint) override {}

  void genSyncToken(GLbyte* sync_token) override {
    gpu::SyncToken sync_token_data(gpu::CommandBufferNamespace::GPU_IO,
                                   gpu::CommandBufferId::FromUnsafeValue(0x123),
                                   next_fence_sync_++);
    sync_token_data.SetVerifyFlush();
    memcpy(sync_token, &sync_token_data, sizeof(sync_token_data));
  }

  GLuint64 GetNextFenceSync() const { return next_fence_sync_; }

  GLuint64 next_fence_sync_ = 1;
};

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

class ResourceProviderTest : public testing::TestWithParam<bool> {
 public:
  explicit ResourceProviderTest(bool child_needs_sync_token)
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
    } else {
      shared_bitmap_manager_ = std::make_unique<viz::TestSharedBitmapManager>();
    }

    resource_provider_ = std::make_unique<viz::DisplayResourceProvider>(
        context_provider_.get(), shared_bitmap_manager_.get());

    MakeChildResourceProvider();
  }

  ResourceProviderTest() : ResourceProviderTest(true) {}

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

  static viz::ReturnCallback GetReturnCallback(
      std::vector<viz::ReturnedResource>* array) {
    return base::Bind(&ResourceProviderTest::CollectResources, array);
  }

  static void SetResourceFilter(viz::DisplayResourceProvider* resource_provider,
                                viz::ResourceId id,
                                GLenum filter) {
    viz::DisplayResourceProvider::ScopedSamplerGL sampler(
        resource_provider, id, GL_TEXTURE_2D, filter);
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
          viz::SingleReleaseCallback::Create(base::BindOnce(
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
          viz::SingleReleaseCallback::Create(base::BindOnce(
              ReleaseSharedBitmapCallback, shared_bitmap_id, release_called,
              release_sync_token, lost_resource));
      return child_resource_provider_->ImportResource(
          viz::TransferableResource::MakeSoftware(shared_bitmap_id, size,
                                                  format),
          std::move(callback));
    }
  }

  viz::ResourceId CreateChildGpuMailboxWithCallback(
      viz::ReleaseCallback callback,
      bool enable_read_lock_fences = false) {
    DCHECK(use_gpu());
    unsigned texture = child_context_->createTexture();
    gpu::Mailbox gpu_mailbox;
    child_context_->genMailboxCHROMIUM(gpu_mailbox.name);
    child_context_->produceTextureDirectCHROMIUM(texture, gpu_mailbox.name);
    gpu::SyncToken sync_token;
    child_context_->genSyncToken(sync_token.GetData());
    EXPECT_TRUE(sync_token.HasData());

    viz::TransferableResource gl_resource = viz::TransferableResource::MakeGL(
        gpu_mailbox, GL_LINEAR, GL_TEXTURE_2D, sync_token);
    gl_resource.read_lock_fences_enabled = enable_read_lock_fences;
    return child_resource_provider_->ImportResource(
        gl_resource, viz::SingleReleaseCallback::Create(std::move(callback)));
  }

  viz::ResourceId CreateChildGpuMailbox(bool enable_read_lock_fences = false) {
    return CreateChildGpuMailboxWithCallback(base::DoNothing(),
                                             enable_read_lock_fences);
  }

  viz::ResourceId MakeGpuResourceAndSendToDisplay(
      char c,
      GLuint filter,
      GLuint target,
      const gpu::SyncToken& sync_token,
      viz::DisplayResourceProvider* resource_provider) {
    viz::ReturnCallback return_callback = base::DoNothing();

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
  std::unique_ptr<viz::DisplayResourceProvider> resource_provider_;
  std::unique_ptr<LayerTreeResourceProvider> child_resource_provider_;
  std::unique_ptr<viz::TestSharedBitmapManager> shared_bitmap_manager_;
};

#if defined(OS_ANDROID)
TEST_P(ResourceProviderTest, OverlayPromotionHint) {
  if (!use_gpu())
    return;

  GLuint external_texture_id = child_context_->createExternalTexture();

  gpu::Mailbox external_mailbox;
  child_context_->genMailboxCHROMIUM(external_mailbox.name);
  child_context_->produceTextureDirectCHROMIUM(external_texture_id,
                                               external_mailbox.name);
  gpu::SyncToken external_sync_token;
  child_context_->genSyncToken(external_sync_token.GetData());
  EXPECT_TRUE(external_sync_token.HasData());

  viz::TransferableResource id1_transfer =
      viz::TransferableResource::MakeGLOverlay(
          external_mailbox, GL_LINEAR, GL_TEXTURE_EXTERNAL_OES,
          external_sync_token, gfx::Size(1, 1), true);
  id1_transfer.wants_promotion_hint = true;
  id1_transfer.is_backed_by_surface_texture = true;
  viz::ResourceId id1 = child_resource_provider_->ImportResource(
      id1_transfer, viz::SingleReleaseCallback::Create(base::DoNothing()));

  viz::TransferableResource id2_transfer =
      viz::TransferableResource::MakeGLOverlay(
          external_mailbox, GL_LINEAR, GL_TEXTURE_EXTERNAL_OES,
          external_sync_token, gfx::Size(1, 1), true);
  id2_transfer.wants_promotion_hint = false;
  id2_transfer.is_backed_by_surface_texture = false;
  viz::ResourceId id2 = child_resource_provider_->ImportResource(
      id2_transfer, viz::SingleReleaseCallback::Create(base::DoNothing()));

  std::vector<viz::ReturnedResource> returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));

  // Transfer some resources to the parent.
  std::vector<viz::ResourceId> resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(id1);
  resource_ids_to_transfer.push_back(id2);

  std::vector<viz::TransferableResource> list;
  child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list,
                                                child_context_provider_.get());
  ASSERT_EQ(2u, list.size());
  resource_provider_->ReceiveFromChild(child_id, list);
  std::unordered_map<viz::ResourceId, viz::ResourceId> resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  viz::ResourceId mapped_id1 = resource_map[list[0].id];
  viz::ResourceId mapped_id2 = resource_map[list[1].id];

  // The promotion hints should not be recorded until after we wait.  This is
  // because we can't notify them until they're synchronized, and we choose to
  // ignore unwaited resources rather than send them a "no" hint.  If they end
  // up in the request set before we wait, then the attempt to notify them wil;
  // DCHECK when we try to lock them for reading in SendPromotionHints.
  EXPECT_EQ(0u, resource_provider_->CountPromotionHintRequestsForTesting());
  {
    resource_provider_->WaitSyncToken(mapped_id1);
    viz::DisplayResourceProvider::ScopedReadLockGL lock(
        resource_provider_.get(), mapped_id1);
  }
  EXPECT_EQ(1u, resource_provider_->CountPromotionHintRequestsForTesting());

  EXPECT_EQ(list[0].mailbox_holder.sync_token,
            context3d_->last_waited_sync_token());
  viz::ResourceIdSet resource_ids_to_receive;
  resource_ids_to_receive.insert(id1);
  resource_ids_to_receive.insert(id2);
  resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                    resource_ids_to_receive);

  EXPECT_EQ(2u, resource_provider_->num_resources());

  EXPECT_NE(0u, mapped_id1);
  EXPECT_NE(0u, mapped_id2);

  // Make sure that the request for a promotion hint was noticed.
  EXPECT_TRUE(resource_provider_->IsOverlayCandidate(mapped_id1));
  EXPECT_TRUE(resource_provider_->IsBackedBySurfaceTexture(mapped_id1));
  EXPECT_TRUE(resource_provider_->WantsPromotionHintForTesting(mapped_id1));

  EXPECT_TRUE(resource_provider_->IsOverlayCandidate(mapped_id2));
  EXPECT_FALSE(resource_provider_->IsBackedBySurfaceTexture(mapped_id2));
  EXPECT_FALSE(resource_provider_->WantsPromotionHintForTesting(mapped_id2));

  // ResourceProvider maintains a set of promotion hint requests that should be
  // cleared when resources are deleted.
  resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                    viz::ResourceIdSet());
  EXPECT_EQ(2u, returned_to_child.size());
  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);

  EXPECT_EQ(0u, resource_provider_->CountPromotionHintRequestsForTesting());

  resource_provider_->DestroyChild(child_id);
}
#endif

TEST_P(ResourceProviderTest, TransferGLResources_NoSyncToken) {
  if (!use_gpu())
    return;

  bool need_sync_tokens = false;
  auto no_token_resource_provider = std::make_unique<LayerTreeResourceProvider>(
      child_context_provider_.get(), need_sync_tokens);

  GLuint external_texture_id = child_context_->createExternalTexture();

  // A sync point is specified directly and should be used.
  gpu::Mailbox external_mailbox;
  child_context_->genMailboxCHROMIUM(external_mailbox.name);
  child_context_->produceTextureDirectCHROMIUM(external_texture_id,
                                               external_mailbox.name);
  gpu::SyncToken external_sync_token;
  child_context_->genSyncToken(external_sync_token.GetData());
  EXPECT_TRUE(external_sync_token.HasData());
  viz::ResourceId id = no_token_resource_provider->ImportResource(
      viz::TransferableResource::MakeGL(external_mailbox, GL_LINEAR,
                                        GL_TEXTURE_EXTERNAL_OES,
                                        external_sync_token),
      viz::SingleReleaseCallback::Create(base::DoNothing()));

  std::vector<viz::ReturnedResource> returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  resource_provider_->SetChildNeedsSyncTokens(child_id, false);
  {
    // Transfer some resources to the parent.
    std::vector<viz::ResourceId> resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id);
    std::vector<viz::TransferableResource> list;
    no_token_resource_provider->PrepareSendToParent(
        resource_ids_to_transfer, &list, child_context_provider_.get());
    ASSERT_EQ(1u, list.size());
    // A given sync point should be passed through.
    EXPECT_EQ(external_sync_token, list[0].mailbox_holder.sync_token);
    resource_provider_->ReceiveFromChild(child_id, list);

    viz::ResourceIdSet resource_ids_to_receive;
    resource_ids_to_receive.insert(id);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_receive);
  }

  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    viz::ResourceIdSet no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    ASSERT_EQ(1u, returned_to_child.size());
    std::map<viz::ResourceId, gpu::SyncToken> returned_sync_tokens;
    for (const auto& returned : returned_to_child)
      returned_sync_tokens[returned.id] = returned.sync_token;

    // Original sync point given should be returned.
    ASSERT_TRUE(returned_sync_tokens.find(id) != returned_sync_tokens.end());
    EXPECT_EQ(external_sync_token, returned_sync_tokens[id]);
    EXPECT_FALSE(returned_to_child[0].lost);
    no_token_resource_provider->ReceiveReturnsFromParent(returned_to_child);
    returned_to_child.clear();
  }

  resource_provider_->DestroyChild(child_id);
}

// Test that SetBatchReturnResources batching works.
TEST_P(ResourceProviderTest, SetBatchPreventsReturn) {
  if (!use_gpu())
    return;

  std::vector<viz::ReturnedResource> returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));

  int release_count = 0;

  // Transfer some resources to the parent.
  std::vector<viz::ResourceId> resource_ids_to_transfer;
  viz::ResourceId ids[2];
  for (size_t i = 0; i < arraysize(ids); i++) {
    ids[i] = CreateChildGpuMailboxWithCallback(
        base::BindOnce(&CountingReleaseCallback, &release_count));
    resource_ids_to_transfer.push_back(ids[i]);
  }

  std::vector<viz::TransferableResource> list;
  child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list,
                                                child_context_provider_.get());
  ASSERT_EQ(2u, list.size());
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(ids[0]));
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(ids[1]));

  resource_provider_->ReceiveFromChild(child_id, list);

  // In viz::DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<viz::ResourceId, viz::ResourceId> resource_map =
      resource_provider_->GetChildToParentMap(child_id);

  std::vector<std::unique_ptr<viz::DisplayResourceProvider::ScopedReadLockGL>>
      read_locks;
  for (auto& resource_id : list) {
    unsigned int mapped_resource_id = resource_map[resource_id.id];
    resource_provider_->WaitSyncToken(mapped_resource_id);
    read_locks.push_back(
        std::make_unique<viz::DisplayResourceProvider::ScopedReadLockGL>(
            resource_provider_.get(), mapped_resource_id));
  }

  resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                    viz::ResourceIdSet());
  std::unique_ptr<viz::DisplayResourceProvider::ScopedBatchReturnResources>
      returner = std::make_unique<
          viz::DisplayResourceProvider::ScopedBatchReturnResources>(
          resource_provider_.get());
  EXPECT_EQ(0u, returned_to_child.size());

  read_locks.clear();
  EXPECT_EQ(0u, returned_to_child.size());

  returner.reset();
  EXPECT_EQ(2u, returned_to_child.size());
  // All resources in a batch should share a sync token.
  EXPECT_EQ(returned_to_child[0].sync_token, returned_to_child[1].sync_token);

  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
  child_resource_provider_->RemoveImportedResource(ids[0]);
  child_resource_provider_->RemoveImportedResource(ids[1]);
  EXPECT_EQ(2, release_count);
}

TEST_P(ResourceProviderTest, ReadLockCountStopsReturnToChildOrDelete) {
  if (!use_gpu())
    return;

  int release_count = 0;
  viz::ResourceId id1 = CreateChildGpuMailboxWithCallback(
      base::BindOnce(&CountingReleaseCallback, &release_count));

  std::vector<viz::ReturnedResource> returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer some resources to the parent.
    std::vector<viz::ResourceId> resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);

    std::vector<viz::TransferableResource> list;
    child_resource_provider_->PrepareSendToParent(
        resource_ids_to_transfer, &list, child_context_provider_.get());
    ASSERT_EQ(1u, list.size());
    EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));

    resource_provider_->ReceiveFromChild(child_id, list);

    // In viz::DisplayResourceProvider's namespace, use the mapped resource id.
    std::unordered_map<viz::ResourceId, viz::ResourceId> resource_map =
        resource_provider_->GetChildToParentMap(child_id);
    viz::ResourceId mapped_resource_id = resource_map[list[0].id];
    resource_provider_->WaitSyncToken(mapped_resource_id);
    viz::DisplayResourceProvider::ScopedReadLockGL lock(
        resource_provider_.get(), mapped_resource_id);

    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      viz::ResourceIdSet());
    EXPECT_EQ(0u, returned_to_child.size());
  }

  EXPECT_EQ(1u, returned_to_child.size());
  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);

  // No need to wait for the sync token here -- it will be returned to the
  // client on delete.
  child_resource_provider_->RemoveImportedResource(id1);
  EXPECT_EQ(1, release_count);

  resource_provider_->DestroyChild(child_id);
}

class TestFence : public viz::ResourceFence {
 public:
  TestFence() = default;

  // viz::ResourceFence implementation.
  void Set() override {}
  bool HasPassed() override { return passed; }
  void Wait() override {}

  bool passed = false;

 private:
  ~TestFence() override = default;
};

TEST_P(ResourceProviderTest, ReadLockFenceStopsReturnToChildOrDelete) {
  if (!use_gpu())
    return;

  constexpr bool enable_read_lock_fences = true;
  int release_count = 0;
  viz::ResourceId id1 = CreateChildGpuMailboxWithCallback(
      base::BindOnce(&CountingReleaseCallback, &release_count),
      enable_read_lock_fences);
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
  EXPECT_TRUE(list[0].read_lock_fences_enabled);

  resource_provider_->ReceiveFromChild(child_id, list);

  // In viz::DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<viz::ResourceId, viz::ResourceId> resource_map =
      resource_provider_->GetChildToParentMap(child_id);

  scoped_refptr<TestFence> fence(new TestFence);
  resource_provider_->SetReadLockFence(fence.get());
  {
    unsigned parent_id = resource_map[list.front().id];
    resource_provider_->WaitSyncToken(parent_id);
    viz::DisplayResourceProvider::ScopedReadLockGL lock(
        resource_provider_.get(), parent_id);
  }
  resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                    viz::ResourceIdSet());
  EXPECT_EQ(0u, returned_to_child.size());

  resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                    viz::ResourceIdSet());
  EXPECT_EQ(0u, returned_to_child.size());
  fence->passed = true;

  resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                    viz::ResourceIdSet());
  EXPECT_EQ(1u, returned_to_child.size());

  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
  child_resource_provider_->RemoveImportedResource(id1);
  EXPECT_EQ(1, release_count);
}

TEST_P(ResourceProviderTest, ReadLockFenceDestroyChild) {
  if (!use_gpu())
    return;

  constexpr bool enable_read_lock_fences = true;
  int release_count = 0;
  viz::ResourceId id1 = CreateChildGpuMailboxWithCallback(
      base::BindOnce(&CountingReleaseCallback, &release_count),
      enable_read_lock_fences);
  viz::ResourceId id2 = CreateChildGpuMailboxWithCallback(
      base::BindOnce(&CountingReleaseCallback, &release_count));

  std::vector<viz::ReturnedResource> returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));

  // Transfer resources to the parent.
  std::vector<viz::ResourceId> resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(id1);
  resource_ids_to_transfer.push_back(id2);

  std::vector<viz::TransferableResource> list;
  child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list,
                                                child_context_provider_.get());
  ASSERT_EQ(2u, list.size());
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id2));

  resource_provider_->ReceiveFromChild(child_id, list);

  // In viz::DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<viz::ResourceId, viz::ResourceId> resource_map =
      resource_provider_->GetChildToParentMap(child_id);

  scoped_refptr<TestFence> fence(new TestFence);
  resource_provider_->SetReadLockFence(fence.get());
  {
    for (size_t i = 0; i < list.size(); i++) {
      unsigned parent_id = resource_map[list[i].id];
      resource_provider_->WaitSyncToken(parent_id);
      viz::DisplayResourceProvider::ScopedReadLockGL lock(
          resource_provider_.get(), parent_id);
    }
  }
  EXPECT_EQ(0u, returned_to_child.size());

  EXPECT_EQ(2u, resource_provider_->num_resources());

  resource_provider_->DestroyChild(child_id);

  EXPECT_EQ(0u, resource_provider_->num_resources());
  EXPECT_EQ(2u, returned_to_child.size());

  // id1 should be lost and id2 should not.
  EXPECT_EQ(returned_to_child[0].lost, returned_to_child[0].id == id1);
  EXPECT_EQ(returned_to_child[1].lost, returned_to_child[1].id == id1);

  child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
  child_resource_provider_->RemoveImportedResource(id1);
  child_resource_provider_->RemoveImportedResource(id2);
  EXPECT_EQ(2, release_count);
}

TEST_P(ResourceProviderTest, ReadLockFenceContextLost) {
  if (!use_gpu())
    return;

  constexpr bool enable_read_lock_fences = true;
  viz::ResourceId id1 = CreateChildGpuMailbox(enable_read_lock_fences);
  viz::ResourceId id2 = CreateChildGpuMailbox();

  std::vector<viz::ReturnedResource> returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));

  // Transfer resources to the parent.
  std::vector<viz::ResourceId> resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(id1);
  resource_ids_to_transfer.push_back(id2);

  std::vector<viz::TransferableResource> list;
  child_resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list,
                                                child_context_provider_.get());
  ASSERT_EQ(2u, list.size());
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id1));
  EXPECT_TRUE(child_resource_provider_->InUseByConsumer(id2));

  resource_provider_->ReceiveFromChild(child_id, list);

  // In viz::DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<viz::ResourceId, viz::ResourceId> resource_map =
      resource_provider_->GetChildToParentMap(child_id);

  scoped_refptr<TestFence> fence(new TestFence);
  resource_provider_->SetReadLockFence(fence.get());
  {
    for (size_t i = 0; i < list.size(); i++) {
      unsigned parent_id = resource_map[list[i].id];
      resource_provider_->WaitSyncToken(parent_id);
      viz::DisplayResourceProvider::ScopedReadLockGL lock(
          resource_provider_.get(), parent_id);
    }
  }
  EXPECT_EQ(0u, returned_to_child.size());

  EXPECT_EQ(2u, resource_provider_->num_resources());
  resource_provider_->DidLoseContextProvider();
  resource_provider_ = nullptr;

  EXPECT_EQ(2u, returned_to_child.size());

  EXPECT_TRUE(returned_to_child[0].lost);
  EXPECT_TRUE(returned_to_child[1].lost);
}

TEST_P(ResourceProviderTest, TransferMailboxResources) {
  // Other mailbox transfers tested elsewhere.
  if (!use_gpu())
    return;
  unsigned texture = context()->createTexture();
  context()->bindTexture(GL_TEXTURE_2D, texture);
  uint8_t data[4] = {1, 2, 3, 4};
  context()->texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                        GL_UNSIGNED_BYTE, &data);
  gpu::Mailbox mailbox;
  context()->genMailboxCHROMIUM(mailbox.name);
  context()->produceTextureDirectCHROMIUM(texture, mailbox.name);
  gpu::SyncToken sync_token;
  context()->genSyncToken(sync_token.GetData());
  EXPECT_TRUE(sync_token.HasData());

  // All the logic below assumes that the sync token releases are all positive.
  EXPECT_LT(0u, sync_token.release_count());

  gpu::SyncToken release_sync_token;
  bool lost_resource = false;
  viz::ReleaseCallback callback =
      base::BindOnce(ReleaseCallback, &release_sync_token, &lost_resource);
  viz::ResourceId resource = child_resource_provider_->ImportResource(
      viz::TransferableResource::MakeGL(mailbox, GL_LINEAR, GL_TEXTURE_2D,
                                        sync_token),
      viz::SingleReleaseCallback::Create(std::move(callback)));
  EXPECT_EQ(1u, context()->NumTextures());
  EXPECT_FALSE(release_sync_token.HasData());
  {
    // Transfer the resource, expect the sync points to be consistent.
    std::vector<viz::ResourceId> resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    std::vector<viz::TransferableResource> list;
    child_resource_provider_->PrepareSendToParent(
        resource_ids_to_transfer, &list, child_context_provider_.get());
    ASSERT_EQ(1u, list.size());
    EXPECT_LE(sync_token.release_count(),
              list[0].mailbox_holder.sync_token.release_count());
    EXPECT_EQ(0, memcmp(mailbox.name, list[0].mailbox_holder.mailbox.name,
                        sizeof(mailbox.name)));
    EXPECT_FALSE(release_sync_token.HasData());

    context()->waitSyncToken(list[0].mailbox_holder.sync_token.GetConstData());
    unsigned other_texture =
        context()->createAndConsumeTextureCHROMIUM(mailbox.name);
    uint8_t test_data[4] = {0};
    context()->GetPixels(gfx::Size(1, 1), viz::RGBA_8888, test_data);
    EXPECT_EQ(0, memcmp(data, test_data, sizeof(data)));

    context()->produceTextureDirectCHROMIUM(other_texture, mailbox.name);
    context()->deleteTexture(other_texture);
    context()->genSyncToken(list[0].mailbox_holder.sync_token.GetData());
    EXPECT_TRUE(list[0].mailbox_holder.sync_token.HasData());

    // Receive the resource, then delete it, expect the sync points to be
    // consistent.
    std::vector<viz::ReturnedResource> returned =
        viz::TransferableResource::ReturnResources(list);
    child_resource_provider_->ReceiveReturnsFromParent(returned);
    EXPECT_EQ(1u, context()->NumTextures());
    EXPECT_FALSE(release_sync_token.HasData());

    child_resource_provider_->RemoveImportedResource(resource);
    EXPECT_LE(list[0].mailbox_holder.sync_token.release_count(),
              release_sync_token.release_count());
    EXPECT_FALSE(lost_resource);
  }

  // We're going to do the same thing as above, but testing the case where we
  // delete the resource before we receive it back.
  sync_token = release_sync_token;
  EXPECT_LT(0u, sync_token.release_count());
  release_sync_token.Clear();
  callback = base::Bind(ReleaseCallback, &release_sync_token, &lost_resource);
  resource = child_resource_provider_->ImportResource(
      viz::TransferableResource::MakeGL(mailbox, GL_LINEAR, GL_TEXTURE_2D,
                                        sync_token),
      viz::SingleReleaseCallback::Create(std::move(callback)));
  EXPECT_EQ(1u, context()->NumTextures());
  EXPECT_FALSE(release_sync_token.HasData());
  {
    // Transfer the resource, expect the sync points to be consistent.
    std::vector<viz::ResourceId> resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    std::vector<viz::TransferableResource> list;
    child_resource_provider_->PrepareSendToParent(
        resource_ids_to_transfer, &list, child_context_provider_.get());
    ASSERT_EQ(1u, list.size());
    EXPECT_LE(sync_token.release_count(),
              list[0].mailbox_holder.sync_token.release_count());
    EXPECT_EQ(0,
              memcmp(mailbox.name,
                     list[0].mailbox_holder.mailbox.name,
                     sizeof(mailbox.name)));
    EXPECT_FALSE(release_sync_token.HasData());

    context()->waitSyncToken(list[0].mailbox_holder.sync_token.GetConstData());
    unsigned other_texture =
        context()->createAndConsumeTextureCHROMIUM(mailbox.name);
    uint8_t test_data[4] = { 0 };
    context()->GetPixels(gfx::Size(1, 1), viz::RGBA_8888, test_data);
    EXPECT_EQ(0, memcmp(data, test_data, sizeof(data)));

    context()->produceTextureDirectCHROMIUM(other_texture, mailbox.name);
    context()->deleteTexture(other_texture);
    context()->genSyncToken(list[0].mailbox_holder.sync_token.GetData());
    EXPECT_TRUE(list[0].mailbox_holder.sync_token.HasData());

    // Delete the resource, which shouldn't do anything.
    child_resource_provider_->RemoveImportedResource(resource);
    EXPECT_EQ(1u, context()->NumTextures());
    EXPECT_FALSE(release_sync_token.HasData());

    // Then receive the resource which should release the mailbox, expect the
    // sync points to be consistent.
    std::vector<viz::ReturnedResource> returned =
        viz::TransferableResource::ReturnResources(list);
    child_resource_provider_->ReceiveReturnsFromParent(returned);
    EXPECT_LE(list[0].mailbox_holder.sync_token.release_count(),
              release_sync_token.release_count());
    EXPECT_FALSE(lost_resource);
  }

  context()->waitSyncToken(release_sync_token.GetConstData());
  texture = context()->createAndConsumeTextureCHROMIUM(mailbox.name);
  context()->deleteTexture(texture);
}

TEST_P(ResourceProviderTest, LostMailboxInParent) {
  gpu::SyncToken release_sync_token;
  bool lost_resource = false;
  bool release_called = false;
  gpu::SyncToken sync_token;
  viz::ResourceId resource =
      CreateChildMailbox(&release_sync_token, &lost_resource, &release_called,
                         &sync_token, viz::RGBA_8888);

  std::vector<viz::ReturnedResource> returned_to_child;
  int child_id =
      resource_provider_->CreateChild(GetReturnCallback(&returned_to_child));
  {
    // Transfer the resource to the parent.
    std::vector<viz::ResourceId> resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    std::vector<viz::TransferableResource> list;
    child_resource_provider_->PrepareSendToParent(
        resource_ids_to_transfer, &list, child_context_provider_.get());
    EXPECT_EQ(1u, list.size());

    resource_provider_->ReceiveFromChild(child_id, list);
    viz::ResourceIdSet resource_ids_to_receive;
    resource_ids_to_receive.insert(resource);
    resource_provider_->DeclareUsedResourcesFromChild(child_id,
                                                      resource_ids_to_receive);
  }

  // Lose the output surface in the parent.
  resource_provider_->DidLoseContextProvider();

  {
    EXPECT_EQ(0u, returned_to_child.size());

    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    viz::ResourceIdSet no_resources;
    resource_provider_->DeclareUsedResourcesFromChild(child_id, no_resources);

    ASSERT_EQ(1u, returned_to_child.size());
    // Losing an output surface only loses hardware resources.
    EXPECT_EQ(returned_to_child[0].lost, use_gpu());
    child_resource_provider_->ReceiveReturnsFromParent(returned_to_child);
    returned_to_child.clear();
  }

  // Delete the resource in the child. Expect the resource to be lost if it's
  // a GL texture.
  child_resource_provider_->RemoveImportedResource(resource);
  EXPECT_EQ(lost_resource, use_gpu());
}

TEST_P(ResourceProviderTest, Shutdown) {
  enum Cases {
    // If not exported, the resource is returned not lost, and doesn't need a
    // sync token.
    kShutdownWithoutExport = 0,
    // If exported and not returned, the resource is lost, and doesn't need/have
    // a sync token to give.
    kShutdownAfterExport,
    // If exported and returned, the resource is not lost.
    kShutdownAfterExportAndReturn,
    // If returned as lost, then the resource must report lost on shutdown too.
    kShutdownAfterExportAndReturnWithLostResource,
    // If the context is lost, it doesn't affect imported resources as they are
    // just weak references.
    kShutdownAfterContextLossWithoutExport,
    // If the context is lost, it doesn't affect imported resources as they are
    // just weak references.
    kShutdownAfterContextLossAfterExportAndReturn,
    kNumCases,
  };

  for (int i = 0; i < kNumCases; ++i) {
    auto c = static_cast<Cases>(i);
    SCOPED_TRACE(c);

    gpu::SyncToken release_sync_token;
    bool lost_resource = false;
    bool release_called = false;
    gpu::SyncToken sync_token;
    viz::ResourceId id =
        CreateChildMailbox(&release_sync_token, &lost_resource, &release_called,
                           &sync_token, viz::RGBA_8888);

    if (i == kShutdownAfterExport || i == kShutdownAfterExportAndReturn ||
        i == kShutdownAfterExportAndReturnWithLostResource ||
        i == kShutdownAfterContextLossAfterExportAndReturn) {
      std::vector<viz::TransferableResource> send_list;
      child_resource_provider_->PrepareSendToParent(
          {id}, &send_list, child_context_provider_.get());
    }

    if (i == kShutdownAfterExportAndReturn ||
        i == kShutdownAfterExportAndReturnWithLostResource ||
        i == kShutdownAfterContextLossAfterExportAndReturn) {
      viz::ReturnedResource r;
      r.id = id;
      r.sync_token = sync_token;
      r.count = 1;
      r.lost = (i == kShutdownAfterExportAndReturnWithLostResource);
      child_resource_provider_->ReceiveReturnsFromParent({r});
    }

    EXPECT_FALSE(release_sync_token.HasData());
    EXPECT_FALSE(lost_resource);

    // Shutdown!
    child_resource_provider_ = nullptr;

    // If the resource was exported and returned, then it should come with a
    // sync token.
    if (use_gpu()) {
      if (i == kShutdownAfterExportAndReturn ||
          i == kShutdownAfterExportAndReturnWithLostResource ||
          i == kShutdownAfterContextLossAfterExportAndReturn) {
        EXPECT_LE(sync_token.release_count(),
                  release_sync_token.release_count());
      }
    }

    // We always get the ReleaseCallback called.
    EXPECT_TRUE(release_called);
    bool expect_lost = i == kShutdownAfterExport ||
                       i == kShutdownAfterExportAndReturnWithLostResource;
    EXPECT_EQ(expect_lost, lost_resource);

    // Recreate it for the next case.
    MakeChildResourceProvider();
  }
}

TEST_P(ResourceProviderTest, ImportedResource_SharedMemory) {
  if (use_gpu())
    return;

  gfx::Size size(64, 64);
  viz::ResourceFormat format = viz::RGBA_8888;
  const uint32_t kBadBeef = 0xbadbeef;
  viz::SharedBitmapId shared_bitmap_id = CreateAndFillSharedBitmap(
      shared_bitmap_manager_.get(), size, format, kBadBeef);

  auto resource_provider = std::make_unique<viz::DisplayResourceProvider>(
      nullptr, shared_bitmap_manager_.get());

  auto child_resource_provider(std::make_unique<LayerTreeResourceProvider>(
      nullptr, kDelegatedSyncPointsRequired));

  gpu::SyncToken release_sync_token;
  bool lost_resource = false;
  std::unique_ptr<viz::SingleReleaseCallback> callback =
      viz::SingleReleaseCallback::Create(
          base::Bind(&ReleaseCallback, &release_sync_token, &lost_resource));
  auto resource =
      viz::TransferableResource::MakeSoftware(shared_bitmap_id, size, format);

  viz::ResourceId resource_id =
      child_resource_provider->ImportResource(resource, std::move(callback));
  EXPECT_NE(0u, resource_id);

  // Transfer resources to the parent.
  std::vector<viz::ResourceId> resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource_id);

  std::vector<viz::TransferableResource> send_to_parent;
  std::vector<viz::ReturnedResource> returned_to_child;
  int child_id = resource_provider->CreateChild(
      base::Bind(&CollectResources, &returned_to_child));
  child_resource_provider->PrepareSendToParent(
      resource_ids_to_transfer, &send_to_parent, child_context_provider_.get());
  resource_provider->ReceiveFromChild(child_id, send_to_parent);

  // In viz::DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<viz::ResourceId, viz::ResourceId> resource_map =
      resource_provider->GetChildToParentMap(child_id);
  viz::ResourceId mapped_resource_id = resource_map[resource_id];

  {
    viz::DisplayResourceProvider::ScopedReadLockSoftware lock(
        resource_provider.get(), mapped_resource_id);
    const SkBitmap* sk_bitmap = lock.sk_bitmap();
    EXPECT_EQ(sk_bitmap->width(), size.width());
    EXPECT_EQ(sk_bitmap->height(), size.height());
    EXPECT_EQ(*sk_bitmap->getAddr32(16, 16), kBadBeef);
  }

  EXPECT_EQ(0u, returned_to_child.size());
  // Transfer resources back from the parent to the child. Set no resources as
  // being in use.
  resource_provider->DeclareUsedResourcesFromChild(child_id,
                                                   viz::ResourceIdSet());
  EXPECT_EQ(1u, returned_to_child.size());
  child_resource_provider->ReceiveReturnsFromParent(returned_to_child);

  child_resource_provider->RemoveImportedResource(resource_id);
  EXPECT_FALSE(release_sync_token.HasData());
  EXPECT_FALSE(lost_resource);
}

class ResourceProviderTestImportedResourceGLFilters
    : public ResourceProviderTest {
 public:
  static void RunTest(
      viz::TestSharedBitmapManager* shared_bitmap_manager,
      bool mailbox_nearest_neighbor,
      GLenum sampler_filter) {
    auto context_owned(std::make_unique<TextureStateTrackingContext>());
    TextureStateTrackingContext* context = context_owned.get();
    auto context_provider =
        viz::TestContextProvider::Create(std::move(context_owned));
    context_provider->BindToCurrentThread();

    auto resource_provider(std::make_unique<viz::DisplayResourceProvider>(
        context_provider.get(), shared_bitmap_manager));

    auto child_context_owned(std::make_unique<TextureStateTrackingContext>());
    TextureStateTrackingContext* child_context = child_context_owned.get();
    auto child_context_provider =
        viz::TestContextProvider::Create(std::move(child_context_owned));
    child_context_provider->BindToCurrentThread();

    auto child_resource_provider(std::make_unique<LayerTreeResourceProvider>(
        child_context_provider.get(), kDelegatedSyncPointsRequired));

    unsigned texture_id = 1;
    gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                              gpu::CommandBufferId::FromUnsafeValue(0x12),
                              0x34);
    const GLuint64 current_fence_sync = child_context->GetNextFenceSync();

    EXPECT_CALL(*child_context, bindTexture(_, _)).Times(0);
    EXPECT_CALL(*child_context, waitSyncToken(_)).Times(0);
    EXPECT_CALL(*child_context, produceTextureDirectCHROMIUM(_, _)).Times(0);
    EXPECT_CALL(*child_context, createAndConsumeTextureCHROMIUM(_)).Times(0);

    gpu::Mailbox gpu_mailbox;
    memcpy(gpu_mailbox.name, "Hello world", strlen("Hello world") + 1);
    gpu::SyncToken release_sync_token;
    bool lost_resource = false;
    std::unique_ptr<viz::SingleReleaseCallback> callback =
        viz::SingleReleaseCallback::Create(
            base::Bind(&ReleaseCallback, &release_sync_token, &lost_resource));

    GLuint filter = mailbox_nearest_neighbor ? GL_NEAREST : GL_LINEAR;
    auto resource = viz::TransferableResource::MakeGL(
        gpu_mailbox, filter, GL_TEXTURE_2D, sync_token);

    viz::ResourceId resource_id =
        child_resource_provider->ImportResource(resource, std::move(callback));
    EXPECT_NE(0u, resource_id);
    EXPECT_EQ(current_fence_sync, child_context->GetNextFenceSync());

    Mock::VerifyAndClearExpectations(child_context);

    // Transfer resources to the parent.
    std::vector<viz::ResourceId> resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource_id);

    std::vector<viz::TransferableResource> send_to_parent;
    std::vector<viz::ReturnedResource> returned_to_child;
    int child_id = resource_provider->CreateChild(
        base::Bind(&CollectResources, &returned_to_child));
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &send_to_parent,
                                                 child_context_provider.get());
    resource_provider->ReceiveFromChild(child_id, send_to_parent);

    // In viz::DisplayResourceProvider's namespace, use the mapped resource id.
    std::unordered_map<viz::ResourceId, viz::ResourceId> resource_map =
        resource_provider->GetChildToParentMap(child_id);
    viz::ResourceId mapped_resource_id = resource_map[resource_id];
    {
      // The verified flush flag will be set by
      // LayerTreeResourceProvider::PrepareSendToParent. Before checking if
      // the gpu::SyncToken matches, set this flag first.
      sync_token.SetVerifyFlush();

      // Mailbox sync point WaitSyncToken before using the texture.
      EXPECT_CALL(*context, waitSyncToken(MatchesSyncToken(sync_token)));
      resource_provider->WaitSyncToken(mapped_resource_id);
      Mock::VerifyAndClearExpectations(context);

      EXPECT_CALL(*context, createAndConsumeTextureCHROMIUM(_))
          .WillOnce(Return(texture_id));
      EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id));

      EXPECT_CALL(*context, produceTextureDirectCHROMIUM(_, _)).Times(0);

      // The sampler will reset these if |mailbox_nearest_neighbor| does not
      // match |sampler_filter|.
      if (mailbox_nearest_neighbor != (sampler_filter == GL_NEAREST)) {
        EXPECT_CALL(*context, texParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampler_filter));
        EXPECT_CALL(*context, texParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampler_filter));
      }

      viz::DisplayResourceProvider::ScopedSamplerGL lock(
          resource_provider.get(), mapped_resource_id, sampler_filter);
      Mock::VerifyAndClearExpectations(context);
      EXPECT_EQ(current_fence_sync, context->GetNextFenceSync());

      // When done with it, a sync point should be inserted, but no produce is
      // necessary.
      EXPECT_CALL(*child_context, bindTexture(_, _)).Times(0);
      EXPECT_CALL(*child_context, produceTextureDirectCHROMIUM(_, _)).Times(0);

      EXPECT_CALL(*child_context, waitSyncToken(_)).Times(0);
      EXPECT_CALL(*child_context, createAndConsumeTextureCHROMIUM(_)).Times(0);
    }

    EXPECT_EQ(0u, returned_to_child.size());
    // Transfer resources back from the parent to the child. Set no resources as
    // being in use.
    resource_provider->DeclareUsedResourcesFromChild(child_id,
                                                     viz::ResourceIdSet());
    EXPECT_EQ(1u, returned_to_child.size());
    child_resource_provider->ReceiveReturnsFromParent(returned_to_child);

    child_resource_provider->RemoveImportedResource(resource_id);
    EXPECT_TRUE(release_sync_token.HasData());
    EXPECT_FALSE(lost_resource);
  }
};

TEST_P(ResourceProviderTest, ImportedResource_GLTexture2D_LinearToLinear) {
  // Mailboxing is only supported for GL textures.
  if (!use_gpu())
    return;

  ResourceProviderTestImportedResourceGLFilters::RunTest(
      shared_bitmap_manager_.get(), false, GL_LINEAR);
}

TEST_P(ResourceProviderTest, ImportedResource_GLTexture2D_NearestToNearest) {
  // Mailboxing is only supported for GL textures.
  if (!use_gpu())
    return;

  ResourceProviderTestImportedResourceGLFilters::RunTest(
      shared_bitmap_manager_.get(), true, GL_NEAREST);
}

TEST_P(ResourceProviderTest, ImportedResource_GLTexture2D_NearestToLinear) {
  // Mailboxing is only supported for GL textures.
  if (!use_gpu())
    return;

  ResourceProviderTestImportedResourceGLFilters::RunTest(
      shared_bitmap_manager_.get(), true, GL_LINEAR);
}

TEST_P(ResourceProviderTest, ImportedResource_GLTexture2D_LinearToNearest) {
  // Mailboxing is only supported for GL textures.
  if (!use_gpu())
    return;

  ResourceProviderTestImportedResourceGLFilters::RunTest(
      shared_bitmap_manager_.get(), false, GL_NEAREST);
}

TEST_P(ResourceProviderTest, ImportedResource_GLTextureExternalOES) {
  // Mailboxing is only supported for GL textures.
  if (!use_gpu())
    return;

  auto context_owned(std::make_unique<TextureStateTrackingContext>());
  TextureStateTrackingContext* context = context_owned.get();
  auto context_provider =
      viz::TestContextProvider::Create(std::move(context_owned));
  context_provider->BindToCurrentThread();

  auto resource_provider(std::make_unique<viz::DisplayResourceProvider>(
      context_provider.get(), shared_bitmap_manager_.get()));

  auto child_context_owned(std::make_unique<TextureStateTrackingContext>());
  TextureStateTrackingContext* child_context = child_context_owned.get();
  auto child_context_provider =
      viz::TestContextProvider::Create(std::move(child_context_owned));
  child_context_provider->BindToCurrentThread();

  auto child_resource_provider(std::make_unique<LayerTreeResourceProvider>(
      child_context_provider.get(), kDelegatedSyncPointsRequired));

  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                            gpu::CommandBufferId::FromUnsafeValue(0x12), 0x34);
  const GLuint64 current_fence_sync = child_context->GetNextFenceSync();

  EXPECT_CALL(*child_context, bindTexture(_, _)).Times(0);
  EXPECT_CALL(*child_context, waitSyncToken(_)).Times(0);
  EXPECT_CALL(*child_context, produceTextureDirectCHROMIUM(_, _)).Times(0);
  EXPECT_CALL(*child_context, createAndConsumeTextureCHROMIUM(_)).Times(0);

  gpu::Mailbox gpu_mailbox;
  memcpy(gpu_mailbox.name, "Hello world", strlen("Hello world") + 1);
  std::unique_ptr<viz::SingleReleaseCallback> callback =
      viz::SingleReleaseCallback::Create(base::DoNothing());

  auto resource = viz::TransferableResource::MakeGL(
      gpu_mailbox, GL_LINEAR, GL_TEXTURE_EXTERNAL_OES, sync_token);

  viz::ResourceId resource_id =
      child_resource_provider->ImportResource(resource, std::move(callback));
  EXPECT_NE(0u, resource_id);
  EXPECT_EQ(current_fence_sync, child_context->GetNextFenceSync());

  Mock::VerifyAndClearExpectations(child_context);

  // Transfer resources to the parent.
  std::vector<viz::ResourceId> resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource_id);

  std::vector<viz::TransferableResource> send_to_parent;
  std::vector<viz::ReturnedResource> returned_to_child;
  int child_id = resource_provider->CreateChild(
      base::Bind(&CollectResources, &returned_to_child));
  child_resource_provider->PrepareSendToParent(
      resource_ids_to_transfer, &send_to_parent, child_context_provider_.get());
  resource_provider->ReceiveFromChild(child_id, send_to_parent);

  // Before create DrawQuad in viz::DisplayResourceProvider's namespace, get the
  // mapped resource id first.
  std::unordered_map<viz::ResourceId, viz::ResourceId> resource_map =
      resource_provider->GetChildToParentMap(child_id);
  viz::ResourceId mapped_resource_id = resource_map[resource_id];
  {
    // The verified flush flag will be set by
    // LayerTreeResourceProvider::PrepareSendToParent. Before checking if
    // the gpu::SyncToken matches, set this flag first.
    sync_token.SetVerifyFlush();

    // Mailbox sync point WaitSyncToken before using the texture.
    EXPECT_CALL(*context, waitSyncToken(MatchesSyncToken(sync_token)));
    resource_provider->WaitSyncToken(mapped_resource_id);
    Mock::VerifyAndClearExpectations(context);

    unsigned texture_id = 1;

    EXPECT_CALL(*context, createAndConsumeTextureCHROMIUM(_))
        .WillOnce(Return(texture_id));

    EXPECT_CALL(*context, produceTextureDirectCHROMIUM(_, _)).Times(0);

    viz::DisplayResourceProvider::ScopedReadLockGL lock(resource_provider.get(),
                                                        mapped_resource_id);
    Mock::VerifyAndClearExpectations(context);

    // When done with it, a sync point should be inserted, but no produce is
    // necessary.
    EXPECT_CALL(*context, bindTexture(_, _)).Times(0);
    EXPECT_CALL(*context, produceTextureDirectCHROMIUM(_, _)).Times(0);

    EXPECT_CALL(*context, waitSyncToken(_)).Times(0);
    EXPECT_CALL(*context, createAndConsumeTextureCHROMIUM(_)).Times(0);
    Mock::VerifyAndClearExpectations(context);
  }
  EXPECT_EQ(0u, returned_to_child.size());
  // Transfer resources back from the parent to the child. Set no resources as
  // being in use.
  resource_provider->DeclareUsedResourcesFromChild(child_id,
                                                   viz::ResourceIdSet());
  EXPECT_EQ(1u, returned_to_child.size());
  child_resource_provider->ReceiveReturnsFromParent(returned_to_child);

  child_resource_provider->RemoveImportedResource(resource_id);
}

TEST_P(ResourceProviderTest, WaitSyncTokenIfNeeded_ResourceFromChild) {
  // Mailboxing is only supported for GL textures.
  if (!use_gpu())
    return;

  auto context_owned(std::make_unique<TextureStateTrackingContext>());
  TextureStateTrackingContext* context = context_owned.get();
  auto context_provider =
      viz::TestContextProvider::Create(std::move(context_owned));
  context_provider->BindToCurrentThread();

  auto resource_provider = std::make_unique<viz::DisplayResourceProvider>(
      context_provider.get(), shared_bitmap_manager_.get());

  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                            gpu::CommandBufferId::FromUnsafeValue(0x12), 0x34);
  const GLuint64 current_fence_sync = context->GetNextFenceSync();

  EXPECT_CALL(*context, bindTexture(_, _)).Times(0);
  EXPECT_CALL(*context, waitSyncToken(_)).Times(0);
  EXPECT_CALL(*context, produceTextureDirectCHROMIUM(_, _)).Times(0);
  EXPECT_CALL(*context, createAndConsumeTextureCHROMIUM(_)).Times(0);

  // Receive a resource from the child.
  std::vector<viz::ReturnedResource> returned;
  viz::ReturnCallback return_callback = base::BindRepeating(
      [](std::vector<viz::ReturnedResource>* out,
         const std::vector<viz::ReturnedResource>& in) {
        *out = std::move(in);
      },
      &returned);
  int child = resource_provider->CreateChild(return_callback);

  // Send a bunch of resources, to make sure things work when there's more than
  // one resource, and to make vectors reallocate and such.
  for (int i = 0; i < 100; ++i) {
    gpu::Mailbox gpu_mailbox;
    memcpy(gpu_mailbox.name, "Hello world", strlen("Hello world") + 1);
    auto resource = viz::TransferableResource::MakeGL(
        gpu_mailbox, GL_LINEAR, GL_TEXTURE_2D, sync_token);
    resource.id = i;
    resource_provider->ReceiveFromChild(child, {resource});
    auto& map = resource_provider->GetChildToParentMap(child);
    EXPECT_EQ(i + 1u, map.size());
  }

  EXPECT_EQ(current_fence_sync, context->GetNextFenceSync());
  resource_provider.reset();
  // Returned resources don't have InsertFenceSyncCHROMIUM() called.
  EXPECT_EQ(current_fence_sync, context->GetNextFenceSync());

  // The returned resource has a verified sync token.
  ASSERT_EQ(returned.size(), 100u);
  for (viz::ReturnedResource& r : returned)
    EXPECT_TRUE(r.sync_token.verified_flush());
}

TEST_P(ResourceProviderTest, WaitSyncTokenIfNeeded_WithSyncToken) {
  // Mailboxing is only supported for GL textures.
  if (!use_gpu())
    return;

  auto context_owned(std::make_unique<TextureStateTrackingContext>());
  TextureStateTrackingContext* context = context_owned.get();
  auto context_provider =
      viz::TestContextProvider::Create(std::move(context_owned));
  context_provider->BindToCurrentThread();

  auto resource_provider = std::make_unique<viz::DisplayResourceProvider>(
      context_provider.get(), shared_bitmap_manager_.get());

  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                            gpu::CommandBufferId::FromUnsafeValue(0x12), 0x34);
  const GLuint64 current_fence_sync = context->GetNextFenceSync();

  EXPECT_CALL(*context, bindTexture(_, _)).Times(0);
  EXPECT_CALL(*context, waitSyncToken(_)).Times(0);
  EXPECT_CALL(*context, produceTextureDirectCHROMIUM(_, _)).Times(0);
  EXPECT_CALL(*context, createAndConsumeTextureCHROMIUM(_)).Times(0);

  viz::ResourceId id = MakeGpuResourceAndSendToDisplay(
      'a', GL_LINEAR, GL_TEXTURE_2D, sync_token, resource_provider.get());
  EXPECT_NE(0u, id);
  EXPECT_EQ(current_fence_sync, context->GetNextFenceSync());

  Mock::VerifyAndClearExpectations(context);

  {
    // First call to WaitSyncToken should call waitSyncToken.
    EXPECT_CALL(*context, waitSyncToken(MatchesSyncToken(sync_token)));
    resource_provider->WaitSyncToken(id);
    Mock::VerifyAndClearExpectations(context);

    // Subsequent calls to WaitSyncToken shouldn't call waitSyncToken.
    EXPECT_CALL(*context, waitSyncToken(_)).Times(0);
    resource_provider->WaitSyncToken(id);
    Mock::VerifyAndClearExpectations(context);
  }
}

TEST_P(ResourceProviderTest,
       ImportedResource_WaitSyncTokenIfNeeded_NoSyncToken) {
  // Mailboxing is only supported for GL textures.
  if (!use_gpu())
    return;

  auto context_owned(std::make_unique<TextureStateTrackingContext>());
  TextureStateTrackingContext* context = context_owned.get();
  auto context_provider =
      viz::TestContextProvider::Create(std::move(context_owned));
  context_provider->BindToCurrentThread();

  auto resource_provider = std::make_unique<viz::DisplayResourceProvider>(
      context_provider.get(), shared_bitmap_manager_.get());

  gpu::SyncToken sync_token;
  const GLuint64 current_fence_sync = context->GetNextFenceSync();

  EXPECT_CALL(*context, bindTexture(_, _)).Times(0);
  EXPECT_CALL(*context, waitSyncToken(_)).Times(0);
  EXPECT_CALL(*context, produceTextureDirectCHROMIUM(_, _)).Times(0);
  EXPECT_CALL(*context, createAndConsumeTextureCHROMIUM(_)).Times(0);

  viz::ResourceId id = MakeGpuResourceAndSendToDisplay(
      'h', GL_LINEAR, GL_TEXTURE_2D, sync_token, resource_provider.get());

  EXPECT_EQ(current_fence_sync, context->GetNextFenceSync());
  EXPECT_NE(0u, id);

  Mock::VerifyAndClearExpectations(context);

  {
    // WaitSyncToken with empty sync_token shouldn't call waitSyncToken.
    EXPECT_CALL(*context, waitSyncToken(_)).Times(0);
    resource_provider->WaitSyncToken(id);
    Mock::VerifyAndClearExpectations(context);
  }
}

TEST_P(ResourceProviderTest, ImportedResource_PrepareSendToParent_NoSyncToken) {
  // Mailboxing is only supported for GL textures.
  if (!use_gpu())
    return;

  auto context_owned(std::make_unique<TextureStateTrackingContext>());
  TextureStateTrackingContext* context = context_owned.get();
  auto context_provider =
      viz::TestContextProvider::Create(std::move(context_owned));
  context_provider->BindToCurrentThread();

  auto resource_provider(std::make_unique<LayerTreeResourceProvider>(
      context_provider.get(), kDelegatedSyncPointsRequired));

  EXPECT_CALL(*context, bindTexture(_, _)).Times(0);
  EXPECT_CALL(*context, waitSyncToken(_)).Times(0);
  EXPECT_CALL(*context, produceTextureDirectCHROMIUM(_, _)).Times(0);
  EXPECT_CALL(*context, createAndConsumeTextureCHROMIUM(_)).Times(0);

  auto resource = viz::TransferableResource::MakeGL(
      gpu::Mailbox::Generate(), GL_LINEAR, GL_TEXTURE_2D, gpu::SyncToken());

  std::unique_ptr<viz::SingleReleaseCallback> callback =
      viz::SingleReleaseCallback::Create(base::DoNothing());

  viz::ResourceId id =
      resource_provider->ImportResource(resource, std::move(callback));
  EXPECT_NE(0u, id);
  Mock::VerifyAndClearExpectations(context);

  std::vector<viz::ResourceId> resource_ids_to_transfer{id};
  std::vector<viz::TransferableResource> list;
  resource_provider->PrepareSendToParent(resource_ids_to_transfer, &list,
                                         context_provider.get());
  ASSERT_EQ(1u, list.size());
  EXPECT_FALSE(list[0].mailbox_holder.sync_token.HasData());
  EXPECT_TRUE(list[0].mailbox_holder.sync_token.verified_flush());
  Mock::VerifyAndClearExpectations(context);
}

INSTANTIATE_TEST_CASE_P(ResourceProviderTests,
                        ResourceProviderTest,
                        ::testing::Values(true, false));

}  // namespace
}  // namespace cc
