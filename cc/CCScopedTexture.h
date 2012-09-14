// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCScopedTexture_h
#define CCScopedTexture_h

#include "CCTexture.h"

#if !ASSERT_DISABLED
#include "base/threading/platform_thread.h"
#endif

namespace cc {

class CCScopedTexture : protected CCTexture {
    WTF_MAKE_NONCOPYABLE(CCScopedTexture);
public:
    static PassOwnPtr<CCScopedTexture> create(CCResourceProvider* resourceProvider) { return adoptPtr(new CCScopedTexture(resourceProvider)); }
    virtual ~CCScopedTexture();

    using CCTexture::id;
    using CCTexture::size;
    using CCTexture::format;
    using CCTexture::bytes;

    bool allocate(int pool, const IntSize&, GC3Denum format, CCResourceProvider::TextureUsageHint);
    void free();
    void leak();

protected:
    explicit CCScopedTexture(CCResourceProvider*);

private:
    CCResourceProvider* m_resourceProvider;

#if !ASSERT_DISABLED
    base::PlatformThreadId m_allocateThreadIdentifier;
#endif
};

}

#endif
