// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/laser.h"

#include "base/numerics/math_constants.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_scene_constants.h"

namespace vr {

Laser::Laser(Model* model) : model_(model) {
  set_name(kLaser);
  set_hit_testable(false);
  SetVisible(true);
}

Laser::~Laser() = default;

void Laser::Render(UiElementRenderer* renderer,
                   const gfx::Transform& model_view_proj_matrix) const {
  // Find the length of the beam (from hand to target).
  const float laser_length =
      std::sqrt(model_->controller.laser_origin.SquaredDistanceTo(
          ScalePoint(model_->reticle.target_point, kReticleOffset)));

  // Build a beam, originating from the origin.
  gfx::Transform mat;

  // Move the beam half its height so that its end sits on the origin.
  mat.matrix().postTranslate(0.0f, 0.5f, 0.0f);
  mat.matrix().postScale(kLaserWidth, laser_length, 1);

  // Tip back 90 degrees to flat, pointing at the scene.
  const gfx::Quaternion quat(gfx::Vector3dF(1.0f, 0.0f, 0.0f),
                             -base::kPiDouble / 2);
  gfx::Transform rotation_mat(quat);
  mat = rotation_mat * mat;

  const gfx::Vector3dF beam_direction =
      model_->reticle.target_point - model_->controller.laser_origin;

  gfx::Transform beam_direction_mat(
      gfx::Quaternion(gfx::Vector3dF(0.0f, 0.0f, -1.0f), beam_direction));

  // Render multiple faces to make the laser appear cylindrical.
  const int faces = 4;
  gfx::Transform face_transform;
  gfx::Transform transform;
  for (int i = 0; i < faces; i++) {
    // Rotate around Z.
    const float angle = base::kPiFloat * 2 * i / faces;
    const gfx::Quaternion rot({0.0f, 0.0f, 1.0f}, angle);
    face_transform = beam_direction_mat * gfx::Transform(rot) * mat;

    // Move the beam origin to the hand.
    face_transform.matrix().postTranslate(model_->controller.laser_origin.x(),
                                          model_->controller.laser_origin.y(),
                                          model_->controller.laser_origin.z());
    transform = model_view_proj_matrix * face_transform;
    renderer->DrawLaser(computed_opacity(), transform);
  }
}

}  // namespace vr
