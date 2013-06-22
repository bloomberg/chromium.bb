// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_aurax11.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "ui/gfx/image/image.h"
#include "ui/linux_ui/linux_ui.h"

ThemeServiceAuraX11::ThemeServiceAuraX11()
    : use_system_theme_(false) {
}

ThemeServiceAuraX11::~ThemeServiceAuraX11() {}

void ThemeServiceAuraX11::Init(Profile* profile) {
  registrar_.Init(profile->GetPrefs());
  registrar_.Add(prefs::kUsesSystemTheme,
                 base::Bind(&ThemeServiceAuraX11::OnUsesSystemThemeChanged,
                            base::Unretained(this)));
  use_system_theme_ = profile->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme);

  ThemeService::Init(profile);
}

gfx::Image ThemeServiceAuraX11::GetImageNamed(int id) const {
  const ui::LinuxUI* linux_ui = ui::LinuxUI::instance();
  if (use_system_theme_ && linux_ui) {
    gfx::Image image = linux_ui->GetThemeImageNamed(id);
    if (!image.IsEmpty())
      return image;
  }

  return ThemeService::GetImageNamed(id);
}

SkColor ThemeServiceAuraX11::GetColor(int id) const {
  const ui::LinuxUI* linux_ui = ui::LinuxUI::instance();
  SkColor color;
  if (use_system_theme_ && linux_ui && linux_ui->GetColor(id, &color))
    return color;

  return ThemeService::GetColor(id);
}

bool ThemeServiceAuraX11::HasCustomImage(int id) const {
  const ui::LinuxUI* linux_ui = ui::LinuxUI::instance();
  if (use_system_theme_ && linux_ui)
    return linux_ui->HasCustomImage(id);

  return ThemeService::HasCustomImage(id);
}

void ThemeServiceAuraX11::SetTheme(const extensions::Extension* extension) {
  profile()->GetPrefs()->SetBoolean(prefs::kUsesSystemTheme, false);
  ThemeService::SetTheme(extension);
}

void ThemeServiceAuraX11::UseDefaultTheme() {
  profile()->GetPrefs()->SetBoolean(prefs::kUsesSystemTheme, false);
  ThemeService::UseDefaultTheme();
}

void ThemeServiceAuraX11::SetNativeTheme() {
  profile()->GetPrefs()->SetBoolean(prefs::kUsesSystemTheme, true);
  ClearAllThemeData();
  NotifyThemeChanged();
}

bool ThemeServiceAuraX11::UsingDefaultTheme() const {
  return !use_system_theme_ && ThemeService::UsingDefaultTheme();
}

bool ThemeServiceAuraX11::UsingNativeTheme() const {
  return use_system_theme_;
}

void ThemeServiceAuraX11::OnUsesSystemThemeChanged() {
  use_system_theme_ =
      profile()->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme);
}
