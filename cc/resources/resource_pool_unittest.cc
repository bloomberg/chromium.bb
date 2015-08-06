// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_pool.h"

#include "cc/resources/resource_util.h"
#include "cc/resources/scoped_resource.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_output_surface_client.h"
#include "cc/test/fake_resource_provider.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class ResourcePoolTest : public testing::Test {
 public:
  void SetUp() override {
    output_surface_ = FakeOutputSurface::Create3d();
    ASSERT_TRUE(output_surface_->BindToClient(&output_surface_client_));
    shared_bitmap_manager_.reset(new TestSharedBitmapManager());
    resource_provider_ = FakeResourceProvider::Create(
        output_surface_.get(), shared_bitmap_manager_.get());
    resource_pool_ =
        ResourcePool::Create(resource_provider_.get(), GL_TEXTURE_2D);
  }

 protected:
  FakeOutputSurfaceClient output_surface_client_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<SharedBitmapManager> shared_bitmap_manager_;
  scoped_ptr<ResourceProvider> resource_provider_;
  scoped_ptr<ResourcePool> resource_pool_;
};

TEST_F(ResourcePoolTest, AcquireRelease) {
  gfx::Size size(100, 100);
  ResourceFormat format = RGBA_8888;
  scoped_ptr<ScopedResource> resource =
      resource_pool_->AcquireResource(size, format);
  EXPECT_EQ(size, resource->size());
  EXPECT_EQ(format, resource->format());
  EXPECT_TRUE(resource_provider_->CanLockForWrite(resource->id()));

  resource_pool_->ReleaseResource(resource.Pass(), 0u);
}

TEST_F(ResourcePoolTest, AccountingSingleResource) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, bytes_limit, count_limit);

  gfx::Size size(100, 100);
  ResourceFormat format = RGBA_8888;
  size_t resource_bytes =
      ResourceUtil::UncheckedSizeInBytes<size_t>(size, format);
  scoped_ptr<ScopedResource> resource =
      resource_pool_->AcquireResource(size, format);

  EXPECT_EQ(resource_bytes, resource_pool_->total_memory_usage_bytes());
  EXPECT_EQ(resource_bytes, resource_pool_->acquired_memory_usage_bytes());
  EXPECT_EQ(1u, resource_pool_->total_resource_count());
  EXPECT_EQ(1u, resource_pool_->acquired_resource_count());
  EXPECT_EQ(0u, resource_pool_->busy_resource_count());

  resource_pool_->ReleaseResource(resource.Pass(), 0u);
  EXPECT_EQ(resource_bytes, resource_pool_->total_memory_usage_bytes());
  EXPECT_EQ(1u, resource_pool_->total_resource_count());
  EXPECT_EQ(1u, resource_pool_->busy_resource_count());

  resource_pool_->CheckBusyResources();
  EXPECT_EQ(resource_bytes, resource_pool_->total_memory_usage_bytes());
  EXPECT_EQ(0u, resource_pool_->acquired_memory_usage_bytes());
  EXPECT_EQ(1u, resource_pool_->total_resource_count());
  EXPECT_EQ(0u, resource_pool_->acquired_resource_count());
  EXPECT_EQ(0u, resource_pool_->busy_resource_count());

  resource_pool_->SetResourceUsageLimits(0u, 0u, 0u);
  resource_pool_->ReduceResourceUsage();
  EXPECT_EQ(0u, resource_pool_->total_memory_usage_bytes());
  EXPECT_EQ(0u, resource_pool_->acquired_memory_usage_bytes());
  EXPECT_EQ(0u, resource_pool_->total_resource_count());
  EXPECT_EQ(0u, resource_pool_->acquired_resource_count());
  EXPECT_EQ(0u, resource_pool_->busy_resource_count());
}

TEST_F(ResourcePoolTest, SimpleResourceReuse) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, bytes_limit, count_limit);

  gfx::Size size(100, 100);
  ResourceFormat format = RGBA_8888;

  scoped_ptr<ScopedResource> resource =
      resource_pool_->AcquireResource(size, format);
  resource_pool_->ReleaseResource(resource.Pass(), 0u);
  resource_pool_->CheckBusyResources();
  EXPECT_EQ(1u, resource_provider_->num_resources());

  // Same size/format should re-use resource.
  resource = resource_pool_->AcquireResource(size, format);
  EXPECT_EQ(1u, resource_provider_->num_resources());
  resource_pool_->ReleaseResource(resource.Pass(), 0u);
  resource_pool_->CheckBusyResources();
  EXPECT_EQ(1u, resource_provider_->num_resources());

  // Different size/format should alloate new resource.
  resource = resource_pool_->AcquireResource(gfx::Size(50, 50), LUMINANCE_8);
  EXPECT_EQ(2u, resource_provider_->num_resources());
  resource_pool_->ReleaseResource(resource.Pass(), 0u);
  resource_pool_->CheckBusyResources();
  EXPECT_EQ(2u, resource_provider_->num_resources());
}

TEST_F(ResourcePoolTest, LostResource) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, bytes_limit, count_limit);

  gfx::Size size(100, 100);
  ResourceFormat format = RGBA_8888;

  scoped_ptr<ScopedResource> resource =
      resource_pool_->AcquireResource(size, format);
  EXPECT_EQ(1u, resource_provider_->num_resources());

  resource_provider_->LoseResourceForTesting(resource->id());
  resource_pool_->ReleaseResource(resource.Pass(), 0u);
  resource_pool_->CheckBusyResources();
  EXPECT_EQ(0u, resource_provider_->num_resources());
}

}  // namespace
}  // namespace cc
