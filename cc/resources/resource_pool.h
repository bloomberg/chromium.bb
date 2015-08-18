// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_POOL_H_
#define CC_RESOURCES_RESOURCE_POOL_H_

#include <deque>

#include "base/memory/scoped_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/base/cc_export.h"
#include "cc/output/renderer.h"
#include "cc/resources/resource.h"
#include "cc/resources/resource_format.h"

namespace cc {
class ScopedResource;

class CC_EXPORT ResourcePool : public base::trace_event::MemoryDumpProvider {
 public:
  static scoped_ptr<ResourcePool> Create(ResourceProvider* resource_provider) {
    return make_scoped_ptr(new ResourcePool(resource_provider));
  }

  static scoped_ptr<ResourcePool> Create(ResourceProvider* resource_provider,
                                         GLenum target) {
    return make_scoped_ptr(new ResourcePool(resource_provider, target));
  }

  ~ResourcePool() override;

  scoped_ptr<ScopedResource> AcquireResource(const gfx::Size& size,
                                             ResourceFormat format);
  scoped_ptr<ScopedResource> TryAcquireResourceWithContentId(uint64 content_id);
  void ReleaseResource(scoped_ptr<ScopedResource> resource,
                       uint64_t content_id);

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

  // Overridden from base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 protected:
  explicit ResourcePool(ResourceProvider* resource_provider);
  ResourcePool(ResourceProvider* resource_provider, GLenum target);

  bool ResourceUsageTooHigh();

 private:
  void DidFinishUsingResource(ScopedResource* resource, uint64_t content_id);
  void DeleteResource(ScopedResource* resource);

  ResourceProvider* resource_provider_;
  const GLenum target_;
  size_t max_memory_usage_bytes_;
  size_t max_unused_memory_usage_bytes_;
  size_t max_resource_count_;
  size_t memory_usage_bytes_;
  size_t unused_memory_usage_bytes_;
  size_t resource_count_;

  struct PoolResource {
    PoolResource(ScopedResource* resource, uint64_t content_id)
        : resource(resource), content_id(content_id) {}
    void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                      const ResourceProvider* resource_provider,
                      bool is_free) const;

    ScopedResource* resource;
    uint64_t content_id;
  };
  typedef std::deque<PoolResource> ResourceList;
  ResourceList unused_resources_;
  ResourceList busy_resources_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePool);
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_POOL_H_
