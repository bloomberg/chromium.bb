// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/geometry_binding.h"

#include "cc/gl_renderer.h" // For the GLC() macro.
#include "third_party/khronos/GLES2/gl2.h"
#include <public/WebGraphicsContext3D.h>

namespace cc {

GeometryBinding::GeometryBinding(WebKit::WebGraphicsContext3D* context, const FloatRect& quadVertexRect)
    : m_context(context)
    , m_quadVerticesVbo(0)
    , m_quadElementsVbo(0)
    , m_initialized(false)
{
    // Vertex positions and texture coordinates for the 4 corners of a 1x1 quad.
    float vertices[] = { quadVertexRect.x(), quadVertexRect.maxY(), 0.0f, 0.0f,  1.0f,
                         quadVertexRect.x(), quadVertexRect.y(), 0.0f, 0.0f,  0.0f,
                         quadVertexRect.maxX(), quadVertexRect.y(), 0.0f, 1.0f,  0.0f,
                         quadVertexRect.maxX(),  quadVertexRect.maxY(), 0.0f, 1.0f,  1.0f };
    uint16_t indices[] = { 0, 1, 2, 0, 2, 3, // The two triangles that make up the layer quad.
                           0, 1, 2, 3}; // A line path for drawing the layer border.

    GLC(m_context, m_quadVerticesVbo = m_context->createBuffer());
    GLC(m_context, m_quadElementsVbo = m_context->createBuffer());
    GLC(m_context, m_context->bindBuffer(GL_ARRAY_BUFFER, m_quadVerticesVbo));
    GLC(m_context, m_context->bufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW));
    GLC(m_context, m_context->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadElementsVbo));
    GLC(m_context, m_context->bufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW));

    m_initialized = true;
}

GeometryBinding::~GeometryBinding()
{
    GLC(m_context, m_context->deleteBuffer(m_quadVerticesVbo));
    GLC(m_context, m_context->deleteBuffer(m_quadElementsVbo));
}

void GeometryBinding::prepareForDraw()
{
    GLC(m_context, m_context->bindBuffer(GL_ARRAY_BUFFER, quadVerticesVbo()));
    GLC(m_context, m_context->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadElementsVbo()));
    unsigned offset = 0;
    GLC(m_context, m_context->vertexAttribPointer(positionAttribLocation(), 3, GL_FLOAT, false, 5 * sizeof(float), offset));
    offset += 3 * sizeof(float);
    GLC(m_context, m_context->vertexAttribPointer(texCoordAttribLocation(), 2, GL_FLOAT, false, 5 * sizeof(float), offset));
    GLC(m_context, m_context->enableVertexAttribArray(positionAttribLocation()));
    GLC(m_context, m_context->enableVertexAttribArray(texCoordAttribLocation()));
}

} // namespace cc
