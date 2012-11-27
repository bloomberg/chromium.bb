// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resource_pool.h"

#include "cc/resource_provider.h"

namespace cc {

ResourcePool::ResourcePool(ResourceProvider* resource_provider,
                           Renderer::ResourcePool pool_id)
    : resource_provider_(resource_provider),
      pool_id_(pool_id) {
}

ResourcePool::~ResourcePool() {
}

ResourceProvider::ResourceId ResourcePool::AcquireResource(
    const gfx::Size& size, GLenum format) {
  return resource_provider_->createResource(
      pool_id_, size, format, ResourceProvider::TextureUsageAny);
}

void ResourcePool::ReleaseResource(ResourceProvider::ResourceId resource_id) {
  resource_provider_->deleteResource(resource_id);
}

}  // namespace cc
