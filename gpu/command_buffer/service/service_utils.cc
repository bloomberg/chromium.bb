// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_utils.h"

#include "base/command_line.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/gl/gl_switches.h"

#if defined(USE_EGL)
#include "ui/gl/gl_surface_egl.h"
#endif  // defined(USE_EGL)

namespace gpu {
namespace gles2 {

gl::GLContextAttribs GenerateGLContextAttribs(
    const ContextCreationAttribHelper& attribs_helper,
    const ContextGroup* context_group) {
  DCHECK(context_group != nullptr);
  gl::GLContextAttribs attribs;
  attribs.gpu_preference = attribs_helper.gpu_preference;
  if (context_group->use_passthrough_cmd_decoder()) {
    attribs.bind_generates_resource = attribs_helper.bind_generates_resource;
    attribs.webgl_compatibility_context =
        IsWebGLContextType(attribs_helper.context_type);

    // Always use the global texture share group for the passthrough command
    // decoder
    attribs.global_texture_share_group = true;

    attribs.robust_resource_initialization = true;

    // Request a specific context version instead of always 3.0
    if (IsWebGL2OrES3ContextType(attribs_helper.context_type)) {
      attribs.client_major_es_version = 3;
      attribs.client_minor_es_version = 0;
    } else {
      DCHECK(IsWebGL1OrES2ContextType(attribs_helper.context_type));
      attribs.client_major_es_version = 2;
      attribs.client_minor_es_version = 0;
    }
  } else {
    attribs.client_major_es_version = 3;
    attribs.client_minor_es_version = 0;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableES3GLContext)) {
    // Forcefully disable ES3 contexts
    attribs.client_major_es_version = 2;
    attribs.client_minor_es_version = 0;
  }

  return attribs;
}

bool UsePassthroughCommandDecoder(const base::CommandLine* command_line) {
  return command_line->HasSwitch(switches::kUsePassthroughCmdDecoder);
}

bool PassthroughCommandDecoderSupported() {
#if defined(USE_EGL)
  // Using the passthrough command buffer requires that specific ANGLE
  // extensions are exposed
  return gl::GLSurfaceEGL::IsCreateContextBindGeneratesResourceSupported() &&
         gl::GLSurfaceEGL::IsCreateContextWebGLCompatabilitySupported() &&
         gl::GLSurfaceEGL::IsRobustResourceInitSupported() &&
         gl::GLSurfaceEGL::IsDisplayTextureShareGroupSupported() &&
         gl::GLSurfaceEGL::IsCreateContextClientArraysSupported();
#else
  // The passthrough command buffer is only supported on top of ANGLE/EGL
  return false;
#endif  // defined(USE_EGL)
}  // namespace gles2

}  // namespace gles2
}  // namespace gpu
