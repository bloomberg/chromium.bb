// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_provider.h"

#include "base/bind.h"
#include "base/logging.h"
#include "cc/base/scoped_ptr_deque.h"
#include "cc/base/scoped_ptr_hash_map.h"
#include "cc/output/output_surface.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/test_web_graphics_context_3d.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/rect.h"

using namespace WebKit;

using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::_;

namespace cc {
namespace {

size_t TextureSize(gfx::Size size, WGC3Denum format) {
  unsigned int components_per_pixel = 4;
  unsigned int bytes_per_component = 1;
  return size.width() * size.height() * components_per_pixel *
      bytes_per_component;
}

struct Texture {
  Texture() : format(0), filter(GL_NEAREST_MIPMAP_LINEAR) {}

  void Reallocate(gfx::Size size, WGC3Denum format) {
    this->size = size;
    this->format = format;
    this->data.reset(new uint8_t[TextureSize(size, format)]);
  }

  gfx::Size size;
  WGC3Denum format;
  WGC3Denum filter;
  scoped_array<uint8_t> data;
};

// Shared data between multiple ResourceProviderContext. This contains mailbox
// contents as well as information about sync points.
class ContextSharedData {
 public:
  static scoped_ptr<ContextSharedData> Create() {
    return make_scoped_ptr(new ContextSharedData());
  }

  unsigned InsertSyncPoint() { return next_sync_point_++; }

  void GenMailbox(WGC3Dbyte* mailbox) {
    memset(mailbox, 0, sizeof(WGC3Dbyte[64]));
    memcpy(mailbox, &next_mailbox_, sizeof(next_mailbox_));
    ++next_mailbox_;
  }

  void ProduceTexture(const WGC3Dbyte* mailbox_name,
                      unsigned sync_point,
                      scoped_ptr<Texture> texture) {
    unsigned mailbox = 0;
    memcpy(&mailbox, mailbox_name, sizeof(mailbox));
    ASSERT_TRUE(mailbox && mailbox < next_mailbox_);
    textures_.set(mailbox, texture.Pass());
    ASSERT_LT(sync_point_for_mailbox_[mailbox], sync_point);
    sync_point_for_mailbox_[mailbox] = sync_point;
  }

  scoped_ptr<Texture> ConsumeTexture(const WGC3Dbyte* mailbox_name,
                                     unsigned sync_point) {
    unsigned mailbox = 0;
    memcpy(&mailbox, mailbox_name, sizeof(mailbox));
    DCHECK(mailbox && mailbox < next_mailbox_);

    // If the latest sync point the context has waited on is before the sync
    // point for when the mailbox was set, pretend we never saw that
    // produceTexture.
    if (sync_point_for_mailbox_[mailbox] > sync_point) {
      NOTREACHED();
      return scoped_ptr<Texture>();
    }
    return textures_.take(mailbox);
  }

 private:
  ContextSharedData() : next_sync_point_(1), next_mailbox_(1) {}

  unsigned next_sync_point_;
  unsigned next_mailbox_;
  typedef ScopedPtrHashMap<unsigned, Texture> TextureMap;
  TextureMap textures_;
  base::hash_map<unsigned, unsigned> sync_point_for_mailbox_;
};

class ResourceProviderContext : public TestWebGraphicsContext3D {
 public:
  static scoped_ptr<ResourceProviderContext> Create(
      ContextSharedData* shared_data) {
    return make_scoped_ptr(
        new ResourceProviderContext(Attributes(), shared_data));
  }

  virtual unsigned insertSyncPoint() OVERRIDE {
    unsigned sync_point = shared_data_->InsertSyncPoint();
    // Commit the produceTextureCHROMIUM calls at this point, so that
    // they're associated with the sync point.
    for (PendingProduceTextureList::iterator it =
             pending_produce_textures_.begin();
         it != pending_produce_textures_.end();
         ++it) {
      shared_data_->ProduceTexture(
          (*it)->mailbox, sync_point, (*it)->texture.Pass());
    }
    pending_produce_textures_.clear();
    return sync_point;
  }

  virtual void waitSyncPoint(unsigned sync_point) OVERRIDE {
    last_waited_sync_point_ = std::max(sync_point, last_waited_sync_point_);
  }

  virtual void bindTexture(WGC3Denum target, WebGLId texture) OVERRIDE {
    ASSERT_EQ(target, GL_TEXTURE_2D);
    ASSERT_TRUE(!texture || textures_.find(texture) != textures_.end());
    current_texture_ = texture;
  }

  virtual WebGLId createTexture() OVERRIDE {
    WebGLId id = TestWebGraphicsContext3D::createTexture();
    textures_.add(id, make_scoped_ptr(new Texture));
    return id;
  }

  virtual void deleteTexture(WebGLId id) OVERRIDE {
    TextureMap::iterator it = textures_.find(id);
    ASSERT_FALSE(it == textures_.end());
    textures_.erase(it);
    if (current_texture_ == id)
      current_texture_ = 0;
  }

