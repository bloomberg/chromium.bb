// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_map>
#include <utility>
#include <vector>

#include "ash/link_handler_model.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/open_with_menu_factory_ash.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// All tests in this file assume that 4 and 10 IDC command IDs are reserved
// for the main and sub menus, respectively.
const int kFirstMainMenuId = IDC_CONTENT_CONTEXT_OPEN_WITH1;
const int kFirstSubMenuId = kFirstMainMenuId + 4;
const int kNumCommandIds =
    IDC_CONTENT_CONTEXT_OPEN_WITH_LAST - IDC_CONTENT_CONTEXT_OPEN_WITH1 + 1;

static_assert(kNumCommandIds == 14,
              "invalid number of command IDs reserved for open with");

using HandlerMap = std::unordered_map<int, ash::LinkHandlerInfo>;

std::pair<HandlerMap, int> BuildHandlersMap(size_t num_apps) {
  std::vector<ash::LinkHandlerInfo> handlers;
  gfx::Image image;
  for (size_t i = 0; i < num_apps; ++i) {
    ash::LinkHandlerInfo info = {base::StringPrintf("App %" PRIuS, i), image,
                                 i};
    handlers.push_back(info);
  }
  return BuildHandlersMapForTesting(handlers);
}

}  // namespace

TEST(OpenWithMenuObserverTest, TestBuildHandlersMap) {
  auto result = BuildHandlersMap(0);
  EXPECT_EQ(0U, result.first.size());
  EXPECT_EQ(-1, result.second);

  result = BuildHandlersMap(1);
  ASSERT_EQ(1U, result.first.size());
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  EXPECT_EQ(-1, result.second);
  EXPECT_EQ("App 0", result.first[kFirstMainMenuId].name);

  result = BuildHandlersMap(2);
  EXPECT_EQ(2U, result.first.size());
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 1));
  EXPECT_EQ(-1, result.second);
  EXPECT_EQ("App 0", result.first[kFirstMainMenuId].name);
  EXPECT_EQ("App 1", result.first[kFirstMainMenuId + 1].name);

  result = BuildHandlersMap(3);
  EXPECT_EQ(3U, result.first.size());
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 1));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 2));
  EXPECT_EQ(-1, result.second);
  EXPECT_EQ("App 0", result.first[kFirstMainMenuId].name);
  EXPECT_EQ("App 1", result.first[kFirstMainMenuId + 1].name);
  EXPECT_EQ("App 2", result.first[kFirstMainMenuId + 2].name);

  // Test if app names will overflow to the sub menu.
  result = BuildHandlersMap(4);
  EXPECT_EQ(4U, result.first.size());
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 1));
  // In this case, kFirstMainMenuId + 2 should be hidden.
  EXPECT_EQ(kFirstMainMenuId + 3, result.second);
  ASSERT_EQ(1U, result.first.count(kFirstSubMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstSubMenuId + 1));
  EXPECT_EQ("App 0", result.first[kFirstMainMenuId].name);
  EXPECT_EQ("App 1", result.first[kFirstMainMenuId + 1].name);
  EXPECT_EQ("App 2", result.first[kFirstSubMenuId].name);
  EXPECT_EQ("App 3", result.first[kFirstSubMenuId + 1].name);

  result = BuildHandlersMap(11);
  EXPECT_EQ(11U, result.first.size());
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 1));
  EXPECT_EQ(kFirstMainMenuId + 3, result.second);
  EXPECT_EQ("App 0", result.first[kFirstMainMenuId].name);
  EXPECT_EQ("App 1", result.first[kFirstMainMenuId + 1].name);
  for (size_t i = 0; i < 9; ++i) {
    ASSERT_EQ(1U, result.first.count(kFirstSubMenuId + i)) << i;
  }

  // The main and sub menus can show up to 12 (=3+10-1) app names.
  result = BuildHandlersMap(12);
  EXPECT_EQ(12U, result.first.size());
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 1));
  EXPECT_EQ(kFirstMainMenuId + 3, result.second);
  EXPECT_EQ("App 0", result.first[kFirstMainMenuId].name);
  EXPECT_EQ("App 1", result.first[kFirstMainMenuId + 1].name);
  for (size_t i = 0; i < 10; ++i) {
    const int id = kFirstSubMenuId + i;
    ASSERT_EQ(1U, result.first.count(id)) << i;
    EXPECT_EQ(base::StringPrintf("App %zu", i + 2), result.first[id].name) << i;
  }

  result = BuildHandlersMap(13);
  EXPECT_EQ(12U, result.first.size());  // still 12
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 1));
  EXPECT_EQ(kFirstMainMenuId + 3, result.second);
  EXPECT_EQ("App 0", result.first[kFirstMainMenuId].name);
  EXPECT_EQ("App 1", result.first[kFirstMainMenuId + 1].name);
  for (size_t i = 0; i < 10; ++i) {  // still 10
    const int id = kFirstSubMenuId + i;
    ASSERT_EQ(1U, result.first.count(id)) << i;
    EXPECT_EQ(base::StringPrintf("App %zu", i + 2), result.first[id].name) << i;
  }

  result = BuildHandlersMap(1000);
  EXPECT_EQ(12U, result.first.size());  // still 12
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId));
  ASSERT_EQ(1U, result.first.count(kFirstMainMenuId + 1));
  EXPECT_EQ(kFirstMainMenuId + 3, result.second);
  EXPECT_EQ("App 0", result.first[kFirstMainMenuId].name);
  EXPECT_EQ("App 1", result.first[kFirstMainMenuId + 1].name);
  for (size_t i = 0; i < 10; ++i) {  // still 10
    const int id = kFirstSubMenuId + i;
    ASSERT_EQ(1U, result.first.count(id)) << i;
    EXPECT_EQ(base::StringPrintf("App %zu", i + 2), result.first[id].name) << i;
  }
}
