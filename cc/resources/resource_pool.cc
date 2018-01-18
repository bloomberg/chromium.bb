// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_pool.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/format_macros.h"
#include "base/memory/memory_coordinator_client_registry.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "cc/base/container_util.h"
#include "cc/resources/layer_tree_resource_provider.h"
#include "cc/resources/resource_util.h"
#include "cc/resources/scoped_resource.h"

using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryDumpLevelOfDetail;

namespace cc {
namespace {
bool ResourceMeetsSizeRequirements(const gfx::Size& requested_size,
                                   const gfx::Size& actual_size,
                                   bool disallow_non_exact_reuse) {
  const float kReuseThreshold = 2.0f;

  if (disallow_non_exact_reuse)
    return requested_size == actual_size;

  // Allocating new resources is expensive, and we'd like to re-use our
  // existing ones within reason. Allow a larger resource to be used for a
  // smaller request.
  if (actual_size.width() < requested_size.width() ||
      actual_size.height() < requested_size.height())
    return false;

  // GetArea will crash on overflow, however all sizes in use are tile sizes.
  // These are capped at LayerTreeResourceProvider::max_texture_size(), and will
  // not overflow.
  float actual_area = actual_size.GetArea();
  float requested_area = requested_size.GetArea();
  // Don't use a resource that is more than |kReuseThreshold| times the
  // requested pixel area, as we want to free unnecessarily large resources.
  if (actual_area / requested_area > kReuseThreshold)
    return false;

  return true;
}

}  // namespace

constexpr base::TimeDelta ResourcePool::kDefaultExpirationDelay;

ResourcePool::ResourcePool(
    LayerTreeResourceProvider* resource_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    gfx::BufferUsage usage,
    const base::TimeDelta& expiration_delay,
    bool disallow_non_exact_reuse)
    : resource_provider_(resource_provider),
      use_gpu_memory_buffers_(true),
      usage_(usage),
      task_runner_(std::move(task_runner)),
      resource_expiration_delay_(expiration_delay),
      disallow_non_exact_reuse_(disallow_non_exact_reuse),
      weak_ptr_factory_(this) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "cc::ResourcePool", task_runner_.get());
  // Register this component with base::MemoryCoordinatorClientRegistry.
  base::MemoryCoordinatorClientRegistry::GetInstance()->Register(this);
}

ResourcePool::ResourcePool(
    LayerTreeResourceProvider* resource_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    viz::ResourceTextureHint hint,
    const base::TimeDelta& expiration_delay,
    bool disallow_non_exact_reuse)
    : resource_provider_(resource_provider),
      use_gpu_resources_(true),
      use_gpu_memory_buffers_(false),
      hint_(hint),
      task_runner_(std::move(task_runner)),
      resource_expiration_delay_(expiration_delay),
      disallow_non_exact_reuse_(disallow_non_exact_reuse),
      weak_ptr_factory_(this) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "cc::ResourcePool", task_runner_.get());
  // Register this component with base::MemoryCoordinatorClientRegistry.
  base::MemoryCoordinatorClientRegistry::GetInstance()->Register(this);
}

ResourcePool::ResourcePool(
    LayerTreeResourceProvider* resource_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TimeDelta& expiration_delay,
    bool disallow_non_exact_reuse)
    : resource_provider_(resource_provider),
      use_gpu_resources_(false),
      use_gpu_memory_buffers_(false),
      hint_(viz::ResourceTextureHint::kDefault),
      task_runner_(std::move(task_runner)),
      resource_expiration_delay_(expiration_delay),
      disallow_non_exact_reuse_(disallow_non_exact_reuse),
      weak_ptr_factory_(this) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "cc::ResourcePool", task_runner_.get());
  // Register this component with base::MemoryCoordinatorClientRegistry.
  base::MemoryCoordinatorClientRegistry::GetInstance()->Register(this);
}

