// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DEFAULT_APPS_H_
#define CHROME_BROWSER_EXTENSIONS_DEFAULT_APPS_H_
#pragma once

#include <set>
#include <string>
#include "chrome/common/extensions/extension.h"
#include "base/gtest_prod_util.h"

class PrefService;

// Encapsulates business logic for:
// - Whether to install default apps on Chrome startup
// - Whether to show the app launcher
// - Whether to show the apps promo in the launcher
class DefaultApps {
 public:
  // The maximum number of times to show the apps promo.
  static const int kAppsPromoCounterMax;

  // Register our preferences.
  static void RegisterUserPrefs(PrefService* prefs);

  explicit DefaultApps(PrefService* prefs,
                       const std::string& application_locale);
  ~DefaultApps();

  // Gets the set of default apps.
  const ExtensionIdSet& default_apps() const;

  // Returns true if the default apps should be installed.
  bool ShouldInstallDefaultApps(const ExtensionIdSet& installed_ids);

  // Returns true if the app launcher in the NTP should be shown.
  bool ShouldShowAppLauncher(const ExtensionIdSet& installed_ids);

  // Returns true if the apps promo should be displayed in the launcher.
  //
  // NOTE: If the default apps have been installed, but |installed_ids| is
  // different than GetDefaultApps(), this will permanently expire the promo.
  bool ShouldShowPromo(const ExtensionIdSet& installed_ids);

  // Should be called after each app is installed. Once installed_ids contains
  // all the default apps, GetAppsToInstall() will start returning NULL.
  void DidInstallApp(const ExtensionIdSet& installed_ids);

  // Should be called after each time the promo is installed.
  void DidShowPromo();

  // Force the promo to be hidden.
  void SetPromoHidden();

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionDefaultApps, HappyPath);
  FRIEND_TEST_ALL_PREFIXES(ExtensionDefaultApps, UnsupportedLocale);
  FRIEND_TEST_ALL_PREFIXES(ExtensionDefaultApps, HidePromo);
  FRIEND_TEST_ALL_PREFIXES(ExtensionDefaultApps, InstallingAnAppHidesPromo);
  FRIEND_TEST_ALL_PREFIXES(ExtensionDefaultApps,
                           ManualAppInstalledWhileInstallingDefaultApps);

  bool DefaultAppSupported();
  bool DefaultAppsSupportedForLanguage();

  bool NonDefaultAppIsInstalled(const ExtensionIdSet& installed_ids) const;

  bool GetDefaultAppsInstalled() const;
  void SetDefaultAppsInstalled(bool val);

  int GetPromoCounter() const;
  void SetPromoCounter(int val);

  // Our permanent state is stored in this PrefService instance.
  PrefService* prefs_;

  // The locale the browser is currently in.
  std::string application_locale_;

  // The set of default extensions. Initialized to a static list in the
  // constructor.
  ExtensionIdSet ids_;

  DISALLOW_COPY_AND_ASSIGN(DefaultApps);
};

#endif  // CHROME_BROWSER_EXTENSIONS_DEFAULT_APPS_H_
