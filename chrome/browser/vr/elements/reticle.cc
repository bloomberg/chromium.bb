// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/reticle.h"

#include "base/numerics/math_constants.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"

namespace vr {

Reticle::Reticle(UiScene* scene, Model* model) : scene_(scene), model_(model) {
  set_name(kReticle);
  set_hit_testable(false);
  SetVisible(true);
}

Reticle::~Reticle() = default;

void Reticle::Render(UiElementRenderer* renderer,
                     const gfx::Transform& model_view_proj_matrix) const {
  // Scale the reticle to have a fixed FOV size at any distance.
  const float eye_to_target =
      std::sqrt(model_->reticle.target_point.SquaredDistanceTo(kOrigin));

  gfx::Transform mat;
  mat.Scale3d(kReticleWidth * eye_to_target, kReticleHeight * eye_to_target, 1);

  gfx::Quaternion rotation;

  UiElement* target =
      scene_->GetUiElementById(model_->reticle.target_element_id);

  if (target) {
    // Make the reticle planar to the element it's hitting.
    rotation = gfx::Quaternion(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                               -target->GetNormal());
  } else {
    // Rotate the reticle to directly face the eyes.
    rotation = gfx::Quaternion(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                               model_->reticle.target_point - kOrigin);
  }
  gfx::Transform rotation_mat(rotation);
  mat = rotation_mat * mat;

  gfx::Point3F target_point =
      ScalePoint(model_->reticle.target_point, kReticleOffset);
  // Place the pointer slightly in front of the plane intersection point.
  mat.matrix().postTranslate(target_point.x(), target_point.y(),
                             target_point.z());

  gfx::Transform transform = model_view_proj_matrix * mat;
  renderer->DrawReticle(computed_opacity(), transform);
}

}  // namespace vr