  virtual void texStorage2DEXT(WGC3Denum target,
                               WGC3Dint levels,
                               WGC3Duint internalformat,
                               WGC3Dint width,
                               WGC3Dint height) OVERRIDE {
    ASSERT_TRUE(current_texture_);
    ASSERT_EQ(target, GL_TEXTURE_2D);
    ASSERT_EQ(levels, 1);
    WGC3Denum format = GL_RGBA;
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

  virtual void texImage2D(WGC3Denum target,
                          WGC3Dint level,
                          WGC3Denum internalformat,
                          WGC3Dsizei width,
                          WGC3Dsizei height,
                          WGC3Dint border,
                          WGC3Denum format,
                          WGC3Denum type,
                          const void* pixels) OVERRIDE {
    ASSERT_TRUE(current_texture_);
    ASSERT_EQ(target, GL_TEXTURE_2D);
    ASSERT_FALSE(level);
    ASSERT_EQ(internalformat, format);
    ASSERT_FALSE(border);
    ASSERT_EQ(type, GL_UNSIGNED_BYTE);
    AllocateTexture(gfx::Size(width, height), format);
    if (pixels)
      SetPixels(0, 0, width, height, pixels);
  }

  virtual void texSubImage2D(WGC3Denum target,
                             WGC3Dint level,
                             WGC3Dint xoffset,
                             WGC3Dint yoffset,
                             WGC3Dsizei width,
                             WGC3Dsizei height,
                             WGC3Denum format,
                             WGC3Denum type,
                             const void* pixels) OVERRIDE {
    ASSERT_TRUE(current_texture_);
    ASSERT_EQ(target, GL_TEXTURE_2D);
    ASSERT_FALSE(level);
    ASSERT_TRUE(textures_.get(current_texture_));
    ASSERT_EQ(textures_.get(current_texture_)->format, format);
    ASSERT_EQ(type, GL_UNSIGNED_BYTE);
    ASSERT_TRUE(pixels);
    SetPixels(xoffset, yoffset, width, height, pixels);
  }

  virtual void texParameteri(WGC3Denum target, WGC3Denum param, WGC3Dint value)
      OVERRIDE {
    ASSERT_TRUE(current_texture_);
    ASSERT_EQ(target, GL_TEXTURE_2D);
    Texture* texture = textures_.get(current_texture_);
    ASSERT_TRUE(texture);
    if (param != GL_TEXTURE_MIN_FILTER)
      return;
    texture->filter = value;
  }

  virtual void genMailboxCHROMIUM(WGC3Dbyte* mailbox) OVERRIDE {
    return shared_data_->GenMailbox(mailbox);
  }

  virtual void produceTextureCHROMIUM(WGC3Denum target,
                                      const WGC3Dbyte* mailbox) OVERRIDE {
    ASSERT_TRUE(current_texture_);
    ASSERT_EQ(target, GL_TEXTURE_2D);

    // Delay moving the texture into the mailbox until the next
    // insertSyncPoint, so that it is not visible to other contexts that
    // haven't waited on that sync point.
    scoped_ptr<PendingProduceTexture> pending(new PendingProduceTexture);
    memcpy(pending->mailbox, mailbox, sizeof(pending->mailbox));
    pending->texture = textures_.take(current_texture_);
    textures_.set(current_texture_, scoped_ptr<Texture>());
    pending_produce_textures_.push_back(pending.Pass());
  }

  virtual void consumeTextureCHROMIUM(WGC3Denum target,
                                      const WGC3Dbyte* mailbox) OVERRIDE {
    ASSERT_TRUE(current_texture_);
    ASSERT_EQ(target, GL_TEXTURE_2D);
    textures_.set(
        current_texture_,
        shared_data_->ConsumeTexture(mailbox, last_waited_sync_point_));
  }

  void GetPixels(gfx::Size size, WGC3Denum format, uint8_t* pixels) {
    ASSERT_TRUE(current_texture_);
    Texture* texture = textures_.get(current_texture_);
    ASSERT_TRUE(texture);
    ASSERT_EQ(texture->size, size);
    ASSERT_EQ(texture->format, format);
    memcpy(pixels, texture->data.get(), TextureSize(size, format));
  }

  WGC3Denum GetTextureFilter() {
    DCHECK(current_texture_);
    Texture* texture = textures_.get(current_texture_);
    DCHECK(texture);
    return texture->filter;
  }

  int texture_count() { return textures_.size(); }

 protected:
  ResourceProviderContext(const Attributes& attrs,
                          ContextSharedData* shared_data)
      : TestWebGraphicsContext3D(attrs),
        shared_data_(shared_data),
        current_texture_(0),
        last_waited_sync_point_(0) {}

 private:
  void AllocateTexture(gfx::Size size, WGC3Denum format) {
    ASSERT_TRUE(current_texture_);
    Texture* texture = textures_.get(current_texture_);
    ASSERT_TRUE(texture);
    texture->Reallocate(size, format);
  }

  void SetPixels(int xoffset,
                 int yoffset,
                 int width,
                 int height,
                 const void* pixels) {
    ASSERT_TRUE(current_texture_);
    Texture* texture = textures_.get(current_texture_);
    ASSERT_TRUE(texture);
    ASSERT_TRUE(texture->data.get());
    ASSERT_TRUE(xoffset >= 0 && xoffset + width <= texture->size.width());
    ASSERT_TRUE(yoffset >= 0 && yoffset + height <= texture->size.height());
    ASSERT_TRUE(pixels);
    size_t in_pitch = TextureSize(gfx::Size(width, 1), texture->format);
    size_t out_pitch =
        TextureSize(gfx::Size(texture->size.width(), 1), texture->format);
    uint8_t* dest = texture->data.get() + yoffset * out_pitch +
                    TextureSize(gfx::Size(xoffset, 1), texture->format);
    const uint8_t* src = static_cast<const uint8_t*>(pixels);
    for (int i = 0; i < height; ++i) {
      memcpy(dest, src, in_pitch);
      dest += out_pitch;
      src += in_pitch;
    }
  }

  typedef ScopedPtrHashMap<WebGLId, Texture> TextureMap;
  struct PendingProduceTexture {
    WGC3Dbyte mailbox[64];
    scoped_ptr<Texture> texture;
  };
  typedef ScopedPtrDeque<PendingProduceTexture> PendingProduceTextureList;
  ContextSharedData* shared_data_;
  WebGLId current_texture_;
  TextureMap textures_;
  unsigned last_waited_sync_point_;
  PendingProduceTextureList pending_produce_textures_;
};

class ResourceProviderTest :
    public testing::TestWithParam<ResourceProvider::ResourceType> {
 public:
  ResourceProviderTest()
      : shared_data_(ContextSharedData::Create()),
        output_surface_(FakeOutputSurface::Create3d(
            ResourceProviderContext::Create(shared_data_.get())
                .PassAs<WebKit::WebGraphicsContext3D>())),
        resource_provider_(ResourceProvider::Create(output_surface_.get())) {
    resource_provider_->set_default_resource_type(GetParam());
  }

  ResourceProviderContext* context() {
    return static_cast<ResourceProviderContext*>(output_surface_->context3d());
  }

  void GetResourcePixels(ResourceProvider::ResourceId id,
                         gfx::Size size,
                         WGC3Denum format,
                         uint8_t* pixels) {
    if (GetParam() == ResourceProvider::GLTexture) {
      ResourceProvider::ScopedReadLockGL lock_gl(resource_provider_.get(), id);
      ASSERT_NE(0U, lock_gl.texture_id());
      context()->bindTexture(GL_TEXTURE_2D, lock_gl.texture_id());
      context()->GetPixels(size, format, pixels);
    } else if (GetParam() == ResourceProvider::Bitmap) {
      ResourceProvider::ScopedReadLockSoftware lock_software(
          resource_provider_.get(), id);
      memcpy(pixels,
             lock_software.sk_bitmap()->getPixels(),
             lock_software.sk_bitmap()->getSize());
    }
  }

  void SetResourceFilter(ResourceProvider* resource_provider,
                         ResourceProvider::ResourceId id,
                         WGC3Denum filter) {
    ResourceProvider::ScopedSamplerGL sampler(
        resource_provider, id, GL_TEXTURE_2D, filter);
  }

  WGC3Denum GetResourceFilter(ResourceProvider* resource_provider,
                              ResourceProvider::ResourceId id) {
    DCHECK_EQ(GetParam(), ResourceProvider::GLTexture);
    ResourceProvider::ScopedReadLockGL lock_gl(resource_provider, id);
    EXPECT_NE(0u, lock_gl.texture_id());
    ResourceProviderContext* context = static_cast<ResourceProviderContext*>(
        resource_provider->GraphicsContext3D());
    context->bindTexture(GL_TEXTURE_2D, lock_gl.texture_id());
    return context->GetTextureFilter();
  }

  void ExpectNumResources(int count) {
    EXPECT_EQ(count, static_cast<int>(resource_provider_->num_resources()));
    if (GetParam() == ResourceProvider::GLTexture)
      EXPECT_EQ(count, context()->texture_count());
  }

 protected:
  scoped_ptr<ContextSharedData> shared_data_;
  scoped_ptr<OutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
};

TEST_P(ResourceProviderTest, Basic) {
  gfx::Size size(1, 1);
  WGC3Denum format = GL_RGBA;
  size_t pixel_size = TextureSize(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id = resource_provider_->CreateResource(
      size, format, ResourceProvider::TextureUsageAny);
  ExpectNumResources(1);

  uint8_t data[4] = { 1, 2, 3, 4 };
  gfx::Rect rect(gfx::Point(), size);
  resource_provider_->SetPixels(id, data, rect, rect, gfx::Vector2d());

  uint8_t result[4] = { 0 };
  GetResourcePixels(id, size, format, result);
  EXPECT_EQ(0, memcmp(data, result, pixel_size));

  resource_provider_->DeleteResource(id);
  ExpectNumResources(0);
}

TEST_P(ResourceProviderTest, Upload) {
  gfx::Size size(2, 2);
  WGC3Denum format = GL_RGBA;
  size_t pixel_size = TextureSize(size, format);
  ASSERT_EQ(16U, pixel_size);

  ResourceProvider::ResourceId id = resource_provider_->CreateResource(
      size, format, ResourceProvider::TextureUsageAny);

  uint8_t image[16] = { 0 };
  gfx::Rect image_rect(gfx::Point(), size);
  resource_provider_->SetPixels(
      id, image, image_rect, image_rect, gfx::Vector2d());

  for (uint8_t i = 0; i < pixel_size; ++i)
    image[i] = i;

  uint8_t result[16] = { 0 };
  {
    gfx::Rect source_rect(0, 0, 1, 1);
    gfx::Vector2d dest_offset(0, 0);
    resource_provider_->SetPixels(
        id, image, image_rect, source_rect, dest_offset);

    uint8_t expected[16] = { 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    GetResourcePixels(id, size, format, result);
    EXPECT_EQ(0, memcmp(expected, result, pixel_size));
  }
  {
    gfx::Rect source_rect(0, 0, 1, 1);
    gfx::Vector2d dest_offset(1, 1);
    resource_provider_->SetPixels(
        id, image, image_rect, source_rect, dest_offset);

    uint8_t expected[16] = { 0, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3 };
    GetResourcePixels(id, size, format, result);
    EXPECT_EQ(0, memcmp(expected, result, pixel_size));
  }
  {
    gfx::Rect source_rect(1, 0, 1, 1);
    gfx::Vector2d dest_offset(0, 1);
    resource_provider_->SetPixels(
        id, image, image_rect, source_rect, dest_offset);

    uint8_t expected[16] = { 0, 1, 2, 3, 0, 0, 0, 0, 4, 5, 6, 7, 0, 1, 2, 3 };
    GetResourcePixels(id, size, format, result);
    EXPECT_EQ(0, memcmp(expected, result, pixel_size));
  }
  {
    gfx::Rect offset_image_rect(gfx::Point(100, 100), size);
    gfx::Rect source_rect(100, 100, 1, 1);
    gfx::Vector2d dest_offset(1, 0);
    resource_provider_->SetPixels(
        id, image, offset_image_rect, source_rect, dest_offset);

    uint8_t expected[16] = { 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3 };
    GetResourcePixels(id, size, format, result);
    EXPECT_EQ(0, memcmp(expected, result, pixel_size));
  }

  resource_provider_->DeleteResource(id);
}

TEST_P(ResourceProviderTest, TransferResources) {
  // Resource transfer is only supported with GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<OutputSurface> child_output_surface(FakeOutputSurface::Create3d(
      ResourceProviderContext::Create(shared_data_.get())
          .PassAs<WebKit::WebGraphicsContext3D>()));
  scoped_ptr<ResourceProvider> child_resource_provider(
      ResourceProvider::Create(child_output_surface.get()));

  gfx::Size size(1, 1);
  WGC3Denum format = GL_RGBA;
  size_t pixel_size = TextureSize(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id1 = child_resource_provider->CreateResource(
      size, format, ResourceProvider::TextureUsageAny);
  uint8_t data1[4] = { 1, 2, 3, 4 };
  gfx::Rect rect(gfx::Point(), size);
  child_resource_provider->SetPixels(id1, data1, rect, rect, gfx::Vector2d());

  ResourceProvider::ResourceId id2 = child_resource_provider->CreateResource(
      size, format, ResourceProvider::TextureUsageAny);
  uint8_t data2[4] = { 5, 5, 5, 5 };
  child_resource_provider->SetPixels(id2, data2, rect, rect, gfx::Vector2d());

  int child_id = resource_provider_->CreateChild();
  {
    // Transfer some resources to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    resource_ids_to_transfer.push_back(id2);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    ASSERT_EQ(2u, list.size());
    EXPECT_NE(0u, list[0].sync_point);
    EXPECT_NE(0u, list[1].sync_point);
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id2));
    resource_provider_->ReceiveFromChild(child_id, list);
  }

