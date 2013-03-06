// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ui.h"

#include "chrome/browser/themes/theme_properties.h"
#include "grit/theme_resources.h"
#include "ui/base/theme_provider.h"

namespace chrome {
namespace search {

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

  SkColor separator_color = SkColorSetARGB(128, 0, 0, 0);
  // If theme is too dark to use 0.5 black for separator, use 0.5 readable
  // color, which is usually 0.5 white.
  SkColor themed_background_color =
      theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR);
  SkColor readable_color = color_utils::GetReadableColor(
      separator_color, themed_background_color);
  if (readable_color != separator_color)
    separator_color = SkColorSetA(readable_color, 128);
  return separator_color;
}

}  // namespace search
}  // namespace chrome
