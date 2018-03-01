// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_task_test_base.h"

#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/task_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

// static
std::vector<PrefetchItemState> PrefetchTaskTestBase::GetAllStatesExcept(
    PrefetchItemState state_to_exclude) {
  static const PrefetchItemState all_states[] = {
      PrefetchItemState::NEW_REQUEST,
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE,
      PrefetchItemState::AWAITING_GCM,
      PrefetchItemState::RECEIVED_GCM,
      PrefetchItemState::SENT_GET_OPERATION,
      PrefetchItemState::RECEIVED_BUNDLE,
      PrefetchItemState::DOWNLOADING,
      PrefetchItemState::DOWNLOADED,
      PrefetchItemState::IMPORTING,
      PrefetchItemState::FINISHED,
      PrefetchItemState::ZOMBIE,
  };
  std::vector<PrefetchItemState> states;
  for (const auto& state : all_states) {
    if (state != state_to_exclude)
      states.push_back(state);
  }
  return states;
}

PrefetchTaskTestBase::PrefetchTaskTestBase()
    : store_test_util_(task_runner()) {}

PrefetchTaskTestBase::~PrefetchTaskTestBase() = default;

void PrefetchTaskTestBase::SetUp() {
  TaskTestBase::SetUp();
  store_test_util_.BuildStoreInMemory();
}

void PrefetchTaskTestBase::TearDown() {
  store_test_util_.DeleteStore();
  TaskTestBase::TearDown();
}

int64_t PrefetchTaskTestBase::InsertPrefetchItemInStateWithOperation(
    std::string operation_name,
    PrefetchItemState state) {
  PrefetchItem item;
  item.state = state;
  item.offline_id = store_utils::GenerateOfflineId();
  std::string offline_id_string = std::to_string(item.offline_id);
  item.url = GURL("http://www.example.com/?id=" + offline_id_string);
  item.operation_name = operation_name;
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item));
  return item.offline_id;
}

std::set<PrefetchItem> PrefetchTaskTestBase::FilterByState(
    const std::set<PrefetchItem>& items,
    PrefetchItemState state) const {
  std::set<PrefetchItem> result;
  for (const PrefetchItem& item : items) {
    if (item.state == state)
      result.insert(item);
  }
  return result;
}

}  // namespace offline_pages
