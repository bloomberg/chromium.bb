// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ui.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "grit/theme_resources.h"

namespace chrome {

SkColor GetDetachedBookmarkBarBackgroundColor(ThemeService* theme_service) {
  if (!theme_service->UsingDefaultTheme())
    return theme_service->GetColor(ThemeProperties::COLOR_TOOLBAR);
  return SkColorSetARGB(0xFF, 0xF1, 0xF1, 0xF1);
}

SkColor GetDetachedBookmarkBarSeparatorColor(ThemeService* theme_service) {
  if (theme_service->UsingDefaultTheme()) {
    return ThemeProperties::GetDefaultColor(
        ThemeProperties::COLOR_TOOLBAR_SEPARATOR);
  }

  // Use 50% of bookmark text color as separator color.
  return SkColorSetA(
      theme_service->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT), 128);
}

}  // namespace chrome
