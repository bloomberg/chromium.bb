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

// Manages the installation of the set of default apps into Chrome, and the
// promotion of those apps in the launcher.
//
// It implements the following rules:
//
// - Only install default apps once per-profile.
// - Don't install default apps if any apps are already installed.
// - Do not start showing the promo until all default apps have been installed.
// - Do not show the promo if it has been hidden by the user.
// - Do not show promo after one app has been manually installed or uninstalled.
// - Do not show promo if the set of installed apps is different than the set of
//   default apps.
// - Only show promo a certain amount of times.
//
// The promo can also be forced on with --force-apps-promo-visible.
class DefaultApps {
 public:
  // The maximum number of times to show the apps promo.
  static const int kAppsPromoCounterMax;

  // Register our preferences.
  static void RegisterUserPrefs(PrefService* prefs);

  explicit DefaultApps(PrefService* prefs);
  ~DefaultApps();

  // Gets the list of default apps that should be installed. Can return NULL if
  // no apps need to be installed.
  const ExtensionIdSet* GetAppsToInstall() const;

  // Gets the list of default apps.
  const ExtensionIdSet* GetDefaultApps() const;

  // Should be called after each app is installed. Once installed_ids contains
  // all the default apps, GetAppsToInstall() will start returning NULL.
  void DidInstallApp(const ExtensionIdSet& installed_ids);

  // Returns true if the apps promo should be displayed in the launcher.
  //
  // NOTE: If the default apps have been installed, but |installed_ids| is
  // different than GetDefaultApps(), this will permanently expire the promo.
  bool CheckShouldShowPromo(const ExtensionIdSet& installed_ids);

  // Should be called after each time the promo is installed.
  void DidShowPromo();

  // Force the promo to be hidden.
  void SetPromoHidden();

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionDefaultApps, Basics);
  FRIEND_TEST_ALL_PREFIXES(ExtensionDefaultApps, HidePromo);
  FRIEND_TEST_ALL_PREFIXES(ExtensionDefaultApps, InstallingAnAppHidesPromo);
  FRIEND_TEST_ALL_PREFIXES(ExtensionDefaultApps,
                           ManualAppInstalledWhileInstallingDefaultApps);

  bool GetDefaultAppsInstalled() const;
  void SetDefaultAppsInstalled(bool val);

  int GetPromoCounter() const;
  void SetPromoCounter(int val);

  // Our permanent state is stored in this PrefService instance.
  PrefService* prefs_;

  // The set of default extensions. Initialized to a static list in the
  // constructor.
  ExtensionIdSet ids_;

  DISALLOW_COPY_AND_ASSIGN(DefaultApps);
};

#endif  // CHROME_BROWSER_EXTENSIONS_DEFAULT_APPS_H_
