// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ui.h"

#include "chrome/browser/themes/theme_properties.h"
#include "grit/theme_resources.h"
#include "ui/base/theme_provider.h"

namespace chrome {

SkColor GetDetachedBookmarkBarBackgroundColor(
    ui::ThemeProvider* theme_provider) {
  bool themed = theme_provider->HasCustomImage(IDR_THEME_NTP_BACKGROUND);
  if (themed)
    return theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR);
  else
    return SkColorSetARGB(0xFF, 0xF1, 0xF1, 0xF1);
}

SkColor GetDetachedBookmarkBarSeparatorColor(
    ui::ThemeProvider* theme_provider) {
  bool themed = theme_provider->HasCustomImage(IDR_THEME_NTP_BACKGROUND);
  if (!themed) {
    return ThemeProperties::GetDefaultColor(
        ThemeProperties::COLOR_TOOLBAR_SEPARATOR);
  }

  // Use 50% of bookmark text color as separator color.
  return SkColorSetA(
      theme_provider->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT), 128);
}

}  // namespace chrome
