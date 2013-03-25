// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/views/controls/button/text_button.h"

typedef BrowserWithTestWindowTest BookmarkBarViewTest;

// Verify that the apps shortcut is never visible without instant extended.
TEST_F(BookmarkBarViewTest, NoAppsShortcutWithoutInstantExtended) {
  profile()->CreateBookmarkModel(true);
  ui_test_utils::WaitForBookmarkModelToLoad(profile());
  BookmarkBarView bookmark_bar_view(browser(), NULL);
  bookmark_bar_view.set_owned_by_client();
  EXPECT_FALSE(bookmark_bar_view.apps_page_shortcut_->visible());
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShowAppsShortcutInBookmarkBar, true);
  EXPECT_FALSE(bookmark_bar_view.apps_page_shortcut_->visible());
}

class BookmarkBarViewInstantExtendedTest : public BrowserWithTestWindowTest {
 public:
  BookmarkBarViewInstantExtendedTest() {
    chrome::search::EnableInstantExtendedAPIForTesting();
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
  static ProfileKeyedService* CreateTemplateURLService(Profile* profile) {
    return new TemplateURLService(profile);
  }

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarViewInstantExtendedTest);
};

// Verify that in instant extended mode the visibility of the apps shortcut
// button properly follows the pref value.
TEST_F(BookmarkBarViewInstantExtendedTest, AppsShortcutVisibility) {
  profile()->CreateBookmarkModel(true);
  ui_test_utils::WaitForBookmarkModelToLoad(profile());
  BookmarkBarView bookmark_bar_view(browser(), NULL);
  bookmark_bar_view.set_owned_by_client();
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShowAppsShortcutInBookmarkBar, false);
  EXPECT_FALSE(bookmark_bar_view.apps_page_shortcut_->visible());
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShowAppsShortcutInBookmarkBar, true);
  EXPECT_TRUE(bookmark_bar_view.apps_page_shortcut_->visible());
  // Make sure we can also properly transition from true to false.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kShowAppsShortcutInBookmarkBar, false);
  EXPECT_FALSE(bookmark_bar_view.apps_page_shortcut_->visible());
}
