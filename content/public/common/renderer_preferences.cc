// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/renderer_preferences.h"

#include "third_party/skia/include/core/SkColor.h"

namespace content {

RendererPreferences::RendererPreferences()
    : can_accept_load_drops(true),
      should_antialias_text(true),
      hinting(RENDERER_PREFERENCES_HINTING_SYSTEM_DEFAULT),
      use_autohinter(false),
      use_bitmaps(false),
      subpixel_rendering(
          RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT),
      use_subpixel_positioning(false),
      focus_ring_color(SkColorSetARGB(255, 229, 151, 0)),
      thumb_active_color(SkColorSetRGB(244, 244, 244)),
      thumb_inactive_color(SkColorSetRGB(234, 234, 234)),
      track_color(SkColorSetRGB(211, 211, 211)),
      active_selection_bg_color(SkColorSetRGB(30, 144, 255)),
      active_selection_fg_color(SK_ColorWHITE),
      inactive_selection_bg_color(SkColorSetRGB(200, 200, 200)),
      inactive_selection_fg_color(SkColorSetRGB(50, 50, 50)),
      browser_handles_non_local_top_level_requests(false),
      browser_handles_all_top_level_requests(false),
      caret_blink_interval(0),
      use_custom_colors(true),
      enable_referrers(true),
      enable_do_not_track(false),
      default_zoom_level(0),
      throttle_input_events(true),
      report_frame_name_changes(false) {
}

}  // namespace content
