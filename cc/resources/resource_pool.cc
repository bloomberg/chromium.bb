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
ResourcePool::ResourcePool(ResourceProvider* resource_provider)
    : resource_provider_(resource_provider),
      target_(0),
      max_memory_usage_bytes_(0),
      max_unused_memory_usage_bytes_(0),
      max_resource_count_(0),
      memory_usage_bytes_(0),
      unused_memory_usage_bytes_(0),
      resource_count_(0) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, base::ThreadTaskRunnerHandle::Get());
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
  DCHECK_NE(0u, target);
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, base::ThreadTaskRunnerHandle::Get());
}

ResourcePool::~ResourcePool() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  DCHECK_EQ(0u, in_use_resources_.size());

  while (!busy_resources_.empty()) {
    DidFinishUsingResource(busy_resources_.take_front());
  }

  SetResourceUsageLimits(0, 0, 0);
  DCHECK_EQ(0u, unused_resources_.size());
  DCHECK_EQ(0u, memory_usage_bytes_);
  DCHECK_EQ(0u, unused_memory_usage_bytes_);
  DCHECK_EQ(0u, resource_count_);
}

Resource* ResourcePool::AcquireResource(const gfx::Size& size,
                                        ResourceFormat format) {
  for (ResourceDeque::iterator it = unused_resources_.begin();
       it != unused_resources_.end(); ++it) {
    ScopedResource* resource = *it;
    DCHECK(resource_provider_->CanLockForWrite(resource->id()));

    if (resource->format() != format)
      continue;
    if (resource->size() != size)
      continue;

    // Transfer resource to |in_use_resources_|.
    in_use_resources_.set(resource->id(), unused_resources_.take(it));

    unused_memory_usage_bytes_ -=
        ResourceUtil::UncheckedSizeInBytes<size_t>(size, format);
    return resource;
  }

  scoped_ptr<PoolResource> pool_resource =
      PoolResource::Create(resource_provider_);
  GLenum target =
      target_ ? target_ : resource_provider_->GetImageTextureTarget(format);
  pool_resource->AllocateManaged(size, target, format);

  DCHECK(ResourceUtil::VerifySizeInBytes<size_t>(pool_resource->size(),
                                                 pool_resource->format()));
  memory_usage_bytes_ += ResourceUtil::UncheckedSizeInBytes<size_t>(
      pool_resource->size(), pool_resource->format());
  ++resource_count_;

  Resource* resource = pool_resource.get();
  in_use_resources_.set(resource->id(), pool_resource.Pass());
  return resource;
}

Resource* ResourcePool::TryAcquireResourceWithContentId(uint64_t content_id) {
  DCHECK(content_id);

  auto it = std::find_if(unused_resources_.begin(), unused_resources_.end(),
                         [content_id](const PoolResource* pool_resource) {
                           return pool_resource->content_id() == content_id;
                         });
  if (it == unused_resources_.end())
    return nullptr;

  Resource* resource = *it;
  DCHECK(resource_provider_->CanLockForWrite(resource->id()));

  // Transfer resource to |in_use_resources_|.
  in_use_resources_.set(resource->id(), unused_resources_.take(it));

  unused_memory_usage_bytes_ -= ResourceUtil::UncheckedSizeInBytes<size_t>(
      resource->size(), resource->format());
  return resource;
}

void ResourcePool::ReleaseResource(Resource* resource, uint64_t content_id) {
  auto it = in_use_resources_.find(resource->id());
  DCHECK(it != in_use_resources_.end());

  PoolResource* pool_resource = it->second;
  pool_resource->set_content_id(content_id);

  // Transfer resource to |busy_resources_|.
  busy_resources_.push_back(in_use_resources_.take_and_erase(it));
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
    scoped_ptr<PoolResource> resource = unused_resources_.take_front();
    unused_memory_usage_bytes_ -= ResourceUtil::UncheckedSizeInBytes<size_t>(
        resource->size(), resource->format());
    DeleteResource(resource.Pass());
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

void ResourcePool::DeleteResource(scoped_ptr<PoolResource> resource) {
  size_t resource_bytes = ResourceUtil::UncheckedSizeInBytes<size_t>(
      resource->size(), resource->format());
  memory_usage_bytes_ -= resource_bytes;
  --resource_count_;
}

void ResourcePool::CheckBusyResources() {
  for (size_t i = 0; i < busy_resources_.size();) {
    ResourceDeque::iterator it(busy_resources_.begin() + i);
    PoolResource* resource = *it;

    if (resource_provider_->CanLockForWrite(resource->id())) {
      DidFinishUsingResource(busy_resources_.take(it));
    } else if (resource_provider_->IsLost(resource->id())) {
      // Remove lost resources from pool.
      DeleteResource(busy_resources_.take(it));
    } else {
      ++i;
    }
  }
}

void ResourcePool::DidFinishUsingResource(scoped_ptr<PoolResource> resource) {
  unused_memory_usage_bytes_ += ResourceUtil::UncheckedSizeInBytes<size_t>(
      resource->size(), resource->format());
  unused_resources_.push_back(resource.Pass());
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
