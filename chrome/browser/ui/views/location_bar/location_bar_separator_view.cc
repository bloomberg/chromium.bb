// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_separator_view.h"

#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point_f.h"

namespace {

// These values are empirical.
constexpr base::TimeDelta kLocationBarSeparatorFadeInDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr base::TimeDelta kLocationBarSeparatorFadeOutDuration =
    base::TimeDelta::FromMilliseconds(175);

}  //  namespace

LocationBarSeparatorView::LocationBarSeparatorView() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

void LocationBarSeparatorView::OnPaint(gfx::Canvas* canvas) {
  DCHECK_NE(gfx::kPlaceholderColor, background_color_);
  const SkColor separator_color = SkColorSetA(
      color_utils::GetColorWithMaxContrast(background_color_), 0x69);
  const float x = 1.f - 1.f / canvas->image_scale();
  canvas->DrawLine(gfx::PointF(x, GetLocalBounds().y()),
                   gfx::PointF(x, GetLocalBounds().bottom()), separator_color);
}

void LocationBarSeparatorView::Show() {
  layer()->SetOpacity(0.0f);
}

void LocationBarSeparatorView::Hide() {
  layer()->SetOpacity(1.0f);
}

void LocationBarSeparatorView::FadeIn() {
  FadeTo(1.0f, kLocationBarSeparatorFadeInDuration);
}

void LocationBarSeparatorView::FadeOut() {
  FadeTo(0.0f, kLocationBarSeparatorFadeOutDuration);
}

void LocationBarSeparatorView::FadeTo(float opacity, base::TimeDelta duration) {
  if (disable_animation_for_test_) {
    layer()->SetOpacity(opacity);
  } else {
    ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
    animation.SetTransitionDuration(duration);
    animation.SetTweenType(gfx::Tween::Type::EASE_IN);
    layer()->SetOpacity(opacity);
  }
}
