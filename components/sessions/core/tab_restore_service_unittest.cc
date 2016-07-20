// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/tab_restore_service.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace sessions {

namespace {

void PopulateTab(TabRestoreService::Tab* tab) {
  tab->timestamp = base::Time::FromDoubleT(100.0);
  tab->from_last_session = true;
  tab->current_navigation_index = 42;
  tab->browser_id = 1;
  tab->tabstrip_index = 5;
  tab->pinned = true;
  tab->extension_app_id = "dummy";
  tab->user_agent_override = "override";
}

void TestEntryEquality(TabRestoreService::Entry* expected,
                       TabRestoreService::Entry* actual) {
  EXPECT_EQ(expected->id, actual->id);
  EXPECT_EQ(expected->type, actual->type);
  EXPECT_EQ(expected->timestamp, actual->timestamp);
  EXPECT_EQ(expected->from_last_session, actual->from_last_session);
}

void TestTabEquality(TabRestoreService::Tab* expected,
                     TabRestoreService::Tab* actual) {
  TestEntryEquality(expected, actual);
  EXPECT_EQ(expected->current_navigation_index,
            actual->current_navigation_index);
  EXPECT_EQ(expected->browser_id, actual->browser_id);
  EXPECT_EQ(expected->tabstrip_index, actual->tabstrip_index);
  EXPECT_EQ(expected->pinned, actual->pinned);
  EXPECT_EQ(expected->extension_app_id, actual->extension_app_id);
  EXPECT_EQ(expected->user_agent_override, actual->user_agent_override);
}

TEST(Tab, CopyConstructor) {
  TabRestoreService::Tab tab_to_copy;
  PopulateTab(&tab_to_copy);

  TabRestoreService::Tab copied_tab(tab_to_copy);
  TestTabEquality(&tab_to_copy, &copied_tab);
}

TEST(Tab, AssignmentOperator) {
  TabRestoreService::Tab tab_to_assign;
  PopulateTab(&tab_to_assign);

  TabRestoreService::Tab assigned_tab;
  assigned_tab = tab_to_assign;
  TestTabEquality(&tab_to_assign, &assigned_tab);
}

}  // namespace

}  // namespace sessions
