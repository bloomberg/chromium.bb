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
class ScopedResource;

class CC_EXPORT ResourcePool {
 public:
  static scoped_ptr<ResourcePool> Create(ResourceProvider* resource_provider,
                                         GLenum target) {
    return make_scoped_ptr(new ResourcePool(resource_provider, target));
  }

  virtual ~ResourcePool();

  scoped_ptr<ScopedResource> AcquireResource(const gfx::Size& size,
                                             ResourceFormat format);
  void ReleaseResource(scoped_ptr<ScopedResource>);

  void SetResourceUsageLimits(size_t max_memory_usage_bytes,
                              size_t max_unused_memory_usage_bytes,
                              size_t max_resource_count);

  void ReduceResourceUsage();
  // This might block if |wait_if_needed| is true and one of the currently
  // busy resources has a read lock fence that needs to be waited upon before
  // it can be locked for write again.
  void CheckBusyResources(bool wait_if_needed);

  size_t total_memory_usage_bytes() const { return memory_usage_bytes_; }
  size_t acquired_memory_usage_bytes() const {
    return memory_usage_bytes_ - unused_memory_usage_bytes_;
  }
  size_t total_resource_count() const { return resource_count_; }
  size_t acquired_resource_count() const {
    return resource_count_ - unused_resources_.size();
  }
  size_t busy_resource_count() const { return busy_resources_.size(); }

 protected:
  ResourcePool(ResourceProvider* resource_provider, GLenum target);

  bool ResourceUsageTooHigh();

 private:
  void DidFinishUsingResource(ScopedResource* resource);

  ResourceProvider* resource_provider_;
  const GLenum target_;
  size_t max_memory_usage_bytes_;
  size_t max_unused_memory_usage_bytes_;
  size_t max_resource_count_;
  size_t memory_usage_bytes_;
  size_t unused_memory_usage_bytes_;
  size_t resource_count_;

  typedef std::list<ScopedResource*> ResourceList;
  ResourceList unused_resources_;
  ResourceList busy_resources_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePool);
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_POOL_H_
