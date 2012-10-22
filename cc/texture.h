// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTexture_h
#define CCTexture_h

#include "IntSize.h"
#include "cc/resource_provider.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

class Texture {
public:
    Texture() : m_id(0) { }
    Texture(unsigned id, IntSize size, GLenum format)
        : m_id(id)
        , m_size(size)
        , m_format(format) { }

    ResourceProvider::ResourceId id() const { return m_id; }
    const IntSize& size() const { return m_size; }
    GLenum format() const { return m_format; }

    void setId(ResourceProvider::ResourceId id) { m_id = id; }
    void setDimensions(const IntSize&, GLenum format);

    size_t bytes() const;

    static size_t memorySizeBytes(const IntSize&, GLenum format);

private:
    ResourceProvider::ResourceId m_id;
    IntSize m_size;
    GLenum m_format;
};

}

#endif
