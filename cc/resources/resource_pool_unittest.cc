// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_pool.h"

#include <stddef.h>

#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/resources/resource_util.h"
#include "cc/resources/scoped_resource.h"
#include "cc/test/fake_resource_provider.h"
#include "cc/test/test_context_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class ResourcePoolTest : public testing::Test {
 public:
  void SetUp() override {
    context_provider_ = TestContextProvider::Create();
    context_provider_->BindToCurrentThread();
    resource_provider_ = FakeResourceProvider::CreateLayerTreeResourceProvider(
        context_provider_.get(), nullptr);
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
    resource_pool_ = std::make_unique<ResourcePool>(
        resource_provider_.get(), task_runner_,
        viz::ResourceTextureHint::kDefault,
        ResourcePool::kDefaultExpirationDelay, false);
  }

 protected:
  void CheckAndReturnResource(ResourcePool::InUsePoolResource resource) {
    EXPECT_TRUE(!!resource);
    resource_pool_->ReleaseResource(std::move(resource));
    resource_pool_->CheckBusyResources();
  }

  scoped_refptr<TestContextProvider> context_provider_;
  std::unique_ptr<LayerTreeResourceProvider> resource_provider_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<ResourcePool> resource_pool_;
};

TEST_F(ResourcePoolTest, AcquireRelease) {
  gfx::Size size(100, 100);
  viz::ResourceFormat format = viz::RGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  EXPECT_EQ(size, resource.size());
  EXPECT_EQ(format, resource.format());
  EXPECT_TRUE(
      resource_provider_->CanLockForWrite(resource.gpu_backing_resource_id()));

  resource_pool_->ReleaseResource(std::move(resource));
}

TEST_F(ResourcePoolTest, AccountingSingleResource) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, count_limit);

  gfx::Size size(100, 100);
  viz::ResourceFormat format = viz::RGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  size_t resource_bytes =
      ResourceUtil::UncheckedSizeInBytes<size_t>(size, format);
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);

  EXPECT_EQ(resource_bytes, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(resource_bytes, resource_pool_->memory_usage_bytes());
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(1u, resource_pool_->resource_count());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());

  resource_pool_->ReleaseResource(std::move(resource));
  EXPECT_EQ(resource_bytes, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(1u, resource_pool_->GetBusyResourceCountForTesting());

  resource_pool_->CheckBusyResources();
  EXPECT_EQ(resource_bytes, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(0u, resource_pool_->memory_usage_bytes());
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->resource_count());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());

  resource_pool_->SetResourceUsageLimits(0u, 0u);
  resource_pool_->ReduceResourceUsage();
  EXPECT_EQ(0u, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(0u, resource_pool_->memory_usage_bytes());
  EXPECT_EQ(0u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->resource_count());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());
}

TEST_F(ResourcePoolTest, SimpleResourceReuse) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, count_limit);

  gfx::Size size(100, 100);
  viz::ResourceFormat format = viz::RGBA_8888;
  gfx::ColorSpace color_space1;
  gfx::ColorSpace color_space2 = gfx::ColorSpace::CreateSRGB();

  CheckAndReturnResource(
      resource_pool_->AcquireResource(size, format, color_space1));
  EXPECT_EQ(1u, resource_provider_->num_resources());

  // Same size/format should re-use resource.
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space1);
  EXPECT_EQ(1u, resource_provider_->num_resources());
  CheckAndReturnResource(std::move(resource));
  EXPECT_EQ(1u, resource_provider_->num_resources());

  // Different size/format should allocate new resource.
  resource = resource_pool_->AcquireResource(gfx::Size(50, 50),
                                             viz::LUMINANCE_8, color_space1);
  EXPECT_EQ(2u, resource_provider_->num_resources());
  CheckAndReturnResource(std::move(resource));
  EXPECT_EQ(2u, resource_provider_->num_resources());

  // Different color space should allocate new resource.
  resource = resource_pool_->AcquireResource(size, format, color_space2);
  EXPECT_EQ(3u, resource_provider_->num_resources());
  CheckAndReturnResource(std::move(resource));
  EXPECT_EQ(3u, resource_provider_->num_resources());
}

TEST_F(ResourcePoolTest, LostResource) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, count_limit);

  gfx::Size size(100, 100);
  viz::ResourceFormat format = viz::RGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();

  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  EXPECT_EQ(1u, resource_provider_->num_resources());

  resource_provider_->LoseResourceForTesting(
      resource.gpu_backing_resource_id());
  resource_pool_->ReleaseResource(std::move(resource));
  resource_pool_->CheckBusyResources();
  EXPECT_EQ(0u, resource_provider_->num_resources());
}