  EXPECT_EQ(2u, resource_provider_->num_resources());
  ResourceProvider::ResourceIdMap resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  ResourceProvider::ResourceId mapped_id1 = resource_map[id1];
  ResourceProvider::ResourceId mapped_id2 = resource_map[id2];
  EXPECT_NE(0u, mapped_id1);
  EXPECT_NE(0u, mapped_id2);
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id1));
  EXPECT_FALSE(resource_provider_->InUseByConsumer(id2));

  uint8_t result[4] = { 0 };
  GetResourcePixels(mapped_id1, size, format, result);
  EXPECT_EQ(0, memcmp(data1, result, pixel_size));

  GetResourcePixels(mapped_id2, size, format, result);
  EXPECT_EQ(0, memcmp(data2, result, pixel_size));
  {
    // Check that transfering again the same resource from the child to the
    // parent is a noop.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    EXPECT_EQ(0u, list.size());
  }
  {
    // Transfer resources back from the parent to the child.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(mapped_id1);
    resource_ids_to_transfer.push_back(mapped_id2);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToChild(
        child_id, resource_ids_to_transfer, &list);
    ASSERT_EQ(2u, list.size());
    EXPECT_NE(0u, list[0].sync_point);
    EXPECT_NE(0u, list[1].sync_point);
    child_resource_provider->ReceiveFromParent(list);
  }
  EXPECT_FALSE(child_resource_provider->InUseByConsumer(id1));
  EXPECT_FALSE(child_resource_provider->InUseByConsumer(id2));

