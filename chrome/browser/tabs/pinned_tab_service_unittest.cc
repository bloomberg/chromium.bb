// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/browser/tabs/pinned_tab_codec.h"
#include "chrome/browser/tabs/pinned_tab_service.h"
#include "chrome/browser/tabs/pinned_tab_test_utils.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class PinnedTabServiceTest : public BrowserWithTestWindowTest {
 public:
  PinnedTabServiceTest() {}

 protected:
  virtual TestingProfile* CreateProfile() OVERRIDE {
    TestingProfile* profile = BrowserWithTestWindowTest::CreateProfile();
    pinned_tab_service_.reset(new PinnedTabService(profile));
    return profile;
  }

 private:
  scoped_ptr<PinnedTabService> pinned_tab_service_;

  DISALLOW_COPY_AND_ASSIGN(PinnedTabServiceTest);
};

// Makes sure closing a popup triggers writing pinned tabs.
TEST_F(PinnedTabServiceTest, Popup) {
  GURL url("http://www.google.com");
  AddTab(browser(), url);
  browser()->tabstrip_model()->SetTabPinned(0, true);

  // Create a popup.
  scoped_ptr<Browser> popup(new Browser(Browser::TYPE_POPUP, profile()));
  scoped_ptr<TestBrowserWindow> popup_window(
      new TestBrowserWindow(popup.get()));
  popup->set_window(popup_window.get());

  // Close the browser. This should trigger saving the tabs.
  browser()->OnWindowClosing();
  DestroyBrowser();
  std::string result = PinnedTabTestUtils::TabsToString(
      PinnedTabCodec::ReadPinnedTabs(profile()));
  EXPECT_EQ("http://www.google.com/::pinned:", result);

  // Close the popup. This shouldn't reset the saved state.
  popup->CloseAllTabs();
  popup.reset(NULL);
  popup_window.reset(NULL);

  // Check the state to make sure it hasn't changed.
  result = PinnedTabTestUtils::TabsToString(
      PinnedTabCodec::ReadPinnedTabs(profile()));
  EXPECT_EQ("http://www.google.com/::pinned:", result);
}

