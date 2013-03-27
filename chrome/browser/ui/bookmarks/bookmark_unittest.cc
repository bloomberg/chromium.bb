// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

typedef BrowserWithTestWindowTest BookmarkTest;

// Verify that the detached bookmark bar is visible on the new tab page.
TEST_F(BookmarkTest, DetachedBookmarkBarOnNTP) {
  AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_EQ(BookmarkBar::DETACHED, browser()->bookmark_bar_state());
}

// Verify that the detached bookmark bar is hidden on custom NTP pages.
TEST_F(BookmarkTest, DetachedBookmarkBarOnCustomNTP) {
  // Create a empty commited web contents.
  content::WebContents* web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(browser()->profile()));
  web_contents->GetController().LoadURL(
        GURL(chrome::kAboutBlankURL), content::Referrer(),
        content::PAGE_TRANSITION_LINK, std::string());

  // Give it a NTP virtual URL.
  content::NavigationController* controller = &web_contents->GetController();
  content::NavigationEntry* entry = controller->GetVisibleEntry();
  entry->SetVirtualURL(GURL(chrome::kChromeUINewTabURL));

  // Verify that the detached bookmark bar is hidden.
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
  browser()->tab_strip_model()->AppendWebContents(web_contents, true);
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());
}

class BookmarkInstantExtendedTest : public BrowserWithTestWindowTest {
 public:
  BookmarkInstantExtendedTest() {
    chrome::EnableInstantExtendedAPIForTesting();
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
