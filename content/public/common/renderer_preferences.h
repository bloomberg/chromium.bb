// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A struct for managing browser's settings that apply to the renderer or its
// webview.  These differ from WebPreferences since they apply to Chromium's
// glue layer rather than applying to just WebKit.
//
// Adding new values to this class probably involves updating
// common/view_messages.h, browser/browser.cc, etc.

#ifndef CONTENT_PUBLIC_COMMON_RENDERER_PREFERENCES_H_
#define CONTENT_PUBLIC_COMMON_RENDERER_PREFERENCES_H_

#include <string>

#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {

enum RendererPreferencesHintingEnum {
  RENDERER_PREFERENCES_HINTING_SYSTEM_DEFAULT = 0,
  RENDERER_PREFERENCES_HINTING_NONE,
  RENDERER_PREFERENCES_HINTING_SLIGHT,
  RENDERER_PREFERENCES_HINTING_MEDIUM,
  RENDERER_PREFERENCES_HINTING_FULL,
};

enum RendererPreferencesSubpixelRenderingEnum {
  RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT = 0,
  RENDERER_PREFERENCES_SUBPIXEL_RENDERING_NONE,
  RENDERER_PREFERENCES_SUBPIXEL_RENDERING_RGB,
  RENDERER_PREFERENCES_SUBPIXEL_RENDERING_BGR,
  RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VRGB,
  RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VBGR,
};

struct CONTENT_EXPORT RendererPreferences {
  RendererPreferences();

  // Whether the renderer's current browser context accept drops from the OS
  // that result in navigations away from the current page.
  bool can_accept_load_drops;

  // Whether text should be antialiased.
  // Currently only used by Linux.
  bool should_antialias_text;

  // The level of hinting to use when rendering text.
  // Currently only used by Linux.
  RendererPreferencesHintingEnum hinting;

  // Whether auto hinter should be used. Currently only used by Linux.
  bool use_autohinter;

  // Whether embedded bitmap strikes in fonts should be used.
  // Current only used by Linux.
  bool use_bitmaps;

  // The type of subpixel rendering to use for text.
  // Currently only used by Linux.
  RendererPreferencesSubpixelRenderingEnum subpixel_rendering;

  // Whether subpixel positioning should be used, permitting fractional X
  // positions for glyphs.  Currently only used by Linux.
  bool use_subpixel_positioning;

  // The color of the focus ring. Currently only used on Linux.
  SkColor focus_ring_color;

  // The color of different parts of the scrollbar. Currently only used on
  // Linux.
  SkColor thumb_active_color;
  SkColor thumb_inactive_color;
  SkColor track_color;

  // The colors used in selection text. Currently only used on Linux and Ash.
  SkColor active_selection_bg_color;
  SkColor active_selection_fg_color;
  SkColor inactive_selection_bg_color;
  SkColor inactive_selection_fg_color;

  // Browser wants a look at all non-local top level navigation requests.
  bool browser_handles_non_local_top_level_requests;

  // Browser wants a look at all top-level navigation requests.
  bool browser_handles_all_top_level_requests;

  // Cursor blink rate in seconds.
  // Currently only changed from default on Linux.  Uses |gtk-cursor-blink|
  // from GtkSettings.
  double caret_blink_interval;

  // Whether or not to set custom colors at all.
  bool use_custom_colors;

  // Set to false to not send referrers.
  bool enable_referrers;

  // Set to true to indicate that the preference to set DNT to 1 is enabled.
  bool enable_do_not_track;

  // Default page zoom level.
  double default_zoom_level;

  // The user agent given to WebKit when it requests one and the user agent is
  // being overridden for the current navigation.
  std::string user_agent_override;

  // Specifies whether renderer input event throttle is enabled.
  bool throttle_input_events;

  // Specifies whether the renderer reports frame name changes to the browser
  // process.
  // TODO(fsamuel): This is a short-term workaround to avoid regressing
  // Sunspider. We need to find an efficient way to report changes to frame
  // names to the browser process. See http://crbug.com/169110 for more
  // information.
  bool report_frame_name_changes;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RENDERER_PREFERENCES_H_
