// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_MATH_H_
#define DEVICE_VR_VR_MATH_H_

#include "device/vr/vr_export.h"
#include "device/vr/vr_types.h"

namespace vr {

void DEVICE_VR_EXPORT SetIdentityM(Mat4f* mat);
void DEVICE_VR_EXPORT TranslateM(const Mat4f& mat,
                                 const gfx::Vector3dF& translation,
                                 Mat4f* out);
void DEVICE_VR_EXPORT ScaleM(const Mat4f& mat,
                             const gfx::Vector3dF& scale,
                             Mat4f* out);

gfx::Vector3dF DEVICE_VR_EXPORT MatrixVectorMul(const Mat4f& m,
                                                const gfx::Vector3dF& v);
gfx::Vector3dF DEVICE_VR_EXPORT MatrixVectorRotate(const Mat4f& m,
                                                   const gfx::Vector3dF& v);
void DEVICE_VR_EXPORT MatrixMul(const Mat4f& matrix1,
                                const Mat4f& matrix2,
                                Mat4f* out);
void DEVICE_VR_EXPORT PerspectiveMatrixFromView(const gfx::RectF& fov,
                                                float z_near,
                                                float z_far,
                                                Mat4f* out);

// Provides the direction the head is looking towards as a 3x1 unit vector.
gfx::Vector3dF DEVICE_VR_EXPORT GetForwardVector(const Mat4f& matrix);

gfx::Vector3dF DEVICE_VR_EXPORT GetTranslation(const Mat4f& matrix);

void DEVICE_VR_EXPORT QuatToMatrix(const Quatf& quat, Mat4f* out);

// Normalize a vector, and return its original length.
float DEVICE_VR_EXPORT NormalizeVector(gfx::Vector3dF* vec);

void DEVICE_VR_EXPORT NormalizeQuat(Quatf* quat);

Quatf DEVICE_VR_EXPORT QuatFromAxisAngle(const RotationAxisAngle& axis_angle);

gfx::Point3F DEVICE_VR_EXPORT GetRayPoint(const gfx::Point3F& rayOrigin,
                                          const gfx::Vector3dF& rayVector,
                                          float scale);

// Angle between the vectors' projections to the XZ plane.
bool DEVICE_VR_EXPORT XZAngle(const gfx::Vector3dF& vec1,
                              const gfx::Vector3dF& vec2,
                              float* angle);

}  // namespace vr

#endif  // DEVICE_VR_VR_MATH_H_