ResourcePool::~ResourcePool() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  // Unregister this component with memory_coordinator::ClientRegistry.
  base::MemoryCoordinatorClientRegistry::GetInstance()->Unregister(this);

  DCHECK_EQ(0u, in_use_resources_.size());

  while (!busy_resources_.empty()) {
    DidFinishUsingResource(PopBack(&busy_resources_));
  }

  SetResourceUsageLimits(0, 0);
  DCHECK_EQ(0u, unused_resources_.size());
  DCHECK_EQ(0u, in_use_memory_usage_bytes_);
  DCHECK_EQ(0u, total_memory_usage_bytes_);
  DCHECK_EQ(0u, total_resource_count_);
}

ResourcePool::PoolResource* ResourcePool::ReuseResource(
    const gfx::Size& size,
    viz::ResourceFormat format,
    const gfx::ColorSpace& color_space) {
  // Finding resources in |unused_resources_| from MRU to LRU direction, touches
  // LRU resources only if needed, which increases possibility of expiring more
  // LRU resources within kResourceExpirationDelayMs.
  for (auto it = unused_resources_.begin(); it != unused_resources_.end();
       ++it) {
    PoolResource* resource = it->get();
    DCHECK(resource_provider_->CanLockForWrite(resource->resource_id()));

    if (resource->format() != format)
      continue;
    if (!ResourceMeetsSizeRequirements(size, resource->size(),
                                       disallow_non_exact_reuse_))
      continue;
    if (resource->color_space() != color_space)
      continue;

    // Transfer resource to |in_use_resources_|.
    in_use_resources_[resource->unique_id()] = std::move(*it);
    unused_resources_.erase(it);
    in_use_memory_usage_bytes_ += ResourceUtil::UncheckedSizeInBytes<size_t>(
        resource->size(), resource->format());
    return resource;
  }
  return nullptr;
}

ResourcePool::PoolResource* ResourcePool::CreateResource(
    const gfx::Size& size,
    viz::ResourceFormat format,
    const gfx::ColorSpace& color_space) {
  DCHECK(ResourceUtil::VerifySizeInBytes<size_t>(size, format));

  viz::ResourceId resource_id;
  if (use_gpu_memory_buffers_) {
    resource_id = resource_provider_->CreateGpuMemoryBufferResource(
        size, viz::ResourceTextureHint::kDefault, format, usage_, color_space);
  } else if (use_gpu_resources_) {
    resource_id = resource_provider_->CreateGpuTextureResource(
        size, hint_, format, color_space);
  } else {
    DCHECK_EQ(format, viz::RGBA_8888);
    resource_id = resource_provider_->CreateBitmapResource(size, color_space);
  }
  auto pool_resource = std::make_unique<PoolResource>(
      next_resource_unique_id_++, size, format, color_space, resource_id);

  total_memory_usage_bytes_ +=
      ResourceUtil::UncheckedSizeInBytes<size_t>(size, format);
  ++total_resource_count_;

  PoolResource* resource = pool_resource.get();
  in_use_resources_[resource->unique_id()] = std::move(pool_resource);
  in_use_memory_usage_bytes_ +=
      ResourceUtil::UncheckedSizeInBytes<size_t>(size, format);

  return resource;
}

ResourcePool::InUsePoolResource ResourcePool::AcquireResource(
    const gfx::Size& size,
    viz::ResourceFormat format,
    const gfx::ColorSpace& color_space) {
  PoolResource* resource = ReuseResource(size, format, color_space);
  if (!resource)
    resource = CreateResource(size, format, color_space);
  return InUsePoolResource(resource,
                           use_gpu_resources_ || use_gpu_memory_buffers_);
}

