// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_utils.h"

#include "base/stl_util.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/sessions/session_types.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

class TabNavigationMock : public TabNavigation {
 public:
  TabNavigationMock(const char* url, const char* title)
      : TabNavigation(0,                                  // index
                      GURL(string16(ASCIIToUTF16(url))),  // virtual_url
                      GURL(),                             // referrer
                      string16(ASCIIToUTF16(title)),      // title
                      "",                                 // state
                      PageTransition::FROM_ADDRESS_BAR) {
  }
};

class SessionUtilsTest : public testing::Test {
 protected:
  class TabMock : public TabRestoreService::Tab {
   public:
    TabMock(const char* url, const char* title) {
      navigations.push_back(TabNavigationMock(url, title));
      current_navigation_index = 0;
    }
  };

  virtual void SetUp() {
    // prefill the entries

    // Two identical
    entries_.push_back(new TabMock("http://a", "a"));
    entries_.push_back(new TabMock("http://a", "a"));

    // Different URL
    entries_.push_back(new TabMock("http://b", "b"));
    entries_.push_back(new TabMock("http://c", "b"));

    // Different Title
    entries_.push_back(new TabMock("http://d", "d"));
    entries_.push_back(new TabMock("http://d", "e"));

    // Nothing in common
    entries_.push_back(new TabMock("http://f", "f"));
    entries_.push_back(new TabMock("http://g", "g"));
  }

  void TearDown() {
    STLDeleteElements(&entries_);
  }

  TabRestoreService::Entries entries_;
};

TEST_F(SessionUtilsTest, SessionUtilsFilter) {
  TabRestoreService::Entries filtered_entries;

  SessionUtils::FilteredEntries(entries_, &filtered_entries);
  ASSERT_EQ(7U, filtered_entries.size());

  // The filtering should have removed the second item
  TabRestoreService::Entries expected = entries_;
  TabRestoreService::Entry* first = expected.front();
  expected.pop_front();
  expected.pop_front();
  expected.push_front(first);
  ASSERT_EQ(expected, filtered_entries);
}
