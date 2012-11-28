// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_FACTORY_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class ThemeService;

namespace extensions {
class Extension;
}

// Singleton that owns all ThemeServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated ThemeService.
class ThemeServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the ThemeService that provides theming resources for
  // |profile|. Note that even if a Profile doesn't have a theme installed, it
  // still needs a ThemeService to hand back the default theme images.
  static ThemeService* GetForProfile(Profile* profile);

  // If the theme service for |profile| has been instantiated,
  // immediately sets its theme to |theme|.  Otherwise, saves
  // |theme|'s ID so that the theme service picks it up when it gets
  // initialized.
  //
  // |theme| must already be installed in the extension service.
  static void SetThemeForProfile(
      Profile* profile,
      const extensions::Extension* theme);

  static ThemeServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ThemeServiceFactory>;

  ThemeServiceFactory();
  virtual ~ThemeServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual void RegisterUserPrefs(PrefService* prefs) OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ThemeServiceFactory);
};

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_FACTORY_H_
