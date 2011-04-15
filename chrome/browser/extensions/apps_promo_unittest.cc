// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/apps_promo.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/webui/shown_sections_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_pref_service.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPromoId[] = "23123123";
const char kPromoHeader[] = "Get great apps!";
const char kPromoButton[] = "Click for apps!";
const char kPromoLink[] = "http://apps.com";
const char kPromoExpire[] = "No thanks.";

} // namespace

class ExtensionAppsPromo : public testing::Test {
 public:
  TestingPrefService* local_state() { return &local_state_; }
  TestingPrefService* prefs() { return &prefs_; }
  AppsPromo* apps_promo() { return apps_promo_; }

 protected:
  explicit ExtensionAppsPromo();
  virtual ~ExtensionAppsPromo();

  // testing::Test
  virtual void SetUp();
  virtual void TearDown();

 private:
  TestingPrefService local_state_;
  TestingPrefService prefs_;
  AppsPromo* apps_promo_;
};

ExtensionAppsPromo::ExtensionAppsPromo() :
    apps_promo_(new AppsPromo(&prefs_)) {
}

ExtensionAppsPromo::~ExtensionAppsPromo() {
  delete apps_promo_;
}

void ExtensionAppsPromo::SetUp() {
  browser::RegisterLocalState(&local_state_);
  browser::RegisterUserPrefs(&prefs_);

  TestingBrowserProcess* testing_browser_process =
      static_cast<TestingBrowserProcess*>(g_browser_process);
  testing_browser_process->SetPrefService(&local_state_);
}

void ExtensionAppsPromo::TearDown() {
  TestingBrowserProcess* testing_browser_process =
      static_cast<TestingBrowserProcess*>(g_browser_process);
  testing_browser_process->SetPrefService(NULL);
}

// TODO(dpolukhin): On Chrome OS all apps are installed via external extensions,
// and the web store promo is never shown.
#if !defined(OS_CHROMEOS)
TEST_F(ExtensionAppsPromo, HappyPath) {
  const ExtensionIdSet& default_app_ids = apps_promo()->old_default_apps();

  EXPECT_GT(default_app_ids.size(), 0u);

  // The promo counter should default to the max, since we only use the counter
  // if they were installed from older versions of Chrome.
  EXPECT_EQ(AppsPromo::kDefaultAppsCounterMax + 1,
            apps_promo()->GetPromoCounter());

  // The app launcher and promo should not be shown if there are no extensions
  // installed and no promo is set.
  ExtensionIdSet installed_ids;
  bool promo_just_expired = false;
  EXPECT_FALSE(AppsPromo::IsPromoSupportedForLocale());
  EXPECT_FALSE(apps_promo()->ShouldShowAppLauncher(installed_ids));
  EXPECT_FALSE(apps_promo()->ShouldShowPromo(installed_ids,
                                             &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);

  // Once the promo is set, we show both the promo and app launcher.
  AppsPromo::SetPromo(
      kPromoId, kPromoHeader, kPromoButton, GURL(kPromoLink), kPromoExpire);

  EXPECT_TRUE(AppsPromo::IsPromoSupportedForLocale());
  EXPECT_TRUE(apps_promo()->ShouldShowAppLauncher(installed_ids));
  EXPECT_TRUE(apps_promo()->ShouldShowPromo(installed_ids,
                                           &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);


  // Now install an app and the promo should not be shown.
  installed_ids.insert(*(default_app_ids.begin()));
  EXPECT_TRUE(AppsPromo::IsPromoSupportedForLocale());
  EXPECT_TRUE(apps_promo()->ShouldShowAppLauncher(installed_ids));
  EXPECT_FALSE(apps_promo()->ShouldShowPromo(installed_ids,
                                             &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);

  // Even if the user installs the exact set of default apps, we still don't
  // show the promo.
  installed_ids = default_app_ids;
  EXPECT_TRUE(AppsPromo::IsPromoSupportedForLocale());
  EXPECT_TRUE(apps_promo()->ShouldShowAppLauncher(installed_ids));
  EXPECT_FALSE(apps_promo()->ShouldShowPromo(installed_ids,
                                             &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);

  // If the user then uninstalls the apps, the app launcher should remain
  // and the promo should return.
  installed_ids.clear();
  EXPECT_TRUE(AppsPromo::IsPromoSupportedForLocale());
  EXPECT_TRUE(apps_promo()->ShouldShowAppLauncher(installed_ids));
  EXPECT_TRUE(apps_promo()->ShouldShowPromo(installed_ids,
                                            &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);
}

// Tests get and set of promo content.
TEST_F(ExtensionAppsPromo, PromoPrefs) {
  // Store a promo....
  AppsPromo::SetPromo(
      kPromoId, kPromoHeader, kPromoButton, GURL(kPromoLink), kPromoExpire);

  // ... then make sure AppsPromo can access it.
  EXPECT_EQ(kPromoId, AppsPromo::GetPromoId());
  EXPECT_EQ(kPromoHeader, AppsPromo::GetPromoHeaderText());
  EXPECT_EQ(kPromoButton, AppsPromo::GetPromoButtonText());
  EXPECT_EQ(GURL(kPromoLink), AppsPromo::GetPromoLink());
  EXPECT_EQ(kPromoExpire, AppsPromo::GetPromoExpireText());
  EXPECT_TRUE(AppsPromo::IsPromoSupportedForLocale());

  AppsPromo::ClearPromo();
  EXPECT_EQ("", AppsPromo::GetPromoId());
  EXPECT_EQ("", AppsPromo::GetPromoHeaderText());
  EXPECT_EQ("", AppsPromo::GetPromoButtonText());
  EXPECT_EQ(GURL(""), AppsPromo::GetPromoLink());
  EXPECT_EQ("", AppsPromo::GetPromoExpireText());
  EXPECT_FALSE(AppsPromo::IsPromoSupportedForLocale());
}

// Tests that the apps section is maxmized when showing a promo for the first
// time.
TEST_F(ExtensionAppsPromo, UpdatePromoFocus) {
  ExtensionIdSet installed_ids;

  bool promo_just_expired = false;
  EXPECT_FALSE(apps_promo()->ShouldShowPromo(installed_ids,
                                             &promo_just_expired));
  EXPECT_FALSE(promo_just_expired);

  // Set the promo content.
  AppsPromo::SetPromo(
      kPromoId, kPromoHeader, kPromoButton, GURL(kPromoLink), kPromoExpire);

  // After asking if we should show the promo, the
  EXPECT_TRUE(AppsPromo::IsPromoSupportedForLocale());
  EXPECT_TRUE(apps_promo()->ShouldShowPromo(installed_ids,
                                            &promo_just_expired));
  apps_promo()->MaximizeAppsIfFirstView();

  EXPECT_TRUE(
      (ShownSectionsHandler::GetShownSections(prefs()) & APPS) != 0);
  EXPECT_FALSE(
      (ShownSectionsHandler::GetShownSections(prefs()) & THUMB) != 0);

  apps_promo()->HidePromo();

  EXPECT_TRUE((ShownSectionsHandler::GetShownSections(prefs()) &
               (MENU_APPS | THUMB)) != 0);
}
#endif  // OS_CHROMEOS
