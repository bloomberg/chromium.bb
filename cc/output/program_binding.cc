// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/program_binding.h"

#include "base/debug/trace_event.h"
#include "cc/output/geometry_binding.h"
#include "cc/output/gl_renderer.h"  // For the GLC() macro.
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"

using blink::WebGraphicsContext3D;

namespace cc {

ProgramBindingBase::ProgramBindingBase()
    : program_(0),
      vertex_shader_id_(0),
      fragment_shader_id_(0),
      initialized_(false) {}

ProgramBindingBase::~ProgramBindingBase() {
  // If you hit these asserts, you initialized but forgot to call Cleanup().
  DCHECK(!program_);
  DCHECK(!vertex_shader_id_);
  DCHECK(!fragment_shader_id_);
  DCHECK(!initialized_);
}

bool ProgramBindingBase::Init(WebGraphicsContext3D* context,
                              const std::string& vertex_shader,
                              const std::string& fragment_shader) {
  TRACE_EVENT0("cc", "ProgramBindingBase::init");
  vertex_shader_id_ = LoadShader(context, GL_VERTEX_SHADER, vertex_shader);
  if (!vertex_shader_id_)
    return false;

  fragment_shader_id_ =
      LoadShader(context, GL_FRAGMENT_SHADER, fragment_shader);
  if (!fragment_shader_id_) {
    GLC(context, context->deleteShader(vertex_shader_id_));
    vertex_shader_id_ = 0;
    return false;
  }

  program_ =
      CreateShaderProgram(context, vertex_shader_id_, fragment_shader_id_);
  return !!program_;
}

bool ProgramBindingBase::Link(WebGraphicsContext3D* context) {
  GLC(context, context->linkProgram(program_));
  CleanupShaders(context);
  if (!program_)
    return false;
#ifndef NDEBUG
  int linked = 0;
  GLC(context, context->getProgramiv(program_, GL_LINK_STATUS, &linked));
  if (!linked) {
    GLC(context, context->deleteProgram(program_));
    return false;
  }
#endif
  return true;
}

void ProgramBindingBase::Cleanup(WebGraphicsContext3D* context) {
  if (!initialized_)
    return;

  initialized_ = false;
  if (!program_)
    return;

  DCHECK(context);
  GLC(context, context->deleteProgram(program_));
  program_ = 0;

  CleanupShaders(context);
}

unsigned ProgramBindingBase::LoadShader(WebGraphicsContext3D* context,
                                        unsigned type,
                                        const std::string& shader_source) {
  unsigned shader = context->createShader(type);
  if (!shader)
    return 0;
  GLC(context, context->shaderSource(shader, shader_source.data()));
  GLC(context, context->compileShader(shader));
#ifndef NDEBUG
  int compiled = 0;
  GLC(context, context->getShaderiv(shader, GL_COMPILE_STATUS, &compiled));
  if (!compiled) {
    GLC(context, context->deleteShader(shader));
    return 0;
  }
#endif
  return shader;
}

unsigned ProgramBindingBase::CreateShaderProgram(WebGraphicsContext3D* context,
                                                 unsigned vertex_shader,
                                                 unsigned fragment_shader) {
  unsigned program_object = context->createProgram();
  if (!program_object)
    return 0;

  GLC(context, context->attachShader(program_object, vertex_shader));
  GLC(context, context->attachShader(program_object, fragment_shader));

  // Bind the common attrib locations.
  GLC(context,
      context->bindAttribLocation(program_object,
                                  GeometryBinding::PositionAttribLocation(),
                                  "a_position"));
  GLC(context,
      context->bindAttribLocation(program_object,
                                  GeometryBinding::TexCoordAttribLocation(),
                                  "a_texCoord"));
  GLC(context,
      context->bindAttribLocation(
          program_object,
          GeometryBinding::TriangleIndexAttribLocation(),
          "a_index"));

  return program_object;
}

void ProgramBindingBase::CleanupShaders(WebGraphicsContext3D* context) {
  if (vertex_shader_id_) {
    GLC(context, context->deleteShader(vertex_shader_id_));
    vertex_shader_id_ = 0;
  }
  if (fragment_shader_id_) {
    GLC(context, context->deleteShader(fragment_shader_id_));
    fragment_shader_id_ = 0;
  }
}

}  // namespace cc
