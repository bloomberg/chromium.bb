// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_AURAX11_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_AURAX11_H_

#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/themes/theme_service.h"

// A subclass of ThemeService that queries the current ui::LinuxUI instance for
// theme data.
class ThemeServiceAuraX11 : public ThemeService {
 public:
  ThemeServiceAuraX11();
  virtual ~ThemeServiceAuraX11();

  // Overridden from ui::ThemeProvider:
  virtual SkColor GetColor(int id) const OVERRIDE;
  virtual bool HasCustomImage(int id) const OVERRIDE;

  // Overridden from ThemeService:
  virtual void Init(Profile* profile) OVERRIDE;
  virtual gfx::Image GetImageNamed(int id) const OVERRIDE;
  virtual void SetTheme(const extensions::Extension* extension) OVERRIDE;
  virtual void UseDefaultTheme() OVERRIDE;
  virtual void SetNativeTheme() OVERRIDE;
  virtual bool UsingDefaultTheme() const OVERRIDE;
  virtual bool UsingNativeTheme() const OVERRIDE;

 private:
  void OnUsesSystemThemeChanged();

  // Whether we'll give the ui::LinuxUI object first shot at providing theme
  // resources.
  bool use_system_theme_;

  PrefChangeRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ThemeServiceAuraX11);
};

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_AURAX11_H_
