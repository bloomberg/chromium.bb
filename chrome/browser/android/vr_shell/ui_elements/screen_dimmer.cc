// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements/screen_dimmer.h"

#include "chrome/browser/android/vr_shell/color_scheme.h"
#include "chrome/browser/android/vr_shell/ui_element_renderer.h"
#include "device/vr/vr_math.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr_shell {

namespace {
static const float kDimmerOpacity = 0.9f;
}  // namespace

ScreenDimmer::ScreenDimmer() {}

ScreenDimmer::~ScreenDimmer() = default;

void ScreenDimmer::Initialize() {
  set_fill(Fill::SELF);
}

void ScreenDimmer::Render(UiElementRenderer* renderer,
                          vr::Mat4f view_proj_matrix) const {
  vr::Mat4f m;
  vr::SetIdentityM(&m);
  vr::ScaleM(m, {2.0f, 2.0f, 1.0f}, &m);

  // Always use normal scheme for dimmer.
  const ColorScheme& color_scheme =
      ColorScheme::GetColorScheme(ColorScheme::kModeNormal);
  renderer->DrawGradientQuad(m, color_scheme.dimmer_outer,
                             color_scheme.dimmer_inner, kDimmerOpacity);
}

}  // namespace vr_shell
