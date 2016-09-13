// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_UTIL_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_UTIL_H_

#include <string>

#include "third_party/gvr-android-sdk/src/ndk-beta/include/vr/gvr/capi/include/gvr_types.h"
#include "ui/gl/gl_bindings.h"

namespace vr_shell {

// 2D rectangles. Unlike gvr::Rectf and gvr::Recti, these have width and height
// rather than right and top.
typedef struct Recti {
  int x;
  int y;
  int width;
  int height;
} Recti;

typedef struct Rectf {
  float x;
  float y;
  float width;
  float height;
} Rectf;

void SetIdentityM(gvr::Mat4f& mat);

void TranslateM(gvr::Mat4f& tmat, gvr::Mat4f& mat, float x, float y, float z);
void TranslateMRight(gvr::Mat4f& tmat,
                     gvr::Mat4f& mat,
                     float x,
                     float y,
                     float z);

void ScaleM(gvr::Mat4f& tmat, const gvr::Mat4f& mat, float x, float y, float z);
void ScaleMRight(gvr::Mat4f& tmat, const gvr::Mat4f& mat,
                 float x, float y, float z);

std::array<float, 16> MatrixToGLArray(const gvr::Mat4f& matrix);

// Util functions that are copied from the treasure_hunt NDK demo in
// third_party/gvr-andoir-sdk/ folder.
gvr::Mat4f MatrixTranspose(const gvr::Mat4f& mat);
std::array<float, 4> MatrixVectorMul(const gvr::Mat4f& matrix,
                                     const std::array<float, 4>& vec);
std::array<float, 3> MatrixVectorMul(const gvr::Mat4f& matrix,
                                     const std::array<float, 3>& vec);
gvr::Vec3f MatrixVectorMul(const gvr::Mat4f& m, const gvr::Vec3f& v);
gvr::Vec3f MatrixVectorRotate(const gvr::Mat4f& m, const gvr::Vec3f& v);
gvr::Mat4f MatrixMul(const gvr::Mat4f& matrix1, const gvr::Mat4f& matrix2);
gvr::Mat4f PerspectiveMatrixFromView(const gvr::Rectf& fov,
                                     float z_near,
                                     float z_far);
gvr::Rectf ModulateRect(const gvr::Rectf& rect, float width, float height);
gvr::Recti CalculatePixelSpaceRect(const gvr::Sizei& texture_size,
                                   const gvr::Rectf& texture_rect);

// Provides the direction the head is looking towards as a 3x1 unit vector.
gvr::Vec3f getForwardVector(const gvr::Mat4f& matrix);

// Provides the relative translation of the head as a 3x1 vector.
gvr::Vec3f getTranslation(const gvr::Mat4f& matrix);

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

gvr::Quatf QuatMultiply(const gvr::Quatf& a, const gvr::Quatf& b);

gvr::Mat4f QuatToMatrix(const gvr::Quatf& quat);

float VectorLength(const gvr::Vec3f& vec);

void NormalizeVector(gvr::Vec3f& vec);

float VectorDot(const gvr::Vec3f& a, const gvr::Vec3f& b);

void NormalizeQuat(gvr::Quatf& quat);

gvr::Quatf QuatFromAxisAngle(float x, float y, float z, float angle);

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_UTIL_H_
