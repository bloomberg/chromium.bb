// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/renderer_preferences.h"

namespace content {

RendererPreferences::RendererPreferences()
    : can_accept_load_drops(true),
      should_antialias_text(true),
      hinting(RENDERER_PREFERENCES_HINTING_SYSTEM_DEFAULT),
      subpixel_rendering(
          RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT),
      use_subpixel_positioning(false),
      focus_ring_color(0),
      thumb_active_color(0),
      thumb_inactive_color(0),
      track_color(0),
      active_selection_bg_color(0),
      active_selection_fg_color(0),
      inactive_selection_bg_color(0),
      inactive_selection_fg_color(0),
      browser_handles_non_local_top_level_requests(false),
      browser_handles_all_top_level_requests(false),
      caret_blink_interval(0),
      enable_referrers(true),
      default_zoom_level(0) {
}

}  // namespace content