TEST_F(ResourcePoolTest, BusyResourcesEventuallyFreed) {
  // Set a quick resource expiration delay so that this test doesn't take long
  // to run.
  resource_pool_ = std::make_unique<ResourcePool>(
      resource_provider_.get(), task_runner_,
      viz::ResourceTextureHint::kDefault, base::TimeDelta::FromMilliseconds(10),
      false);

  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, count_limit);

  gfx::Size size(100, 100);
  viz::ResourceFormat format = viz::RGBA_8888;
  gfx::ColorSpace color_space;

  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  EXPECT_EQ(1u, resource_provider_->num_resources());
  EXPECT_EQ(40000u, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(1u, resource_pool_->resource_count());

  resource_pool_->ReleaseResource(std::move(resource));
  EXPECT_EQ(1u, resource_provider_->num_resources());
  EXPECT_EQ(40000u, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(0u, resource_pool_->memory_usage_bytes());
  EXPECT_EQ(1u, resource_pool_->GetBusyResourceCountForTesting());

  // Wait for our resource pool to evict resources. We expect resources to be
  // released within 10 ms, give the thread up to 200.
  base::RunLoop run_loop;
  task_runner_->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                                base::TimeDelta::FromMillisecondsD(200));
  run_loop.Run();

  EXPECT_EQ(0u, resource_provider_->num_resources());
  EXPECT_EQ(0u, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(0u, resource_pool_->memory_usage_bytes());
}

TEST_F(ResourcePoolTest, UnusedResourcesEventuallyFreed) {
  // Set a quick resource expiration delay so that this test doesn't take long
  // to run.
  resource_pool_ = std::make_unique<ResourcePool>(
      resource_provider_.get(), task_runner_,
      viz::ResourceTextureHint::kDefault,
      base::TimeDelta::FromMilliseconds(100), false);

  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, count_limit);

  gfx::Size size(100, 100);
  viz::ResourceFormat format = viz::RGBA_8888;
  gfx::ColorSpace color_space;

  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  EXPECT_EQ(1u, resource_provider_->num_resources());
  EXPECT_EQ(40000u, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(1u, resource_pool_->resource_count());

  resource_pool_->ReleaseResource(std::move(resource));
  EXPECT_EQ(1u, resource_provider_->num_resources());
  EXPECT_EQ(40000u, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(1u, resource_pool_->GetBusyResourceCountForTesting());

  // Transfer the resource from the busy pool to the unused pool.
  resource_pool_->CheckBusyResources();
  EXPECT_EQ(1u, resource_provider_->num_resources());
  EXPECT_EQ(40000u, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->resource_count());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());

  // Wait for our resource pool to evict resources. We expect resources to be
  // released within 100 ms, give the thread up to 200.
  base::RunLoop run_loop;
  task_runner_->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                                base::TimeDelta::FromMillisecondsD(200));
  run_loop.Run();

  EXPECT_EQ(0u, resource_provider_->num_resources());
  EXPECT_EQ(0u, resource_pool_->GetTotalMemoryUsageForTesting());
}

TEST_F(ResourcePoolTest, UpdateContentId) {
  gfx::Size size(100, 100);
  viz::ResourceFormat format = viz::RGBA_8888;
  gfx::ColorSpace color_space;
  uint64_t content_id = 42;
  uint64_t new_content_id = 43;
  gfx::Rect new_invalidated_rect(20, 20, 10, 10);

  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  resource_pool_->OnContentReplaced(resource, content_id);
  auto original_id = resource.gpu_backing_resource_id();
  resource_pool_->ReleaseResource(std::move(resource));
  resource_pool_->CheckBusyResources();

  // Ensure that we can retrieve the resource based on |content_id|.
  gfx::Rect invalidated_rect;
  ResourcePool::InUsePoolResource reacquired_resource =
      resource_pool_->TryAcquireResourceForPartialRaster(
          new_content_id, new_invalidated_rect, content_id, &invalidated_rect);
  EXPECT_EQ(original_id, reacquired_resource.gpu_backing_resource_id());
  EXPECT_EQ(new_invalidated_rect, invalidated_rect);
  resource_pool_->ReleaseResource(std::move(reacquired_resource));
}

TEST_F(ResourcePoolTest, UpdateContentIdAndInvalidatedRect) {
  gfx::Size size(100, 100);
  viz::ResourceFormat format = viz::RGBA_8888;
  gfx::ColorSpace color_space;
  uint64_t content_ids[] = {42, 43, 44};
  gfx::Rect invalidated_rect(20, 20, 10, 10);
  gfx::Rect second_invalidated_rect(25, 25, 10, 10);
  gfx::Rect expected_total_invalidated_rect(20, 20, 15, 15);

  // Acquire a new resource with the first content id.
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  resource_pool_->OnContentReplaced(resource, content_ids[0]);
  auto original_id = resource.gpu_backing_resource_id();

  // Attempt to acquire this resource. It is in use, so its ID and invalidated
  // rect should be updated, but a new resource will be returned.
  gfx::Rect new_invalidated_rect;
  ResourcePool::InUsePoolResource reacquired_resource =
      resource_pool_->TryAcquireResourceForPartialRaster(
          content_ids[1], invalidated_rect, content_ids[0],
          &new_invalidated_rect);
  EXPECT_FALSE(!!reacquired_resource);
  EXPECT_EQ(gfx::Rect(), new_invalidated_rect);

  // Release the original resource, returning it to the unused pool.
  resource_pool_->ReleaseResource(std::move(resource));
  resource_pool_->CheckBusyResources();

  // Ensure that we cannot retrieve a resource based on the original content id.
  reacquired_resource = resource_pool_->TryAcquireResourceForPartialRaster(
      content_ids[1], invalidated_rect, content_ids[0], &new_invalidated_rect);
  EXPECT_FALSE(!!reacquired_resource);
  EXPECT_EQ(gfx::Rect(), new_invalidated_rect);

  // Ensure that we can retrieve the resource based on the second (updated)
  // content ID and that it has the expected invalidated rect.
  gfx::Rect total_invalidated_rect;
  reacquired_resource = resource_pool_->TryAcquireResourceForPartialRaster(
      content_ids[2], second_invalidated_rect, content_ids[1],
      &total_invalidated_rect);
  EXPECT_EQ(original_id, reacquired_resource.gpu_backing_resource_id());
  EXPECT_EQ(expected_total_invalidated_rect, total_invalidated_rect);
  resource_pool_->ReleaseResource(std::move(reacquired_resource));
}

