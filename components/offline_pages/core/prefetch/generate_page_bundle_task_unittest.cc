// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/generate_page_bundle_task.h"

#include "base/test/mock_callback.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
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
  GeneratePageBundleTask task(std::vector<PrefetchURL>(), gcm_handler(),
                              prefetch_request_factory(), callback.Get());
  ExpectTaskCompletes(&task);

  task.Run();
  task_runner->RunUntilIdle();

  EXPECT_EQ(nullptr,
            prefetch_request_factory()->CurrentGeneratePageBundleRequest());
}

TEST_F(GeneratePageBundleTaskTest, TaskMakesNetworkRequest) {
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  std::vector<PrefetchURL> urls = {
      {"id1", GURL("https://example.com/1.html")},
      {"id2", GURL("https://example.com/2.html")},
  };
  GeneratePageBundleTask task(urls, gcm_handler(), prefetch_request_factory(),
                              callback.Get());
  ExpectTaskCompletes(&task);

  task.Run();
  task_runner->RunUntilIdle();

  EXPECT_NE(nullptr,
            prefetch_request_factory()->CurrentGeneratePageBundleRequest());
  std::string upload_data =
      url_fetcher_factory()->GetFetcherByID(0)->upload_data();
  EXPECT_THAT(upload_data, HasSubstr("example.com"));
}

}  // namespace offline_pages
