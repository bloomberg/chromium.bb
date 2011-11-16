// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/sensors/screen_orientation_listener.h"

#include "base/memory/scoped_ptr.h"
#include "content/browser/sensors/sensors_provider.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animation_sequence.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/compositor/screen_rotation.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/views/desktop/desktop_window_view.h"

#if defined(USE_AURA)
#include "ui/aura/desktop.h"
#endif

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
    const sensors::ScreenOrientation& change) {
  ui::Layer* to_rotate = NULL;
  ui::LayerAnimationObserver* observer = NULL;
#if defined(USE_AURA)
  aura::Desktop* aura_desktop = aura::Desktop::GetInstance();
  if (aura_desktop) {
    to_rotate = aura_desktop->layer();
    observer = aura_desktop;
  }
#endif
  if (!to_rotate) {
    views::desktop::DesktopWindowView* views_desktop =
        views::desktop::DesktopWindowView::desktop_window_view;
    if (views_desktop) {
      views_desktop->SetPaintToLayer(true);
      to_rotate = views_desktop->layer();
      observer = views_desktop;
    }
  }

  if (!to_rotate || !observer)
    return;

  bool should_rotate = true;
  int new_degrees = 0;
  switch (change.upward) {
    case sensors::ScreenOrientation::TOP: break;
    case sensors::ScreenOrientation::RIGHT: new_degrees = 90; break;
    case sensors::ScreenOrientation::LEFT: new_degrees = -90; break;
    case sensors::ScreenOrientation::BOTTOM: new_degrees = 180; break;
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
