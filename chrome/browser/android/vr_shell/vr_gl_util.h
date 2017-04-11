// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_GL_UTIL_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_GL_UTIL_H_

#include <array>
#include <string>

#include "device/vr/vr_types.h"
#include "ui/gl/gl_bindings.h"

namespace vr_shell {

std::array<float, 16> MatrixToGLArray(const vr::Mat4f& matrix);

gfx::Rect CalculatePixelSpaceRect(const gfx::Size& texture_size,
                                  const gfx::RectF& texture_rect);

// Compile a shader.
GLuint CompileShader(GLenum shader_type,
                     const GLchar* shader_source,
                     std::string& error);

// Compile and link a program.
GLuint CreateAndLinkProgram(GLuint vertex_shader_handle,
                            GLuint fragment_shader_handle,
                            std::string& error);

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_GL_UTIL_H_
