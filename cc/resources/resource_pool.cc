// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_pool.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "cc/base/container_util.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/resource_util.h"
#include "cc/resources/scoped_resource.h"

namespace cc {
namespace {

// Delay before a resource is considered expired.
const int kResourceExpirationDelayMs = 1000;

}  // namespace

void ResourcePool::PoolResource::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    const ResourceProvider* resource_provider,
    bool is_free) const {
  // Resource IDs are not process-unique, so log with the ResourceProvider's
  // unique id.
  std::string parent_node =
      base::StringPrintf("cc/resource_memory/provider_%d/resource_%d",
                         resource_provider->tracing_id(), id());

  std::string dump_name =
      base::StringPrintf("cc/tile_memory/provider_%d/resource_%d",
                         resource_provider->tracing_id(), id());
  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name);

  pmd->AddSuballocation(dump->guid(), parent_node);

  uint64_t total_bytes =
      ResourceUtil::UncheckedSizeInBytesAligned<size_t>(size(), format());
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  total_bytes);

  if (is_free) {
    dump->AddScalar("free_size",
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    total_bytes);
  }
}

ResourcePool::ResourcePool(ResourceProvider* resource_provider,
                           base::SingleThreadTaskRunner* task_runner,
                           bool use_gpu_memory_buffers)
    : resource_provider_(resource_provider),
      use_gpu_memory_buffers_(use_gpu_memory_buffers),
      max_memory_usage_bytes_(0),
      max_resource_count_(0),
      in_use_memory_usage_bytes_(0),
      total_memory_usage_bytes_(0),
      total_resource_count_(0),
      task_runner_(task_runner),
      evict_expired_resources_pending_(false),
      resource_expiration_delay_(
          base::TimeDelta::FromMilliseconds(kResourceExpirationDelayMs)),
      weak_ptr_factory_(this) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "cc::ResourcePool", task_runner_.get());
}

ResourcePool::~ResourcePool() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

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

Resource* ResourcePool::AcquireResource(const gfx::Size& size,
                                        ResourceFormat format) {
  // Finding resources in |unused_resources_| from MRU to LRU direction, touches
  // LRU resources only if needed, which increases possibility of expiring more
  // LRU resources within kResourceExpirationDelayMs.
  for (ResourceDeque::iterator it = unused_resources_.begin();
       it != unused_resources_.end(); ++it) {
    ScopedResource* resource = it->get();
    DCHECK(resource_provider_->CanLockForWrite(resource->id()));

    if (resource->format() != format)
      continue;
    if (resource->size() != size)
      continue;

    // Transfer resource to |in_use_resources_|.
    in_use_resources_[resource->id()] = std::move(*it);
    unused_resources_.erase(it);
    in_use_memory_usage_bytes_ += ResourceUtil::UncheckedSizeInBytes<size_t>(
        resource->size(), resource->format());
    return resource;
  }

  std::unique_ptr<PoolResource> pool_resource =
      PoolResource::Create(resource_provider_);

  if (use_gpu_memory_buffers_) {
    pool_resource->AllocateWithGpuMemoryBuffer(size, format);
  } else {
    pool_resource->Allocate(size, ResourceProvider::TEXTURE_HINT_IMMUTABLE,
                            format);
  }

  DCHECK(ResourceUtil::VerifySizeInBytes<size_t>(pool_resource->size(),
                                                 pool_resource->format()));
  total_memory_usage_bytes_ += ResourceUtil::UncheckedSizeInBytes<size_t>(
      pool_resource->size(), pool_resource->format());
  ++total_resource_count_;

  // Clear the invalidated rect and content ID, as we are about to raster new
  // content. These will be re-set when rasterization completes successfully.
  pool_resource->set_invalidated_rect(gfx::Rect());
  pool_resource->set_content_id(0);

  Resource* resource = pool_resource.get();
  in_use_resources_[resource->id()] = std::move(pool_resource);
  in_use_memory_usage_bytes_ += ResourceUtil::UncheckedSizeInBytes<size_t>(
      resource->size(), resource->format());

  return resource;
}

