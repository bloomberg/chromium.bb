// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/renderer_preferences_util.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "ui/gfx/font_render_params.h"

#if defined(OS_ANDROID)
#include "content/browser/android/android_ui_constants.h"
#endif

namespace content {

void UpdateFontRendererPreferencesFromSystemSettings(
    blink::mojom::RendererPreferences* prefs) {
  static const base::NoDestructor<gfx::FontRenderParams> params(
      gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), nullptr));
  prefs->should_antialias_text = params->antialiasing;
  prefs->use_subpixel_positioning = params->subpixel_positioning;
  prefs->hinting = params->hinting;
  prefs->use_autohinter = params->autohinter;
  prefs->use_bitmaps = params->use_bitmaps;
  prefs->subpixel_rendering = params->subpixel_rendering;
}

void UpdateFocusRingPreferencesFromSystemSettings(
    blink::mojom::RendererPreferences* prefs) {
#if defined(OS_ANDROID)
  prefs->is_focus_ring_outset = AndroidUiConstants::IsFocusRingOutset();

  base::Optional<float> stroke_width =
      AndroidUiConstants::GetMinimumStrokeWidthForFocusRing();
  if (stroke_width)
    prefs->minimum_stroke_width_for_focus_ring = *stroke_width;

  base::Optional<SkColor> color = AndroidUiConstants::GetFocusRingColor();
  if (color) {
    prefs->use_custom_colors = true;
    prefs->focus_ring_color = *color;
  }
#endif
}

}  // namespace content
