// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TOOLS_SHADER_BENCH_GPU_PAINTER_H_
#define MEDIA_TOOLS_SHADER_BENCH_GPU_PAINTER_H_

#include "media/tools/shader_bench/painter.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"

// Class that renders video frames to a window via GPU.
class GPUPainter : public Painter {
 public:
  GPUPainter();
  virtual ~GPUPainter();

  // Returns a reference to the GL context.
  gfx::GLSurface* surface() const { return surface_; }
  gfx::GLContext* context() const { return context_; }

  // Sets context for subsequent gl calls in this painter.
  virtual void SetGLContext(gfx::GLSurface* surface, gfx::GLContext* context);

  // Creates shader program into given context, from the vertex and fragment
  // shader source code. Returns the id of the shader program.
  virtual GLuint CreateShaderProgram(const char* vertex_shader_source,
                                     const char* fragment_shader_source);

 private:
  // Loads shader into given context, from the source code of the
  // shader. type refers to the shader type, either GL_VERTEX_SHADER or
  // GL_FRAGMENT_SHADER. Returns id of shader.
  GLuint LoadShader(unsigned type, const char* shader_source);

  // Reference to the gl context.
  gfx::GLSurface* surface_;
  gfx::GLContext* context_;

  DISALLOW_COPY_AND_ASSIGN(GPUPainter);
};

#endif  // MEDIA_TOOLS_SHADER_BENCH_GPU_PAINTER_H_
