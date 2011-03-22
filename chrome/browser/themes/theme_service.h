// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_THEME_SERVICE_H_
#define CHROME_BROWSER_THEMES_THEME_SERVICE_H_

#include <map>

#include "base/singleton.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class BrowserThemeProvider;
class Extension;
class Profile;

// Singleton that owns all BrowserThemeProviders and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated BrowserThemeProvider.
class ThemeServiceFactory : public NotificationObserver {
 public:
  // Returns the BrowserThemeProvider that provides theming resources for
  // |profile|. Note that even if a Profile doesn't have a theme installed, it
  // still needs a BrowserThemeProvider to hand back the default theme images.
  static BrowserThemeProvider* GetForProfile(Profile* profile);

  // Returns the Extension that implements the theme associated with
  // |profile|. Returns NULL if the theme is no longer installed, if there is
  // no installed theme, or the theme was cleared.
  static const Extension* GetThemeForProfile(Profile* profile);

  // Forces an association between |profile| and |provider|. Used in unit tests
  // where we need to mock BrowserThemeProvider.
  static void ForceAssociationBetween(Profile* profile,
                                      BrowserThemeProvider* provider);

  static ThemeServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ThemeServiceFactory>;

  ThemeServiceFactory();
  ~ThemeServiceFactory();

  // Maps |profile| to |provider| and listens for notifications relating to
  // either.
  void Associate(Profile* profile, BrowserThemeProvider* provider);

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  NotificationRegistrar registrar_;
  std::map<Profile*, BrowserThemeProvider*> mapping_;
};

#endif  // CHROME_BROWSER_THEMES_THEME_SERVICE_H_
