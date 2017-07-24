// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeOmniboxNavigationObserverTest : public testing::Test {
 protected:
  ChromeOmniboxNavigationObserverTest() {}
  ~ChromeOmniboxNavigationObserverTest() override {}

  content::NavigationController* navigation_controller() {
    return &(web_contents_->GetController());
  }

  TestingProfile* profile() { return &profile_; }
  const ChromeOmniboxNavigationObserver* observer() const {
    return observer_.get();
  }

 private:
  // testing::Test:
  void SetUp() override {
    web_contents_.reset(content::WebContentsTester::CreateTestWebContents(
        profile(), content::SiteInstance::Create(profile())));

    InfoBarService::CreateForWebContents(web_contents_.get());

    observer_ = base::MakeUnique<ChromeOmniboxNavigationObserver>(
        &profile_, base::ASCIIToUTF16("test text"), AutocompleteMatch(),
        AutocompleteMatch());
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<ChromeOmniboxNavigationObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOmniboxNavigationObserverTest);
};

TEST_F(ChromeOmniboxNavigationObserverTest, LoadStateAfterPendingNavigation) {
  EXPECT_EQ(ChromeOmniboxNavigationObserver::LOAD_NOT_SEEN,
            observer()->load_state());

  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationController::CreateNavigationEntry(
          GURL(), content::Referrer(), ui::PAGE_TRANSITION_FROM_ADDRESS_BAR,
          false, std::string(), profile()));

  content::NotificationService::current()->Notify(
      content::NOTIFICATION_NAV_ENTRY_PENDING,
      content::Source<content::NavigationController>(navigation_controller()),
      content::Details<content::NavigationEntry>(entry.get()));

  // A pending navigation notification should synchronously update the load
  // state to pending.
  EXPECT_EQ(ChromeOmniboxNavigationObserver::LOAD_PENDING,
            observer()->load_state());
}