Resource* ResourcePool::TryAcquireResourceForPartialRaster(
    uint64_t new_content_id,
    const gfx::Rect& new_invalidated_rect,
    uint64_t previous_content_id,
    gfx::Rect* total_invalidated_rect) {
  DCHECK(new_content_id);
  DCHECK(previous_content_id);
  *total_invalidated_rect = gfx::Rect();

  // Try to find |previous_content_id| in |unused_resources_|.
  auto found_unused = std::find_if(
      unused_resources_.begin(), unused_resources_.end(),
      [previous_content_id](const ResourceDeque::value_type& pool_resource) {
        return pool_resource->content_id() == previous_content_id;
      });
  if (found_unused != unused_resources_.end()) {
    PoolResource* found_resource = found_unused->get();
    DCHECK(resource_provider_->CanLockForWrite(found_resource->id()));

    // Transfer resource to |in_use_resources_|.
    in_use_resources_[found_resource->id()] = std::move(*found_unused);
    unused_resources_.erase(found_unused);
    in_use_memory_usage_bytes_ += ResourceUtil::UncheckedSizeInBytes<size_t>(
        found_resource->size(), found_resource->format());

    *total_invalidated_rect = new_invalidated_rect;
    if (!found_resource->invalidated_rect().IsEmpty())
      total_invalidated_rect->Union(found_resource->invalidated_rect());

    // Clear the invalidated rect and content ID on the resource being retunred.
    // These will be updated when raster completes successfully.
    found_resource->set_invalidated_rect(gfx::Rect());
    found_resource->set_content_id(0);

    return found_resource;
  }

  // Couldn't find an unused previous resource. If possible, update the
  // invalidation on the in-use resource to allow for future partial raster.
  //
  // We could update all in-use resources (rather than just the first one we
  // find), however the latency on resources being returned by the browser
  // is typically one frame, so this extra searching would be largely wasted.
  //
  // Skipping any additional resources is not a correctness risk, as the
  // un-updated resources will have their old content ID, and will not be found
  // by future partial raster searches, which will use the new content ID.
  auto found_in_use = std::find_if(
      in_use_resources_.begin(), in_use_resources_.end(),
      [previous_content_id](const InUseResourceMap::value_type& in_use_entry) {
        return in_use_entry.second->content_id() == previous_content_id;
      });
  if (found_in_use != in_use_resources_.end()) {
    PoolResource* found_resource = found_in_use->second.get();
    gfx::Rect updated_invalidated_rect = new_invalidated_rect;
    if (!found_resource->invalidated_rect().IsEmpty())
      updated_invalidated_rect.Union(found_resource->invalidated_rect());

    found_resource->set_content_id(new_content_id);
    found_resource->set_invalidated_rect(updated_invalidated_rect);
  }

  return nullptr;
}

void ResourcePool::ReleaseResource(Resource* resource) {
  // Ensure that the provided resource is valid.
  // TODO(ericrk): Remove this once we've investigated further.
  // crbug.com/598286.
  CHECK(resource);
  CHECK(resource->id());

  auto it = in_use_resources_.find(resource->id());
  if (it == in_use_resources_.end()) {
    // We should never hit this. Do some digging to try to determine the cause.
    // TODO(ericrk): Remove this once we've investigated further.
    // crbug.com/598286.

    // Maybe this is a double free - see if the resource exists in our busy
    // list.
    auto found_busy = std::find_if(
        busy_resources_.begin(), busy_resources_.end(),
        [resource](const std::unique_ptr<PoolResource>& busy_resource) {
          return busy_resource->id() == resource->id();
        });
    CHECK(found_busy == busy_resources_.end());

    // Also check if the resource exists in our unused resources list.
    auto found_unused = std::find_if(
        unused_resources_.begin(), unused_resources_.end(),
        [resource](const std::unique_ptr<PoolResource>& pool_resource) {
          return pool_resource->id() == resource->id();
        });
    CHECK(found_unused == unused_resources_.end());

    // Resource doesn't exist in any of our lists. CHECK.
    CHECK(false);
  }

  // Also ensure that the resource wasn't null in our list.
  // TODO(ericrk): Remove this once we've investigated further.
  // crbug.com/598286.
  CHECK(it->second.get());

  PoolResource* pool_resource = it->second.get();
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

void ResourcePool::OnContentReplaced(ResourceId resource_id,
                                     uint64_t content_id) {
  auto found = in_use_resources_.find(resource_id);
  DCHECK(found != in_use_resources_.end());
  found->second->set_content_id(content_id);
  found->second->set_invalidated_rect(gfx::Rect());
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
}

void ResourcePool::CheckBusyResources() {
  for (size_t i = 0; i < busy_resources_.size();) {
    ResourceDeque::iterator it(busy_resources_.begin() + i);
    PoolResource* resource = it->get();

    if (resource_provider_->CanLockForWrite(resource->id())) {
      DidFinishUsingResource(std::move(*it));
      busy_resources_.erase(it);
    } else if (resource_provider_->IsLost(resource->id())) {
      // Remove lost resources from pool.
      DeleteResource(std::move(*it));
      busy_resources_.erase(it);
    } else {
      ++i;
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

  task_runner_->PostDelayedTask(FROM_HERE,
                                base::Bind(&ResourcePool::EvictExpiredResources,
                                           weak_ptr_factory_.GetWeakPtr()),
                                time_from_now);
}

void ResourcePool::EvictExpiredResources() {
  evict_expired_resources_pending_ = false;
  base::TimeTicks current_time = base::TimeTicks::Now();

  EvictResourcesNotUsedSince(current_time - resource_expiration_delay_);

  if (unused_resources_.empty() && busy_resources_.empty()) {
    // Nothing is evictable.
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
  for (const auto& resource : unused_resources_) {
    resource->OnMemoryDump(pmd, resource_provider_, true /* is_free */);
  }
  for (const auto& resource : busy_resources_) {
    resource->OnMemoryDump(pmd, resource_provider_, false /* is_free */);
  }
  for (const auto& entry : in_use_resources_) {
    entry.second->OnMemoryDump(pmd, resource_provider_, false /* is_free */);
  }
  return true;
}

}  // namespace cc