  ResourceProviderContext* child_context =
      static_cast<ResourceProviderContext*>(child_output_surface->context3d());
  {
    ResourceProvider::ScopedReadLockGL lock(child_resource_provider.get(), id1);
    ASSERT_NE(0U, lock.texture_id());
    child_context->bindTexture(GL_TEXTURE_2D, lock.texture_id());
    child_context->GetPixels(size, format, result);
    EXPECT_EQ(0, memcmp(data1, result, pixel_size));
  }
  {
    ResourceProvider::ScopedReadLockGL lock(child_resource_provider.get(), id2);
    ASSERT_NE(0U, lock.texture_id());
    child_context->bindTexture(GL_TEXTURE_2D, lock.texture_id());
    child_context->GetPixels(size, format, result);
    EXPECT_EQ(0, memcmp(data2, result, pixel_size));
  }
  {
    // Transfer resources to the parent again.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id1);
    resource_ids_to_transfer.push_back(id2);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    ASSERT_EQ(2u, list.size());
    EXPECT_NE(0u, list[0].sync_point);
    EXPECT_NE(0u, list[1].sync_point);
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id1));
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id2));
    resource_provider_->ReceiveFromChild(child_id, list);
  }

  EXPECT_EQ(2u, resource_provider_->num_resources());
  resource_provider_->DestroyChild(child_id);
  EXPECT_EQ(0u, resource_provider_->num_resources());
}

