// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_pool.h"

#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_resource.h"

namespace cc {

ResourcePool::ResourcePool(ResourceProvider* resource_provider, GLenum target)
    : resource_provider_(resource_provider),
      target_(target),
      max_memory_usage_bytes_(0),
      max_unused_memory_usage_bytes_(0),
      max_resource_count_(0),
      memory_usage_bytes_(0),
      unused_memory_usage_bytes_(0),
      resource_count_(0) {}

ResourcePool::~ResourcePool() {
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
        Resource::UncheckedMemorySizeBytes(size, format);
    return make_scoped_ptr(resource);
  }

  scoped_ptr<ScopedResource> resource =
      ScopedResource::Create(resource_provider_);
  resource->AllocateManaged(size, target_, format);

  DCHECK(Resource::VerifySizeInBytes(resource->size(), resource->format()));
  memory_usage_bytes_ +=
      Resource::UncheckedMemorySizeBytes(resource->size(), resource->format());
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
  unused_memory_usage_bytes_ -=
      Resource::UncheckedMemorySizeBytes(resource->size(), resource->format());
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
    size_t resource_bytes = Resource::UncheckedMemorySizeBytes(
        resource->size(), resource->format());
    memory_usage_bytes_ -= resource_bytes;
    unused_memory_usage_bytes_ -= resource_bytes;
    --resource_count_;
    delete resource;
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

void ResourcePool::CheckBusyResources(bool wait_if_needed) {
  ResourceList::iterator it = busy_resources_.begin();

  while (it != busy_resources_.end()) {
    ScopedResource* resource = it->resource;

    if (wait_if_needed)
      resource_provider_->WaitReadLockIfNeeded(resource->id());

    if (resource_provider_->CanLockForWrite(resource->id())) {
      DidFinishUsingResource(resource, it->content_id);
      it = busy_resources_.erase(it);
    } else {
      ++it;
    }
  }
}

void ResourcePool::DidFinishUsingResource(ScopedResource* resource,
                                          uint64_t content_id) {
  unused_memory_usage_bytes_ +=
      Resource::UncheckedMemorySizeBytes(resource->size(), resource->format());
  unused_resources_.push_back(PoolResource(resource, content_id));
}

}  // namespace cc
