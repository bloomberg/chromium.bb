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

#if defined(USE_X11)
#include "ui/views/linux_ui/linux_ui.h"
#endif

namespace {

constexpr ui::NativeTheme::ColorId kInvalidColorId =
    ui::NativeTheme::kColorId_NumColors;

template <class T>
constexpr T NormalHoveredSelectedOrBoth(OmniboxPartState state,
                                        T normal,
                                        T hovered,
                                        T selected,
                                        T hovered_and_selected) {
  const T args[] = {normal, hovered, selected, hovered_and_selected};
  return args[static_cast<size_t>(state)];
}

template <class T>
constexpr T NormalHoveredSelected(OmniboxPartState state,
                                  T normal,
                                  T hovered,
                                  T selected) {
  // Use selected if state is HOVERED_AND_SELECTED.
  const T args[] = {normal, hovered, selected, selected};
  return args[static_cast<size_t>(state)];
}

ui::NativeTheme::ColorId GetLegacyColorId(OmniboxPart part,
                                          OmniboxPartState state) {
  using NativeId = ui::NativeTheme::ColorId;
  switch (part) {
    case OmniboxPart::RESULTS_BACKGROUND:
      return NormalHoveredSelected(
          state, NativeId::kColorId_ResultsTableNormalBackground,
          NativeId::kColorId_ResultsTableHoveredBackground,
          NativeId::kColorId_ResultsTableSelectedBackground);
    case OmniboxPart::TEXT_DEFAULT:
      return NormalHoveredSelected(state,
                                   NativeId::kColorId_ResultsTableNormalText,
                                   NativeId::kColorId_ResultsTableHoveredText,
                                   NativeId::kColorId_ResultsTableSelectedText);
    case OmniboxPart::TEXT_DIMMED:
      return NormalHoveredSelected(
          state, NativeId::kColorId_ResultsTableNormalDimmedText,
          NativeId::kColorId_ResultsTableHoveredDimmedText,
          NativeId::kColorId_ResultsTableSelectedDimmedText);
    case OmniboxPart::TEXT_NEGATIVE:
      return NormalHoveredSelected(
          state, NativeId::kColorId_ResultsTableNegativeText,
          NativeId::kColorId_ResultsTableNegativeHoveredText,
          NativeId::kColorId_ResultsTableNegativeSelectedText);
    case OmniboxPart::TEXT_POSITIVE:
      return NormalHoveredSelected(
          state, NativeId::kColorId_ResultsTablePositiveText,
          NativeId::kColorId_ResultsTablePositiveHoveredText,
          NativeId::kColorId_ResultsTablePositiveSelectedText);
    case OmniboxPart::TEXT_URL:
      return NormalHoveredSelected(state,
                                   NativeId::kColorId_ResultsTableNormalUrl,
                                   NativeId::kColorId_ResultsTableHoveredUrl,
                                   NativeId::kColorId_ResultsTableSelectedUrl);

    case OmniboxPart::TEXT_INVISIBLE:
    case OmniboxPart::RESULTS_SEPARATOR:
      NOTREACHED();
      break;
  }
  return kInvalidColorId;
}

SkColor GetLegacyColor(OmniboxPart part,
                       OmniboxTint tint,
                       OmniboxPartState state) {
  if (part == OmniboxPart::TEXT_INVISIBLE)
    return SK_ColorTRANSPARENT;

  ui::NativeTheme* native_theme = nullptr;
#if defined(USE_AURA)
  if (tint == OmniboxTint::DARK)
    native_theme = ui::NativeThemeDarkAura::instance();
#endif
#if defined(USE_X11)
  // Note: passing null to GetNativeTheme() always returns the native GTK theme.
  if (tint == OmniboxTint::NATIVE && views::LinuxUI::instance())
    native_theme = views::LinuxUI::instance()->GetNativeTheme(nullptr);
#endif
  if (!native_theme)
    native_theme = ui::NativeTheme::GetInstanceForNativeUi();

  ui::NativeTheme::ColorId color_id = GetLegacyColorId(part, state);
  return color_id == kInvalidColorId ? gfx::kPlaceholderColor
                                     : native_theme->GetSystemColor(color_id);
}

}  // namespace

SkColor GetOmniboxColor(OmniboxPart part,
                        OmniboxTint tint,
                        OmniboxPartState state) {
  if (!ui::MaterialDesignController::IsTouchOptimizedUiEnabled())
    return GetLegacyColor(part, tint, state);

  // Note this will use LIGHT for OmniboxTint::NATIVE.
  // TODO(https://crbug.com/819452): Determine the role GTK should play in this.
  const bool dark = tint == OmniboxTint::DARK;

  switch (part) {
    case OmniboxPart::RESULTS_BACKGROUND:
      // The spec calls for transparent black (or white) overlays for hover (6%)
      // and select (8%), which can overlap (for 14%). Pre-blend these with the
      // background for the best text AA result.
      return color_utils::BlendTowardOppositeLuma(
          dark ? gfx::kGoogleGrey800 : SK_ColorWHITE,
          NormalHoveredSelectedOrBoth<SkAlpha>(state, 0x00, 0x0f, 0x14, 0x24));
    case OmniboxPart::RESULTS_SEPARATOR:
      // The dark base color doesn't appear in the MD2 spec, just Chrome's.
      return dark ? SkColorSetARGB(0x6e, 0x16, 0x17, 0x1a)   // 43% alpha.
                  : SkColorSetA(gfx::kGoogleGrey900, 0x24);  // 14% alpha.

    // TODO(tapted): Add these.
    case OmniboxPart::TEXT_DEFAULT:
    case OmniboxPart::TEXT_DIMMED:
    case OmniboxPart::TEXT_INVISIBLE:
    case OmniboxPart::TEXT_NEGATIVE:
    case OmniboxPart::TEXT_POSITIVE:
    case OmniboxPart::TEXT_URL:
      return GetLegacyColor(part, tint, state);
  }
  return gfx::kPlaceholderColor;
}
