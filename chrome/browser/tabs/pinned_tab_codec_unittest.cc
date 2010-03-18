// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/browser/tabs/pinned_tab_codec.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef BrowserInit::LaunchWithProfile::Tab Tab;

typedef BrowserWithTestWindowTest PinnedTabCodecTest;

namespace {

std::string TabToString(const Tab& tab) {
  return tab.url.spec() + ":" + (tab.is_app ? "app" : "") + ":" +
      (tab.is_pinned ? "pinned" : "") + ":" + tab.app_id;
}

std::string TabsToString(const std::vector<Tab>& values) {
  std::string result;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0)
      result += " ";
    result += TabToString(values[i]);
  }
  return result;
}

}  // namespace

// Make sure nothing is restored when the browser has no pinned tabs.
TEST_F(PinnedTabCodecTest, NoPinnedTabs) {
  GURL url1("http://www.google.com");
  AddTab(browser(), url1);

  PinnedTabCodec::WritePinnedTabs(profile());

  std::string result = TabsToString(PinnedTabCodec::ReadPinnedTabs(profile()));
  EXPECT_EQ("", result);
}

// Creates a browser with one pinned tab and one normal tab, does restore and
// makes sure we get back another pinned tab.
TEST_F(PinnedTabCodecTest, PinnedAndNonPinned) {
  GURL url1("http://www.google.com");
  GURL url2("http://www.google.com/2");
  AddTab(browser(), url2);

  // AddTab inserts at index 0, so order after this is url1, url2.
  AddTab(browser(), url1);

  browser()->tabstrip_model()->SetTabPinned(0, true);

  PinnedTabCodec::WritePinnedTabs(profile());

  std::string result = TabsToString(PinnedTabCodec::ReadPinnedTabs(profile()));
  EXPECT_EQ("http://www.google.com/::pinned:", result);
}

