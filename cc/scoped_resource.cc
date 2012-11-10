// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scoped_resource.h"

namespace cc {

ScopedResource::ScopedResource(ResourceProvider* resourceProvider)
    : m_resourceProvider(resourceProvider)
{
    DCHECK(m_resourceProvider);
}

ScopedResource::~ScopedResource()
{
    free();
}

bool ScopedResource::allocate(int pool, const gfx::Size& size, GLenum format, ResourceProvider::TextureUsageHint hint)
{
    DCHECK(!id());
    DCHECK(!size.IsEmpty());

    setDimensions(size, format);
    setId(m_resourceProvider->createResource(pool, size, format, hint));

#ifndef NDEBUG
    m_allocateThreadIdentifier = base::PlatformThread::CurrentId();
#endif

    return id();
}

void ScopedResource::free()
{
    if (id()) {
#ifndef NDEBUG
        DCHECK(m_allocateThreadIdentifier == base::PlatformThread::CurrentId());
#endif
        m_resourceProvider->deleteResource(id());
    }
    setId(0);
}

void ScopedResource::leak()
{
    setId(0);
}

}  // namespace cc
