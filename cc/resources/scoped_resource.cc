// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/scoped_resource.h"

#include "cc/resources/layer_tree_resource_provider.h"

namespace cc {

ScopedResource::ScopedResource(LayerTreeResourceProvider* resource_provider)
    : resource_provider_(resource_provider) {
  DCHECK(resource_provider_);
}

ScopedResource::~ScopedResource() {
  Free();
}

void ScopedResource::AllocateSoftware(const gfx::Size& size,
                                      const gfx::ColorSpace& color_space) {
  DCHECK(!id());
  DCHECK(!size.IsEmpty());

  set_dimensions(size, viz::RGBA_8888);
  set_id(resource_provider_->CreateBitmapResource(size, color_space));
  set_color_space(color_space);
  hint_ = viz::ResourceTextureHint::kDefault;

#if DCHECK_IS_ON()
  allocate_thread_id_ = base::PlatformThread::CurrentId();
#endif
}

void ScopedResource::AllocateGpuTexture(const gfx::Size& size,
                                        viz::ResourceTextureHint hint,
                                        viz::ResourceFormat format,
                                        const gfx::ColorSpace& color_space) {
  DCHECK(!id());
  DCHECK(!size.IsEmpty());

  set_dimensions(size, format);
  set_id(resource_provider_->CreateGpuTextureResource(size, hint, format,
                                                      color_space));
  set_color_space(color_space);
  hint_ = hint;

#if DCHECK_IS_ON()
  allocate_thread_id_ = base::PlatformThread::CurrentId();
#endif
}

void ScopedResource::AllocateGpuMemoryBuffer(
    const gfx::Size& size,
    viz::ResourceFormat format,
    gfx::BufferUsage usage,
    const gfx::ColorSpace& color_space) {
  DCHECK(!id());
  DCHECK(!size.IsEmpty());

  set_dimensions(size, format);
  set_id(resource_provider_->CreateGpuMemoryBufferResource(
      size, viz::ResourceTextureHint::kDefault, format, usage, color_space));
  set_color_space(color_space);
  hint_ = viz::ResourceTextureHint::kDefault;

#if DCHECK_IS_ON()
  allocate_thread_id_ = base::PlatformThread::CurrentId();
#endif
}

void ScopedResource::Free() {
  if (id()) {
#if DCHECK_IS_ON()
    DCHECK(allocate_thread_id_ == base::PlatformThread::CurrentId());
#endif
    resource_provider_->DeleteResource(id());
  }
  set_id(0);
}

}  // namespace cc
