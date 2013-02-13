// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"

typedef BrowserWithTestWindowTest BookmarkTest;

// Verify that the detached bookmark bar is visible on the new tab page.
TEST_F(BookmarkTest, DetachedBookmarkBarOnNTP) {
  AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(BookmarkBar::DETACHED, browser()->bookmark_bar_state());
}

class BookmarkInstantExtendedTest : public BrowserWithTestWindowTest {
 public:
  BookmarkInstantExtendedTest() {
    chrome::search::EnableInstantExtendedAPIForTesting();
  }

 protected:
  virtual TestingProfile* CreateProfile() OVERRIDE {
    TestingProfile* profile = BrowserWithTestWindowTest::CreateProfile();
    // TemplateURLService is normally NULL during testing. Instant extended
    // needs this service so set a custom factory function.
    TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        profile, &BookmarkInstantExtendedTest::CreateTemplateURLService);
    return profile;
  }

 private:
  static ProfileKeyedService* CreateTemplateURLService(Profile* profile) {
    return new TemplateURLService(profile);
  }

  DISALLOW_COPY_AND_ASSIGN(BookmarkInstantExtendedTest);
};

// Verify that in instant extended mode the detached bookmark bar is visible on
// the new tab page.
TEST_F(BookmarkInstantExtendedTest, DetachedBookmarkBarOnNTP) {
  AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(BookmarkBar::DETACHED, browser()->bookmark_bar_state());
}
