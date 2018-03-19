// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/color_scheme.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "chrome/browser/vr/assets_loader.h"
#include "ui/gfx/color_palette.h"

namespace vr {

namespace {

base::LazyInstance<ColorScheme>::Leaky g_normal_scheme =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<ColorScheme>::Leaky g_fullscreen_scheme =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<ColorScheme>::Leaky g_incognito_scheme =
    LAZY_INSTANCE_INITIALIZER;

void InitializeColorSchemes() {
  static bool initialized = false;
  if (initialized)
    return;

  ColorScheme& normal_scheme = g_normal_scheme.Get();
  normal_scheme.world_background = 0xFF999999;
  normal_scheme.floor = 0xFF8C8C8C;
  normal_scheme.ceiling = normal_scheme.floor;
  normal_scheme.floor_grid = 0x26FFFFFF;
  normal_scheme.web_vr_background = SK_ColorBLACK;
  normal_scheme.element_foreground = 0xFF333333;
  normal_scheme.element_background = 0xCCB3B3B3;
  normal_scheme.element_background_hover = 0xCCE3E3E3;
  normal_scheme.element_background_down = 0xCCF3F3F3;
  normal_scheme.button_colors.foreground = 0x87000000;
  normal_scheme.button_colors.background = normal_scheme.element_background;
  normal_scheme.button_colors.background_hover =
      normal_scheme.element_background_hover;
  normal_scheme.button_colors.background_down =
      normal_scheme.element_background_down;
  normal_scheme.button_colors.foreground_disabled = 0x33333333;
  normal_scheme.loading_indicator_foreground = 0xFF2979FF;
  normal_scheme.loading_indicator_background = 0xFF454545;
  normal_scheme.exit_warning_foreground = SK_ColorWHITE;
  normal_scheme.exit_warning_background = 0xCC1A1A1A;
  normal_scheme.web_vr_transient_toast_foreground = 0xFFF3F3F3;
  normal_scheme.web_vr_transient_toast_background = SK_ColorBLACK;
  normal_scheme.exclusive_screen_toast_foreground = 0xCCFFFFFF;
  normal_scheme.exclusive_screen_toast_background = 0xCC2F2F2F;

  normal_scheme.system_indicator_foreground = normal_scheme.element_foreground;
  normal_scheme.system_indicator_background = normal_scheme.element_background;
  normal_scheme.modal_prompt_icon_foreground = 0xFF4285F4;
  normal_scheme.modal_prompt_background = 0xFFF5F5F5;
  normal_scheme.modal_prompt_foreground = 0xFF333333;
  normal_scheme.modal_prompt_secondary_button_colors.foreground = 0xFF4285F4;
  normal_scheme.modal_prompt_secondary_button_colors.foreground_disabled =
      normal_scheme.button_colors.foreground_disabled;
  normal_scheme.modal_prompt_secondary_button_colors.background =
      normal_scheme.modal_prompt_background;
  normal_scheme.modal_prompt_secondary_button_colors.background_hover =
      0x19999999;
  normal_scheme.modal_prompt_secondary_button_colors.background_down =
      0x33999999;
  normal_scheme.modal_prompt_primary_button_colors.foreground =
      normal_scheme.modal_prompt_background;
  normal_scheme.modal_prompt_secondary_button_colors.foreground_disabled =
      normal_scheme.button_colors.foreground_disabled;
  normal_scheme.modal_prompt_primary_button_colors.background = 0xFF4285F4;
  normal_scheme.modal_prompt_primary_button_colors.background_hover =
      0xFF3E7DE6;
  normal_scheme.modal_prompt_primary_button_colors.background_down = 0xFF3E7DE6;
  normal_scheme.back_button.background = normal_scheme.element_background;
  normal_scheme.back_button.background_down =
      normal_scheme.element_background_down;
  normal_scheme.back_button.background_hover =
      normal_scheme.element_background_hover;
  normal_scheme.back_button.foreground = normal_scheme.element_foreground;
  normal_scheme.back_button.foreground_disabled = 0x33333333;
  normal_scheme.url_bar_separator = 0xFF9E9E9E;
  normal_scheme.url_bar_hint = 0xFF5A5A5A;
  normal_scheme.url_bar.deemphasized = 0xFF5A5A5A;
  normal_scheme.url_bar.emphasized = SK_ColorBLACK;
  normal_scheme.url_bar.default_icon = 0xFF535353;
  normal_scheme.url_bar.dangerous_icon = gfx::kGoogleRed700;
  normal_scheme.url_bar.offline_page_warning =
      normal_scheme.url_bar.default_icon;
  normal_scheme.url_bar.separator = normal_scheme.url_bar_separator;
  normal_scheme.prompt_foreground = 0xCC000000;
  normal_scheme.prompt_primary_button_colors.foreground = 0xA6000000;
  normal_scheme.prompt_primary_button_colors.foreground_disabled = 0xA6000000;
  normal_scheme.prompt_primary_button_colors.background = 0xBFFFFFFF;
  normal_scheme.prompt_primary_button_colors.background_hover = 0xFFFFFFFF;
  normal_scheme.prompt_primary_button_colors.background_down = 0xE6FFFFFF;
  normal_scheme.prompt_secondary_button_colors.foreground = 0xA6000000;
  normal_scheme.prompt_secondary_button_colors.foreground_disabled = 0xA6000000;
  normal_scheme.prompt_secondary_button_colors.background = 0x66FFFFFF;
  normal_scheme.prompt_secondary_button_colors.background_hover = 0xFFFFFFFF;
  normal_scheme.prompt_secondary_button_colors.background_down = 0xE6FFFFFF;
  normal_scheme.dimmer_inner = 0xCC0D0D0D;
  normal_scheme.dimmer_outer = 0xE6000000;
  normal_scheme.splash_screen_background = SK_ColorBLACK;
  normal_scheme.splash_screen_text_color = 0xA6FFFFFF;
  normal_scheme.web_vr_timeout_spinner = 0xFFF3F3F3;
  normal_scheme.web_vr_timeout_message_background = 0xFF444444;
  normal_scheme.web_vr_timeout_message_foreground =
      normal_scheme.web_vr_timeout_spinner;
  normal_scheme.speech_recognition_circle_background = 0xFF4285F4;
  normal_scheme.omnibox_background = 0xFFEEEEEE;
  normal_scheme.omnibox_icon = 0xA6000000;
  normal_scheme.omnibox_text = 0xFF595959;
  normal_scheme.omnibox_hint = 0xFF999999;
  normal_scheme.omnibox_text_selection.cursor = 0xFF5595FE;
  normal_scheme.omnibox_text_selection.background = 0xFFC6DAFC;
  normal_scheme.omnibox_text_selection.foreground = normal_scheme.omnibox_text;
  normal_scheme.suggestion_text = 0xFF595959;
  normal_scheme.suggestion_dim_text = 0xFF999999;
  normal_scheme.suggestion_url_text = 0xFF5595FE;
  normal_scheme.suggestion_button_colors.foreground =
      normal_scheme.suggestion_text;
  normal_scheme.suggestion_button_colors.background =
      normal_scheme.omnibox_background;
  normal_scheme.suggestion_button_colors.background_hover = 0xFFE0E0E0;
  normal_scheme.suggestion_button_colors.background_down = 0xFFE0E0E0;
  normal_scheme.omnibox_voice_search_button_colors.foreground =
      normal_scheme.suggestion_text;
  normal_scheme.omnibox_voice_search_button_colors.background =
      normal_scheme.omnibox_background;
  normal_scheme.omnibox_voice_search_button_colors.background_hover =
      0x14000000;
  normal_scheme.omnibox_voice_search_button_colors.background_down = 0x14000000;

  normal_scheme.snackbar_foreground = 0xFFEEEEEE;
  normal_scheme.snackbar_background = 0xDD212121;
  normal_scheme.snackbar_button_colors.background =
      normal_scheme.snackbar_background;
  normal_scheme.snackbar_button_colors.foreground = 0xFFFFD500;
  normal_scheme.snackbar_button_colors.background_hover = 0xDD2D2D2D;
  normal_scheme.snackbar_button_colors.background_down = 0xDD2D2D2D;

  normal_scheme.controller_label_callout = SK_ColorWHITE;
  normal_scheme.controller_button = 0xFFEFEFEF;
  normal_scheme.controller_button_down = 0xFF2979FF;

  normal_scheme.reposition_label = SK_ColorWHITE;
  normal_scheme.reposition_label_background = 0xAA333333;

  normal_scheme.normal_factor = 1.0f;
  normal_scheme.incognito_factor = 0.0f;
  normal_scheme.fullscreen_factor = 0.0f;

  normal_scheme.content_reposition_frame = 0x66FFFFFF;

  normal_scheme.cursor_background_center = 0x24000000;
  normal_scheme.cursor_background_edge = SK_ColorTRANSPARENT;
  normal_scheme.cursor_foreground = SK_ColorWHITE;

  g_fullscreen_scheme.Get() = normal_scheme;
  ColorScheme& fullscreen_scheme = g_fullscreen_scheme.Get();
  fullscreen_scheme.world_background = 0xFF000714;
  fullscreen_scheme.floor = 0xFF070F1C;
  fullscreen_scheme.ceiling = 0xFF04080F;
  fullscreen_scheme.floor_grid = 0x40A3E0FF;
  fullscreen_scheme.element_foreground = 0x80FFFFFF;
  fullscreen_scheme.element_background = 0xCC2B3E48;
  fullscreen_scheme.element_background_hover = 0xCC536B77;
  fullscreen_scheme.element_background_down = 0xCC96AFBB;

  fullscreen_scheme.button_colors.foreground =
      fullscreen_scheme.element_foreground;
  fullscreen_scheme.button_colors.foreground_disabled =
      fullscreen_scheme.element_foreground;
  fullscreen_scheme.button_colors.background =
      fullscreen_scheme.element_background;
  fullscreen_scheme.button_colors.background_hover =
      fullscreen_scheme.element_background_hover;
  fullscreen_scheme.button_colors.background_down =
      fullscreen_scheme.element_background_down;

  fullscreen_scheme.system_indicator_foreground =
      fullscreen_scheme.element_foreground;
  fullscreen_scheme.system_indicator_background =
      fullscreen_scheme.element_background;

  fullscreen_scheme.normal_factor = 0.0f;
  fullscreen_scheme.incognito_factor = 0.0f;
  fullscreen_scheme.fullscreen_factor = 1.0f;

  g_incognito_scheme.Get() = normal_scheme;
  ColorScheme& incognito_scheme = g_incognito_scheme.Get();
  incognito_scheme.world_background = 0xFF2E2E2E;
  incognito_scheme.floor = 0xFF282828;
  incognito_scheme.ceiling = 0xFF2F2F2F;
  incognito_scheme.floor_grid = 0xCC595959;
  incognito_scheme.element_foreground = 0xFFFFFFFF;
  incognito_scheme.element_background = 0xFF454545;
  incognito_scheme.element_background_hover = 0xCC505050;
  incognito_scheme.element_background_down = 0xCC888888;

  incognito_scheme.button_colors.foreground =
      fullscreen_scheme.element_foreground;
  incognito_scheme.button_colors.foreground_disabled = 0x33E6E6E6;
  incognito_scheme.button_colors.background =
      fullscreen_scheme.element_background;
  incognito_scheme.button_colors.background_hover =
      fullscreen_scheme.element_background_hover;
  incognito_scheme.button_colors.background_down =
      fullscreen_scheme.element_background_down;

  incognito_scheme.system_indicator_foreground =
      incognito_scheme.element_foreground;
  incognito_scheme.system_indicator_background =
      incognito_scheme.element_background;
  incognito_scheme.back_button.background = incognito_scheme.element_background;
  incognito_scheme.back_button.background_down =
      incognito_scheme.element_background_down;
  incognito_scheme.back_button.background_hover =
      incognito_scheme.element_background_hover;
  incognito_scheme.back_button.foreground = incognito_scheme.element_foreground;
  incognito_scheme.back_button.foreground_disabled = 0x33E6E6E6;
  incognito_scheme.url_bar_separator = 0x1FFFFFFF;
  incognito_scheme.url_bar_hint = 0xCCFFFFFF;
  incognito_scheme.url_bar.deemphasized = 0xCCFFFFFF;
  incognito_scheme.url_bar.emphasized = SK_ColorWHITE;
  incognito_scheme.url_bar.default_icon = SK_ColorWHITE;
  incognito_scheme.url_bar.dangerous_icon = SK_ColorWHITE;
  incognito_scheme.url_bar.offline_page_warning = SK_ColorWHITE;
  incognito_scheme.url_bar.separator = incognito_scheme.url_bar_separator;
  incognito_scheme.prompt_foreground = 0xCCFFFFFF;
  incognito_scheme.prompt_primary_button_colors.foreground = 0xD9000000;
  incognito_scheme.prompt_primary_button_colors.foreground_disabled =
      0xD9000000;
  incognito_scheme.prompt_primary_button_colors.background = 0xD9FFFFFF;
  incognito_scheme.prompt_primary_button_colors.background_hover = 0xFF8C8C8C;
  incognito_scheme.prompt_primary_button_colors.background_down = 0xE6FFFFFF;
  incognito_scheme.prompt_secondary_button_colors.foreground = 0xD9000000;
  incognito_scheme.prompt_secondary_button_colors.foreground_disabled =
      0xD9000000;
  incognito_scheme.prompt_secondary_button_colors.background = 0x80FFFFFF;
  incognito_scheme.prompt_secondary_button_colors.background_hover = 0xFF8C8C8C;
  incognito_scheme.prompt_secondary_button_colors.background_down = 0xE6FFFFFF;
  incognito_scheme.omnibox_background = 0xFF454545;
  incognito_scheme.omnibox_icon = 0xCCFFFFFF;
  incognito_scheme.omnibox_text = 0xCCFFFFFF;
  incognito_scheme.omnibox_hint = 0x80FFFFFF;
  incognito_scheme.omnibox_text_selection.foreground =
      incognito_scheme.omnibox_text;
  incognito_scheme.omnibox_text_selection.background =
      incognito_scheme.omnibox_text_selection.cursor;
  incognito_scheme.suggestion_text = 0xCCFFFFFF;
  incognito_scheme.suggestion_dim_text = 0x88FFFFFF;
  incognito_scheme.suggestion_url_text = 0xFF5595FE;
  incognito_scheme.suggestion_button_colors.foreground =
      incognito_scheme.suggestion_text;
  incognito_scheme.suggestion_button_colors.background =
      incognito_scheme.omnibox_background;
  incognito_scheme.suggestion_button_colors.background_hover = 0xFF656565;
  incognito_scheme.suggestion_button_colors.background_down = 0xFF656565;

  incognito_scheme.normal_factor = 0.0f;
  incognito_scheme.incognito_factor = 1.0f;
  incognito_scheme.fullscreen_factor = 0.0f;

  initialized = true;
}

static constexpr size_t kButtonColorsSize = 20;
static constexpr size_t kUrlBarColorsSize = 24;

}  // namespace

ColorScheme::ColorScheme() = default;
ColorScheme::ColorScheme(const ColorScheme& other) {
  *this = other;
}

static_assert(kButtonColorsSize == sizeof(ButtonColors),
              "If the new colors are added to ButtonColors, we must explicitly "
              "bump this size and update operator== below");

bool ButtonColors::operator==(const ButtonColors& other) const {
  return background == other.background &&
         background_hover == other.background_hover &&
         background_down == other.background_down &&
         foreground == other.foreground &&
         foreground_disabled == other.foreground_disabled;
}

bool ButtonColors::operator!=(const ButtonColors& other) const {
  return !(*this == other);
}

SkColor ButtonColors::GetBackgroundColor(bool hovered, bool pressed) const {
  if (pressed)
    return background_down;
  if (hovered)
    return background_hover;
  return background;
}

SkColor ButtonColors::GetForegroundColor(bool disabled) const {
  return disabled ? foreground_disabled : foreground;
}

static_assert(kUrlBarColorsSize == sizeof(UrlBarColors),
              "If the new colors are added to UrlBarColors, we must explicitly "
              "bump this size and update operator== below");

bool UrlBarColors::operator==(const UrlBarColors& other) const {
  return deemphasized == other.deemphasized && emphasized == other.emphasized &&
         default_icon == other.default_icon &&
         dangerous_icon == other.dangerous_icon &&
         offline_page_warning == other.offline_page_warning &&
         separator == other.separator;
}

bool UrlBarColors::operator!=(const UrlBarColors& other) const {
  return !(*this == other);
}

bool TextSelectionColors::operator==(const TextSelectionColors& other) const {
  return cursor == other.cursor && background == other.background &&
         foreground == other.foreground;
}

bool TextSelectionColors::operator!=(const TextSelectionColors& other) const {
  return !(*this == other);
}

const ColorScheme& ColorScheme::GetColorScheme(ColorScheme::Mode mode) {
  InitializeColorSchemes();
  if (mode == kModeIncognito)
    return g_incognito_scheme.Get();
  if (mode == kModeFullscreen)
    return g_fullscreen_scheme.Get();
  return g_normal_scheme.Get();
}

void ColorScheme::UpdateForComponent(const base::Version& component_version) {
  if (component_version >= AssetsLoader::MinVersionWithGradients()) {
    ColorScheme& normal_scheme = g_normal_scheme.Get();
    normal_scheme.element_background = 0xFFEEEEEE;
    normal_scheme.element_background_hover = 0xFFFFFFFF;
    normal_scheme.button_colors.foreground = 0xA6000000;
    normal_scheme.button_colors.background = 0xCCEEEEEE;
    normal_scheme.button_colors.foreground_disabled = 0x33000000;
    normal_scheme.url_bar_separator = 0xFFD0D0D0;
    normal_scheme.url_bar_hint = 0x61333333;
    normal_scheme.url_bar.deemphasized = 0x61333333;
    normal_scheme.url_bar.emphasized = 0xFF333333;

    normal_scheme.back_button.background = normal_scheme.element_background;
    normal_scheme.back_button.background_down =
        normal_scheme.element_background_down;
    normal_scheme.back_button.background_hover =
        normal_scheme.element_background_hover;
    normal_scheme.back_button.foreground = normal_scheme.element_foreground;
    normal_scheme.back_button.foreground_disabled = 0x33333333;

    normal_scheme.button_colors.background = normal_scheme.element_background;
    normal_scheme.button_colors.background_hover =
        normal_scheme.element_background_hover;
    normal_scheme.button_colors.background_down =
        normal_scheme.element_background_down;
    normal_scheme.system_indicator_foreground =
        normal_scheme.element_foreground;
    normal_scheme.system_indicator_background =
        normal_scheme.element_background;

    normal_scheme.modal_prompt_secondary_button_colors.foreground_disabled =
        normal_scheme.button_colors.foreground_disabled;
    normal_scheme.modal_prompt_secondary_button_colors.background =
        normal_scheme.modal_prompt_background;

    ColorScheme& incognito_scheme = g_incognito_scheme.Get();
    incognito_scheme.element_foreground = 0xA6FFFFFF;
    incognito_scheme.element_background = 0xFF263238;
    incognito_scheme.element_background_hover = 0xCC404A50;
    incognito_scheme.element_background_down = 0xCC212B31;
    incognito_scheme.url_bar_separator = 0xFF445056;
    incognito_scheme.url_bar_hint = 0x80FFFFFF;
    incognito_scheme.url_bar.deemphasized = 0x80FFFFFF;
    incognito_scheme.url_bar.emphasized = 0xCCFFFFFF;
    incognito_scheme.url_bar.separator = 0xFF445056;
    incognito_scheme.url_bar.default_icon = 0xA7FFFFFF;
    incognito_scheme.url_bar.dangerous_icon = 0xA7FFFFFF;
    incognito_scheme.url_bar.offline_page_warning = 0xA7FFFFFF;
    incognito_scheme.omnibox_background = incognito_scheme.element_background;
    incognito_scheme.omnibox_icon = 0xA7FFFFFF;

    incognito_scheme.button_colors.background =
        incognito_scheme.element_background;
    incognito_scheme.button_colors.background_hover =
        incognito_scheme.element_background_hover;
    incognito_scheme.button_colors.background_down =
        incognito_scheme.element_background_down;

    incognito_scheme.system_indicator_foreground =
        incognito_scheme.element_foreground;
    incognito_scheme.system_indicator_background =
        incognito_scheme.element_background;
    incognito_scheme.back_button.background =
        incognito_scheme.element_background;
    incognito_scheme.back_button.background_down =
        incognito_scheme.element_background_down;
    incognito_scheme.back_button.background_hover = 0xFF445056;
    incognito_scheme.back_button.foreground =
        incognito_scheme.element_foreground;
    incognito_scheme.suggestion_button_colors.foreground =
        incognito_scheme.suggestion_text;
    incognito_scheme.suggestion_button_colors.background =
        incognito_scheme.omnibox_background;
    incognito_scheme.suggestion_button_colors.background_hover = 0xFF3A464C;
    incognito_scheme.suggestion_button_colors.background_down = 0xFF3A464C;
  }
}

}  // namespace vr
