// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/scoped_resource.h"

namespace cc {

ScopedResource::ScopedResource(ResourceProvider* resource_provider)
    : resource_provider_(resource_provider) {
  DCHECK(resource_provider_);
}

ScopedResource::~ScopedResource() {
  Free();
}

bool ScopedResource::Allocate(const gfx::Size& size, GLenum format,
                              ResourceProvider::TextureUsageHint hint) {
  DCHECK(!id());
  DCHECK(!size.IsEmpty());

  set_dimensions(size, format);
  set_id(resource_provider_->CreateResource(size, format, hint));

#ifndef NDEBUG
  allocate_thread_id_ = base::PlatformThread::CurrentId();
#endif

  return id();
}

void ScopedResource::Free() {
  if (id()) {
#ifndef NDEBUG
    DCHECK(allocate_thread_id_ == base::PlatformThread::CurrentId());
#endif
    resource_provider_->DeleteResource(id());
  }
  set_id(0);
}

void ScopedResource::Leak() {
  set_id(0);
}

}  // namespace cc
