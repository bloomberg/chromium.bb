// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCScopedTexture.h"

namespace cc {

CCScopedTexture::CCScopedTexture(CCResourceProvider* resourceProvider)
    : m_resourceProvider(resourceProvider)
{
    DCHECK(m_resourceProvider);
}

CCScopedTexture::~CCScopedTexture()
{
    free();
}

bool CCScopedTexture::allocate(int pool, const IntSize& size, GC3Denum format, CCResourceProvider::TextureUsageHint hint)
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

void CCScopedTexture::free()
{
    if (id()) {
#ifndef NDEBUG
        DCHECK(m_allocateThreadIdentifier == base::PlatformThread::CurrentId());
#endif
        m_resourceProvider->deleteResource(id());
    }
    setId(0);
}

void CCScopedTexture::leak()
{
    setId(0);
}

}
