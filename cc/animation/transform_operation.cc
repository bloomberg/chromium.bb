// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <limits>

#include "cc/animation/transform_operation.h"
#include "ui/gfx/vector3d_f.h"

namespace {
const double kAngleEpsilon = 1e-4;
}

namespace cc {

bool TransformOperation::IsIdentity() const {
  return matrix.IsIdentity();
}

static bool IsOperationIdentity(const TransformOperation* operation) {
  return !operation || operation->IsIdentity();
}

static bool ShareSameAxis(const TransformOperation* from,
                          const TransformOperation* to,
                          double& axis_x, double& axis_y, double& axis_z,
                          double& angle_from) {
  if (IsOperationIdentity(from) && IsOperationIdentity(to))
    return false;

  if (IsOperationIdentity(from) && !IsOperationIdentity(to)) {
    axis_x = to->rotate.axis.x;
    axis_y = to->rotate.axis.y;
    axis_z = to->rotate.axis.z;
    angle_from = 0;
    return true;
  }

  if (!IsOperationIdentity(from) && IsOperationIdentity(to)) {
    axis_x = from->rotate.axis.x;
    axis_y = from->rotate.axis.y;
    axis_z = from->rotate.axis.z;
    angle_from = from->rotate.angle;
    return true;
  }

  double length_2 = from->rotate.axis.x * from->rotate.axis.x +
                    from->rotate.axis.y * from->rotate.axis.y +
                    from->rotate.axis.z * from->rotate.axis.z;
  double other_length_2 = to->rotate.axis.x * to->rotate.axis.x +
                          to->rotate.axis.y * to->rotate.axis.y +
                          to->rotate.axis.z * to->rotate.axis.z;

  if (length_2 <= kAngleEpsilon || other_length_2 <= kAngleEpsilon)
    return false;

  double dot = to->rotate.axis.x * from->rotate.axis.x +
               to->rotate.axis.y * from->rotate.axis.y +
               to->rotate.axis.z * from->rotate.axis.z;
  double error = std::fabs(1.0 - (dot * dot) / (length_2 * other_length_2));
  bool result = error < kAngleEpsilon;
  if (result) {
    axis_x = to->rotate.axis.x;
    axis_y = to->rotate.axis.y;
    axis_z = to->rotate.axis.z;
    // If the axes are pointing in opposite directions, we need to reverse
    // the angle.
    angle_from = dot > 0 ? from->rotate.angle : -from->rotate.angle;
  }
  return result;
}

static double BlendDoubles(double from, double to, double progress) {
  if (progress <= 0.0)
    return from;

  if (progress >= 1.0)
    return to;

  return from * (1 - progress) + to * progress;
}

bool TransformOperation::BlendTransformOperations(
    const TransformOperation* from,
    const TransformOperation* to,
    double progress,
    gfx::Transform& result) {
  if (IsOperationIdentity(from) && IsOperationIdentity(to))
    return true;

  TransformOperation::Type interpolation_type =
      TransformOperation::TransformOperationIdentity;
  if (IsOperationIdentity(to))
    interpolation_type = from->type;
  else
    interpolation_type = to->type;

  switch (interpolation_type) {
  case TransformOperation::TransformOperationTranslate: {
    double from_x = IsOperationIdentity(from) ? 0 : from->translate.x;
    double from_y = IsOperationIdentity(from) ? 0 : from->translate.y;
    double from_z = IsOperationIdentity(from) ? 0 : from->translate.z;
    double to_x = IsOperationIdentity(to) ? 0 : to->translate.x;
    double to_y = IsOperationIdentity(to) ? 0 : to->translate.y;
    double to_z = IsOperationIdentity(to) ? 0 : to->translate.z;
    result.Translate3d(BlendDoubles(from_x, to_x, progress),
                       BlendDoubles(from_y, to_y, progress),
                       BlendDoubles(from_z, to_z, progress));
    break;
  }
  case TransformOperation::TransformOperationRotate: {
    double axis_x = 0;
    double axis_y = 0;
    double axis_z = 1;
    double from_angle = 0;
    double to_angle = IsOperationIdentity(to) ? 0 : to->rotate.angle;
    if (ShareSameAxis(from, to, axis_x, axis_y, axis_z, from_angle)) {
      result.RotateAbout(gfx::Vector3dF(axis_x, axis_y, axis_z),
                         BlendDoubles(from_angle, to_angle, progress));
    } else {
      gfx::Transform to_matrix;
      if (!IsOperationIdentity(to))
        to_matrix = to->matrix;
      gfx::Transform from_matrix;
      if (!IsOperationIdentity(from))
        from_matrix = from->matrix;
      result = to_matrix;
      if (!result.Blend(from_matrix, progress))
        return false;
    }
    break;
  }
  case TransformOperation::TransformOperationScale: {
    double from_x = IsOperationIdentity(from) ? 1 : from->scale.x;
    double from_y = IsOperationIdentity(from) ? 1 : from->scale.y;
    double from_z = IsOperationIdentity(from) ? 1 : from->scale.z;
    double to_x = IsOperationIdentity(to) ? 1 : to->scale.x;
    double to_y = IsOperationIdentity(to) ? 1 : to->scale.y;
    double to_z = IsOperationIdentity(to) ? 1 : to->scale.z;
    result.Scale3d(BlendDoubles(from_x, to_x, progress),
                   BlendDoubles(from_y, to_y, progress),
                   BlendDoubles(from_z, to_z, progress));
    break;
  }
  case TransformOperation::TransformOperationSkew: {
    double from_x = IsOperationIdentity(from) ? 0 : from->skew.x;
    double from_y = IsOperationIdentity(from) ? 0 : from->skew.y;
    double to_x = IsOperationIdentity(to) ? 0 : to->skew.x;
    double to_y = IsOperationIdentity(to) ? 0 : to->skew.y;
    result.SkewX(BlendDoubles(from_x, to_x, progress));
    result.SkewY(BlendDoubles(from_y, to_y, progress));
    break;
  }
  case TransformOperation::TransformOperationPerspective: {
    double from_perspective_depth = IsOperationIdentity(from) ?
        std::numeric_limits<double>::max() : from->perspective_depth;
    double to_perspective_depth = IsOperationIdentity(to) ?
        std::numeric_limits<double>::max() : to->perspective_depth;
    result.ApplyPerspectiveDepth(
        BlendDoubles(from_perspective_depth, to_perspective_depth, progress));
    break;
  }
  case TransformOperation::TransformOperationMatrix: {
    gfx::Transform to_matrix;
    if (!IsOperationIdentity(to))
      to_matrix = to->matrix;
    gfx::Transform from_matrix;
    if (!IsOperationIdentity(from))
      from_matrix = from->matrix;
    result = to_matrix;
    if (!result.Blend(from_matrix, progress))
      return false;
    break;
  }
  case TransformOperation::TransformOperationIdentity:
    // Do nothing.
    break;
  }

  return true;
}

}  // namespace cc