// Iterate over all three resource lists (unused, in-use, and busy), updating
// the invalidation and content IDs to allow for future partial raster. The
// first unused resource found (if any) will be returned and used for partial
// raster directly.
//
// Note that this may cause us to have multiple resources with the same content
// ID. This is not a correctness risk, as all these resources will have valid
// invalidations can can be used safely. Note that we could improve raster
// performance at the cost of search time if we found the resource with the
// smallest invalidation ID to raster in to.
ResourcePool::InUsePoolResource
ResourcePool::TryAcquireResourceForPartialRaster(
    uint64_t new_content_id,
    const gfx::Rect& new_invalidated_rect,
    uint64_t previous_content_id,
    gfx::Rect* total_invalidated_rect) {
  DCHECK(new_content_id);
  DCHECK(previous_content_id);
  *total_invalidated_rect = gfx::Rect();

  auto iter_resource_to_return = unused_resources_.end();
  int minimum_area = 0;

  // First update all unused resources. While updating, track the resource with
  // the smallest invalidation. That resource will be returned to the caller.
  for (auto it = unused_resources_.begin(); it != unused_resources_.end();
       ++it) {
    PoolResource* resource = it->get();
    if (resource->content_id() == previous_content_id) {
      UpdateResourceContentIdAndInvalidation(resource, new_content_id,
                                             new_invalidated_rect);

      // Return the resource with the smallest invalidation.
      int area = resource->invalidated_rect().size().GetArea();
      if (iter_resource_to_return == unused_resources_.end() ||
          area < minimum_area) {
        iter_resource_to_return = it;
        minimum_area = area;
      }
    }
  }

  // Next, update all busy and in_use resources.
  for (const auto& resource : busy_resources_) {
    if (resource->content_id() == previous_content_id) {
      UpdateResourceContentIdAndInvalidation(resource.get(), new_content_id,
                                             new_invalidated_rect);
    }
  }
  for (const auto& resource_pair : in_use_resources_) {
    PoolResource* resource = resource_pair.second.get();
    if (resource->content_id() == previous_content_id) {
      UpdateResourceContentIdAndInvalidation(resource, new_content_id,
                                             new_invalidated_rect);
    }
  }

  // If we found an unused resource to return earlier, move it to
  // |in_use_resources_| and return it.
  if (iter_resource_to_return != unused_resources_.end()) {
    PoolResource* resource = iter_resource_to_return->get();
    DCHECK(resource_provider_->CanLockForWrite(resource->resource_id()));

    // Transfer resource to |in_use_resources_|.
    in_use_resources_[resource->unique_id()] =
        std::move(*iter_resource_to_return);
    unused_resources_.erase(iter_resource_to_return);
    in_use_memory_usage_bytes_ += ResourceUtil::UncheckedSizeInBytes<size_t>(
        resource->size(), resource->format());
    *total_invalidated_rect = resource->invalidated_rect();

    // Clear the invalidated rect and content ID on the resource being retunred.
    // These will be updated when raster completes successfully.
    resource->set_invalidated_rect(gfx::Rect());
    resource->set_content_id(0);
    return InUsePoolResource(resource,
                             use_gpu_resources_ || use_gpu_memory_buffers_);
  }

  return InUsePoolResource();
}

void ResourcePool::ReleaseResource(InUsePoolResource in_use_resource) {
  PoolResource* pool_resource = in_use_resource.resource_;
  in_use_resource.SetWasFreedByResourcePool();

  // Ensure that the provided resource is valid.
  // TODO(ericrk): Remove this once we've investigated further.
  // crbug.com/598286.
  CHECK(pool_resource);

  auto it = in_use_resources_.find(pool_resource->unique_id());
  if (it == in_use_resources_.end()) {
    // We should never hit this. Do some digging to try to determine the cause.
    // TODO(ericrk): Remove this once we've investigated further.
    // crbug.com/598286.

    // Maybe this is a double free - see if the resource exists in our busy
    // list.
    auto found_busy = std::find_if(
        busy_resources_.begin(), busy_resources_.end(),
        [pool_resource](const std::unique_ptr<PoolResource>& busy_resource) {
          return busy_resource->unique_id() == pool_resource->unique_id();
        });
    CHECK(found_busy == busy_resources_.end());

    // Also check if the resource exists in our unused resources list.
    auto found_unused = std::find_if(
        unused_resources_.begin(), unused_resources_.end(),
        [pool_resource](const std::unique_ptr<PoolResource>& unused_resource) {
          return unused_resource->unique_id() == pool_resource->unique_id();
        });
    CHECK(found_unused == unused_resources_.end());

    // Resource doesn't exist in any of our lists. CHECK.
    CHECK(false);
  }

  // Also ensure that the resource wasn't null in our list.
  // TODO(ericrk): Remove this once we've investigated further.
  // crbug.com/598286.
  CHECK(it->second.get());

  pool_resource->set_last_usage(base::TimeTicks::Now());

  // Transfer resource to |busy_resources_|.
  busy_resources_.push_front(std::move(it->second));
  in_use_resources_.erase(it);
  in_use_memory_usage_bytes_ -= ResourceUtil::UncheckedSizeInBytes<size_t>(
      pool_resource->size(), pool_resource->format());

  // Now that we have evictable resources, schedule an eviction call for this
  // resource if necessary.
  ScheduleEvictExpiredResourcesIn(resource_expiration_delay_);
}

