// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility OpenGL ES 2.0 functions.

#ifndef GPU_DEMOS_APP_FRAMEWORK_GLES2_UTILS_H_
#define GPU_DEMOS_APP_FRAMEWORK_GLES2_UTILS_H_

#include <GLES2/gl2.h>

namespace gpu_demos {
namespace gles2_utils {

// Uploads and compiles shader source. Returns non-zero shader object id.
// Returns 0 if an error occurs creating or compiling the shader object.
// All errors are logged.
extern GLuint LoadShader(GLenum type, const char* shader_src);

// Uploads, compiles, and links shader program. Returns non-zero program id.
// Returns 0 if an error occurs creating, compiling, or linking the program
// object. All errors are logged.
extern GLuint LoadProgram(const char* v_shader_src, const char* f_shader_src);

}  // namespace gles2_utils
}  // namespace gpu_demos
#endif  // GPU_DEMOS_APP_FRAMEWORK_GLES2_UTILS_H_
