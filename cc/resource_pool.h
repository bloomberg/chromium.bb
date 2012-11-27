// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCE_POOL_H_
#define CC_RESOURCE_POOL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/renderer.h"
#include "cc/resource_provider.h"

namespace cc {

class ResourcePool {
 public:
  static scoped_ptr<ResourcePool> Create(ResourceProvider* resource_provider,
                                         Renderer::ResourcePool pool_id) {
    return make_scoped_ptr(new ResourcePool(resource_provider, pool_id));
  }

  virtual ~ResourcePool();

  ResourceProvider* resource_provider() { return resource_provider_; };

  ResourceProvider::ResourceId AcquireResource(
      const gfx::Size&, GLenum format);
  void ReleaseResource(ResourceProvider::ResourceId);

 protected:
  ResourcePool(ResourceProvider* resource_provider,
               Renderer::ResourcePool pool_id);

 private:
  ResourceProvider* resource_provider_;
  Renderer::ResourcePool pool_id_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePool);
};

}  // namespace cc

#endif  // CC_RESOURCE_POOL_H_
