// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_preferences_util.h"

#include "chrome/browser/profile.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/gtk_util.h"
#endif

namespace renderer_preferences_util {

void UpdateFromSystemSettings(RendererPreferences* prefs, Profile* profile) {
#if defined(TOOLKIT_USES_GTK)
  gtk_util::UpdateGtkFontSettings(prefs);

#if !defined(TOOLKIT_VIEWS)
  GtkThemeProvider* provider = GtkThemeProvider::GetFrom(profile);

  prefs->focus_ring_color = provider->get_focus_ring_color();
  prefs->thumb_active_color = provider->get_thumb_active_color();
  prefs->thumb_inactive_color = provider->get_thumb_inactive_color();
  prefs->track_color = provider->get_track_color();
  prefs->active_selection_bg_color = provider->get_active_selection_bg_color();
  prefs->active_selection_fg_color = provider->get_active_selection_fg_color();
  prefs->inactive_selection_bg_color =
      provider->get_inactive_selection_bg_color();
  prefs->inactive_selection_fg_color =
      provider->get_inactive_selection_fg_color();
#endif  // !defined(TOOLKIT_VIEWS)

#if defined(OS_CHROMEOS)
  prefs->focus_ring_color = SkColorSetARGB(255, 229, 151, 0);
  prefs->active_selection_bg_color = SkColorSetRGB(0xDC, 0xE4, 0xFA);
  prefs->active_selection_fg_color = SK_ColorBLACK;
  prefs->inactive_selection_bg_color = SkColorSetRGB(0xF7, 0xF7, 0xF7);
  prefs->inactive_selection_fg_color = SK_ColorBLACK;
#endif  // defined(OS_CHROMEOS)

#endif  // defined(TOOLKIT_USES_GTK)
}

}  // renderer_preferences_util
