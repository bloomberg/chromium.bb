// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTexture_h
#define CCTexture_h

#include "third_party/khronos/GLES2/gl2.h"
#include "CCResourceProvider.h"
#include "CCTexture.h"
#include "IntSize.h"

namespace cc {

class CCTexture {
public:
    CCTexture() : m_id(0) { }
    CCTexture(unsigned id, IntSize size, GLenum format)
        : m_id(id)
        , m_size(size)
        , m_format(format) { }

    CCResourceProvider::ResourceId id() const { return m_id; }
    const IntSize& size() const { return m_size; }
    GLenum format() const { return m_format; }

    void setId(CCResourceProvider::ResourceId id) { m_id = id; }
    void setDimensions(const IntSize&, GLenum format);

    size_t bytes() const;

    static size_t memorySizeBytes(const IntSize&, GLenum format);

private:
    CCResourceProvider::ResourceId m_id;
    IntSize m_size;
    GLenum m_format;
};

}

#endif
