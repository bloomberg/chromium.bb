// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/apps_promo.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/webui/ntp/shown_sections_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPromoId[] = "23123123";
const char kPromoHeader[] = "Get great apps!";
const char kPromoButton[] = "Click for apps!";
const char kPromoLink[] = "http://apps.com";
const char kPromoLogo[] = "chrome://theme/IDR_WEBSTORE_ICON";
const char kPromoExpire[] = "No thanks.";
const int kPromoUserGroup =
    AppsPromo::USERS_NEW | AppsPromo::USERS_EXISTING;

void ExpectAppsSectionMaximized(PrefService* prefs, bool maximized) {
  EXPECT_EQ(maximized,
            (ShownSectionsHandler::GetShownSections(prefs) & APPS) != 0);
  EXPECT_EQ(!maximized,
            (ShownSectionsHandler::GetShownSections(prefs) & THUMB) != 0);
}

void ExpectAppsPromoHidden(PrefService* prefs) {
  // Hiding the promo places the apps section in menu mode and maximizes the
  // most visited section.
  EXPECT_TRUE((ShownSectionsHandler::GetShownSections(prefs) &
               APPS) == 0);
  EXPECT_TRUE((ShownSectionsHandler::GetShownSections(prefs) &
               (MENU_APPS | THUMB)) != 0);
}

} // namespace

class ExtensionAppsPromo : public testing::Test {
 public:
  TestingPrefService* prefs() { return &prefs_; }
  AppsPromo* apps_promo() { return &apps_promo_; }

 protected:
  explicit ExtensionAppsPromo();

  // testing::Test
  virtual void SetUp();

 private:
  TestingPrefService prefs_;
  ScopedTestingLocalState local_state_;
  AppsPromo apps_promo_;
};

ExtensionAppsPromo::ExtensionAppsPromo()
    : local_state_(static_cast<TestingBrowserProcess*>(g_browser_process)),
      apps_promo_(&prefs_) {
}