TEST_P(ResourceProviderTest, DeleteTransferredResources) {
  // Resource transfer is only supported with GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<OutputSurface> child_output_surface(FakeOutputSurface::Create3d(
      ResourceProviderContext::Create(shared_data_.get())
          .PassAs<WebKit::WebGraphicsContext3D>()));
  scoped_ptr<ResourceProvider> child_resource_provider(
      ResourceProvider::Create(child_output_surface.get()));

  gfx::Size size(1, 1);
  WGC3Denum format = GL_RGBA;
  size_t pixel_size = TextureSize(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id = child_resource_provider->CreateResource(
      size, format, ResourceProvider::TextureUsageAny);
  uint8_t data[4] = { 1, 2, 3, 4 };
  gfx::Rect rect(gfx::Point(), size);
  child_resource_provider->SetPixels(id, data, rect, rect, gfx::Vector2d());

  int child_id = resource_provider_->CreateChild();
  {
    // Transfer some resource to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    ASSERT_EQ(1u, list.size());
    EXPECT_NE(0u, list[0].sync_point);
    EXPECT_TRUE(child_resource_provider->InUseByConsumer(id));
    resource_provider_->ReceiveFromChild(child_id, list);
  }

  // Delete textures in the child, while they are transfered.
  child_resource_provider->DeleteResource(id);
  EXPECT_EQ(1u, child_resource_provider->num_resources());
  {
    // Transfer resources back from the parent to the child.
    ResourceProvider::ResourceIdMap resource_map =
        resource_provider_->GetChildToParentMap(child_id);
    ResourceProvider::ResourceId mapped_id = resource_map[id];
    EXPECT_NE(0u, mapped_id);
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(mapped_id);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToChild(
        child_id, resource_ids_to_transfer, &list);
    ASSERT_EQ(1u, list.size());
    EXPECT_NE(0u, list[0].sync_point);
    child_resource_provider->ReceiveFromParent(list);
  }
  EXPECT_EQ(0u, child_resource_provider->num_resources());
}

TEST_P(ResourceProviderTest, TextureFilters) {
  // Resource transfer is only supported with GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<OutputSurface> child_output_surface(FakeOutputSurface::Create3d(
      ResourceProviderContext::Create(shared_data_.get())
          .PassAs<WebKit::WebGraphicsContext3D>()));
  scoped_ptr<ResourceProvider> child_resource_provider(
      ResourceProvider::Create(child_output_surface.get()));

  gfx::Size size(1, 1);
  WGC3Denum format = GL_RGBA;
  size_t pixel_size = TextureSize(size, format);
  ASSERT_EQ(4U, pixel_size);

  ResourceProvider::ResourceId id = child_resource_provider->CreateResource(
      size, format, ResourceProvider::TextureUsageAny);
  uint8_t data[4] = { 1, 2, 3, 4 };
  gfx::Rect rect(gfx::Point(), size);
  child_resource_provider->SetPixels(id, data, rect, rect, gfx::Vector2d());
  EXPECT_EQ(GL_LINEAR, GetResourceFilter(child_resource_provider.get(), id));
  SetResourceFilter(child_resource_provider.get(), id, GL_NEAREST);
  EXPECT_EQ(GL_NEAREST, GetResourceFilter(child_resource_provider.get(), id));

  int child_id = resource_provider_->CreateChild();
  {
    // Transfer some resource to the parent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(id);
    TransferableResourceArray list;
    child_resource_provider->PrepareSendToParent(resource_ids_to_transfer,
                                                 &list);
    ASSERT_EQ(1u, list.size());
    EXPECT_EQ(GL_NEAREST, list[0].filter);
    resource_provider_->ReceiveFromChild(child_id, list);
  }
  ResourceProvider::ResourceIdMap resource_map =
      resource_provider_->GetChildToParentMap(child_id);
  ResourceProvider::ResourceId mapped_id = resource_map[id];
  EXPECT_NE(0u, mapped_id);
  EXPECT_EQ(GL_NEAREST, GetResourceFilter(resource_provider_.get(), mapped_id));
  SetResourceFilter(resource_provider_.get(), mapped_id, GL_LINEAR);
  EXPECT_EQ(GL_LINEAR, GetResourceFilter(resource_provider_.get(), mapped_id));
  {
    // Transfer resources back from the parent to the child.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(mapped_id);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToChild(
        child_id, resource_ids_to_transfer, &list);
    ASSERT_EQ(1u, list.size());
    EXPECT_EQ(GL_LINEAR, list[0].filter);
    child_resource_provider->ReceiveFromParent(list);
  }
  EXPECT_EQ(GL_LINEAR, GetResourceFilter(child_resource_provider.get(), id));
  SetResourceFilter(child_resource_provider.get(), id, GL_NEAREST);
  EXPECT_EQ(GL_NEAREST, GetResourceFilter(child_resource_provider.get(), id));
}

void ReleaseTextureMailbox(unsigned* release_sync_point, unsigned sync_point) {
  *release_sync_point = sync_point;
}

