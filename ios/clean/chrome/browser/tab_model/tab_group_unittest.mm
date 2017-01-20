// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/tab_model/tab_group.h"

#include "base/logging.h"
#import "ios/clean/chrome/browser/web/web_mediator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

namespace {

// Creates a tab group with three tabs.
TabGroup* TestTabGroup() {
  TabGroup* group = [[TabGroup alloc] init];
  [group appendTab:[[WebMediator alloc] init]];
  [group appendTab:[[WebMediator alloc] init]];
  [group appendTab:[[WebMediator alloc] init]];
  group.activeTab = [group tabAtIndex:0];
  return group;
}

TEST(TabGroupTest, TestEmptyGroup) {
  TabGroup* group = [[TabGroup alloc] init];

  EXPECT_TRUE(group.empty);
  EXPECT_EQ(0UL, group.count);
  EXPECT_EQ(nil, [group tabAtIndex:0]);
  EXPECT_EQ(nil, group.activeTab);
  WebMediator* tab = [[WebMediator alloc] init];
  EXPECT_EQ(static_cast<NSUInteger>(NSNotFound), [group indexOfTab:tab]);
}

TEST(TabGroupTest, TestProperties) {
  TabGroup* group = TestTabGroup();

  EXPECT_EQ(3UL, group.count);
  EXPECT_FALSE(group.empty);
  EXPECT_NE(nil, [group tabAtIndex:0]);
  EXPECT_NE(nil, group.activeTab);
  EXPECT_EQ(1UL, [group indexOfTab:[group tabAtIndex:1]]);
}

TEST(TabGroupTest, TestRemoving) {
  TabGroup* group = TestTabGroup();

  [group removeTab:group.activeTab];
  EXPECT_EQ(2UL, group.count);
  EXPECT_FALSE(group.empty);
  EXPECT_NE(nil, [group tabAtIndex:0]);
  EXPECT_EQ(nil, group.activeTab);
  EXPECT_EQ(nil, [group tabAtIndex:2]);

  WebMediator* tab = [[WebMediator alloc] init];
  [group removeTab:tab];
  EXPECT_EQ(2UL, group.count);

  [group appendTab:tab];
  group.activeTab = tab;
  [group removeTab:tab];
  EXPECT_EQ(nil, group.activeTab);
}

TEST(TabGroupTest, TestAppending) {
  TabGroup* group = [[TabGroup alloc] init];
  WebMediator* tab1 = [[WebMediator alloc] init];
  WebMediator* tab2 = [[WebMediator alloc] init];

  [group appendTab:tab1];
  EXPECT_EQ(1UL, group.count);
  EXPECT_FALSE(group.empty);
  EXPECT_EQ(tab1, [group tabAtIndex:0]);
  EXPECT_EQ(nil, [group tabAtIndex:1]);

  // Setting a tab not in the group as the active tab does nothing.
  group.activeTab = tab2;
  EXPECT_EQ(nil, group.activeTab);

  group.activeTab = tab1;
  EXPECT_EQ(tab1, group.activeTab);
  // Setting an invalid tab as active will override a previous valid active tab.
  group.activeTab = tab2;
  EXPECT_EQ(nil, group.activeTab);

  [group appendTab:tab2];
  EXPECT_EQ(2UL, group.count);
  EXPECT_EQ(nil, group.activeTab);
  EXPECT_EQ(tab1, [group tabAtIndex:0]);
  EXPECT_EQ(tab2, [group tabAtIndex:1]);

  group.activeTab = tab2;
  EXPECT_EQ(tab2, group.activeTab);
}

TEST(TabGroupTest, TestEnumerating) {
  TabGroup* group = TestTabGroup();

  NSUInteger count = 0;
  NSMutableSet<WebMediator*>* seenTabs = [[NSMutableSet alloc] init];
  for (WebMediator* tab in group) {
    EXPECT_FALSE([seenTabs containsObject:tab]);
    [seenTabs addObject:tab];
    for (int i = count; i >= 0; i--) {
      // When we see the ith element, expect that we have already seen
      // the i-1, i-2 .. 0th elements.
      EXPECT_TRUE([seenTabs containsObject:[group tabAtIndex:i]])
          << "Expect to have seen tab " << i << " at or before iteration "
          << count;
    }
    count++;
  }
  EXPECT_EQ(group.count, seenTabs.count);
}

}  // namespace
