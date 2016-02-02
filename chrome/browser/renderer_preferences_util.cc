// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_preferences_util.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/webrtc_ip_handling_policy.h"
#include "third_party/skia/include/core/SkColor.h"

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "ui/gfx/font_render_params.h"
#endif

#if !defined(OS_ANDROID)
#include "components/ui/zoom/zoom_controller.h"
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

void UpdateFromSystemSettings(content::RendererPreferences* prefs,
                              Profile* profile,
                              content::WebContents* web_contents) {
  const PrefService* pref_service = profile->GetPrefs();
  prefs->accept_languages = pref_service->GetString(prefs::kAcceptLanguages);
  prefs->enable_referrers = pref_service->GetBoolean(prefs::kEnableReferrers);
  prefs->enable_do_not_track =
      pref_service->GetBoolean(prefs::kEnableDoNotTrack);
#if defined(ENABLE_WEBRTC)
  prefs->webrtc_ip_handling_policy = std::string();
  // Handling the backward compatibility of previous boolean verions of policy
  // controls.
  if (!pref_service->HasPrefPath(prefs::kWebRTCIPHandlingPolicy)) {
    if (!pref_service->GetBoolean(prefs::kWebRTCNonProxiedUdpEnabled)) {
      prefs->webrtc_ip_handling_policy =
          content::kWebRTCIPHandlingDisableNonProxiedUdp;
    } else if (!pref_service->GetBoolean(prefs::kWebRTCMultipleRoutesEnabled)) {
      prefs->webrtc_ip_handling_policy =
          content::kWebRTCIPHandlingDefaultPublicInterfaceOnly;
    }
  }
  if (prefs->webrtc_ip_handling_policy.empty()) {
    prefs->webrtc_ip_handling_policy =
        pref_service->GetString(prefs::kWebRTCIPHandlingPolicy);
  }
#endif

  double default_zoom_level = 0;
  bool default_zoom_level_set = false;
#if !defined(OS_ANDROID)
  ui_zoom::ZoomController* zoom_controller =
      ui_zoom::ZoomController::FromWebContents(web_contents);
  if (zoom_controller) {
    default_zoom_level = zoom_controller->GetDefaultZoomLevel();
    default_zoom_level_set = true;
  }
#endif

  if (!default_zoom_level_set) {
    default_zoom_level =
        content::HostZoomMap::Get(web_contents->GetSiteInstance())
            ->GetDefaultZoomLevel();
  }
  prefs->default_zoom_level = default_zoom_level;

#if defined(USE_DEFAULT_RENDER_THEME)
  prefs->focus_ring_color = SkColorSetRGB(0x4D, 0x90, 0xFE);
#if defined(OS_CHROMEOS)
  // This color is 0x544d90fe modulated with 0xffffff.
  prefs->active_selection_bg_color = SkColorSetRGB(0xCB, 0xE4, 0xFA);
  prefs->active_selection_fg_color = SK_ColorBLACK;
  prefs->inactive_selection_bg_color = SkColorSetRGB(0xEA, 0xEA, 0xEA);
  prefs->inactive_selection_fg_color = SK_ColorBLACK;
#endif
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

#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_WIN)
  CR_DEFINE_STATIC_LOCAL(const gfx::FontRenderParams, params,
      (gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), NULL)));
  prefs->should_antialias_text = params.antialiasing;
  prefs->use_subpixel_positioning = params.subpixel_positioning;
  prefs->hinting = params.hinting;
  prefs->use_autohinter = params.autohinter;
  prefs->use_bitmaps = params.use_bitmaps;
  prefs->subpixel_rendering = params.subpixel_rendering;
#endif

#if !defined(OS_MACOSX)
  prefs->plugin_fullscreen_allowed =
      pref_service->GetBoolean(prefs::kFullscreenAllowed);
#endif
}

}  // namespace renderer_preferences_util