void ExtensionAppsPromo::SetUp() {
  browser::RegisterUserPrefs(&prefs_);
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

  // Make sure the web store can be supported even when the promo is not active.
  AppsPromo::SetWebStoreSupportedForLocale(true);
  EXPECT_FALSE(AppsPromo::IsPromoSupportedForLocale());
  EXPECT_TRUE(apps_promo()->ShouldShowAppLauncher(installed_ids));
  EXPECT_FALSE(apps_promo()->ShouldShowPromo(installed_ids,
                                             &promo_just_expired));

  // We should be able to disable the web store as well.
  AppsPromo::SetWebStoreSupportedForLocale(false);
  EXPECT_FALSE(AppsPromo::IsPromoSupportedForLocale());
  EXPECT_FALSE(apps_promo()->ShouldShowAppLauncher(installed_ids));
  EXPECT_FALSE(apps_promo()->ShouldShowPromo(installed_ids,
                                             &promo_just_expired));

  // Once the promo is set, we show both the promo and app launcher.
  AppsPromo::PromoData promo_data(kPromoId, kPromoHeader, kPromoButton,
                                  GURL(kPromoLink), kPromoExpire, GURL(""),
                                  kPromoUserGroup);
  AppsPromo::SetPromo(promo_data);
  AppsPromo::SetWebStoreSupportedForLocale(true);
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
  AppsPromo::PromoData promo_data(kPromoId, kPromoHeader, kPromoButton,
                                  GURL(kPromoLink), kPromoExpire, GURL(""),
                                  kPromoUserGroup);
  AppsPromo::SetPromo(promo_data);

  // ... then make sure AppsPromo can access it.
  AppsPromo::PromoData actual_data = AppsPromo::GetPromo();
  EXPECT_EQ(kPromoId, actual_data.id);
  EXPECT_EQ(kPromoHeader, actual_data.header);
  EXPECT_EQ(kPromoButton, actual_data.button);
  EXPECT_EQ(GURL(kPromoLink), actual_data.link);
  EXPECT_EQ(kPromoExpire, actual_data.expire);
  EXPECT_EQ(kPromoUserGroup, actual_data.user_group);
  // The promo logo should be the default value.
  EXPECT_EQ(GURL(kPromoLogo), actual_data.logo);
  EXPECT_TRUE(AppsPromo::IsPromoSupportedForLocale());

  AppsPromo::ClearPromo();
  actual_data = AppsPromo::GetPromo();
  EXPECT_EQ("", actual_data.id);
  EXPECT_EQ("", actual_data.header);
  EXPECT_EQ("", actual_data.button);
  EXPECT_EQ(GURL(""), actual_data.link);
  EXPECT_EQ("", actual_data.expire);
  EXPECT_EQ(AppsPromo::USERS_NONE, actual_data.user_group);
  EXPECT_EQ(GURL(kPromoLogo), actual_data.logo);
  EXPECT_FALSE(AppsPromo::IsPromoSupportedForLocale());

  // Make sure we can set the logo to something other than the default.
  std::string promo_logo = "data:image/png;base64,iVBORw0kGgoAAAN";
  promo_data.logo = GURL(promo_logo);
  AppsPromo::SetPromo(promo_data);
  EXPECT_EQ(GURL(promo_logo), AppsPromo::GetPromo().logo);
  EXPECT_TRUE(AppsPromo::IsPromoSupportedForLocale());

  // Verify that the default is returned instead of HTTP or HTTPS URLs.
  promo_data.logo = GURL("http://google.com/logo.png");
  AppsPromo::SetPromo(promo_data);
  EXPECT_EQ(GURL(kPromoLogo), AppsPromo::GetPromo().logo);
  EXPECT_TRUE(AppsPromo::IsPromoSupportedForLocale());

  promo_data.logo = GURL("https://google.com/logo.png");
  AppsPromo::SetPromo(promo_data);
  EXPECT_EQ(GURL(kPromoLogo), AppsPromo::GetPromo().logo);
  EXPECT_TRUE(AppsPromo::IsPromoSupportedForLocale());

  // Try an invalid URL.
  promo_data.logo = GURL("sldkfjlsdn");
  AppsPromo::SetPromo(promo_data);
  EXPECT_EQ(GURL(kPromoLogo), AppsPromo::GetPromo().logo);
  EXPECT_TRUE(AppsPromo::IsPromoSupportedForLocale());

  // Try the web store supported flag.
  EXPECT_FALSE(AppsPromo::IsWebStoreSupportedForLocale());
  AppsPromo::SetWebStoreSupportedForLocale(true);
  EXPECT_TRUE(AppsPromo::IsWebStoreSupportedForLocale());
  AppsPromo::SetWebStoreSupportedForLocale(false);
  EXPECT_FALSE(AppsPromo::IsWebStoreSupportedForLocale());

  // Try setting and getting the source logo URL.
  GURL expected_source("https://www.google.com/images/test.png");
  AppsPromo::SetSourcePromoLogoURL(expected_source);
  EXPECT_EQ(expected_source, AppsPromo::GetSourcePromoLogoURL());
}

// Tests maximizing the promo for USERS_NONE.
TEST_F(ExtensionAppsPromo, UpdatePromoFocus_UsersNone) {
  // Verify that the apps section is not already maximized.
  ExpectAppsSectionMaximized(prefs(), false);

  // The promo shouldn't maximize for anyone.
  AppsPromo::PromoData promo_data(kPromoId, kPromoHeader, kPromoButton,
                                  GURL(kPromoLink), kPromoExpire, GURL(""),
                                  AppsPromo::USERS_NONE);
  AppsPromo::SetPromo(promo_data);
  apps_promo()->MaximizeAppsIfNecessary();
  ExpectAppsSectionMaximized(prefs(), false);

  // The promo still shouldn't maximize if we change it's ID.
  promo_data.id = "lkksdf";
  AppsPromo::SetPromo(promo_data);
  apps_promo()->MaximizeAppsIfNecessary();
  ExpectAppsSectionMaximized(prefs(), false);
}