TEST_P(ResourceProviderTest, TransferMailboxResources) {
  // Resource transfer is only supported with GL textures for now.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  unsigned texture = context()->createTexture();
  context()->bindTexture(GL_TEXTURE_2D, texture);
  uint8_t data[4] = { 1, 2, 3, 4 };
  context()->texImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data);
  gpu::Mailbox mailbox;
  context()->genMailboxCHROMIUM(mailbox.name);
  context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  unsigned sync_point = context()->insertSyncPoint();

  // All the logic below assumes that the sync points are all positive.
  EXPECT_LT(0u, sync_point);

  unsigned release_sync_point = 0;
  TextureMailbox::ReleaseCallback callback =
      base::Bind(ReleaseTextureMailbox, &release_sync_point);
  ResourceProvider::ResourceId resource =
      resource_provider_->CreateResourceFromTextureMailbox(
          TextureMailbox(mailbox, callback, sync_point));
  EXPECT_EQ(1u, context()->texture_count());
  EXPECT_EQ(0u, release_sync_point);
  {
    // Transfer the resource, expect the sync points to be consistent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list);
    ASSERT_EQ(1u, list.size());
    EXPECT_LE(sync_point, list[0].sync_point);
    EXPECT_EQ(0u,
              memcmp(mailbox.name, list[0].mailbox.name, sizeof(mailbox.name)));
    EXPECT_EQ(0u, release_sync_point);

    context()->waitSyncPoint(list[0].sync_point);
    unsigned other_texture = context()->createTexture();
    context()->bindTexture(GL_TEXTURE_2D, other_texture);
    context()->consumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    uint8_t test_data[4] = { 0 };
    context()->GetPixels(gfx::Size(1, 1), GL_RGBA, test_data);
    EXPECT_EQ(0u, memcmp(data, test_data, sizeof(data)));
    context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    context()->deleteTexture(other_texture);
    list[0].sync_point = context()->insertSyncPoint();
    EXPECT_LT(0u, list[0].sync_point);

    // Receive the resource, then delete it, expect the sync points to be
    // consistent.
    resource_provider_->ReceiveFromParent(list);
    EXPECT_EQ(1u, context()->texture_count());
    EXPECT_EQ(0u, release_sync_point);

    resource_provider_->DeleteResource(resource);
    EXPECT_LE(list[0].sync_point, release_sync_point);
  }

  // We're going to do the same thing as above, but testing the case where we
  // delete the resource before we receive it back.
  sync_point = release_sync_point;
  EXPECT_LT(0u, sync_point);
  release_sync_point = 0;
  resource = resource_provider_->CreateResourceFromTextureMailbox(
      TextureMailbox(mailbox, callback, sync_point));
  EXPECT_EQ(1u, context()->texture_count());
  EXPECT_EQ(0u, release_sync_point);
  {
    // Transfer the resource, expect the sync points to be consistent.
    ResourceProvider::ResourceIdArray resource_ids_to_transfer;
    resource_ids_to_transfer.push_back(resource);
    TransferableResourceArray list;
    resource_provider_->PrepareSendToParent(resource_ids_to_transfer, &list);
    ASSERT_EQ(1u, list.size());
    EXPECT_LE(sync_point, list[0].sync_point);
    EXPECT_EQ(0u,
              memcmp(mailbox.name, list[0].mailbox.name, sizeof(mailbox.name)));
    EXPECT_EQ(0u, release_sync_point);

    context()->waitSyncPoint(list[0].sync_point);
    unsigned other_texture = context()->createTexture();
    context()->bindTexture(GL_TEXTURE_2D, other_texture);
    context()->consumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    uint8_t test_data[4] = { 0 };
    context()->GetPixels(gfx::Size(1, 1), GL_RGBA, test_data);
    EXPECT_EQ(0u, memcmp(data, test_data, sizeof(data)));
    context()->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
    context()->deleteTexture(other_texture);
    list[0].sync_point = context()->insertSyncPoint();
    EXPECT_LT(0u, list[0].sync_point);

    // Delete the resource, which shouldn't do anything.
    resource_provider_->DeleteResource(resource);
    EXPECT_EQ(1u, context()->texture_count());
    EXPECT_EQ(0u, release_sync_point);

    // Then receive the resource which should release the mailbox, expect the
    // sync points to be consistent.
    resource_provider_->ReceiveFromParent(list);
    EXPECT_LE(list[0].sync_point, release_sync_point);
  }

  context()->waitSyncPoint(release_sync_point);
  context()->bindTexture(GL_TEXTURE_2D, texture);
  context()->consumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);
  context()->deleteTexture(texture);
}

class TextureStateTrackingContext : public TestWebGraphicsContext3D {
 public:
  MOCK_METHOD2(bindTexture, void(WGC3Denum target, WebGLId texture));
  MOCK_METHOD3(texParameteri,
               void(WGC3Denum target, WGC3Denum pname, WGC3Dint param));

  // Force all textures to be "1" so we can test for them.
  virtual WebKit::WebGLId NextTextureId() OVERRIDE { return 1; }
};

