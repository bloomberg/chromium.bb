// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search_ui.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "ui/base/resource/material_design/material_design_controller.h"

namespace chrome {

SkColor GetDetachedBookmarkBarBackgroundColor(Profile* profile) {
  if (ThemeServiceFactory::GetForProfile(profile)->UsingDefaultTheme()) {
    return ui::MaterialDesignController::IsModeMaterial()
               ? SK_ColorWHITE
               : SkColorSetARGB(0xFF, 0xF1, 0xF1, 0xF1);
  }

  return ThemeService::GetThemeProviderForProfile(profile)
      .GetColor(ThemeProperties::COLOR_TOOLBAR);
}

SkColor GetDetachedBookmarkBarSeparatorColor(Profile* profile) {
  if (ThemeServiceFactory::GetForProfile(profile)->UsingDefaultTheme()) {
    return ThemeProperties::GetDefaultColor(
        ThemeProperties::COLOR_TOOLBAR_SEPARATOR);
  }

  // Use 50% of bookmark text color as separator color.
  return SkColorSetA(ThemeService::GetThemeProviderForProfile(profile)
                         .GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT),
                     128);
}

}  // namespace chrome
