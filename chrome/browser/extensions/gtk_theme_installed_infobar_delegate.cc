// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/gtk_theme_installed_infobar_delegate.h"

#include "chrome/browser/themes/theme_service.h"

GtkThemeInstalledInfoBarDelegate::GtkThemeInstalledInfoBarDelegate(
    TabContents* tab_contents,
    const Extension* new_theme,
    const std::string& previous_theme_id,
    bool previous_use_gtk_theme)
    : ThemeInstalledInfoBarDelegate(tab_contents, new_theme, previous_theme_id),
      previous_use_gtk_theme_(previous_use_gtk_theme) {
}

bool GtkThemeInstalledInfoBarDelegate::Cancel() {
  if (previous_use_gtk_theme_) {
    theme_service()->SetNativeTheme();
    return true;
  } else {
    return ThemeInstalledInfoBarDelegate::Cancel();
  }
}