TEST_P(ResourceProviderTest, ScopedSampler) {
  // Sampling is only supported for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<OutputSurface> outputSurface(
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new TextureStateTrackingContext)));
  TextureStateTrackingContext* context =
      static_cast<TextureStateTrackingContext*>(outputSurface->context3d());
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(outputSurface.get()));

  gfx::Size size(1, 1);
  WGC3Denum format = GL_RGBA;
  int texture_id = 1;

  // Check that the texture gets created with the right sampler settings.
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id))
      .Times(2);  // Once to create and once to allocate.
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  EXPECT_CALL(
      *context,
      texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  EXPECT_CALL(
      *context,
      texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D,
                            GL_TEXTURE_POOL_CHROMIUM,
                            GL_TEXTURE_POOL_UNMANAGED_CHROMIUM));
  ResourceProvider::ResourceId id = resource_provider->CreateResource(
      size, format, ResourceProvider::TextureUsageAny);
  resource_provider->AllocateForTesting(id);

  // Creating a sampler with the default filter should not change any texture
  // parameters.
  {
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id));
    ResourceProvider::ScopedSamplerGL sampler(
        resource_provider.get(), id, GL_TEXTURE_2D, GL_LINEAR);
  }

  // Using a different filter should be reflected in the texture parameters.
  {
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id));
    EXPECT_CALL(
        *context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    EXPECT_CALL(
        *context,
        texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    ResourceProvider::ScopedSamplerGL sampler(
        resource_provider.get(), id, GL_TEXTURE_2D, GL_NEAREST);
  }

  // Test resetting to the default filter.
  {
    EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id));
    EXPECT_CALL(*context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    EXPECT_CALL(*context,
                texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    ResourceProvider::ScopedSamplerGL sampler(
        resource_provider.get(), id, GL_TEXTURE_2D, GL_LINEAR);
  }

  Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, ManagedResource) {
  // Sampling is only supported for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;

  scoped_ptr<OutputSurface> outputSurface(
      FakeOutputSurface::Create3d(scoped_ptr<WebKit::WebGraphicsContext3D>(
          new TextureStateTrackingContext)));
  TextureStateTrackingContext* context =
      static_cast<TextureStateTrackingContext*>(outputSurface->context3d());
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(outputSurface.get()));

  gfx::Size size(1, 1);
  WGC3Denum format = GL_RGBA;
  int texture_id = 1;

  // Check that the texture gets created with the right sampler settings.
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id));
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  EXPECT_CALL(
      *context,
      texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  EXPECT_CALL(
      *context,
      texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
  EXPECT_CALL(*context,
              texParameteri(GL_TEXTURE_2D,
                            GL_TEXTURE_POOL_CHROMIUM,
                            GL_TEXTURE_POOL_MANAGED_CHROMIUM));
  ResourceProvider::ResourceId id = resource_provider->CreateManagedResource(
      size, format, ResourceProvider::TextureUsageAny);

  Mock::VerifyAndClearExpectations(context);
}

class AllocationTrackingContext3D : public TestWebGraphicsContext3D {
 public:
  MOCK_METHOD0(createTexture, WebGLId());
  MOCK_METHOD1(deleteTexture, void(WebGLId texture_id));
  MOCK_METHOD2(bindTexture, void(WGC3Denum target, WebGLId texture));
  MOCK_METHOD9(texImage2D,
               void(WGC3Denum target,
                    WGC3Dint level,
                    WGC3Denum internalformat,
                    WGC3Dsizei width,
                    WGC3Dsizei height,
                    WGC3Dint border,
                    WGC3Denum format,
                    WGC3Denum type,
                    const void* pixels));
  MOCK_METHOD9(texSubImage2D,
               void(WGC3Denum target,
                    WGC3Dint level,
                    WGC3Dint xoffset,
                    WGC3Dint yoffset,
                    WGC3Dsizei width,
                    WGC3Dsizei height,
                    WGC3Denum format,
                    WGC3Denum type,
                    const void* pixels));
  MOCK_METHOD9(asyncTexImage2DCHROMIUM,
               void(WGC3Denum target,
                    WGC3Dint level,
                    WGC3Denum internalformat,
                    WGC3Dsizei width,
                    WGC3Dsizei height,
                    WGC3Dint border,
                    WGC3Denum format,
                    WGC3Denum type,
                    const void* pixels));
  MOCK_METHOD9(asyncTexSubImage2DCHROMIUM,
               void(WGC3Denum target,
                    WGC3Dint level,
                    WGC3Dint xoffset,
                    WGC3Dint yoffset,
                    WGC3Dsizei width,
                    WGC3Dsizei height,
                    WGC3Denum format,
                    WGC3Denum type,
                    const void* pixels));
  MOCK_METHOD1(waitAsyncTexImage2DCHROMIUM, void(WGC3Denum target));
};

TEST_P(ResourceProviderTest, TextureAllocation) {
  // Only for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  scoped_ptr<WebKit::WebGraphicsContext3D> mock_context(
      static_cast<WebKit::WebGraphicsContext3D*>(
          new NiceMock<AllocationTrackingContext3D>));
  scoped_ptr<OutputSurface> outputSurface(
      FakeOutputSurface::Create3d(mock_context.Pass()));

  gfx::Size size(2, 2);
  gfx::Vector2d offset(0, 0);
  gfx::Rect rect(0, 0, 2, 2);
  WGC3Denum format = GL_RGBA;
  ResourceProvider::ResourceId id = 0;
  uint8_t pixels[16] = { 0 };
  int texture_id = 123;

  AllocationTrackingContext3D* context =
      static_cast<AllocationTrackingContext3D*>(outputSurface->context3d());
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(outputSurface.get()));

  // Lazy allocation. Don't allocate when creating the resource.
  EXPECT_CALL(*context, createTexture()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, deleteTexture(texture_id)).Times(1);
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(1);
  EXPECT_CALL(*context, texImage2D(_, _, _, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*context, asyncTexImage2DCHROMIUM(_, _, _, _, _, _, _, _, _))
      .Times(0);
  id = resource_provider->CreateResource(
      size, format, ResourceProvider::TextureUsageAny);
  resource_provider->DeleteResource(id);
  Mock::VerifyAndClearExpectations(context);

  // Do allocate when we set the pixels.
  EXPECT_CALL(*context, createTexture()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, deleteTexture(texture_id)).Times(1);
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(3);
  EXPECT_CALL(*context, texImage2D(_, _, _, 2, 2, _, _, _, _)).Times(1);
  EXPECT_CALL(*context, texSubImage2D(_, _, _, _, 2, 2, _, _, _)).Times(1);
  id = resource_provider->CreateResource(
      size, format, ResourceProvider::TextureUsageAny);
  resource_provider->SetPixels(id, pixels, rect, rect, offset);
  resource_provider->DeleteResource(id);
  Mock::VerifyAndClearExpectations(context);

  // Same for setPixelsFromBuffer
  EXPECT_CALL(*context, createTexture()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, deleteTexture(texture_id)).Times(1);
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(3);
  EXPECT_CALL(*context, texImage2D(_, _, _, 2, 2, _, _, _, _)).Times(1);
  EXPECT_CALL(*context, texSubImage2D(_, _, _, _, 2, 2, _, _, _)).Times(1);
  id = resource_provider->CreateResource(
      size, format, ResourceProvider::TextureUsageAny);
  resource_provider->AcquirePixelBuffer(id);
  resource_provider->SetPixelsFromBuffer(id);
  resource_provider->ReleasePixelBuffer(id);
  resource_provider->DeleteResource(id);
  Mock::VerifyAndClearExpectations(context);

  // Same for async version.
  EXPECT_CALL(*context, createTexture()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, deleteTexture(texture_id)).Times(1);
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(2);
  EXPECT_CALL(*context, asyncTexImage2DCHROMIUM(_, _, _, 2, 2, _, _, _, _))
      .Times(1);
  id = resource_provider->CreateResource(
      size, format, ResourceProvider::TextureUsageAny);
  resource_provider->AcquirePixelBuffer(id);
  resource_provider->BeginSetPixels(id);
  resource_provider->ReleasePixelBuffer(id);
  resource_provider->DeleteResource(id);
  Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, ForcingAsyncUploadToComplete) {
  // Only for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  scoped_ptr<WebKit::WebGraphicsContext3D> mock_context(
      static_cast<WebKit::WebGraphicsContext3D*>(
          new NiceMock<AllocationTrackingContext3D>));
  scoped_ptr<OutputSurface> outputSurface(
      FakeOutputSurface::Create3d(mock_context.Pass()));

  gfx::Size size(2, 2);
  WGC3Denum format = GL_RGBA;
  ResourceProvider::ResourceId id = 0;
  int texture_id = 123;

  AllocationTrackingContext3D* context =
      static_cast<AllocationTrackingContext3D*>(outputSurface->context3d());
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(outputSurface.get()));

  EXPECT_CALL(*context, createTexture()).WillOnce(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(3);
  EXPECT_CALL(*context, asyncTexImage2DCHROMIUM(_, _, _, 2, 2, _, _, _, _))
      .Times(1);
  EXPECT_CALL(*context, waitAsyncTexImage2DCHROMIUM(GL_TEXTURE_2D)).Times(1);
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, 0)).Times(1);
  id = resource_provider->CreateResource(
      size, format, ResourceProvider::TextureUsageAny);
  resource_provider->AcquirePixelBuffer(id);
  resource_provider->BeginSetPixels(id);
  resource_provider->ForceSetPixelsToComplete(id);
  resource_provider->ReleasePixelBuffer(id);
  Mock::VerifyAndClearExpectations(context);
}

