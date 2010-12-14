// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/default_apps.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(dpolukhin): On Chrome OS all apps are installed via external extensions,
// and the web store promo is never shown.
#if !defined(OS_CHROMEOS)
TEST(ExtensionDefaultApps, Basics) {
  TestingPrefService pref_service;
  DefaultApps::RegisterUserPrefs(&pref_service);
  DefaultApps default_apps(&pref_service);

  ExtensionIdSet default_app_ids = *default_apps.GetAppsToInstall();
  ASSERT_GT(default_app_ids.size(), 0u);
  EXPECT_FALSE(default_apps.GetDefaultAppsInstalled());
  EXPECT_EQ(0, default_apps.GetPromoCounter());
  EXPECT_EQ(default_app_ids, *default_apps.GetDefaultApps());

  // The promo should not be shown until the default apps have been installed.
  ExtensionIdSet installed_app_ids;
  EXPECT_FALSE(default_apps.CheckShouldShowPromo(installed_app_ids));

  // Simulate installing the apps one by one and notifying default_apps after
  // each intallation. Nothing should change until we have installed all the
  // default apps.
  ExtensionIdSet extension_id_sets[] = {
    default_app_ids,
    default_app_ids,
    default_app_ids
  };
  extension_id_sets[0].clear();
  extension_id_sets[1].erase(extension_id_sets[1].begin());
  extension_id_sets[2].erase(extension_id_sets[2].begin(),
                             ++extension_id_sets[2].begin());
  for (size_t i = 0; i < arraysize(extension_id_sets); ++i) {
    default_apps.DidInstallApp(extension_id_sets[i]);
    EXPECT_TRUE(default_app_ids == *default_apps.GetAppsToInstall());
    EXPECT_FALSE(default_apps.GetDefaultAppsInstalled());
    EXPECT_FALSE(default_apps.CheckShouldShowPromo(extension_id_sets[i]));
  }

  // Simulate all the default apps being installed. Now we should stop getting
  // default apps to install.
  default_apps.DidInstallApp(default_app_ids);
  EXPECT_EQ(NULL, default_apps.GetAppsToInstall());
  EXPECT_TRUE(default_apps.GetDefaultAppsInstalled());

  // And the promo should become available.
  EXPECT_TRUE(default_apps.CheckShouldShowPromo(default_app_ids));

  // The promo should be available up to the max allowed times, then stop.
  for (int i = 0; i < DefaultApps::kAppsPromoCounterMax; ++i) {
    EXPECT_TRUE(default_apps.CheckShouldShowPromo(default_app_ids));
    default_apps.DidShowPromo();
    EXPECT_EQ(i + 1, default_apps.GetPromoCounter());
  }
  EXPECT_FALSE(default_apps.CheckShouldShowPromo(default_app_ids));
  EXPECT_EQ(DefaultApps::kAppsPromoCounterMax, default_apps.GetPromoCounter());
}

TEST(ExtensionDefaultApps, HidePromo) {
  TestingPrefService pref_service;
  DefaultApps::RegisterUserPrefs(&pref_service);
  DefaultApps default_apps(&pref_service);

  ExtensionIdSet default_app_ids = *default_apps.GetAppsToInstall();
  default_apps.DidInstallApp(default_app_ids);

  EXPECT_TRUE(default_apps.CheckShouldShowPromo(default_app_ids));
  default_apps.DidShowPromo();
  EXPECT_EQ(1, default_apps.GetPromoCounter());

  default_apps.SetPromoHidden();
  EXPECT_FALSE(default_apps.CheckShouldShowPromo(default_app_ids));
  EXPECT_EQ(DefaultApps::kAppsPromoCounterMax, default_apps.GetPromoCounter());
}

TEST(ExtensionDefaultApps, InstallingAnAppHidesPromo) {
  TestingPrefService pref_service;
  DefaultApps::RegisterUserPrefs(&pref_service);
  DefaultApps default_apps(&pref_service);

  ExtensionIdSet default_app_ids = *default_apps.GetAppsToInstall();
  ExtensionIdSet installed_app_ids = default_app_ids;
  default_apps.DidInstallApp(installed_app_ids);

  EXPECT_TRUE(default_apps.CheckShouldShowPromo(installed_app_ids));
  default_apps.DidShowPromo();
  EXPECT_EQ(1, default_apps.GetPromoCounter());

  // Now simulate a new extension being installed. This should cause the promo
  // to be hidden.
  installed_app_ids.insert("foo");
  EXPECT_FALSE(default_apps.CheckShouldShowPromo(installed_app_ids));
  EXPECT_EQ(DefaultApps::kAppsPromoCounterMax, default_apps.GetPromoCounter());
}

TEST(ExtensionDefaultApps, ManualAppInstalledWhileInstallingDefaultApps) {
  // It is possible to have apps manually installed while the default apps are
  // being installed. The network or server might be down, causing the default
  // app installation to fail. The updater might take awhile to get around to
  // updating, giving the user a chance to manually intall.
  //
  // In these cases, we should keep trying to install default apps until we have
  // them all, and then stop, even if at that point, we have more apps than just
  // the default ones.
  TestingPrefService pref_service;
  DefaultApps::RegisterUserPrefs(&pref_service);
  DefaultApps default_apps(&pref_service);

  // Simulate an app getting installed before the complete set of default apps.
  // This shouldn't affect us installing default apps. We should keep trying.
  ExtensionIdSet installed_ids;
  installed_ids.insert("foo");
  default_apps.DidInstallApp(installed_ids);
  EXPECT_FALSE(default_apps.GetDefaultAppsInstalled());
  EXPECT_TRUE(default_apps.GetAppsToInstall());

  // Now add all the default apps in addition to the extra app. We should stop
  // trying to install default apps.
  installed_ids = *default_apps.GetAppsToInstall();
  installed_ids.insert("foo");
  default_apps.DidInstallApp(installed_ids);
  EXPECT_TRUE(default_apps.GetDefaultAppsInstalled());
  EXPECT_FALSE(default_apps.GetAppsToInstall());

  // The promo shouldn't turn on though, because it would look weird with the
  // user's extra, manually installed extensions.
  EXPECT_FALSE(default_apps.CheckShouldShowPromo(installed_ids));
  EXPECT_EQ(DefaultApps::kAppsPromoCounterMax, default_apps.GetPromoCounter());
}
#endif  // OS_CHROMEOS
