// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_aurax11.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/common/pref_names.h"
#include "ui/gfx/image/image.h"
#include "ui/views/linux_ui/linux_ui.h"

namespace {

class NativeThemeX11 : public CustomThemeSupplier {
 public:
  explicit NativeThemeX11(PrefService* pref_service);

  // Overridden from CustomThemeSupplier:
  virtual void StartUsingTheme() OVERRIDE;
  virtual void StopUsingTheme() OVERRIDE;
  virtual bool GetColor(int id, SkColor* color) const OVERRIDE;
  virtual gfx::Image GetImageNamed(int id) OVERRIDE;
  virtual bool HasCustomImage(int id) const OVERRIDE;

 private:
  virtual ~NativeThemeX11();

  // These pointers are not owned by us.
  const views::LinuxUI* const linux_ui_;
  PrefService* const pref_service_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeX11);
};

NativeThemeX11::NativeThemeX11(PrefService* pref_service)
    : CustomThemeSupplier(NATIVE_X11),
      linux_ui_(views::LinuxUI::instance()),
      pref_service_(pref_service) {}

void NativeThemeX11::StartUsingTheme() {
  pref_service_->SetBoolean(prefs::kUsesSystemTheme, true);
}

void NativeThemeX11::StopUsingTheme() {
  pref_service_->SetBoolean(prefs::kUsesSystemTheme, false);
}

bool NativeThemeX11::GetColor(int id, SkColor* color) const {
  return linux_ui_ && linux_ui_->GetColor(id, color);
}

gfx::Image NativeThemeX11::GetImageNamed(int id) {
  return linux_ui_ ? linux_ui_->GetThemeImageNamed(id) : gfx::Image();
}

bool NativeThemeX11::HasCustomImage(int id) const {
  return linux_ui_ && linux_ui_->HasCustomImage(id);
}

NativeThemeX11::~NativeThemeX11() {}

}  // namespace

ThemeServiceAuraX11::ThemeServiceAuraX11() {}

ThemeServiceAuraX11::~ThemeServiceAuraX11() {}

bool ThemeServiceAuraX11::ShouldInitWithNativeTheme() const {
  return profile()->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme);
}

void ThemeServiceAuraX11::SetNativeTheme() {
  SetCustomDefaultTheme(new NativeThemeX11(profile()->GetPrefs()));
}

bool ThemeServiceAuraX11::UsingDefaultTheme() const {
  return ThemeService::UsingDefaultTheme() && !UsingNativeTheme();
}

bool ThemeServiceAuraX11::UsingNativeTheme() const {
  const CustomThemeSupplier* theme_supplier = get_theme_supplier();
  return theme_supplier &&
         theme_supplier->get_theme_type() == CustomThemeSupplier::NATIVE_X11;
}
