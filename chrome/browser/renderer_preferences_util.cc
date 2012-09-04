// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_preferences_util.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/common/renderer_preferences.h"

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "ui/gfx/font_render_params_linux.h"
#endif

#if defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "ui/gfx/gtk_util.h"
#endif

namespace renderer_preferences_util {

namespace {

#if defined(TOOLKIT_GTK)
// Dividing GTK's cursor blink cycle time (in milliseconds) by this value yields
// an appropriate value for content::RendererPreferences::caret_blink_interval.
// This matches the logic in the WebKit GTK port.
const double kGtkCursorBlinkCycleFactor = 2000.0;
#endif  // defined(TOOLKIT_GTK)

#if defined(OS_LINUX) || defined(OS_ANDROID)
content::RendererPreferencesHintingEnum GetRendererPreferencesHintingEnum(
    gfx::FontRenderParams::Hinting hinting) {
  switch (hinting) {
    case gfx::FontRenderParams::HINTING_NONE:
      return content::RENDERER_PREFERENCES_HINTING_NONE;
    case gfx::FontRenderParams::HINTING_SLIGHT:
      return content::RENDERER_PREFERENCES_HINTING_SLIGHT;
    case gfx::FontRenderParams::HINTING_MEDIUM:
      return content::RENDERER_PREFERENCES_HINTING_MEDIUM;
    case gfx::FontRenderParams::HINTING_FULL:
      return content::RENDERER_PREFERENCES_HINTING_FULL;
    default:
      NOTREACHED() << "Unhandled hinting style " << hinting;
      return content::RENDERER_PREFERENCES_HINTING_SYSTEM_DEFAULT;
  }
}

content::RendererPreferencesSubpixelRenderingEnum
GetRendererPreferencesSubpixelRenderingEnum(
    gfx::FontRenderParams::SubpixelRendering subpixel_rendering) {
  switch (subpixel_rendering) {
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE:
      return content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_NONE;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB:
      return content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_RGB;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR:
      return content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_BGR;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB:
      return content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VRGB;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_VBGR:
      return content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VBGR;
    default:
      NOTREACHED() << "Unhandled subpixel rendering style "
                   << subpixel_rendering;
      return content::RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT;
  }
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

}  // namespace

void UpdateFromSystemSettings(
    content::RendererPreferences* prefs, Profile* profile) {
  const PrefService* pref_service = profile->GetPrefs();
  prefs->enable_referrers = pref_service->GetBoolean(prefs::kEnableReferrers);
  prefs->default_zoom_level = pref_service->GetDouble(prefs::kDefaultZoomLevel);

#if defined(TOOLKIT_GTK)
  GtkThemeService* theme_service = GtkThemeService::GetFrom(profile);
  prefs->focus_ring_color = theme_service->get_focus_ring_color();
  prefs->thumb_active_color = theme_service->get_thumb_active_color();
  prefs->thumb_inactive_color = theme_service->get_thumb_inactive_color();
  prefs->track_color = theme_service->get_track_color();
  prefs->active_selection_bg_color =
      theme_service->get_active_selection_bg_color();
  prefs->active_selection_fg_color =
      theme_service->get_active_selection_fg_color();
  prefs->inactive_selection_bg_color =
      theme_service->get_inactive_selection_bg_color();
  prefs->inactive_selection_fg_color =
      theme_service->get_inactive_selection_fg_color();

  const base::TimeDelta cursor_blink_time = gfx::GetCursorBlinkCycle();
  prefs->caret_blink_interval =
      cursor_blink_time.InMilliseconds() ?
      cursor_blink_time.InMilliseconds() / kGtkCursorBlinkCycleFactor :
      0;
#elif defined(USE_ASH)
  // This color is 0x544d90fe modulated with 0xffffff.
  prefs->active_selection_bg_color = SkColorSetRGB(0xCB, 0xE4, 0xFA);
  prefs->active_selection_fg_color = SK_ColorBLACK;
  prefs->inactive_selection_bg_color = SkColorSetRGB(0xEA, 0xEA, 0xEA);
  prefs->inactive_selection_fg_color = SK_ColorBLACK;
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
  const gfx::FontRenderParams& params = gfx::GetDefaultWebKitFontRenderParams();
  prefs->should_antialias_text = params.antialiasing;
  prefs->use_subpixel_positioning = params.subpixel_positioning;
  prefs->hinting = GetRendererPreferencesHintingEnum(params.hinting);
  prefs->use_autohinter = params.autohinter;
  prefs->use_bitmaps = params.use_bitmaps;
  prefs->subpixel_rendering =
      GetRendererPreferencesSubpixelRenderingEnum(params.subpixel_rendering);
#endif
}

}  // renderer_preferences_util