void ResourcePool::OnContentReplaced(
    const ResourcePool::InUsePoolResource& in_use_resource,
    uint64_t content_id) {
  PoolResource* resource = in_use_resource.resource_;
  DCHECK(resource);
  resource->set_content_id(content_id);
  resource->set_invalidated_rect(gfx::Rect());
}

void ResourcePool::SetResourceUsageLimits(size_t max_memory_usage_bytes,
                                          size_t max_resource_count) {
  max_memory_usage_bytes_ = max_memory_usage_bytes;
  max_resource_count_ = max_resource_count;

  ReduceResourceUsage();
}

void ResourcePool::ReduceResourceUsage() {
  while (!unused_resources_.empty()) {
    if (!ResourceUsageTooHigh())
      break;

    // LRU eviction pattern. Most recently used might be blocked by
    // a read lock fence but it's still better to evict the least
    // recently used as it prevents a resource that is hard to reuse
    // because of unique size from being kept around. Resources that
    // can't be locked for write might also not be truly free-able.
    // We can free the resource here but it doesn't mean that the
    // memory is necessarily returned to the OS.
    DeleteResource(PopBack(&unused_resources_));
  }
}

bool ResourcePool::ResourceUsageTooHigh() {
  if (total_resource_count_ > max_resource_count_)
    return true;
  if (total_memory_usage_bytes_ > max_memory_usage_bytes_)
    return true;
  return false;
}

void ResourcePool::DeleteResource(std::unique_ptr<PoolResource> resource) {
  size_t resource_bytes = ResourceUtil::UncheckedSizeInBytes<size_t>(
      resource->size(), resource->format());
  total_memory_usage_bytes_ -= resource_bytes;
  --total_resource_count_;
  resource_provider_->DeleteResource(resource->resource_id());
}

void ResourcePool::UpdateResourceContentIdAndInvalidation(
    PoolResource* resource,
    uint64_t new_content_id,
    const gfx::Rect& new_invalidated_rect) {
  gfx::Rect updated_invalidated_rect = new_invalidated_rect;
  if (!resource->invalidated_rect().IsEmpty())
    updated_invalidated_rect.Union(resource->invalidated_rect());

  resource->set_content_id(new_content_id);
  resource->set_invalidated_rect(updated_invalidated_rect);
}

void ResourcePool::CheckBusyResources() {
  for (auto it = busy_resources_.begin(); it != busy_resources_.end();) {
    PoolResource* resource = it->get();

    if (resource_provider_->CanLockForWrite(resource->resource_id())) {
      DidFinishUsingResource(std::move(*it));
      it = busy_resources_.erase(it);
    } else if (resource_provider_->IsLost(resource->resource_id())) {
      // Remove lost resources from pool.
      DeleteResource(std::move(*it));
      it = busy_resources_.erase(it);
    } else {
      ++it;
    }
  }
}

void ResourcePool::DidFinishUsingResource(
    std::unique_ptr<PoolResource> resource) {
  unused_resources_.push_front(std::move(resource));
}

void ResourcePool::ScheduleEvictExpiredResourcesIn(
    base::TimeDelta time_from_now) {
  if (evict_expired_resources_pending_)
    return;

  evict_expired_resources_pending_ = true;

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ResourcePool::EvictExpiredResources,
                     weak_ptr_factory_.GetWeakPtr()),
      time_from_now);
}

