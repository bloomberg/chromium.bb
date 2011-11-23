// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/screen_orientation_listener.h"

#include "base/memory/scoped_ptr.h"
#include "content/browser/sensors/sensors_provider.h"
#include "ui/aura/desktop.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animation_sequence.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/compositor/screen_rotation.h"
#include "ui/gfx/interpolated_transform.h"

namespace {

// Converts degrees to an angle in the range [-180, 180).
int NormalizeAngle(int degrees) {
  while (degrees <= -180) degrees += 360;
  while (degrees > 180) degrees -= 360;
  return degrees;
}

static int SymmetricRound(float x) {
  return static_cast<int>(
    x > 0
      ? std::floor(x + 0.5f)
      : std::ceil(x - 0.5f));
}

}  // namespace

ScreenOrientationListener::ScreenOrientationListener() {
  sensors::Provider::GetInstance()->AddListener(this);
}

ScreenOrientationListener::~ScreenOrientationListener() {
  sensors::Provider::GetInstance()->RemoveListener(this);
}

void ScreenOrientationListener::OnScreenOrientationChanged(
    content::ScreenOrientation change) {
  ui::Layer* to_rotate = NULL;
  ui::LayerAnimationObserver* observer = NULL;
  // Desktop is initialized before the listener, so this will not return NULL.
  aura::Desktop* aura_desktop = aura::Desktop::GetInstance();
  to_rotate = aura_desktop->layer();
  observer = aura_desktop;

  if (!to_rotate)
    return;

  bool should_rotate = true;
  int new_degrees = 0;
  switch (change) {
    case content::SCREEN_ORIENTATION_TOP: break;
    case content::SCREEN_ORIENTATION_RIGHT: new_degrees = 90; break;
    case content::SCREEN_ORIENTATION_LEFT: new_degrees = -90; break;
    case content::SCREEN_ORIENTATION_BOTTOM: new_degrees = 180; break;
    // Ignore front and back orientations.
    default: should_rotate = false;
  }

  if (!should_rotate)
    return;

  float rotation = 0.0f;
  int delta = 0;
  const ui::Transform& transform = to_rotate->GetTargetTransform();
  if (ui::InterpolatedTransform::FactorTRS(transform,
                                           NULL, &rotation, NULL))
    delta = NormalizeAngle(new_degrees - SymmetricRound(rotation));

  // No rotation to do.
  if (delta == 0)
    return;

  scoped_ptr<ui::LayerAnimationSequence> screen_rotation(
      new ui::LayerAnimationSequence(new ui::ScreenRotation(delta)));
  screen_rotation->AddObserver(observer);
  to_rotate->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  to_rotate->GetAnimator()->ScheduleAnimation(screen_rotation.release());
}

// static
ScreenOrientationListener* ScreenOrientationListener::GetInstance() {
  return Singleton<ScreenOrientationListener>::get();
}
