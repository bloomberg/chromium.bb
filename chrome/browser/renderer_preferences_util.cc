// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_preferences_util.h"

#include "base/singleton.h"
#include "chrome/browser/profile.h"

#if defined(OS_LINUX)
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/common/gtk_util.h"
#endif

namespace renderer_preferences_util {

RendererPreferences GetInitedRendererPreferences(Profile* profile) {
  RendererPreferences* prefs = Singleton<RendererPreferences>::get();
#if defined(OS_LINUX)
  static bool inited = false;
  if (!inited) {
    gtk_util::InitRendererPrefsFromGtkSettings(prefs,
      GtkThemeProvider::GetFrom(profile)->UseGtkTheme());
    inited = true;
  }
#endif
  return *prefs;
}

}  // renderer_preferences_util
