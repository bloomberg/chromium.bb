// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A struct for managing browser's settings that apply to the renderer or its
// webview.  These differ from WebPreferences since they apply to Chromium's
// glue layer rather than applying to just WebKit.
//
// Adding new values to this class probably involves updating
// common/render_messages.h, browser/browser.cc, etc.

#ifndef CHROME_COMMON_RENDERER_PREFERENCES_H_
#define CHROME_COMMON_RENDERER_PREFERENCES_H_
#pragma once

#include "third_party/skia/include/core/SkColor.h"

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

struct RendererPreferences {
  // Whether the renderer's current browser context accept drops from the OS
  // that result in navigations away from the current page.
  bool can_accept_load_drops;

  // Whether text should be antialiased.
  // Currently only used by Linux.
  bool should_antialias_text;

  // The level of hinting to use when rendering text.
  // Currently only used by Linux.
  RendererPreferencesHintingEnum hinting;

  // The type of subpixel rendering to use for text.
  // Currently only used by Linux.
  RendererPreferencesSubpixelRenderingEnum subpixel_rendering;

  // The color of the focus ring. Currently only used on Linux.
  SkColor focus_ring_color;

  // The color of different parts of the scrollbar. Currently only used on
  // Linux.
  SkColor thumb_active_color;
  SkColor thumb_inactive_color;
  SkColor track_color;

  // The colors used in selection text. Currently only used on Linux.
  SkColor active_selection_bg_color;
  SkColor active_selection_fg_color;
  SkColor inactive_selection_bg_color;
  SkColor inactive_selection_fg_color;

  // Browser wants a look at all top level requests
  bool browser_handles_top_level_requests;

  // Cursor blink rate in seconds.
  // Currently only changed from default on Linux.  Uses |gtk-cursor-blink|
  // from GtkSettings.
  double caret_blink_interval;

  RendererPreferences()
      : can_accept_load_drops(true),
        should_antialias_text(true),
        hinting(RENDERER_PREFERENCES_HINTING_SYSTEM_DEFAULT),
        subpixel_rendering(
            RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT),
        focus_ring_color(0),
        thumb_active_color(0),
        thumb_inactive_color(0),
        track_color(0),
        active_selection_bg_color(0),
        active_selection_fg_color(0),
        inactive_selection_bg_color(0),
        inactive_selection_fg_color(0),
        browser_handles_top_level_requests(false),
        caret_blink_interval(0) {
  }
};

#endif  // CHROME_COMMON_RENDERER_PREFERENCES_H_
