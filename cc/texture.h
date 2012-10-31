// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTexture_h
#define CCTexture_h

#include "cc/resource_provider.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/size.h"

namespace cc {

class Texture {
public:
    Texture() : m_id(0) { }
    Texture(unsigned id, gfx::Size size, GLenum format)
        : m_id(id)
        , m_size(size)
        , m_format(format) { }

    ResourceProvider::ResourceId id() const { return m_id; }
    const gfx::Size& size() const { return m_size; }
    GLenum format() const { return m_format; }

    void setId(ResourceProvider::ResourceId id) { m_id = id; }
    void setDimensions(const gfx::Size&, GLenum format);

    size_t bytes() const;

    static size_t bytesPerPixel(GLenum format);
    static size_t memorySizeBytes(const gfx::Size&, GLenum format);

private:
    ResourceProvider::ResourceId m_id;
    gfx::Size m_size;
    GLenum m_format;
};

}

#endif