TEST_F(ResourcePoolTest, ReuseResource) {
  viz::ResourceFormat format = viz::RGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();

  // Create unused resource with size 100x100.
  ResourcePool::InUsePoolResource original =
      resource_pool_->AcquireResource(gfx::Size(100, 100), format, color_space);
  auto original_id = original.gpu_backing_resource_id();
  CheckAndReturnResource(std::move(original));

  // Try some cases that are too large, none should succeed.
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(101, 100), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(100, 101), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(90, 120), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(120, 120), format,
                                                   color_space));

  // Try some cases that are more than 2x smaller than 100x100 in area and
  // won't be re-used.
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(49, 100), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(100, 49), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(50, 50), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(70, 70), format,
                                                   color_space));

  // Try some cases that are smaller than 100x100, but within 2x area. Reuse
  // should succeed if non-exact requests are supported. Some platforms never
  // support these.
  if (resource_pool_->AllowsNonExactReUseForTesting()) {
    ResourcePool::InUsePoolResource reused = resource_pool_->AcquireResource(
        gfx::Size(50, 100), format, color_space);
    EXPECT_EQ(original_id, reused.gpu_backing_resource_id());
    CheckAndReturnResource(std::move(reused));
    reused = resource_pool_->AcquireResource(gfx::Size(100, 50), format,
                                             color_space);
    EXPECT_EQ(original_id, reused.gpu_backing_resource_id());
    CheckAndReturnResource(std::move(reused));
    reused =
        resource_pool_->AcquireResource(gfx::Size(71, 71), format, color_space);
    EXPECT_EQ(original_id, reused.gpu_backing_resource_id());
    CheckAndReturnResource(std::move(reused));
  } else {
    EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(50, 100), format,
                                                     color_space));
    EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(100, 50), format,
                                                     color_space));
    EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(71, 71), format,
                                                     color_space));
  }

  // 100x100 is an exact match and should succeed. A subsequent request for
  // the same size should fail (the resource is already in use).
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(gfx::Size(100, 100), format, color_space);
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(100, 100), format,
                                                   color_space));
  CheckAndReturnResource(std::move(resource));
}

TEST_F(ResourcePoolTest, MemoryStateSuspended) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, count_limit);

  gfx::Size size(100, 100);
  viz::ResourceFormat format = viz::RGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);

  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());

  // Purging and suspending should not impact an in-use resource.
  resource_pool_->OnPurgeMemory();
  resource_pool_->OnMemoryStateChange(base::MemoryState::SUSPENDED);
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());
  resource_pool_->OnMemoryStateChange(base::MemoryState::NORMAL);

  // Release the resource making it busy.
  resource_pool_->ReleaseResource(std::move(resource));
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(1u, resource_pool_->GetBusyResourceCountForTesting());

  // Purging and suspending should now free the busy resource.
  resource_pool_->OnPurgeMemory();
  resource_pool_->OnMemoryStateChange(base::MemoryState::SUSPENDED);
  EXPECT_EQ(0u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());
}

TEST_F(ResourcePoolTest, ExactRequestsRespected) {
  viz::ResourceFormat format = viz::RGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();

  resource_pool_ = std::make_unique<ResourcePool>(
      resource_provider_.get(), task_runner_,
      viz::ResourceTextureHint::kDefault,
      base::TimeDelta::FromMilliseconds(100), true);

  // Create unused resource with size 100x100.
  CheckAndReturnResource(resource_pool_->AcquireResource(gfx::Size(100, 100),
                                                         format, color_space));

  // Try some cases that are smaller than 100x100, but within 2x area which
  // would typically allow reuse. Reuse should fail.
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(50, 100), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(100, 50), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(71, 71), format,
                                                   color_space));

  // 100x100 is an exact match and should succeed. A subsequent request for
  // the same size should fail (the resource is already in use).
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(gfx::Size(100, 100), format, color_space);
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(100, 100), format,
                                                   color_space));
  CheckAndReturnResource(std::move(resource));
}

}  // namespace cc
