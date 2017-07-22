// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_gesture_util.h"

#include <cmath>

namespace ash {

const float kHorizontalStrokeLengthThreshold = 20;
const float kHorizontalStrokeThicknessThreshold = 2;
const float kHorizontalStrokeFlatnessThreshold = 0.1;

const double kClosedShapeWrapThreshold = M_PI * 2 * 0.95;
const double kClosedShapeSweepThreshold = M_PI * 2 * 0.8;
const double kClosedShapeJiggleThreshold = 0.1;

gfx::RectF GetBoundingBox(const std::vector<gfx::PointF>& points) {
  if (points.empty())
    return gfx::RectF();

  gfx::PointF min_point = points[0];
  gfx::PointF max_point = points[0];
  for (size_t i = 1; i < points.size(); ++i) {
    min_point.SetToMin(points[i]);
    max_point.SetToMax(points[i]);
  }

  return gfx::BoundingRect(min_point, max_point);
}

bool DetectHorizontalStroke(const gfx::RectF& box,
                            const gfx::SizeF& pen_tip_size) {
  return box.width() > kHorizontalStrokeLengthThreshold &&
         box.height() <
             pen_tip_size.height() * kHorizontalStrokeThicknessThreshold &&
         box.height() < box.width() * kHorizontalStrokeFlatnessThreshold;
}

bool DetectClosedShape(const gfx::RectF& box,
                       const std::vector<gfx::PointF>& points) {
  if (points.size() < 3)
    return false;

  const gfx::PointF center = box.CenterPoint();

  // Analyze vectors pointing from the center to each point.
  // Compute the cumulative swept angle and count positive
  // and negative angles separately.
  double swept_angle = 0.0;
  int positive = 0;
  int negative = 0;

  double prev_angle = 0.0;
  bool has_prev_angle = false;

  for (const auto& point : points) {
    const double angle = atan2(point.y() - center.y(), point.x() - center.x());
    if (has_prev_angle) {
      double diff_angle = angle - prev_angle;
      if (diff_angle > kClosedShapeWrapThreshold) {
        diff_angle -= M_PI * 2;
      } else if (diff_angle < -kClosedShapeWrapThreshold) {
        diff_angle += M_PI * 2;
      }
      swept_angle += diff_angle;
      if (diff_angle > 0)
        positive++;
      if (diff_angle < 0)
        negative++;
    } else {
      has_prev_angle = true;
    }
    prev_angle = angle;
  }

  if (std::abs(swept_angle) < kClosedShapeSweepThreshold) {
    // Has not swept enough of the full circle.
    return false;
  }

  if (swept_angle > 0 && (static_cast<double>(negative) / positive) >
                             kClosedShapeJiggleThreshold) {
    // Main direction is positive, but went too often in the negative direction.
    return false;
  }
  if (swept_angle < 0 && (static_cast<double>(positive) / negative) >
                             kClosedShapeJiggleThreshold) {
    // Main direction is negative, but went too often in the positive direction.
    return false;
  }

  return true;
}

}  // namespace ash
