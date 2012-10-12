// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTexture_h
#define CCTexture_h

#include "CCResourceProvider.h"
#include "CCTexture.h"
#include "GraphicsContext3D.h"
#include "IntSize.h"

namespace cc {

class CCTexture {
public:
    CCTexture() : m_id(0) { }
    CCTexture(unsigned id, IntSize size, GC3Denum format)
        : m_id(id)
        , m_size(size)
        , m_format(format) { }

    CCResourceProvider::ResourceId id() const { return m_id; }
    const IntSize& size() const { return m_size; }
    GC3Denum format() const { return m_format; }

    void setId(CCResourceProvider::ResourceId id) { m_id = id; }
    void setDimensions(const IntSize&, GC3Denum format);

    size_t bytes() const;

    static size_t memorySizeBytes(const IntSize&, GC3Denum format);

private:
    CCResourceProvider::ResourceId m_id;
    IntSize m_size;
    GC3Denum m_format;
};

}

#endif
