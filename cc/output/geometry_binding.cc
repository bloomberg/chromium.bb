// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/geometry_binding.h"

#include "cc/output/gl_renderer.h"  // For the GLC() macro.
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/rect_f.h"

namespace cc {

GeometryBinding::GeometryBinding(WebKit::WebGraphicsContext3D* context,
                                 const gfx::RectF& quad_vertex_rect)
    : context_(context),
      quad_vertices_vbo_(0),
      quad_elements_vbo_(0) {
  float vertices[] = {
      quad_vertex_rect.x(), quad_vertex_rect.bottom(), 0.0f, 0.0f,
      1.0f, quad_vertex_rect.x(), quad_vertex_rect.y(), 0.0f,
      0.0f, 0.0f, quad_vertex_rect.right(), quad_vertex_rect.y(),
      0.0f, 1.0f, 0.0f, quad_vertex_rect.right(),
      quad_vertex_rect.bottom(), 0.0f, 1.0f, 1.0f
  };

  struct Vertex {
    float a_position[3];
    float a_texCoord[2];
    // Index of the vertex, divide by 4 to have the matrix for this quad.
    float a_index;
  };
  struct Quad {
    Vertex v0, v1, v2, v3;
  };
  struct QuadIndex {
    uint16_t data[6];
  };

  COMPILE_ASSERT(
      sizeof(Quad) == 24 * sizeof(float),  // NOLINT(runtime/sizeof)
      struct_is_densely_packed);
  COMPILE_ASSERT(
      sizeof(QuadIndex) == 6 * sizeof(uint16_t),  // NOLINT(runtime/sizeof)
      struct_is_densely_packed);

  Quad quad_list[8];
  QuadIndex quad_index_list[8];
  for (int i = 0; i < 8; i++) {
    Vertex v0 = { quad_vertex_rect.x(), quad_vertex_rect.bottom(), 0.0f, 0.0f,
                  1.0f, i * 4.0f + 0.0f };
    Vertex v1 = { quad_vertex_rect.x(), quad_vertex_rect.y(), 0.0f, 0.0f, 0.0f,
                  i * 4.0f + 1.0f };
    Vertex v2 = { quad_vertex_rect.right(), quad_vertex_rect.y(), 0.0f, 1.0f,
                  0.0f, i * 4.0f + 2.0f };
    Vertex v3 = { quad_vertex_rect.right(), quad_vertex_rect.bottom(), 0.0f,
                  1.0f, 1.0f, i * 4.0f + 3.0f };
    Quad x = { v0, v1, v2, v3 };
    quad_list[i] = x;
    QuadIndex y = { 0 + 4 * i, 1 + 4 * i, 2 + 4 * i, 3 + 4 * i, 0 + 4 * i,
                    2 + 4 * i };
    quad_index_list[i] = y;
  }

  GLC(context_, quad_vertices_vbo_ = context_->createBuffer());
  GLC(context_, quad_elements_vbo_ = context_->createBuffer());
  GLC(context_, context_->bindBuffer(GL_ARRAY_BUFFER, quad_vertices_vbo_));
  GLC(context_,
      context_->bufferData(
          GL_ARRAY_BUFFER, sizeof(quad_list), quad_list, GL_STATIC_DRAW));
  GLC(context_,
      context_->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_elements_vbo_));
  GLC(context_,
      context_->bufferData(GL_ELEMENT_ARRAY_BUFFER,
                           sizeof(quad_index_list),
                           quad_index_list,
                           GL_STATIC_DRAW));
}

GeometryBinding::~GeometryBinding() {
  GLC(context_, context_->deleteBuffer(quad_vertices_vbo_));
  GLC(context_, context_->deleteBuffer(quad_elements_vbo_));
}

void GeometryBinding::PrepareForDraw() {
  GLC(context_,
      context_->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_elements_vbo_));

  GLC(context_, context_->bindBuffer(GL_ARRAY_BUFFER, quad_vertices_vbo_));
  GLC(context_,
      context_->vertexAttribPointer(
          PositionAttribLocation(),
          3,
          GL_FLOAT,
          false,
          6 * sizeof(float),  // NOLINT(runtime/sizeof)
          0));
  GLC(context_,
      context_->vertexAttribPointer(
          TexCoordAttribLocation(),
          2,
          GL_FLOAT,
          false,
          6 * sizeof(float),  // NOLINT(runtime/sizeof)
          3 * sizeof(float)));  // NOLINT(runtime/sizeof)
  GLC(context_,
      context_->vertexAttribPointer(
          TriangleIndexAttribLocation(),
          1,
          GL_FLOAT,
          false,
          6 * sizeof(float),  // NOLINT(runtime/sizeof)
          5 * sizeof(float)));  // NOLINT(runtime/sizeof)
  GLC(context_, context_->enableVertexAttribArray(PositionAttribLocation()));
  GLC(context_, context_->enableVertexAttribArray(TexCoordAttribLocation()));
  GLC(context_,
      context_->enableVertexAttribArray(TriangleIndexAttribLocation()));
}

}  // namespace cc
