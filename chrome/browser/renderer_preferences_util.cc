// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_preferences_util.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#endif

namespace renderer_preferences_util {

void UpdateFromSystemSettings(RendererPreferences* prefs, Profile* profile) {
#if defined(TOOLKIT_USES_GTK)
  gtk_util::UpdateGtkFontSettings(prefs);

#if !defined(OS_CHROMEOS)
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
#else
  prefs->focus_ring_color = SkColorSetRGB(0x50, 0x7A, 0xD5);
  prefs->active_selection_bg_color = SkColorSetRGB(0xDC, 0xE4, 0xFA);
  prefs->active_selection_fg_color = SK_ColorBLACK;
  prefs->inactive_selection_bg_color = SkColorSetRGB(0xF7, 0xF7, 0xF7);
  prefs->inactive_selection_fg_color = SK_ColorBLACK;
#endif  // defined(OS_CHROMEOS)

#endif  // defined(TOOLKIT_USES_GTK)

  prefs->enable_referrers =
      profile->GetPrefs()->GetBoolean(prefs::kEnableReferrers);
}

}  // renderer_preferences_util
