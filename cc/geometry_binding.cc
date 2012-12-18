// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/geometry_binding.h"

#include "cc/gl_renderer.h" // For the GLC() macro.
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/rect_f.h"

namespace cc {

GeometryBinding::GeometryBinding(WebKit::WebGraphicsContext3D* context, const gfx::RectF& quadVertexRect)
    : m_context(context)
    , m_quadVerticesVbo(0)
    , m_quadElementsVbo(0)
    , m_initialized(false)
{
    float vertices[] = { quadVertexRect.x(), quadVertexRect.bottom(), 0.0f, 0.0f,  1.0f,
                         quadVertexRect.x(), quadVertexRect.y(), 0.0f, 0.0f,  0.0f,
                         quadVertexRect.right(), quadVertexRect.y(), 0.0f, 1.0f,  0.0f,
                         quadVertexRect.right(), quadVertexRect.bottom(), 0.0f, 1.0f,  1.0f };

    struct Vertex {
        float a_position[3];
        float a_texCoord[2];
        float a_index; // index of the vertex, divide by 4 to have the matrix for this quad
    };
    struct Quad { Vertex v0, v1, v2, v3; };
    struct QuadIndex { uint16_t data[6]; };

    COMPILE_ASSERT(sizeof(Quad) == 24 * sizeof(float), struct_is_densely_packed);
    COMPILE_ASSERT(sizeof(QuadIndex) == 6 * sizeof(uint16_t), struct_is_densely_packed);

    Quad quad_list[8];
    QuadIndex quad_index_list[8];
    for (int i = 0; i < 8; i++) {
        Vertex v0 = { quadVertexRect.x()    , quadVertexRect.bottom(), 0.0f,  0.0f, 1.0f, i * 4.0f + 0.0f };
        Vertex v1 = { quadVertexRect.x()    , quadVertexRect.y()     , 0.0f,  0.0f, 0.0f, i * 4.0f + 1.0f };
        Vertex v2 = { quadVertexRect.right(), quadVertexRect.y()     , 0.0f,  1.0f, 0.0f, i * 4.0f + 2.0f };
        Vertex v3 = { quadVertexRect.right(), quadVertexRect.bottom(), 0.0f,  1.0f, 1.0f, i * 4.0f + 3.0f };
        Quad x = { v0, v1, v2, v3 };
        quad_list[i] = x;
        QuadIndex y = { 0 + 4 * i, 1 + 4 * i, 2 + 4 * i, 3 + 4 * i, 0 + 4 * i, 2 + 4 * i };
        quad_index_list[i] = y;
    }

    GLC(m_context, m_quadVerticesVbo = m_context->createBuffer());
    GLC(m_context, m_quadElementsVbo = m_context->createBuffer());
    GLC(m_context, m_quadListVerticesVbo = m_context->createBuffer());
    GLC(m_context, m_context->bindBuffer(GL_ARRAY_BUFFER, m_quadVerticesVbo));
    GLC(m_context, m_context->bufferData(GL_ARRAY_BUFFER, sizeof(quad_list), quad_list, GL_STATIC_DRAW));
    GLC(m_context, m_context->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadElementsVbo));
    GLC(m_context, m_context->bufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_index_list), quad_index_list, GL_STATIC_DRAW));

    m_initialized = true;
}

GeometryBinding::~GeometryBinding()
{
    GLC(m_context, m_context->deleteBuffer(m_quadVerticesVbo));
    GLC(m_context, m_context->deleteBuffer(m_quadElementsVbo));
}

void GeometryBinding::prepareForDraw()
{
    GLC(m_context, m_context->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadElementsVbo()));

    GLC(m_context, m_context->bindBuffer(GL_ARRAY_BUFFER, quadVerticesVbo()));
    GLC(m_context, m_context->vertexAttribPointer(positionAttribLocation(), 3, GL_FLOAT, false, 6 * sizeof(float), 0));
    GLC(m_context, m_context->vertexAttribPointer(texCoordAttribLocation(), 2, GL_FLOAT, false, 6 * sizeof(float), 3 * sizeof(float)));
    GLC(m_context, m_context->vertexAttribPointer(triangleIndexAttribLocation(), 1, GL_FLOAT, false, 6 * sizeof(float), 5 * sizeof(float)));
    GLC(m_context, m_context->enableVertexAttribArray(positionAttribLocation()));
    GLC(m_context, m_context->enableVertexAttribArray(texCoordAttribLocation()));
    GLC(m_context, m_context->enableVertexAttribArray(triangleIndexAttribLocation()));
}

}  // namespace cc
