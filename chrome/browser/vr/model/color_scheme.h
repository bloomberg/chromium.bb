// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_COLOR_SCHEME_H_
#define CHROME_BROWSER_VR_MODEL_COLOR_SCHEME_H_

#include "base/version.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

struct ButtonColors {
  bool operator==(const ButtonColors& other) const;
  bool operator!=(const ButtonColors& other) const;

  SkColor GetBackgroundColor(bool hovered, bool pressed) const;
  SkColor GetForegroundColor(bool disabled) const;

  SkColor background = SK_ColorBLACK;
  SkColor background_hover = SK_ColorBLACK;
  SkColor background_down = SK_ColorBLACK;
  SkColor foreground = SK_ColorBLACK;
  SkColor foreground_disabled = SK_ColorBLACK;
};

struct UrlBarColors {
  bool operator==(const UrlBarColors& other) const;
  bool operator!=(const UrlBarColors& other) const;
  SkColor deemphasized = SK_ColorBLACK;
  SkColor emphasized = SK_ColorBLACK;
  SkColor default_icon = SK_ColorBLACK;
  SkColor dangerous_icon = SK_ColorBLACK;
  SkColor offline_page_warning = SK_ColorBLACK;
  SkColor separator = SK_ColorBLACK;
};

struct TextSelectionColors {
  bool operator==(const TextSelectionColors& other) const;
  bool operator!=(const TextSelectionColors& other) const;
  SkColor cursor;
  SkColor background;
  SkColor foreground;
};

struct ColorScheme {
  enum Mode : int {
    kModeNormal = 0,
    kModeFullscreen,
    kModeIncognito,
    kNumModes,
  };

  static const ColorScheme& GetColorScheme(Mode mode);
  static void UpdateForComponent(const base::Version& component_version);

  ColorScheme();
  ColorScheme(const ColorScheme& other);

  // These colors should be named generically, if possible, so that they can be
  // meaningfully reused by multiple elements.
  SkColor world_background;
  SkColor floor;
  SkColor ceiling;
  SkColor floor_grid;
  SkColor web_vr_background;

  // The foreground color is used for text and sometimes for icons.
  SkColor element_foreground;
  // The background color is used behind text or icons in the foreground color.
  // The related hover and down colors are to be used for buttons.
  SkColor element_background;
  SkColor element_background_hover;
  SkColor element_background_down;

  // Specific element background and foregrounds
  ButtonColors button_colors;
  SkColor loading_indicator_foreground;
  SkColor loading_indicator_background;
  SkColor exit_warning_foreground;
  SkColor exit_warning_background;
  SkColor web_vr_transient_toast_foreground;
  SkColor web_vr_transient_toast_background;
  SkColor exclusive_screen_toast_foreground;
  SkColor exclusive_screen_toast_background;
  SkColor system_indicator_foreground;
  SkColor system_indicator_background;
  SkColor modal_prompt_icon_foreground;
  SkColor modal_prompt_background;
  SkColor modal_prompt_foreground;
  ButtonColors modal_prompt_secondary_button_colors;
  ButtonColors modal_prompt_primary_button_colors;

  // The colors used for text and buttons on prompts.
  SkColor prompt_foreground;
  ButtonColors prompt_secondary_button_colors;
  ButtonColors prompt_primary_button_colors;

  ButtonColors back_button;
  SkColor url_bar_separator;
  SkColor url_bar_hint;

  // These colors feed the URL origin texture.
  UrlBarColors url_bar;

  SkColor dimmer_outer;
  SkColor dimmer_inner;

  SkColor splash_screen_background;
  SkColor splash_screen_text_color;

  SkColor web_vr_timeout_spinner;
  SkColor web_vr_timeout_message_background;
  SkColor web_vr_timeout_message_foreground;

  SkColor speech_recognition_circle_background;

  SkColor omnibox_background;
  SkColor omnibox_icon;
  SkColor omnibox_text;
  SkColor omnibox_hint;
  TextSelectionColors omnibox_text_selection;
  SkColor suggestion_text;
  SkColor suggestion_dim_text;
  SkColor suggestion_url_text;
  ButtonColors omnibox_voice_search_button_colors;
  ButtonColors suggestion_button_colors;

  SkColor snackbar_background;
  SkColor snackbar_foreground;
  ButtonColors snackbar_button_colors;

  SkColor controller_label_callout;
  SkColor controller_button;
  SkColor controller_button_down;

  SkColor reposition_label;
  SkColor reposition_label_background;

  SkColor content_reposition_frame;

  SkColor cursor_background_center;
  SkColor cursor_background_edge;
  SkColor cursor_foreground;

  // These are used for blending between colors that are available only in
  // shaders. They are, as you might expect, one for a given mode, but zero
  // otherwise.
  float normal_factor = 0.0f;
  float incognito_factor = 0.0f;
  float fullscreen_factor = 0.0f;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_COLOR_SCHEME_H_
