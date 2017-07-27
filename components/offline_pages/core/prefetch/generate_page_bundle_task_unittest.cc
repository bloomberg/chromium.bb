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

using testing::_;
using testing::HasSubstr;
using testing::DoAll;
using testing::SaveArg;

namespace offline_pages {
const int kStoreFailure = PrefetchStoreTestUtil::kStoreCommandFailed;

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

  EXPECT_EQ(nullptr,
            prefetch_request_factory()->CurrentGeneratePageBundleRequest());
}

TEST_F(GeneratePageBundleTaskTest, TaskMakesNetworkRequest) {
  base::MockCallback<PrefetchRequestFinishedCallback> request_callback;

  // TODO(dimich): Replace this with GeneratePrefetchItem once it lands.
  PrefetchItem item;
  item.state = PrefetchItemState::NEW_REQUEST;
  item.offline_id = PrefetchStoreUtils::GenerateOfflineId();
  item.url = GURL("http://www.example.com/");
  int64_t id1 = store_util()->InsertPrefetchItem(item);
  item.offline_id = PrefetchStoreUtils::GenerateOfflineId();
  int64_t id2 = store_util()->InsertPrefetchItem(item);

  EXPECT_NE(kStoreFailure, id1);
  EXPECT_NE(kStoreFailure, id2);
  EXPECT_NE(id1, id2);
  EXPECT_EQ(2, store_util()->CountPrefetchItems());

  GeneratePageBundleTask task(store(), gcm_handler(),
                              prefetch_request_factory(),
                              request_callback.Get());
  ExpectTaskCompletes(&task);

  task.Run();
  RunUntilIdle();

  EXPECT_NE(nullptr,
            prefetch_request_factory()->CurrentGeneratePageBundleRequest());
  std::string upload_data =
      url_fetcher_factory()->GetFetcherByID(0)->upload_data();
  EXPECT_THAT(upload_data, HasSubstr("example.com"));

  EXPECT_EQ(2, store_util()->CountPrefetchItems());

  EXPECT_TRUE(store_util()->GetPrefetchItem(id1));
  EXPECT_TRUE(store_util()->GetPrefetchItem(id2));

  EXPECT_EQ(store_util()->GetPrefetchItem(id1)->state,
            PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  EXPECT_EQ(store_util()->GetPrefetchItem(id2)->state,
            PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
}

}  // namespace offline_pages
