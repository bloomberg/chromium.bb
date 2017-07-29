// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_STATIC_GEOMETRY_BINDING_H_
#define CC_OUTPUT_STATIC_GEOMETRY_BINDING_H_

#include "base/macros.h"
#include "cc/cc_export.h"
#include "cc/output/geometry_binding.h"

using gpu::gles2::GLES2Interface;

namespace cc {

class CC_EXPORT StaticGeometryBinding {
 public:
  StaticGeometryBinding(gpu::gles2::GLES2Interface* gl,
                        const gfx::RectF& quad_vertex_rect);
  ~StaticGeometryBinding();

  void PrepareForDraw();

  enum {
    NUM_QUADS = 9,
  };

 private:
  gpu::gles2::GLES2Interface* gl_;

  GLuint quad_vertices_vbo_;
  GLuint quad_elements_vbo_;

  DISALLOW_COPY_AND_ASSIGN(StaticGeometryBinding);
};

}  // namespace cc

#endif  // CC_OUTPUT_STATIC_GEOMETRY_BINDING_H_
