// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_GEOMETRY_BINDING_H_
#define CC_GEOMETRY_BINDING_H_

namespace gfx {
class RectF;
}

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {

class GeometryBinding {
public:
    GeometryBinding(WebKit::WebGraphicsContext3D*, const gfx::RectF& quadVertexRect);
    ~GeometryBinding();

    bool initialized() const { return m_initialized; }

    WebKit::WebGraphicsContext3D* context() const { return m_context; }
    unsigned quadVerticesVbo() const { return m_quadVerticesVbo; }
    unsigned quadElementsVbo() const { return m_quadElementsVbo; }
    unsigned quadListVerticesVbo() const { return m_quadListVerticesVbo; }

    void prepareForDraw();

    // All layer shaders share the same attribute locations for the vertex
    // positions and texture coordinates. This allows switching shaders without
    // rebinding attribute arrays.
    static int positionAttribLocation() { return 0; }
    static int texCoordAttribLocation() { return 1; }
    static int triangleIndexAttribLocation() { return 2; }

private:
    WebKit::WebGraphicsContext3D* m_context;
    bool m_initialized;

    unsigned m_quadVerticesVbo;
    unsigned m_quadElementsVbo;
    unsigned m_quadListVerticesVbo;
};

} // namespace cc

#endif // CC_GEOMETRY_BINDING_H_

