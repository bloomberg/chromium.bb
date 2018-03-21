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
  DCHECK_LT(static_cast<size_t>(state), arraysize(args));
  return args[static_cast<size_t>(state)];
}

template <class T>
constexpr T NormalHoveredSelected(OmniboxPartState state,
                                  T normal,
                                  T hovered,
                                  T selected) {
  // Use selected if state is HOVERED_AND_SELECTED.
  const T args[] = {normal, hovered, selected, selected};
  DCHECK_LT(static_cast<size_t>(state), arraysize(args));
  return args[static_cast<size_t>(state)];
}

ui::NativeTheme::ColorId GetLegacyColorId(ui::NativeTheme* native_theme,
                                          OmniboxPart part,
                                          OmniboxPartState state) {
  using NativeId = ui::NativeTheme::ColorId;
  switch (part) {
    case OmniboxPart::LOCATION_BAR_BACKGROUND:
      return NativeId::kColorId_TextfieldDefaultBackground;
    case OmniboxPart::LOCATION_BAR_CLEAR_ALL:
      return NativeId::kColorId_TextfieldDefaultColor;
    case OmniboxPart::LOCATION_BAR_IME_AUTOCOMPLETE_BACKGROUND:
      return NativeId::kColorId_TextfieldSelectionBackgroundFocused;
    case OmniboxPart::LOCATION_BAR_IME_AUTOCOMPLETE_TEXT:
      return NativeId::kColorId_TextfieldSelectionColor;
    case OmniboxPart::LOCATION_BAR_SELECTED_KEYWORD:
      return color_utils::IsDark(native_theme->GetSystemColor(
                 NativeId::kColorId_TextfieldDefaultBackground))
                 ? NativeId::kColorId_TextfieldDefaultColor
                 : NativeId::kColorId_LinkEnabled;
    case OmniboxPart::LOCATION_BAR_TEXT_DEFAULT:
      return NativeId::kColorId_TextfieldDefaultColor;
    case OmniboxPart::RESULTS_BACKGROUND:
      return NormalHoveredSelected(
          state, NativeId::kColorId_ResultsTableNormalBackground,
          NativeId::kColorId_ResultsTableHoveredBackground,
          NativeId::kColorId_ResultsTableSelectedBackground);
    case OmniboxPart::RESULTS_TEXT_DEFAULT:
      return NormalHoveredSelected(state,
                                   NativeId::kColorId_ResultsTableNormalText,
                                   NativeId::kColorId_ResultsTableHoveredText,
                                   NativeId::kColorId_ResultsTableSelectedText);
    case OmniboxPart::RESULTS_TEXT_DIMMED:
      return NormalHoveredSelected(
          state, NativeId::kColorId_ResultsTableNormalDimmedText,
          NativeId::kColorId_ResultsTableHoveredDimmedText,
          NativeId::kColorId_ResultsTableSelectedDimmedText);
    case OmniboxPart::RESULTS_TEXT_NEGATIVE:
      return NormalHoveredSelected(
          state, NativeId::kColorId_ResultsTableNegativeText,
          NativeId::kColorId_ResultsTableNegativeHoveredText,
          NativeId::kColorId_ResultsTableNegativeSelectedText);
    case OmniboxPart::RESULTS_TEXT_POSITIVE:
      return NormalHoveredSelected(
          state, NativeId::kColorId_ResultsTablePositiveText,
          NativeId::kColorId_ResultsTablePositiveHoveredText,
          NativeId::kColorId_ResultsTablePositiveSelectedText);
    case OmniboxPart::RESULTS_TEXT_URL:
      return NormalHoveredSelected(state,
                                   NativeId::kColorId_ResultsTableNormalUrl,
                                   NativeId::kColorId_ResultsTableHoveredUrl,
                                   NativeId::kColorId_ResultsTableSelectedUrl);

    case OmniboxPart::LOCATION_BAR_SECURITY_CHIP:
    case OmniboxPart::LOCATION_BAR_TEXT_DIMMED:
    case OmniboxPart::RESULTS_ICON:
    case OmniboxPart::RESULTS_TEXT_INVISIBLE:
    case OmniboxPart::RESULTS_SEPARATOR:
      NOTREACHED();
      break;
  }
  return kInvalidColorId;
}

