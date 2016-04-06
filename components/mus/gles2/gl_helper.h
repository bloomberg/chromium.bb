// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_GL_HELPER_H_
#define COMPONENTS_MUS_GLES2_GL_HELPER_H_

#include "gpu/command_buffer/client/gles2_interface.h"

class SkRegion;

namespace mus {

void GLCopySubBufferDamage(gpu::gles2::GLES2Interface* gl,
                           GLenum target,
                           GLuint texture,
                           GLuint previous_texture,
                           const SkRegion& new_damage,
                           const SkRegion& old_damage);

}  // namespace mus

#endif  // COMPONENTS_MUS_GLES2_GL_HELPER_H_