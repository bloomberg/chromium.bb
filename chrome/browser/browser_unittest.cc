// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const struct NavigationScenario {
  bool pinned;
  const char* url;
  const char* referrer;
  PageTransition::Type transition;
  WindowOpenDisposition original_disposition;
  WindowOpenDisposition result_disposition;
} kNavigationScenarios[] = {
  // Disposition changes to new foreground.
  { true,
    "http://www.example.com",
    "http://www.google.com",
    PageTransition::LINK,
    CURRENT_TAB,
    NEW_FOREGROUND_TAB },
  // Also works with AUTO_BOOKMARK.
  { true,
    "http://www.example.com",
    "http://www.google.com",
    PageTransition::AUTO_BOOKMARK,
    CURRENT_TAB,
    NEW_FOREGROUND_TAB },
  // Also works with TYPED.
  { true,
    "http://www.example.com",
    "http://www.google.com",
    PageTransition::TYPED,
    CURRENT_TAB,
    NEW_FOREGROUND_TAB },
  // Also happens if the schemes differ.
  { true,
    "ftp://www.example.com",
    "http://www.example.com",
    PageTransition::LINK,
    CURRENT_TAB,
    NEW_FOREGROUND_TAB },
  // Don't choke on an empty referrer.
  { true,
    "ftp://www.example.com",
    "",
    PageTransition::LINK,
    CURRENT_TAB,
    NEW_FOREGROUND_TAB },
  // Unpinned tab - no change.
  { false,
    "http://www.example.com",
    "http://www.google.com",
    PageTransition::LINK,
    CURRENT_TAB,
    CURRENT_TAB },
  // Original disposition is not CURRENT_TAB - no change.
  { true,
    "http://www.example.com",
    "http://www.google.com",
    PageTransition::LINK,
    NEW_BACKGROUND_TAB,
    NEW_BACKGROUND_TAB },
  // Other PageTransition type - no change.
  { true,
    "http://www.example.com",
    "http://www.google.com",
    PageTransition::RELOAD,
    CURRENT_TAB,
    CURRENT_TAB },
  // Same domain and scheme - no change.
  { true,
    "http://www.google.com/reader",
    "http://www.google.com",
    PageTransition::LINK,
    CURRENT_TAB,
    CURRENT_TAB },
  // Switching between http and https - no change.
  { true,
    "https://www.example.com",
    "http://www.example.com",
    PageTransition::LINK,
    CURRENT_TAB,
    CURRENT_TAB },
  // Switching between https and http - no change.
  { true,
    "http://www.example.com",
    "https://www.example.com",
    PageTransition::LINK,
    CURRENT_TAB,
    CURRENT_TAB },
};

}  // namespace

TEST(BrowserTest, PinnedTabDisposition) {
  for (size_t i = 0; i < arraysize(kNavigationScenarios); ++i) {
    EXPECT_EQ(kNavigationScenarios[i].result_disposition,
              Browser::AdjustWindowOpenDispositionForTab(
                  kNavigationScenarios[i].pinned,
                  GURL(kNavigationScenarios[i].url),
                  GURL(kNavigationScenarios[i].referrer),
                  kNavigationScenarios[i].transition,
                  kNavigationScenarios[i].original_disposition)) << i;
  }
}
