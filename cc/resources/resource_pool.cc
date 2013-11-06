// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_pool.h"

#include "cc/resources/resource_provider.h"

namespace cc {

ResourcePool::Resource::Resource(cc::ResourceProvider* resource_provider,
                                 gfx::Size size,
                                 ResourceFormat format)
    : cc::Resource(resource_provider->CreateManagedResource(
                       size,
                       GL_CLAMP_TO_EDGE,
                       ResourceProvider::TextureUsageAny,
                       format),
                   size,
                   format),
      resource_provider_(resource_provider) {
  DCHECK(id());
}

ResourcePool::Resource::~Resource() {
  DCHECK(id());
  DCHECK(resource_provider_);
  resource_provider_->DeleteResource(id());
}

ResourcePool::ResourcePool(ResourceProvider* resource_provider)
    : resource_provider_(resource_provider),
      max_memory_usage_bytes_(0),
      max_unused_memory_usage_bytes_(0),
      max_resource_count_(0),
      memory_usage_bytes_(0),
      unused_memory_usage_bytes_(0),
      resource_count_(0) {
}

ResourcePool::~ResourcePool() {
  SetResourceUsageLimits(0, 0, 0);
}

scoped_ptr<ResourcePool::Resource> ResourcePool::AcquireResource(
    gfx::Size size, ResourceFormat format) {
  for (ResourceList::iterator it = unused_resources_.begin();
       it != unused_resources_.end(); ++it) {
    Resource* resource = *it;
    DCHECK(resource_provider_->CanLockForWrite(resource->id()));

    if (resource->size() != size)
      continue;
    if (resource->format() != format)
      continue;

    unused_resources_.erase(it);
    unused_memory_usage_bytes_ -= resource->bytes();
    return make_scoped_ptr(resource);
  }

  // Create new resource.
  Resource* resource = new Resource(resource_provider_, size, format);

  // Extend all read locks on all resources until the resource is
  // finished being used, such that we know when resources are
  // truly safe to recycle.
  resource_provider_->EnableReadLockFences(resource->id(), true);

  memory_usage_bytes_ += resource->bytes();
  ++resource_count_;
  return make_scoped_ptr(resource);
}

void ResourcePool::ReleaseResource(
    scoped_ptr<ResourcePool::Resource> resource) {
  busy_resources_.push_back(resource.release());
}

void ResourcePool::SetResourceUsageLimits(
    size_t max_memory_usage_bytes,
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
    Resource* resource = unused_resources_.front();
    unused_resources_.pop_front();
    memory_usage_bytes_ -= resource->bytes();
    unused_memory_usage_bytes_ -= resource->bytes();
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

void ResourcePool::CheckBusyResources() {
  ResourceList::iterator it = busy_resources_.begin();

  while (it != busy_resources_.end()) {
    Resource* resource = *it;

    if (resource_provider_->CanLockForWrite(resource->id())) {
      unused_memory_usage_bytes_ += resource->bytes();
      unused_resources_.push_back(resource);
      it = busy_resources_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace cc
