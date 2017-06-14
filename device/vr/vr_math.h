// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_MATH_H_
#define DEVICE_VR_VR_MATH_H_

#include "device/vr/vr_export.h"
#include "device/vr/vr_types.h"

#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/transform.h"

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

// Provides the direction the head is looking towards as a 3x1 unit vector.
gfx::Vector3dF DEVICE_VR_EXPORT GetForwardVector(const Mat4f& matrix);

gfx::Vector3dF DEVICE_VR_EXPORT GetTranslation(const Mat4f& matrix);

void DEVICE_VR_EXPORT QuatToMatrix(const Quatf& quat, Mat4f* out);

// Creates a rotation which rotates `from` vector to `to`.
Quatf DEVICE_VR_EXPORT GetVectorRotation(const gfx::Vector3dF& from,
                                         const gfx::Vector3dF& to);

Quatf DEVICE_VR_EXPORT QuatSum(const Quatf& a, const Quatf& b);
Quatf DEVICE_VR_EXPORT QuatProduct(const Quatf& a, const Quatf& b);
Quatf DEVICE_VR_EXPORT ScaleQuat(const Quatf& q, float s);

Quatf DEVICE_VR_EXPORT InvertQuat(const Quatf& quat);

float DEVICE_VR_EXPORT QuatAngleDegrees(const Quatf& a, const Quatf& b);

Quatf DEVICE_VR_EXPORT QuatLerp(const Quatf& a, const Quatf& b, float t);

// Spherical linear interpolation.
gfx::Vector3dF DEVICE_VR_EXPORT QuatSlerp(const gfx::Vector3dF& v_start,
                                          const gfx::Vector3dF& v_end,
                                          float percent);

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

gfx::Vector3dF DEVICE_VR_EXPORT ToVector(const gfx::Point3F& p);

gfx::Point3F DEVICE_VR_EXPORT ToPoint(const gfx::Vector3dF& p);

// Scale components of the point by the components of the vector.
gfx::Point3F DEVICE_VR_EXPORT ScalePoint(const gfx::Point3F& p,
                                         const gfx::Vector3dF& s);

// Scale components of a vector by the components of another.
gfx::Vector3dF DEVICE_VR_EXPORT ScaleVector(const gfx::Vector3dF& p,
                                            const gfx::Vector3dF& s);

float DEVICE_VR_EXPORT Clampf(float value, float min, float max);

// This is a convenient function to ease the transition to purely gfx types.
Quatf DEVICE_VR_EXPORT ToVRQuatF(const gfx::Quaternion& q);

gfx::Quaternion DEVICE_VR_EXPORT ToQuaternion(const vr::Quatf& q);

Mat4f DEVICE_VR_EXPORT ToMat4F(const gfx::Transform& t);

gfx::Transform DEVICE_VR_EXPORT ToTransform(const vr::Mat4f& m);

}  // namespace vr

#endif  // DEVICE_VR_VR_MATH_H_
