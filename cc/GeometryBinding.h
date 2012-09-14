// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GeometryBinding_h
#define GeometryBinding_h

#include "FloatRect.h"

#if USE(ACCELERATED_COMPOSITING)

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {

class GeometryBinding {
public:
    GeometryBinding(WebKit::WebGraphicsContext3D*, const FloatRect& quadVertexRect);
    ~GeometryBinding();

    bool initialized() const { return m_initialized; }

    WebKit::WebGraphicsContext3D* context() const { return m_context; }
    unsigned quadVerticesVbo() const { return m_quadVerticesVbo; }
    unsigned quadElementsVbo() const { return m_quadElementsVbo; }

    void prepareForDraw();

    // All layer shaders share the same attribute locations for the vertex
    // positions and texture coordinates. This allows switching shaders without
    // rebinding attribute arrays.
    static int positionAttribLocation() { return 0; }
    static int texCoordAttribLocation() { return 1; }

private:
    WebKit::WebGraphicsContext3D* m_context;
    unsigned m_quadVerticesVbo;
    unsigned m_quadElementsVbo;
    bool m_initialized;
};

} // namespace cc

#endif // USE(ACCELERATED_COMPOSITING)

#endif
