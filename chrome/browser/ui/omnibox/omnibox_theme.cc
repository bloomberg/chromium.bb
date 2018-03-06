// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_theme.h"

#include "base/logging.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

#if defined(USE_AURA)
#include "ui/native_theme/native_theme_dark_aura.h"
#endif

namespace {

constexpr ui::NativeTheme::ColorId kInvalidColorId =
    ui::NativeTheme::kColorId_NumColors;

template <class T>
constexpr T NormalHoveredSelectedOrBoth(OmniboxState state,
                                        T normal,
                                        T hovered,
                                        T selected,
                                        T hovered_and_selected) {
  const T args[] = {normal, hovered, selected, hovered_and_selected};
  return args[static_cast<size_t>(state)];
}

template <class T>
constexpr T NormalHoveredSelected(OmniboxState state,
                                  T normal,
                                  T hovered,
                                  T selected) {
  // Use selected if state is HOVERED_AND_SELECTED.
  const T args[] = {normal, hovered, selected, selected};
  return args[static_cast<size_t>(state)];
}

ui::NativeTheme::ColorId GetLegacyColorId(OmniboxPart part,
                                          OmniboxState state) {
  using NativeId = ui::NativeTheme::ColorId;
  switch (part) {
    case OmniboxPart::RESULTS_BACKGROUND:
      return NormalHoveredSelected(
          state, NativeId::kColorId_ResultsTableNormalBackground,
          NativeId::kColorId_ResultsTableHoveredBackground,
          NativeId::kColorId_ResultsTableSelectedBackground);
  }
  return kInvalidColorId;
}

SkColor GetLegacyColor(OmniboxPart part, OmniboxTint tint, OmniboxState state) {
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
#if defined(USE_AURA)
  if (tint == OmniboxTint::DARK)
    native_theme = ui::NativeThemeDarkAura::instance();
#endif

  ui::NativeTheme::ColorId color_id = GetLegacyColorId(part, state);
  return color_id == kInvalidColorId ? gfx::kPlaceholderColor
                                     : native_theme->GetSystemColor(color_id);
}

}  // namespace

SkColor GetOmniboxColor(OmniboxPart part,
                        OmniboxTint tint,
                        OmniboxState state) {
  if (!ui::MaterialDesignController::IsTouchOptimizedUiEnabled())
    return GetLegacyColor(part, tint, state);

  const bool dark = tint == OmniboxTint::DARK;

  switch (part) {
    case OmniboxPart::RESULTS_BACKGROUND:
      // The spec calls for transparent black (or white) overlays for hover (6%)
      // and select (8%), which can overlap (for 14%). Pre-blend these with the
      // background for the best text AA result.
      return color_utils::BlendTowardOppositeLuma(
          dark ? gfx::kGoogleGrey800 : SK_ColorWHITE,
          NormalHoveredSelectedOrBoth<SkAlpha>(state, 0x00, 0x0f, 0x14, 0x24));
  }
  return gfx::kPlaceholderColor;
}