void ResourcePool::EvictExpiredResources() {
  evict_expired_resources_pending_ = false;
  base::TimeTicks current_time = base::TimeTicks::Now();

  EvictResourcesNotUsedSince(current_time - resource_expiration_delay_);

  if (unused_resources_.empty() && busy_resources_.empty()) {
    // If nothing is evictable, we have deleted one (and possibly more)
    // resources without any new activity. Flush to ensure these deletions are
    // processed.
    resource_provider_->FlushPendingDeletions();
    return;
  }

  // If we still have evictable resources, schedule a call to
  // EvictExpiredResources at the time when the LRU buffer expires.
  ScheduleEvictExpiredResourcesIn(GetUsageTimeForLRUResource() +
                                  resource_expiration_delay_ - current_time);
}

void ResourcePool::EvictResourcesNotUsedSince(base::TimeTicks time_limit) {
  while (!unused_resources_.empty()) {
    // |unused_resources_| is not strictly ordered with regards to last_usage,
    // as this may not exactly line up with the time a resource became non-busy.
    // However, this should be roughly ordered, and will only introduce slight
    // delays in freeing expired resources.
    if (unused_resources_.back()->last_usage() > time_limit)
      return;

    DeleteResource(PopBack(&unused_resources_));
  }

  // Also free busy resources older than the delay. With a sufficiently large
  // delay, such as the 1 second used here, any "busy" resources which have
  // expired are not likely to be busy. Additionally, freeing a "busy" resource
  // has no downside other than incorrect accounting.
  while (!busy_resources_.empty()) {
    if (busy_resources_.back()->last_usage() > time_limit)
      return;

    DeleteResource(PopBack(&busy_resources_));
  }
}

base::TimeTicks ResourcePool::GetUsageTimeForLRUResource() const {
  if (!unused_resources_.empty()) {
    return unused_resources_.back()->last_usage();
  }

  // This is only called when we have at least one evictable resource.
  DCHECK(!busy_resources_.empty());
  return busy_resources_.back()->last_usage();
}

bool ResourcePool::OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                                base::trace_event::ProcessMemoryDump* pmd) {
  if (args.level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND) {
    std::string dump_name = base::StringPrintf(
        "cc/tile_memory/provider_%d", resource_provider_->tracing_id());
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes,
                    total_memory_usage_bytes_);
  } else {
    for (const auto& resource : unused_resources_) {
      resource->OnMemoryDump(pmd, resource_provider_, true /* is_free */);
    }
    for (const auto& resource : busy_resources_) {
      resource->OnMemoryDump(pmd, resource_provider_, false /* is_free */);
    }
    for (const auto& entry : in_use_resources_) {
      entry.second->OnMemoryDump(pmd, resource_provider_, false /* is_free */);
    }
  }
  return true;
}

void ResourcePool::OnPurgeMemory() {
  // Release all resources, regardless of how recently they were used.
  EvictResourcesNotUsedSince(base::TimeTicks() + base::TimeDelta::Max());
}

ResourcePool::PoolResource::PoolResource(size_t unique_id,
                                         const gfx::Size& size,
                                         viz::ResourceFormat format,
                                         const gfx::ColorSpace& color_space,
                                         viz::ResourceId resource_id)
    : unique_id_(unique_id),
      size_(size),
      format_(format),
      color_space_(color_space),
      resource_id_(resource_id) {}

ResourcePool::PoolResource::~PoolResource() = default;

void ResourcePool::PoolResource::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    const LayerTreeResourceProvider* resource_provider,
    bool is_free) const {
  // Resource IDs are not process-unique, so log with the
  // LayerTreeResourceProvider's unique id.
  std::string parent_node =
      base::StringPrintf("cc/resource_memory/provider_%d/resource_%d",
                         resource_provider->tracing_id(), resource_id_);

  std::string dump_name =
      base::StringPrintf("cc/tile_memory/provider_%d/resource_%d",
                         resource_provider->tracing_id(), resource_id_);
  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
  pmd->AddSuballocation(dump->guid(), parent_node);

  uint64_t total_bytes =
      ResourceUtil::UncheckedSizeInBytesAligned<size_t>(size_, format_);
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes, total_bytes);

  if (is_free) {
    dump->AddScalar("free_size", MemoryAllocatorDump::kUnitsBytes, total_bytes);
  }
}

}  // namespace cc
