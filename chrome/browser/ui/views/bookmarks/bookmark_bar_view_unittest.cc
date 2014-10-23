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
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "ui/views/controls/button/label_button.h"

class BookmarkBarViewTest : public BrowserWithTestWindowTest {
 public:
  BookmarkBarViewTest() {}

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    local_state_.reset(
        new ScopedTestingLocalState(TestingBrowserProcess::GetGlobal()));
  }

  void TearDown() override {
    test_helper_.reset();
    bookmark_bar_view_.reset();
    local_state_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  TestingProfile* CreateProfile() override {
    TestingProfile* profile = BrowserWithTestWindowTest::CreateProfile();
    // TemplateURLService is normally NULL during testing. Instant extended
    // needs this service so set a custom factory function.
    TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        profile, &BookmarkBarViewTest::CreateTemplateURLService);
    return profile;
  }

  // Creates the BookmarkBarView and BookmarkBarViewTestHelper. Generally you'll
  // want to use CreateBookmarkModelAndBookmarkBarView(), but use this if
  // need to create the BookmarkBarView after the model has populated.
  void CreateBookmarkBarView() {
    bookmark_bar_view_.reset(new BookmarkBarView(browser(), nullptr));
    test_helper_.reset(new BookmarkBarViewTestHelper(bookmark_bar_view_.get()));
  }

  // Creates the model, blocking until it loads, then creates the
  // BookmarkBarView.
  void CreateBookmarkModelAndBookmarkBarView() {
    profile()->CreateBookmarkModel(true);
    bookmarks::test::WaitForBookmarkModelToLoad(
        BookmarkModelFactory::GetForProfile(profile()));
    CreateBookmarkBarView();
  }

  scoped_ptr<BookmarkBarViewTestHelper> test_helper_;
  scoped_ptr<BookmarkBarView> bookmark_bar_view_;

 private:
  static KeyedService* CreateTemplateURLService(
      content::BrowserContext* profile) {
    return new TemplateURLService(
        static_cast<Profile*>(profile)->GetPrefs(),
        make_scoped_ptr(new SearchTermsData), NULL,
        scoped_ptr<TemplateURLServiceClient>(), NULL, NULL, base::Closure());
  }

  scoped_ptr<ScopedTestingLocalState> local_state_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarViewTest);
};

// Verify that in instant extended mode the visibility of the apps shortcut
// button properly follows the pref value.
TEST_F(BookmarkBarViewTest, AppsShortcutVisibility) {
  CreateBookmarkModelAndBookmarkBarView();
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar, false);
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->visible());

  // Try to make the Apps shortcut visible. Its visibility depends on whether
  // the app launcher is enabled.
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar, true);
  if (IsAppLauncherEnabled()) {
    EXPECT_FALSE(test_helper_->apps_page_shortcut()->visible());
  } else {
    EXPECT_TRUE(test_helper_->apps_page_shortcut()->visible());
  }

  // Make sure we can also properly transition from true to false.
  browser()->profile()->GetPrefs()->SetBoolean(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar, false);
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->visible());
}

#if !defined(OS_CHROMEOS)
// Verifies that the apps shortcut is shown or hidden following the policy
// value. This policy (and the apps shortcut) isn't present on ChromeOS.
TEST_F(BookmarkBarViewTest, ManagedShowAppsShortcutInBookmarksBar) {
  CreateBookmarkModelAndBookmarkBarView();
  // By default, the pref is not managed and the apps shortcut is shown.
  TestingPrefServiceSyncable* prefs = profile()->GetTestingPrefService();
  EXPECT_FALSE(prefs->IsManagedPreference(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar));
  EXPECT_TRUE(test_helper_->apps_page_shortcut()->visible());

  // Hide the apps shortcut by policy, via the managed pref.
  prefs->SetManagedPref(bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
                        new base::FundamentalValue(false));
  EXPECT_FALSE(test_helper_->apps_page_shortcut()->visible());

  // And try showing it via policy too.
  prefs->SetManagedPref(bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
                        new base::FundamentalValue(true));
  EXPECT_TRUE(test_helper_->apps_page_shortcut()->visible());
}
#endif
