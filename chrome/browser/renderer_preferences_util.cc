// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_preferences_util.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/common/renderer_preferences.h"
#include "third_party/skia/include/core/SkColor.h"

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "ui/gfx/font_render_params.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "ui/views/controls/textfield/textfield.h"
#endif

#if defined(USE_AURA) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "ui/views/linux_ui/linux_ui.h"
#endif

namespace renderer_preferences_util {

namespace {

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
  prefs->accept_languages = pref_service->GetString(prefs::kAcceptLanguages);
  prefs->enable_referrers = pref_service->GetBoolean(prefs::kEnableReferrers);
  prefs->enable_do_not_track =
      pref_service->GetBoolean(prefs::kEnableDoNotTrack);
  prefs->default_zoom_level = pref_service->GetDouble(prefs::kDefaultZoomLevel);

#if defined(USE_DEFAULT_RENDER_THEME)
  prefs->focus_ring_color = SkColorSetRGB(0x4D, 0x90, 0xFE);

#if defined(OS_CHROMEOS)
  // This color is 0x544d90fe modulated with 0xffffff.
  prefs->active_selection_bg_color = SkColorSetRGB(0xCB, 0xE4, 0xFA);
  prefs->active_selection_fg_color = SK_ColorBLACK;
  prefs->inactive_selection_bg_color = SkColorSetRGB(0xEA, 0xEA, 0xEA);
  prefs->inactive_selection_fg_color = SK_ColorBLACK;
#endif

  prefs->touchpad_fling_profile[0] =
      pref_service->GetDouble(prefs::kFlingCurveTouchpadAlpha);
  prefs->touchpad_fling_profile[1] =
      pref_service->GetDouble(prefs::kFlingCurveTouchpadBeta);
  prefs->touchpad_fling_profile[2] =
      pref_service->GetDouble(prefs::kFlingCurveTouchpadGamma);
  prefs->touchscreen_fling_profile[0] =
      pref_service->GetDouble(prefs::kFlingCurveTouchscreenAlpha);
  prefs->touchscreen_fling_profile[1] =
      pref_service->GetDouble(prefs::kFlingCurveTouchscreenBeta);
  prefs->touchscreen_fling_profile[2] =
      pref_service->GetDouble(prefs::kFlingCurveTouchscreenGamma);
#endif

#if defined(TOOLKIT_VIEWS)
  prefs->caret_blink_interval = views::Textfield::GetCaretBlinkMs() / 1000.0;
#endif

#if defined(USE_AURA) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
  views::LinuxUI* linux_ui = views::LinuxUI::instance();
  if (linux_ui) {
    if (ThemeServiceFactory::GetForProfile(profile)->UsingSystemTheme()) {
      prefs->focus_ring_color = linux_ui->GetFocusRingColor();
      prefs->thumb_active_color = linux_ui->GetThumbActiveColor();
      prefs->thumb_inactive_color = linux_ui->GetThumbInactiveColor();
      prefs->track_color = linux_ui->GetTrackColor();
      prefs->active_selection_bg_color = linux_ui->GetActiveSelectionBgColor();
      prefs->active_selection_fg_color = linux_ui->GetActiveSelectionFgColor();
      prefs->inactive_selection_bg_color =
        linux_ui->GetInactiveSelectionBgColor();
      prefs->inactive_selection_fg_color =
        linux_ui->GetInactiveSelectionFgColor();
    }

    // If we have a linux_ui object, set the caret blink interval regardless of
    // whether we're in native theme mode.
    prefs->caret_blink_interval = linux_ui->GetCursorBlinkInterval();
  }
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

#if !defined(OS_MACOSX)
  prefs->plugin_fullscreen_allowed =
      pref_service->GetBoolean(prefs::kFullscreenAllowed);
#endif
}

}  // namespace renderer_preferences_util
