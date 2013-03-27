// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_GEOMETRY_BINDING_H_
#define CC_OUTPUT_GEOMETRY_BINDING_H_

#include "base/basictypes.h"

namespace gfx { class RectF; }

namespace WebKit { class WebGraphicsContext3D; }

namespace cc {

class GeometryBinding {
 public:
  GeometryBinding(WebKit::WebGraphicsContext3D* context,
                  const gfx::RectF& quad_vertex_rect);
  ~GeometryBinding();

  void PrepareForDraw();

  // All layer shaders share the same attribute locations for the vertex
  // positions and texture coordinates. This allows switching shaders without
  // rebinding attribute arrays.
  static int PositionAttribLocation() { return 0; }
  static int TexCoordAttribLocation() { return 1; }
  static int TriangleIndexAttribLocation() { return 2; }

 private:
  WebKit::WebGraphicsContext3D* context_;

  unsigned quad_vertices_vbo_;
  unsigned quad_elements_vbo_;

  DISALLOW_COPY_AND_ASSIGN(GeometryBinding);
};

}  // namespace cc

#endif  // CC_OUTPUT_GEOMETRY_BINDING_H_
