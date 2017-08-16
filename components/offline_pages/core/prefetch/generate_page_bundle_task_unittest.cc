// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/generate_page_bundle_task.h"

#include "base/logging.h"
#include "base/test/mock_callback.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"
#include "components/offline_pages/core/prefetch/task_test_base.h"
#include "components/offline_pages/core/prefetch/test_prefetch_gcm_handler.h"
#include "components/offline_pages/core/task.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Contains;
using testing::HasSubstr;
using testing::Not;

namespace offline_pages {

// All tests cases here only validate the request data and check for general
// http response. The tests for the Operation proto data returned in the http
// response are covered in PrefetchRequestOperationResponseTest.
class GeneratePageBundleTaskTest : public TaskTestBase {
 public:
  GeneratePageBundleTaskTest() = default;
  ~GeneratePageBundleTaskTest() override = default;

  TestPrefetchGCMHandler* gcm_handler() { return &gcm_handler_; }

 private:
  TestPrefetchGCMHandler gcm_handler_;
};

TEST_F(GeneratePageBundleTaskTest, EmptyTask) {
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  GeneratePageBundleTask task(store(), gcm_handler(),
                              prefetch_request_factory(), callback.Get());
  ExpectTaskCompletes(&task);

  task.Run();
  RunUntilIdle();

  EXPECT_FALSE(prefetch_request_factory()->HasOutstandingRequests());
  auto requested_urls = prefetch_request_factory()->GetAllUrlsRequested();
  EXPECT_TRUE(requested_urls->empty());
}

TEST_F(GeneratePageBundleTaskTest, TaskMakesNetworkRequest) {
  base::MockCallback<PrefetchRequestFinishedCallback> request_callback;

  PrefetchItem item1 =
      item_generator()->CreateItem(PrefetchItemState::NEW_REQUEST);
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item1));
  PrefetchItem item2 =
      item_generator()->CreateItem(PrefetchItemState::NEW_REQUEST);
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item2));
  EXPECT_NE(item1.offline_id, item2.offline_id);

  // This item should be unaffected by the task.
  PrefetchItem item3 =
      item_generator()->CreateItem(PrefetchItemState::FINISHED);
  EXPECT_TRUE(store_util()->InsertPrefetchItem(item3));

  EXPECT_EQ(3, store_util()->CountPrefetchItems());

  GeneratePageBundleTask task(store(), gcm_handler(),
                              prefetch_request_factory(),
                              request_callback.Get());
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();

  auto requested_urls = prefetch_request_factory()->GetAllUrlsRequested();
  EXPECT_THAT(*requested_urls, Contains(item1.url.spec()));
  EXPECT_THAT(*requested_urls, Contains(item2.url.spec()));
  EXPECT_THAT(*requested_urls, Not(Contains(item3.url.spec())));

  std::string upload_data =
      url_fetcher_factory()->GetFetcherByID(0)->upload_data();
  EXPECT_THAT(upload_data, HasSubstr(MockPrefetchItemGenerator::kUrlPrefix));

  EXPECT_EQ(3, store_util()->CountPrefetchItems());

  ASSERT_TRUE(store_util()->GetPrefetchItem(item1.offline_id));
  ASSERT_TRUE(store_util()->GetPrefetchItem(item2.offline_id));

  EXPECT_EQ(store_util()->GetPrefetchItem(item1.offline_id)->state,
            PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  EXPECT_EQ(store_util()->GetPrefetchItem(item2.offline_id)->state,
            PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
}

}  // namespace offline_pages
