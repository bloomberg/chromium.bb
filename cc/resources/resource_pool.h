// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_POOL_H_
#define CC_RESOURCES_RESOURCE_POOL_H_

#include <list>

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/output/renderer.h"
#include "cc/resources/resource.h"
#include "cc/resources/resource_format.h"

namespace cc {

class CC_EXPORT ResourcePool {
 public:
  class CC_EXPORT Resource : public cc::Resource {
   public:
    Resource(ResourceProvider* resource_provider,
             gfx::Size size,
             ResourceFormat format);
    ~Resource();

   private:
    ResourceProvider* resource_provider_;

    DISALLOW_COPY_AND_ASSIGN(Resource);
  };

  static scoped_ptr<ResourcePool> Create(ResourceProvider* resource_provider) {
    return make_scoped_ptr(new ResourcePool(resource_provider));
  }

  virtual ~ResourcePool();

  scoped_ptr<ResourcePool::Resource> AcquireResource(
      gfx::Size size, ResourceFormat format);
  void ReleaseResource(scoped_ptr<ResourcePool::Resource>);

  void SetResourceUsageLimits(size_t max_memory_usage_bytes,
                              size_t max_unused_memory_usage_bytes,
                              size_t max_resource_count);

  void ReduceResourceUsage();
  void CheckBusyResources();

  size_t total_memory_usage_bytes() const {
    return memory_usage_bytes_;
  }
  size_t acquired_memory_usage_bytes() const {
    return memory_usage_bytes_ - unused_memory_usage_bytes_;
  }
  size_t acquired_resource_count() const {
    return resource_count_ - unused_resources_.size();
  }

 protected:
  explicit ResourcePool(ResourceProvider* resource_provider);

  bool ResourceUsageTooHigh();

 private:
  void DidFinishUsingResource(ResourcePool::Resource* resource);

  ResourceProvider* resource_provider_;
  size_t max_memory_usage_bytes_;
  size_t max_unused_memory_usage_bytes_;
  size_t max_resource_count_;
  size_t memory_usage_bytes_;
  size_t unused_memory_usage_bytes_;
  size_t resource_count_;

  typedef std::list<Resource*> ResourceList;
  ResourceList unused_resources_;
  ResourceList busy_resources_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePool);
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_POOL_H_
