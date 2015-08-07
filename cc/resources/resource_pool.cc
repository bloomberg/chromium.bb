// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_pool.h"

#include <algorithm>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/resource_util.h"
#include "cc/resources/scoped_resource.h"

namespace cc {

void ResourcePool::PoolResource::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    bool is_free) const {
  // TODO(ericrk): Add per-compositor ID in name.
  std::string parent_node =
      base::StringPrintf("cc/resource_memory/resource_%d", resource->id());
  std::string dump_name =
      base::StringPrintf("cc/tile_memory/resource_%d", resource->id());
  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name);

  pmd->AddSuballocation(dump->guid(), parent_node);

  uint64_t total_bytes = ResourceUtil::UncheckedSizeInBytesAligned<size_t>(
      resource->size(), resource->format());
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  total_bytes);
  dump->AddScalar("free_size",
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  is_free ? total_bytes : 0);
}

ResourcePool::ResourcePool(ResourceProvider* resource_provider, GLenum target)
    : resource_provider_(resource_provider),
      target_(target),
      max_memory_usage_bytes_(0),
      max_unused_memory_usage_bytes_(0),
      max_resource_count_(0),
      memory_usage_bytes_(0),
      unused_memory_usage_bytes_(0),
      resource_count_(0) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, base::ThreadTaskRunnerHandle::Get());
}

ResourcePool::~ResourcePool() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  while (!busy_resources_.empty()) {
    auto const& front = busy_resources_.front();
    DidFinishUsingResource(front.resource, front.content_id);
    busy_resources_.pop_front();
  }

  SetResourceUsageLimits(0, 0, 0);
  DCHECK_EQ(0u, unused_resources_.size());
  DCHECK_EQ(0u, memory_usage_bytes_);
  DCHECK_EQ(0u, unused_memory_usage_bytes_);
  DCHECK_EQ(0u, resource_count_);
}

scoped_ptr<ScopedResource> ResourcePool::AcquireResource(
    const gfx::Size& size, ResourceFormat format) {
  for (ResourceList::iterator it = unused_resources_.begin();
       it != unused_resources_.end();
       ++it) {
    ScopedResource* resource = it->resource;
    DCHECK(resource_provider_->CanLockForWrite(resource->id()));

    if (resource->format() != format)
      continue;
    if (resource->size() != size)
      continue;

    unused_resources_.erase(it);
    unused_memory_usage_bytes_ -=
        ResourceUtil::UncheckedSizeInBytes<size_t>(size, format);
    return make_scoped_ptr(resource);
  }

  scoped_ptr<ScopedResource> resource =
      ScopedResource::Create(resource_provider_);
  resource->AllocateManaged(size, target_, format);

  DCHECK(ResourceUtil::VerifySizeInBytes<size_t>(resource->size(),
                                                 resource->format()));
  memory_usage_bytes_ += ResourceUtil::UncheckedSizeInBytes<size_t>(
      resource->size(), resource->format());
  ++resource_count_;
  return resource.Pass();
}

scoped_ptr<ScopedResource> ResourcePool::TryAcquireResourceWithContentId(
    uint64_t content_id) {
  DCHECK(content_id);

  auto it = std::find_if(unused_resources_.begin(), unused_resources_.end(),
                         [content_id](const PoolResource& pool_resource) {
                           return pool_resource.content_id == content_id;
                         });
  if (it == unused_resources_.end())
    return nullptr;

  ScopedResource* resource = it->resource;
  DCHECK(resource_provider_->CanLockForWrite(resource->id()));

  unused_resources_.erase(it);
  unused_memory_usage_bytes_ -= ResourceUtil::UncheckedSizeInBytes<size_t>(
      resource->size(), resource->format());
  return make_scoped_ptr(resource);
}

void ResourcePool::ReleaseResource(scoped_ptr<ScopedResource> resource,
                                   uint64_t content_id) {
  busy_resources_.push_back(PoolResource(resource.release(), content_id));
}

void ResourcePool::SetResourceUsageLimits(size_t max_memory_usage_bytes,
                                          size_t max_unused_memory_usage_bytes,
                                          size_t max_resource_count) {
  max_memory_usage_bytes_ = max_memory_usage_bytes;
  max_unused_memory_usage_bytes_ = max_unused_memory_usage_bytes;
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
    ScopedResource* resource = unused_resources_.front().resource;
    unused_resources_.pop_front();
    unused_memory_usage_bytes_ -= ResourceUtil::UncheckedSizeInBytes<size_t>(
        resource->size(), resource->format());
    DeleteResource(resource);
  }
}

bool ResourcePool::ResourceUsageTooHigh() {
  if (resource_count_ > max_resource_count_)
    return true;
  if (memory_usage_bytes_ > max_memory_usage_bytes_)
    return true;
  if (unused_memory_usage_bytes_ > max_unused_memory_usage_bytes_)
    return true;
  return false;
}

void ResourcePool::DeleteResource(ScopedResource* resource) {
  size_t resource_bytes = ResourceUtil::UncheckedSizeInBytes<size_t>(
      resource->size(), resource->format());
  memory_usage_bytes_ -= resource_bytes;
  --resource_count_;
  delete resource;
}

void ResourcePool::CheckBusyResources(bool wait_if_needed) {
  ResourceList::iterator it = busy_resources_.begin();

  while (it != busy_resources_.end()) {
    ScopedResource* resource = it->resource;

    if (wait_if_needed)
      resource_provider_->WaitReadLockIfNeeded(resource->id());

    if (resource_provider_->CanLockForWrite(resource->id())) {
      DidFinishUsingResource(resource, it->content_id);
      it = busy_resources_.erase(it);
    } else if (resource_provider_->IsLost(resource->id())) {
      // Remove lost resources from pool.
      DeleteResource(resource);
      it = busy_resources_.erase(it);
    } else {
      ++it;
    }
  }
}

void ResourcePool::DidFinishUsingResource(ScopedResource* resource,
                                          uint64_t content_id) {
  unused_memory_usage_bytes_ += ResourceUtil::UncheckedSizeInBytes<size_t>(
      resource->size(), resource->format());
  unused_resources_.push_back(PoolResource(resource, content_id));
}

bool ResourcePool::OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                                base::trace_event::ProcessMemoryDump* pmd) {
  for (const auto& resource : unused_resources_) {
    resource.OnMemoryDump(pmd, true /* is_free */);
  }
  for (const auto& resource : busy_resources_) {
    resource.OnMemoryDump(pmd, false /* is_free */);
  }
  // TODO(ericrk): Dump vended out resources once that data is available.
  // crbug.com/516541
  return true;
}

}  // namespace cc
