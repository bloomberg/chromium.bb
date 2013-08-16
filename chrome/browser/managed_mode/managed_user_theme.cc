// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_theme.h"

#include "chrome/browser/themes/theme_properties.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

const SkColor kDefaultColorFrameManagedUser = SkColorSetRGB(165, 197, 225);
const SkColor kDefaultColorFrameManagedUserInactive =
    SkColorSetRGB(180, 225, 247);
const SkColor kDefaultColorManagedUserLabelBackground =
    SkColorSetRGB(108, 167, 210);

// Map resource ids to the managed user resource ids.
int MapToManagedUserResourceIds(int id) {
  switch (id) {
    case IDR_THEME_FRAME:
    case IDR_THEME_FRAME_WIN:
      return IDR_MANAGED_USER_THEME_FRAME;
    case IDR_THEME_FRAME_INACTIVE:
    case IDR_THEME_FRAME_INACTIVE_WIN:
      return IDR_MANAGED_USER_THEME_FRAME_INACTIVE;
    case IDR_THEME_TAB_BACKGROUND:
    case IDR_THEME_TAB_BACKGROUND_V:
    case IDR_THEME_TAB_BACKGROUND_WIN:
      return IDR_MANAGED_USER_THEME_TAB_BACKGROUND;
  }
  return id;
}

}  // namespace

ManagedUserTheme::ManagedUserTheme()
    : CustomThemeSupplier(MANAGED_USER_THEME) {}

ManagedUserTheme::~ManagedUserTheme() {}

bool ManagedUserTheme::GetColor(int id, SkColor* color) const {
  switch (id) {
    case ThemeProperties::COLOR_FRAME:
      *color = kDefaultColorFrameManagedUser;
      return true;
    case ThemeProperties::COLOR_FRAME_INACTIVE:
      *color = kDefaultColorFrameManagedUserInactive;
      return true;
    case ThemeProperties::COLOR_MANAGED_USER_LABEL:
      *color = SK_ColorWHITE;
      return true;
    case ThemeProperties::COLOR_MANAGED_USER_LABEL_BACKGROUND:
      *color = kDefaultColorManagedUserLabelBackground;
      return true;
  }
  return false;
}

gfx::Image ManagedUserTheme::GetImageNamed(int id) {
  if (!HasCustomImage(id))
    return gfx::Image();

  id = MapToManagedUserResourceIds(id);
  return ResourceBundle::GetSharedInstance().GetNativeImageNamed(id);
}

bool ManagedUserTheme::HasCustomImage(int id) const {
  return id != MapToManagedUserResourceIds(id);
}
