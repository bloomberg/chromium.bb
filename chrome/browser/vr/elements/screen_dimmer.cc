// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/screen_dimmer.h"

#include "chrome/browser/vr/color_scheme.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/transform.h"

namespace vr {

namespace {
static const float kDimmerOpacity = 0.9f;
}  // namespace

ScreenDimmer::ScreenDimmer() {}

ScreenDimmer::~ScreenDimmer() = default;

void ScreenDimmer::Render(UiElementRenderer* renderer,
                          const gfx::Transform& view_proj_matrix) const {
  gfx::Transform m;
  m.Scale3d(2.0f, 2.0f, 1.0f);

  // Always use normal scheme for dimmer.
  const ColorScheme& color_scheme =
      ColorScheme::GetColorScheme(ColorScheme::kModeNormal);
  renderer->DrawGradientQuad(m, color_scheme.dimmer_outer,
                             color_scheme.dimmer_inner, kDimmerOpacity, size(),
                             corner_radius());
}

}  // namespace vr