// Tests maximizing the promo for USERS_EXISTING.
TEST_F(ExtensionAppsPromo, UpdatePromoFocus_UsersExisting) {
  // Verify that the apps section is not already maximized.
  ExpectAppsSectionMaximized(prefs(), false);

  // Set the promo content.
  AppsPromo::PromoData promo_data(kPromoId, kPromoHeader, kPromoButton,
                                  GURL(kPromoLink), kPromoExpire, GURL(""),
                                  AppsPromo::USERS_EXISTING);
  AppsPromo::SetPromo(promo_data);
  // This is a new user so the apps section shouldn't maximize.
  apps_promo()->MaximizeAppsIfNecessary();
  ExpectAppsSectionMaximized(prefs(), false);

  // Set a new promo and now it should maximize.
  promo_data.id = "lksdf";
  AppsPromo::SetPromo(promo_data);

  apps_promo()->MaximizeAppsIfNecessary();
  ExpectAppsSectionMaximized(prefs(), true);

  apps_promo()->HidePromo();
  ExpectAppsPromoHidden(prefs());
}

// Tests maximizing the promo for USERS_NEW.
TEST_F(ExtensionAppsPromo, UpdatePromoFocus_UsersNew) {
  // Verify that the apps section is not already maximized.
  ExpectAppsSectionMaximized(prefs(), false);

  // The promo should maximize for new users.
  AppsPromo::PromoData promo_data(kPromoId, kPromoHeader, kPromoButton,
                                  GURL(kPromoLink), kPromoExpire, GURL(""),
                                  AppsPromo::USERS_NEW);
  AppsPromo::SetPromo(promo_data);
  apps_promo()->MaximizeAppsIfNecessary();
  ExpectAppsSectionMaximized(prefs(), true);

  // Try hiding the promo.
  apps_promo()->HidePromo();
  ExpectAppsPromoHidden(prefs());

  // The same promo should not maximize twice.
  apps_promo()->MaximizeAppsIfNecessary();
  ExpectAppsSectionMaximized(prefs(), false);

  // Another promo targetting new users should not maximize.
  promo_data.id = "lksdf";
  AppsPromo::SetPromo(promo_data);
  apps_promo()->MaximizeAppsIfNecessary();
  ExpectAppsSectionMaximized(prefs(), false);
}

// Tests maximizing the promo for USERS_NEW | USERS_EXISTING.
TEST_F(ExtensionAppsPromo, UpdatePromoFocus_UsersAll) {
  // Verify that the apps section is not already maximized.
  ExpectAppsSectionMaximized(prefs(), false);

  // The apps section should maximize for all users.
  AppsPromo::PromoData promo_data(
      kPromoId, kPromoHeader, kPromoButton, GURL(kPromoLink), kPromoExpire,
      GURL(""), AppsPromo::USERS_NEW | AppsPromo::USERS_EXISTING);
  AppsPromo::SetPromo(promo_data);
  apps_promo()->MaximizeAppsIfNecessary();
  ExpectAppsSectionMaximized(prefs(), true);

  apps_promo()->HidePromo();
  ExpectAppsPromoHidden(prefs());

  // The same promo should not maximize twice.
  apps_promo()->MaximizeAppsIfNecessary();
  ExpectAppsSectionMaximized(prefs(), false);

  // A promo with a new ID should maximize though.
  promo_data.id = "lkksdf";
  AppsPromo::SetPromo(promo_data);
  apps_promo()->MaximizeAppsIfNecessary();
  ExpectAppsSectionMaximized(prefs(), true);
}

TEST_F(ExtensionAppsPromo, PromoHiddenByPref) {
  prefs()->SetInteger(prefs::kAppsPromoCounter, 0);
  prefs()->SetBoolean(prefs::kDefaultAppsInstalled, true);

  // When the "hide" pref is false, the promo should still appear.
  prefs()->SetBoolean(prefs::kNTPHideWebStorePromo, false);
  AppsPromo::PromoData promo_data(
      kPromoId, kPromoHeader, kPromoButton, GURL(kPromoLink), kPromoExpire,
      GURL(""), AppsPromo::USERS_NEW | AppsPromo::USERS_EXISTING);
  AppsPromo::SetPromo(promo_data);
  bool just_expired;
  bool show_promo = apps_promo()->ShouldShowPromo(
      apps_promo()->old_default_apps(), &just_expired);
  EXPECT_TRUE(show_promo);

  // When the "hide" pref is true, the promo should NOT appear.
  prefs()->SetBoolean(prefs::kNTPHideWebStorePromo, true);
  show_promo = apps_promo()->ShouldShowPromo(
      apps_promo()->old_default_apps(), &just_expired);
  EXPECT_FALSE(show_promo);
}

#endif // OS_CHROMEOS
