// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_UTIL_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_UTIL_H_

#include <string>

#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr_types.h"
#include "ui/gl/gl_bindings.h"

namespace vr_shell {

std::array<float, 16> MatrixToGLArray(const gvr::Mat4f& matrix);

// Util functions that are copied from the treasure_hunt NDK demo in
// third_party/gvr-andoir-sdk/ folder.
gvr::Mat4f MatrixTranspose(const gvr::Mat4f& mat);
gvr::Mat4f MatrixMul(const gvr::Mat4f& matrix1, const gvr::Mat4f& matrix2);
gvr::Mat4f PerspectiveMatrixFromView(const gvr::Rectf& fov,
                                     float z_near,
                                     float z_far);
gvr::Rectf ModulateRect(const gvr::Rectf& rect, float width, float height);
gvr::Recti CalculatePixelSpaceRect(const gvr::Sizei& texture_size,
                                   const gvr::Rectf& texture_rect);

// Compile a shader.
GLuint CompileShader(GLenum shader_type,
                     const GLchar* shader_source,
                     std::string& error);

// Compile and link a program.
GLuint CreateAndLinkProgram(GLuint vertex_shader_handle,
                            GLuint fragment_shader_handle,
                            int num_attributes,
                            const GLchar** attributes,
                            std::string& error);

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_UTIL_H_
