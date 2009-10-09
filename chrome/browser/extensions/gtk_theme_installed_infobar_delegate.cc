// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/gtk_theme_preview_infobar_delegate.h"

#include "chrome/browser/profile.h"

GtkThemePreviewInfobarDelegate::GtkThemePreviewInfobarDelegate(
    TabContents* tab_contents,
    const std::string& name,
    const std::string& previous_theme,
    bool previous_use_gtk_theme)
    : ThemePreviewInfobarDelegate(tab_contents, name, previous_theme),
      previous_use_gtk_theme_(previous_use_gtk_theme) {
}

bool GtkThemePreviewInfobarDelegate::Cancel() {
  if (previous_use_gtk_theme_) {
    profile()->SetNativeTheme();
    return true;
  } else {
    return ThemePreviewInfobarDelegate::Cancel();
  }
}
