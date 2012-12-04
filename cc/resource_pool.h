// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCE_POOL_H_
#define CC_RESOURCE_POOL_H_

#include <list>

#include "base/memory/scoped_ptr.h"
#include "cc/renderer.h"
#include "cc/resource.h"

namespace cc {
class ResourceProvider;

class ResourcePool {
 public:
  class Resource : public cc::Resource {
   public:
    Resource(ResourceProvider* resource_provider,
             Renderer::ResourcePool pool_id,
             const gfx::Size& size,
             GLenum format);
    ~Resource();

   private:
    ResourceProvider* resource_provider_;

    DISALLOW_COPY_AND_ASSIGN(Resource);
  };

  static scoped_ptr<ResourcePool> Create(ResourceProvider* resource_provider,
                                         Renderer::ResourcePool pool_id) {
    return make_scoped_ptr(new ResourcePool(resource_provider, pool_id));
  }

  virtual ~ResourcePool();

  ResourceProvider* resource_provider() { return resource_provider_; };

  scoped_ptr<ResourcePool::Resource> AcquireResource(
      const gfx::Size&, GLenum format);
  void ReleaseResource(scoped_ptr<ResourcePool::Resource>);

  void SetMaxMemoryUsageBytes(size_t max_memory_usage_bytes);

 protected:
  ResourcePool(ResourceProvider* resource_provider,
               Renderer::ResourcePool pool_id);

 private:
  ResourceProvider* resource_provider_;
  Renderer::ResourcePool pool_id_;
  size_t max_memory_usage_bytes_;
  size_t memory_usage_bytes_;

  typedef std::list<Resource*> ResourceList;
  ResourceList resources_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePool);
};

}  // namespace cc

#endif  // CC_RESOURCE_POOL_H_
