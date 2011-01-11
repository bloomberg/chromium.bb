// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/default_apps.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(dpolukhin): On Chrome OS all apps are installed via external extensions,
// and the web store promo is never shown.
#if !defined(OS_CHROMEOS)
TEST(ExtensionDefaultApps, HappyPath) {
  TestingPrefService pref_service;
  DefaultApps::RegisterUserPrefs(&pref_service);
  DefaultApps default_apps(&pref_service, "en-US");

  const ExtensionIdSet& default_app_ids = default_apps.default_apps();
  ASSERT_GT(default_app_ids.size(), 0u);
  EXPECT_FALSE(default_apps.GetDefaultAppsInstalled());
  EXPECT_EQ(0, default_apps.GetPromoCounter());

  // If no apps are installed, the default apps should be installed.
  ExtensionIdSet installed_app_ids;
  EXPECT_TRUE(default_apps.ShouldInstallDefaultApps(installed_app_ids));

  // The launcher should not be shown until the default apps have been
  // installed.
  EXPECT_FALSE(default_apps.ShouldShowAppLauncher(installed_app_ids));

  // The promo should not be shown until the default apps have been installed.
  bool promo_just_expired = false;
  EXPECT_FALSE(default_apps.ShouldShowPromo(installed_app_ids,
                                            &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);

  // Simulate installing the apps one by one and notifying default_apps after
  // each intallation. Nothing should change until we have installed all the
  // default apps.
  for (size_t i = 0; i < default_app_ids.size() - 1; ++i) {
    ExtensionIdSet::const_iterator iter = default_app_ids.begin();
    for (size_t j = 0; j <= i; ++j)
      ++iter;
    installed_app_ids.insert(*iter);
    default_apps.DidInstallApp(installed_app_ids);
    EXPECT_FALSE(default_apps.GetDefaultAppsInstalled());
    EXPECT_TRUE(default_apps.ShouldInstallDefaultApps(installed_app_ids));
    EXPECT_FALSE(default_apps.ShouldShowAppLauncher(installed_app_ids));
    EXPECT_FALSE(default_apps.ShouldShowPromo(installed_app_ids,
                                              &promo_just_expired));
    EXPECT_FALSE(promo_just_expired);
  }

  // Simulate all the default apps being installed. Now we should stop getting
  // default apps to install.
  installed_app_ids = default_app_ids;
  default_apps.DidInstallApp(installed_app_ids);
  EXPECT_TRUE(default_apps.GetDefaultAppsInstalled());
  EXPECT_FALSE(default_apps.ShouldInstallDefaultApps(installed_app_ids));

  // And the promo and launcher should become available.
  EXPECT_TRUE(default_apps.ShouldShowAppLauncher(installed_app_ids));
  EXPECT_TRUE(default_apps.ShouldShowPromo(installed_app_ids,
                                           &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);

  // The promo should be available up to the max allowed times, then stop.
  // We start counting at 1 because of the call to ShouldShowPromo() above.
  for (int i = 1; i < DefaultApps::kAppsPromoCounterMax; ++i) {
    EXPECT_TRUE(default_apps.ShouldShowPromo(installed_app_ids,
                                             &promo_just_expired));
    EXPECT_FALSE(promo_just_expired);
    EXPECT_EQ(i + 1, default_apps.GetPromoCounter());
  }

  // The first time, should_show_promo should flip to true, then back to false.
  EXPECT_FALSE(default_apps.ShouldShowPromo(installed_app_ids,
                                            &promo_just_expired));
  EXPECT_TRUE(promo_just_expired);
  EXPECT_EQ(DefaultApps::kAppsPromoCounterMax + 1,
            default_apps.GetPromoCounter());

  // Even if all the apps are subsequently removed, the apps section should
  // remain.
  installed_app_ids.clear();
  EXPECT_FALSE(default_apps.ShouldInstallDefaultApps(installed_app_ids));
  EXPECT_TRUE(default_apps.ShouldShowAppLauncher(installed_app_ids));
  EXPECT_FALSE(default_apps.ShouldShowPromo(installed_app_ids,
                                            &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);
  EXPECT_EQ(DefaultApps::kAppsPromoCounterMax + 1,
            default_apps.GetPromoCounter());
}

TEST(ExtensionDefaultApps, UnsupportedLocale) {
  TestingPrefService pref_service;
  DefaultApps::RegisterUserPrefs(&pref_service);
  DefaultApps default_apps(&pref_service, "fr");

  const ExtensionIdSet& default_app_ids = default_apps.default_apps();
  EXPECT_GT(default_app_ids.size(), 0u);

  // Since the store only supports en-US at the moment, we don't install default
  // apps or promote the store.
  ExtensionIdSet installed_ids;
  EXPECT_FALSE(default_apps.ShouldInstallDefaultApps(installed_ids));
  EXPECT_FALSE(default_apps.ShouldShowAppLauncher(installed_ids));

  bool promo_just_expired = false;
  EXPECT_FALSE(default_apps.ShouldShowPromo(installed_ids,
                                            &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);

  // If the user installs an app manually, then we show the apps section, but
  // no promotion or default apps.
  installed_ids.insert(*(default_app_ids.begin()));
  EXPECT_FALSE(default_apps.ShouldInstallDefaultApps(installed_ids));
  EXPECT_TRUE(default_apps.ShouldShowAppLauncher(installed_ids));
  EXPECT_FALSE(default_apps.ShouldShowPromo(installed_ids,
                                            &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);

  // Even if the user installs the exact set of default apps, we don't show the
  // promo.
  installed_ids = default_app_ids;
  EXPECT_FALSE(default_apps.ShouldInstallDefaultApps(installed_ids));
  EXPECT_TRUE(default_apps.ShouldShowAppLauncher(installed_ids));
  EXPECT_FALSE(default_apps.ShouldShowPromo(installed_ids,
                                            &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);

  // If the user uninstalls the apps again, we go back to not showing the
  // apps section.
  installed_ids.clear();
  EXPECT_FALSE(default_apps.ShouldInstallDefaultApps(installed_ids));
  EXPECT_FALSE(default_apps.ShouldShowAppLauncher(installed_ids));
  EXPECT_FALSE(default_apps.ShouldShowPromo(installed_ids,
                                            &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);
}

TEST(ExtensionDefaultApps, HidePromo) {
  TestingPrefService pref_service;
  DefaultApps::RegisterUserPrefs(&pref_service);
  DefaultApps default_apps(&pref_service, "en-US");

  const ExtensionIdSet& default_app_ids = default_apps.default_apps();
  default_apps.DidInstallApp(default_app_ids);

  bool promo_just_expired = false;
  EXPECT_TRUE(default_apps.ShouldShowPromo(default_app_ids,
                                           &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);
  EXPECT_EQ(1, default_apps.GetPromoCounter());

  default_apps.SetPromoHidden();
  EXPECT_FALSE(default_apps.ShouldShowPromo(default_app_ids,
                                            &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);
  EXPECT_EQ(DefaultApps::kAppsPromoCounterMax + 1,
            default_apps.GetPromoCounter());
}

TEST(ExtensionDefaultApps, InstallingAnAppHidesPromo) {
  TestingPrefService pref_service;
  DefaultApps::RegisterUserPrefs(&pref_service);
  DefaultApps default_apps(&pref_service, "en-US");

  const ExtensionIdSet& default_app_ids = default_apps.default_apps();
  ExtensionIdSet installed_app_ids = default_app_ids;
  default_apps.DidInstallApp(installed_app_ids);

  bool promo_just_expired = false;
  EXPECT_TRUE(default_apps.ShouldShowPromo(installed_app_ids,
                                           &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);
  EXPECT_EQ(1, default_apps.GetPromoCounter());

  // Now simulate a new extension being installed. This should cause the promo
  // to be hidden.
  installed_app_ids.insert("foo");
  EXPECT_FALSE(default_apps.ShouldShowPromo(installed_app_ids,
                                            &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);
  EXPECT_EQ(DefaultApps::kAppsPromoCounterMax + 1,
            default_apps.GetPromoCounter());
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
  DefaultApps default_apps(&pref_service, "en-US");

  // Simulate an app getting installed before the complete set of default apps.
  // This should stop the default apps from trying to be installed. The launcher
  // should also immediately show up.
  ExtensionIdSet installed_ids;
  installed_ids.insert("foo");
  EXPECT_FALSE(default_apps.ShouldInstallDefaultApps(installed_ids));
  EXPECT_TRUE(default_apps.GetDefaultAppsInstalled());
  EXPECT_TRUE(default_apps.ShouldShowAppLauncher(installed_ids));

  // The promo shouldn't turn on though, because it would look weird with the
  // user's extra, manually installed extensions.
  bool promo_just_expired = false;
  EXPECT_FALSE(default_apps.ShouldShowPromo(installed_ids,
                                            &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);
  EXPECT_EQ(DefaultApps::kAppsPromoCounterMax + 1,
            default_apps.GetPromoCounter());

  // Going back to a subset of the default apps shouldn't allow the default app
  // install to continue.
  installed_ids.clear();
  EXPECT_FALSE(default_apps.ShouldInstallDefaultApps(installed_ids));
  EXPECT_TRUE(default_apps.GetDefaultAppsInstalled());
  EXPECT_TRUE(default_apps.ShouldShowAppLauncher(installed_ids));
  EXPECT_FALSE(default_apps.ShouldShowPromo(installed_ids,
                                            &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);

  // Going to the exact set of default apps shouldn't show the promo.
  EXPECT_FALSE(default_apps.ShouldShowPromo(default_apps.default_apps(),
                                            &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);
}
#endif  // OS_CHROMEOS
