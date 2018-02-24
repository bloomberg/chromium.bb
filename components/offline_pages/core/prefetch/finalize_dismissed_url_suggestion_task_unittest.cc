// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/finalize_dismissed_url_suggestion_task.h"

#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_task_test_base.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class FinalizeDismissedUrlSuggestionTaskTest : public PrefetchTaskTestBase {
 public:
  ~FinalizeDismissedUrlSuggestionTaskTest() override = default;

  PrefetchItem AddItem(PrefetchItemState state) {
    PrefetchItem item = item_generator()->CreateItem(state);
    EXPECT_TRUE(store_util()->InsertPrefetchItem(item));
    return item;
  }
};

TEST_F(FinalizeDismissedUrlSuggestionTaskTest, StoreFailure) {
  PrefetchItem item = AddItem(PrefetchItemState::RECEIVED_BUNDLE);
  store_util()->SimulateInitializationError();

  FinalizeDismissedUrlSuggestionTask task(store(), item.client_id);
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();
}

TEST_F(FinalizeDismissedUrlSuggestionTaskTest, NotFound) {
  PrefetchItem item = AddItem(PrefetchItemState::RECEIVED_BUNDLE);
  FinalizeDismissedUrlSuggestionTask task(store(), ClientId("abc", "123"));
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();
  EXPECT_EQ(1, store_util()->CountPrefetchItems());
}

TEST_F(FinalizeDismissedUrlSuggestionTaskTest, Change) {
  // All states where we expect a transition to SUGGESTION_INVALIDATED.
  // TODO(carlosk): Make this test robust to future changes to the
  // |PrefetchItemState| enum.
  const std::vector<PrefetchItemState> change_states = {
      PrefetchItemState::NEW_REQUEST,
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE,
      PrefetchItemState::AWAITING_GCM,
      PrefetchItemState::RECEIVED_GCM,
      PrefetchItemState::SENT_GET_OPERATION,
      PrefetchItemState::RECEIVED_BUNDLE,
  };
  // Add an item for each state, and add the FINISHED item to the expectation.
  std::vector<PrefetchItem> items;
  std::set<PrefetchItem> want_items;
  for (const PrefetchItemState state : change_states) {
    PrefetchItem item = AddItem(state);
    items.push_back(item);
    item.state = PrefetchItemState::FINISHED;
    item.error_code = PrefetchItemErrorCode::SUGGESTION_INVALIDATED;
    want_items.insert(item);
  }
  for (const PrefetchItem& item : items) {
    FinalizeDismissedUrlSuggestionTask task(store(), item.client_id);
    ExpectTaskCompletes(&task);
    task.Run();
    RunUntilIdle();
  }

  std::set<PrefetchItem> final_items;
  store_util()->GetAllItems(&final_items);
  EXPECT_EQ(want_items, final_items);
}

TEST_F(FinalizeDismissedUrlSuggestionTaskTest, NoChange) {
  // All states where no change is made.
  // TODO(carlosk): Make this test robust to future changes to the
  // |PrefetchItemState| enum.
  const std::vector<PrefetchItemState> no_change_states = {
      PrefetchItemState::DOWNLOADING, PrefetchItemState::DOWNLOADED,
      PrefetchItemState::IMPORTING,   PrefetchItemState::FINISHED,
      PrefetchItemState::ZOMBIE,
  };
  std::set<PrefetchItem> items;
  for (const PrefetchItemState state : no_change_states) {
    items.insert(AddItem(state));
  }
  for (const PrefetchItem& item : items) {
    FinalizeDismissedUrlSuggestionTask task(store(), item.client_id);
    ExpectTaskCompletes(&task);
    task.Run();
    RunUntilIdle();
  }

  std::set<PrefetchItem> final_items;
  store_util()->GetAllItems(&final_items);
  EXPECT_EQ(items, final_items);
}

}  // namespace offline_pages