SkColor GetLegacyColor(OmniboxPart part,
                       OmniboxTint tint,
                       OmniboxPartState state);

SkColor GetLegacySecurityChipColor(OmniboxTint tint, OmniboxPartState state) {
  SkColor background_color =
      GetLegacyColor(OmniboxPart::LOCATION_BAR_BACKGROUND, tint, state);
  const bool is_dark = color_utils::IsDark(background_color);

  SkColor chip_color = gfx::kPlaceholderColor;
  switch (state) {
    case OmniboxPartState::CHIP_DEFAULT:
      chip_color = color_utils::DeriveDefaultIconColor(
          GetLegacyColor(OmniboxPart::LOCATION_BAR_TEXT_DEFAULT, tint, state));
      break;
    case OmniboxPartState::CHIP_SECURE:
      chip_color = is_dark ? SK_ColorWHITE : gfx::kGoogleGreen700;
      break;
    case OmniboxPartState::CHIP_DANGEROUS:
      chip_color = is_dark ? SK_ColorWHITE : gfx::kGoogleRed700;
      break;
    default:
      NOTREACHED();
  }

  return color_utils::GetReadableColor(chip_color, background_color);
}

SkColor GetLegacyResultsIconColor(OmniboxTint tint, OmniboxPartState state) {
  SkColor color =
      GetLegacyColor(OmniboxPart::RESULTS_TEXT_DEFAULT, tint, state);

  // Selected rows have the same icon color as text color.
  switch (state) {
    case OmniboxPartState::NORMAL:
    case OmniboxPartState::HOVERED:
      return color_utils::DeriveDefaultIconColor(color);
    case OmniboxPartState::SELECTED:
    case OmniboxPartState::HOVERED_AND_SELECTED:
      return color;
    default:
      NOTREACHED();
  }
  return gfx::kPlaceholderColor;
}

SkColor GetLegacyColor(OmniboxPart part,
                       OmniboxTint tint,
                       OmniboxPartState state) {
  switch (part) {
    case OmniboxPart::LOCATION_BAR_SECURITY_CHIP:
      return GetLegacySecurityChipColor(tint, state);
    case OmniboxPart::LOCATION_BAR_TEXT_DIMMED:
      return color_utils::AlphaBlend(
          GetLegacyColor(OmniboxPart::LOCATION_BAR_TEXT_DEFAULT, tint, state),
          GetLegacyColor(OmniboxPart::LOCATION_BAR_BACKGROUND, tint, state),
          128);
    case OmniboxPart::RESULTS_ICON:
      return GetLegacyResultsIconColor(tint, state);
    case OmniboxPart::RESULTS_TEXT_INVISIBLE:
      return SK_ColorTRANSPARENT;
    default:
      break;
  }

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

  ui::NativeTheme::ColorId color_id =
      GetLegacyColorId(native_theme, part, state);

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
    case OmniboxPart::LOCATION_BAR_BACKGROUND:
    case OmniboxPart::LOCATION_BAR_CLEAR_ALL:
    case OmniboxPart::LOCATION_BAR_IME_AUTOCOMPLETE_BACKGROUND:
    case OmniboxPart::LOCATION_BAR_IME_AUTOCOMPLETE_TEXT:
    case OmniboxPart::LOCATION_BAR_SECURITY_CHIP:
    case OmniboxPart::LOCATION_BAR_SELECTED_KEYWORD:
    case OmniboxPart::LOCATION_BAR_TEXT_DEFAULT:
    case OmniboxPart::LOCATION_BAR_TEXT_DIMMED:
    case OmniboxPart::RESULTS_ICON:
    case OmniboxPart::RESULTS_TEXT_DEFAULT:
    case OmniboxPart::RESULTS_TEXT_DIMMED:
    case OmniboxPart::RESULTS_TEXT_INVISIBLE:
    case OmniboxPart::RESULTS_TEXT_NEGATIVE:
    case OmniboxPart::RESULTS_TEXT_POSITIVE:
    case OmniboxPart::RESULTS_TEXT_URL:
      return GetLegacyColor(part, tint, state);
  }
  return gfx::kPlaceholderColor;
}
