// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/scoped_texture.h"

namespace cc {

ScopedTexture::ScopedTexture(ResourceProvider* resourceProvider)
    : m_resourceProvider(resourceProvider)
{
    DCHECK(m_resourceProvider);
}

ScopedTexture::~ScopedTexture()
{
    free();
}

bool ScopedTexture::allocate(int pool, const IntSize& size, GLenum format, ResourceProvider::TextureUsageHint hint)
{
    DCHECK(!id());
    DCHECK(!size.isEmpty());

    setDimensions(size, format);
    setId(m_resourceProvider->createResource(pool, size, format, hint));

#ifndef NDEBUG
    m_allocateThreadIdentifier = base::PlatformThread::CurrentId();
#endif

    return id();
}

void ScopedTexture::free()
{
    if (id()) {
#ifndef NDEBUG
        DCHECK(m_allocateThreadIdentifier == base::PlatformThread::CurrentId());
#endif
        m_resourceProvider->deleteResource(id());
    }
    setId(0);
}

void ScopedTexture::leak()
{
    setId(0);
}

}
