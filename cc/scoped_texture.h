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

class ScopedTexture : protected Texture {
public:
    static scoped_ptr<ScopedTexture> create(ResourceProvider* resourceProvider) { return make_scoped_ptr(new ScopedTexture(resourceProvider)); }
    virtual ~ScopedTexture();

    using Texture::id;
    using Texture::size;
    using Texture::format;
    using Texture::bytes;

    bool allocate(int pool, const IntSize&, GLenum format, ResourceProvider::TextureUsageHint);
    void free();
    void leak();

protected:
    explicit ScopedTexture(ResourceProvider*);

private:
    ResourceProvider* m_resourceProvider;

#ifndef NDEBUG
    base::PlatformThreadId m_allocateThreadIdentifier;
#endif

    DISALLOW_COPY_AND_ASSIGN(ScopedTexture);
};

}

#endif
