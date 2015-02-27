// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/geometry_binding.h"

#include "cc/output/gl_renderer.h"  // For the GLC() macro.
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

void SetupGLContext(gpu::gles2::GLES2Interface* gl,
                    GLuint quad_elements_vbo,
                    GLuint quad_vertices_vbo) {
  GLC(gl, gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_elements_vbo));

  GLC(gl, gl->BindBuffer(GL_ARRAY_BUFFER, quad_vertices_vbo));
  // OpenGL defines the last parameter to VertexAttribPointer as type
  // "const GLvoid*" even though it is actually an offset into the buffer
  // object's data store and not a pointer to the client's address space.
  const void* offsets[3] = {
      0,
      reinterpret_cast<const void*>(3 * sizeof(float)),
      reinterpret_cast<const void*>(5 * sizeof(float)),
  };

  GLC(gl,
      gl->VertexAttribPointer(GeometryBinding::PositionAttribLocation(), 3,
                              GL_FLOAT, false, 6 * sizeof(float), offsets[0]));
  GLC(gl,
      gl->VertexAttribPointer(GeometryBinding::TexCoordAttribLocation(), 2,
                              GL_FLOAT, false, 6 * sizeof(float), offsets[1]));
  GLC(gl,
      gl->VertexAttribPointer(GeometryBinding::TriangleIndexAttribLocation(), 1,
                              GL_FLOAT, false, 6 * sizeof(float), offsets[2]));
  GLC(gl,
      gl->EnableVertexAttribArray(GeometryBinding::PositionAttribLocation()));
  GLC(gl,
      gl->EnableVertexAttribArray(GeometryBinding::TexCoordAttribLocation()));
  GLC(gl, gl->EnableVertexAttribArray(
              GeometryBinding::TriangleIndexAttribLocation()));
}

}  // namespace cc
