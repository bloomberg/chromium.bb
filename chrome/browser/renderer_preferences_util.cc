// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_preferences_util.h"

#include "chrome/browser/profile.h"

#if defined(OS_LINUX)
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/common/gtk_util.h"
#endif

namespace renderer_preferences_util {

void UpdateFromSystemSettings(RendererPreferences* prefs, Profile* profile) {
#if defined(OS_LINUX)
  gtk_util::UpdateGtkFontSettings(prefs);

#if !defined(TOOLKIT_VIEWS)
  GtkThemeProvider* provider = GtkThemeProvider::GetFrom(profile);

  prefs->focus_ring_color = provider->get_focus_ring_color();
  prefs->thumb_active_color = provider->get_thumb_active_color();
  prefs->thumb_inactive_color = provider->get_thumb_inactive_color();
  prefs->track_color = provider->get_track_color();
#endif  // !defined(TOOLKIT_VIEWS)
#endif  // defined(OS_LINUX)
}

}  // renderer_preferences_util