TEST_P(ResourceProviderTest, AbortForcedAsyncUpload) {
  // Only for GL textures.
  if (GetParam() != ResourceProvider::GLTexture)
    return;
  scoped_ptr<WebKit::WebGraphicsContext3D> mock_context(
      static_cast<WebKit::WebGraphicsContext3D*>(
          new NiceMock<AllocationTrackingContext3D>));
  scoped_ptr<OutputSurface> outputSurface(
      FakeOutputSurface::Create3d(mock_context.Pass()));

  gfx::Size size(2, 2);
  WGC3Denum format = GL_RGBA;
  ResourceProvider::ResourceId id = 0;
  int texture_id = 123;

  AllocationTrackingContext3D* context =
      static_cast<AllocationTrackingContext3D*>(outputSurface->context3d());
  scoped_ptr<ResourceProvider> resource_provider(
      ResourceProvider::Create(outputSurface.get()));

  EXPECT_CALL(*context, createTexture()).WillRepeatedly(Return(texture_id));
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, texture_id)).Times(4);
  EXPECT_CALL(*context, asyncTexImage2DCHROMIUM(_, _, _, 2, 2, _, _, _, _))
      .Times(1);
  EXPECT_CALL(*context, waitAsyncTexImage2DCHROMIUM(GL_TEXTURE_2D)).Times(1);
  EXPECT_CALL(*context, bindTexture(GL_TEXTURE_2D, 0)).Times(1);
  EXPECT_CALL(*context, deleteTexture(_)).Times(1);
  id = resource_provider->CreateResource(
      size, format, ResourceProvider::TextureUsageAny);
  resource_provider->AcquirePixelBuffer(id);
  resource_provider->BeginSetPixels(id);
  resource_provider->ForceSetPixelsToComplete(id);
  resource_provider->AbortSetPixels(id);
  resource_provider->ReleasePixelBuffer(id);
  Mock::VerifyAndClearExpectations(context);
}

INSTANTIATE_TEST_CASE_P(
    ResourceProviderTests,
    ResourceProviderTest,
    ::testing::Values(ResourceProvider::GLTexture, ResourceProvider::Bitmap));

}  // namespace
}  // namespace cc
