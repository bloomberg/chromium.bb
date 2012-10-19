// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCScopedTexture_h
#define CCScopedTexture_h

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "cc/texture.h"

#ifndef NDEBUG
#include "base/threading/platform_thread.h"
#endif

namespace cc {

class CCScopedTexture : protected CCTexture {
public:
    static scoped_ptr<CCScopedTexture> create(CCResourceProvider* resourceProvider) { return make_scoped_ptr(new CCScopedTexture(resourceProvider)); }
    virtual ~CCScopedTexture();

    using CCTexture::id;
    using CCTexture::size;
    using CCTexture::format;
    using CCTexture::bytes;

    bool allocate(int pool, const IntSize&, GLenum format, CCResourceProvider::TextureUsageHint);
    void free();
    void leak();

protected:
    explicit CCScopedTexture(CCResourceProvider*);

private:
    CCResourceProvider* m_resourceProvider;

#ifndef NDEBUG
    base::PlatformThreadId m_allocateThreadIdentifier;
#endif

    DISALLOW_COPY_AND_ASSIGN(CCScopedTexture);
};

}

#endif
