// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_utils.h"

#include "base/command_line.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "ui/gl/gl_switches.h"

namespace gpu {
namespace gles2 {

gl::GLContextAttribs GenerateGLContextAttribs(
    const ContextCreationAttribHelper& attribs_helper,
    const GpuPreferences& gpu_preferences) {
  gl::GLContextAttribs attribs;
  attribs.gpu_preference = attribs_helper.gpu_preference;
  if (gpu_preferences.use_passthrough_cmd_decoder) {
    attribs.bind_generates_resource = attribs_helper.bind_generates_resource;
    attribs.webgl_compatibility_context =
        IsWebGLContextType(attribs_helper.context_type);

    // Always use the global texture share group for the passthrough command
    // decoder
    attribs.global_texture_share_group = true;

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

}  // namespace gles2
}  // namespace gpu
