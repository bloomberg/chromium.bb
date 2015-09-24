// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/tab_manager.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/memory/tab_manager_delegate_chromeos.h"
#include "chrome/browser/memory/tab_stats.h"
#include "chrome/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace memory {

typedef testing::Test TabManagerDelegateTest;

TEST_F(TabManagerDelegateTest, GetProcessHandles) {
  TabStats stats;
  std::vector<TabManagerDelegate::ProcessInfo> process_id_pairs;

  // Empty stats list gives empty |process_id_pairs| list.
  TabStatsList empty_list;
  process_id_pairs =
      TabManagerDelegate::GetChildProcessInfos(empty_list);
  EXPECT_EQ(0u, process_id_pairs.size());

  // Two tabs in two different processes generates two
  // |child_process_host_id| out.
  TabStatsList two_list;
  stats.child_process_host_id = 100;
  stats.renderer_handle = 101;
  two_list.push_back(stats);
  stats.child_process_host_id = 200;
  stats.renderer_handle = 201;
  two_list.push_back(stats);
  process_id_pairs = TabManagerDelegate::GetChildProcessInfos(two_list);
  EXPECT_EQ(2u, process_id_pairs.size());
  EXPECT_EQ(100, process_id_pairs[0].first);
  EXPECT_EQ(101, process_id_pairs[0].second);
  EXPECT_EQ(200, process_id_pairs[1].first);
  EXPECT_EQ(201, process_id_pairs[1].second);

  // Zero handles are removed.
  TabStatsList zero_handle_list;
  stats.child_process_host_id = 100;
  stats.renderer_handle = 0;
  zero_handle_list.push_back(stats);
  process_id_pairs =
      TabManagerDelegate::GetChildProcessInfos(zero_handle_list);
  EXPECT_EQ(0u, process_id_pairs.size());

  // Two tabs in the same process generates one handle out. When a duplicate
  // occurs the later instance is dropped.
  TabStatsList same_process_list;
  stats.child_process_host_id = 100;
  stats.renderer_handle = 101;
  same_process_list.push_back(stats);
  stats.child_process_host_id = 200;
  stats.renderer_handle = 201;
  same_process_list.push_back(stats);
  stats.child_process_host_id = 300;
  stats.renderer_handle = 101;  // Duplicate.
  same_process_list.push_back(stats);
  process_id_pairs =
      TabManagerDelegate::GetChildProcessInfos(same_process_list);
  EXPECT_EQ(2u, process_id_pairs.size());
  EXPECT_EQ(100, process_id_pairs[0].first);
  EXPECT_EQ(101, process_id_pairs[0].second);
  EXPECT_EQ(200, process_id_pairs[1].first);
  EXPECT_EQ(201, process_id_pairs[1].second);
}

}  // namespace memory
