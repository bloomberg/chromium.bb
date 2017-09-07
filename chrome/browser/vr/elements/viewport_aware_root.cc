// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/viewport_aware_root.h"

#include <cmath>

#include "cc/base/math_util.h"

namespace vr {

// static
const float ViewportAwareRoot::kViewportRotationTriggerDegrees = 55.0f;

ViewportAwareRoot::ViewportAwareRoot() {
  set_viewport_aware(true);
  SetTransitionedProperties({OPACITY});
}

ViewportAwareRoot::~ViewportAwareRoot() = default;

void ViewportAwareRoot::AdjustRotationForHeadPose(
    const gfx::Vector3dF& look_at) {
  DCHECK(viewport_aware());
  DCHECK(!look_at.IsZero());

  gfx::Vector3dF rotated_center_vector{0.f, 0.f, -1.0f};
  LocalTransform().TransformVector(&rotated_center_vector);
  gfx::Vector3dF top_projected_look_at{look_at.x(), 0.f, look_at.z()};
  float degrees = gfx::ClockwiseAngleBetweenVectorsInDegrees(
      top_projected_look_at, rotated_center_vector, {0.f, 1.0f, 0.f});
  if (degrees <= kViewportRotationTriggerDegrees ||
      degrees >= 360.0f - kViewportRotationTriggerDegrees) {
    return;
  }
  viewport_aware_total_rotation_ += degrees;
  viewport_aware_total_rotation_ = fmod(viewport_aware_total_rotation_, 360.0f);
  SetRotate(0.f, 1.f, 0.f,
            cc::MathUtil::Deg2Rad(viewport_aware_total_rotation_));

  // Immediately hide the element.
  SetVisibleImmediately(false);

  // Fade it back in.
  SetVisible(true);
}

void ViewportAwareRoot::OnUpdatedInheritedProperties() {
  // We must not inherit a transform.
  DCHECK(parent()->world_space_transform().IsIdentity());
}

}  // namespace vr
