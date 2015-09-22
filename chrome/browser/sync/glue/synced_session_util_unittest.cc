// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/synced_session_util.h"

#include "base/memory/scoped_ptr.h"
#include "components/sessions/serialized_navigation_entry_test_helper.h"
#include "components/sessions/session_types.h"
#include "content/public/browser/navigation_entry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using sessions::SessionTab;
using sessions::SessionWindow;

namespace browser_sync {

// TODO(skym): Test ShouldSyncSessionTab
// TODO(skym): Test SessionWindowHasNoTabsToSync

namespace {

static const std::string kValidUrl = "http://www.example.com";
static const std::string kInvalidUrl = "invalid.url";

sessions::SerializedNavigationEntry Entry(std::string url) {
  return sessions::SerializedNavigationEntryTestHelper::CreateNavigation(url,
                                                                         "");
}

}  // namespace

TEST(SyncedSessionUtilTest, ShouldSyncURL) {
  EXPECT_TRUE(ShouldSyncURL(GURL(kValidUrl)));
  EXPECT_TRUE(ShouldSyncURL(GURL("other://anything")));
  EXPECT_TRUE(ShouldSyncURL(GURL("chrome-other://anything")));

  EXPECT_FALSE(ShouldSyncURL(GURL(kInvalidUrl)));
  EXPECT_FALSE(ShouldSyncURL(GURL("file://anything")));
  EXPECT_FALSE(ShouldSyncURL(GURL("chrome://anything")));
  EXPECT_FALSE(ShouldSyncURL(GURL("chrome-native://anything")));
}

TEST(SyncedSessionUtilTest, ShouldSyncSessionTabValid) {
  SessionTab tab;
  tab.navigations.push_back(Entry(kValidUrl));
  EXPECT_TRUE(ShouldSyncSessionTab(tab));
}

TEST(SyncedSessionUtilTest, ShouldSyncSessionTabEmpty) {
  SessionTab tab;
  EXPECT_FALSE(ShouldSyncSessionTab(tab));
}

TEST(SyncedSessionUtilTest, ShouldSyncSessionTabInvalid) {
  SessionTab tab;
  tab.navigations.push_back(Entry(kInvalidUrl));
  EXPECT_FALSE(ShouldSyncSessionTab(tab));
}

TEST(SyncedSessionUtilTest, ShouldSyncSessionTabMultiple) {
  SessionTab tab;
  tab.navigations.push_back(Entry(kInvalidUrl));
  tab.navigations.push_back(Entry(kValidUrl));
  tab.navigations.push_back(Entry(kInvalidUrl));
  EXPECT_TRUE(ShouldSyncSessionTab(tab));
}

TEST(SyncedSessionUtilTest, ShouldSyncSessionWindowValid) {
  scoped_ptr<SessionTab> tab(new SessionTab());
  tab->navigations.push_back(Entry(kValidUrl));

  SessionWindow window;
  window.tabs.push_back(tab.release());

  EXPECT_TRUE(ShouldSyncSessionWindow(window));
}

TEST(SyncedSessionUtilTest, ShouldSyncSessionWindowEmpty) {
  SessionWindow window;
  EXPECT_FALSE(ShouldSyncSessionWindow(window));
}

TEST(SyncedSessionUtilTest, ShouldSyncSessionWindowInvalid) {
  scoped_ptr<SessionTab> tab(new SessionTab());
  tab->navigations.push_back(Entry(kInvalidUrl));

  SessionWindow window;
  window.tabs.push_back(tab.release());

  EXPECT_FALSE(ShouldSyncSessionWindow(window));
}

TEST(SyncedSessionUtilTest, ShouldSyncSessionWindowMultiple) {
  scoped_ptr<SessionTab> tab1(new SessionTab());
  tab1->navigations.push_back(Entry(kInvalidUrl));

  scoped_ptr<SessionTab> tab2(new SessionTab());

  scoped_ptr<SessionTab> tab3(new SessionTab());
  tab3->navigations.push_back(Entry(kInvalidUrl));
  tab3->navigations.push_back(Entry(kValidUrl));
  tab3->navigations.push_back(Entry(kInvalidUrl));

  SessionWindow window;
  window.tabs.push_back(tab1.release());
  window.tabs.push_back(tab2.release());
  window.tabs.push_back(tab3.release());

  EXPECT_TRUE(ShouldSyncSessionWindow(window));
}

}  // namespace browser_sync
