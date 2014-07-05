// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"

#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "ui/views/controls/button/label_button.h"

class BookmarkBarViewInstantExtendedTest : public BrowserWithTestWindowTest {
 public:
  BookmarkBarViewInstantExtendedTest() {
  }

 protected:
  virtual TestingProfile* CreateProfile() OVERRIDE {
    TestingProfile* profile = BrowserWithTestWindowTest::CreateProfile();
    // TemplateURLService is normally NULL during testing. Instant extended
    // needs this service so set a custom factory function.
    TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        profile, &BookmarkBarViewInstantExtendedTest::CreateTemplateURLService);
    return profile;
  }

 private:
  static KeyedService* CreateTemplateURLService(
      content::BrowserContext* profile) {
    return new TemplateURLService(
        static_cast<Profile*>(profile)->GetPrefs(),
        make_scoped_ptr(new SearchTermsData), NULL,
        scoped_ptr<TemplateURLServiceClient>(), NULL, NULL, base::Closure());
  }

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarViewInstantExtendedTest);
};

// Verify that in instant extended mode the visibility of the apps shortcut
// button properly follows the pref value.
TEST_F(BookmarkBarViewInstantExtendedTest, AppsShortcutVisibility) {
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  profile()->CreateBookmarkModel(true);
  test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForProfile(profile()));
  BookmarkBarView bookmark_bar_view(browser(), NULL);
  bookmark_bar_view.set_owned_by_client();
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShowAppsShortcutInBookmarkBar, false);
  EXPECT_FALSE(bookmark_bar_view.apps_page_shortcut_->visible());

  // Try to make the Apps shortcut visible. Its visibility depends on whether
  // the app launcher is enabled.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShowAppsShortcutInBookmarkBar, true);
  if (IsAppLauncherEnabled()) {
    EXPECT_FALSE(bookmark_bar_view.apps_page_shortcut_->visible());
  } else {
    EXPECT_TRUE(bookmark_bar_view.apps_page_shortcut_->visible());
  }

  // Make sure we can also properly transition from true to false.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShowAppsShortcutInBookmarkBar, false);
  EXPECT_FALSE(bookmark_bar_view.apps_page_shortcut_->visible());
}

#if !defined(OS_CHROMEOS)
typedef BrowserWithTestWindowTest BookmarkBarViewTest;

// Verifies that the apps shortcut is shown or hidden following the policy
// value. This policy (and the apps shortcut) isn't present on ChromeOS.
TEST_F(BookmarkBarViewTest, ManagedShowAppsShortcutInBookmarksBar) {
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  profile()->CreateBookmarkModel(true);
  test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForProfile(profile()));
  BookmarkBarView bookmark_bar_view(browser(), NULL);
  bookmark_bar_view.set_owned_by_client();

  // By default, the pref is not managed and the apps shortcut is shown.
  TestingPrefServiceSyncable* prefs = profile()->GetTestingPrefService();
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kShowAppsShortcutInBookmarkBar));
  EXPECT_TRUE(bookmark_bar_view.apps_page_shortcut_->visible());

  // Hide the apps shortcut by policy, via the managed pref.
  prefs->SetManagedPref(prefs::kShowAppsShortcutInBookmarkBar,
                        new base::FundamentalValue(false));
  EXPECT_FALSE(bookmark_bar_view.apps_page_shortcut_->visible());

  // And try showing it via policy too.
  prefs->SetManagedPref(prefs::kShowAppsShortcutInBookmarkBar,
                        new base::FundamentalValue(true));
  EXPECT_TRUE(bookmark_bar_view.apps_page_shortcut_->visible());
}
#endif
