// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/get_operation_task.h"

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
class GetOperationTaskTest : public TaskTestBase {
 public:
  GetOperationTaskTest() = default;
  ~GetOperationTaskTest() override = default;

  TestPrefetchGCMHandler* gcm_handler() { return &gcm_handler_; }

 private:
  TestPrefetchGCMHandler gcm_handler_;
};

TEST_F(GetOperationTaskTest, NormalOperationTask) {
  base::MockCallback<PrefetchRequestFinishedCallback> callback;
  std::string operation_name = "anoperation";
  GetOperationTask task(operation_name, prefetch_request_factory(),
                        callback.Get());
  ExpectTaskCompletes(&task);

  task.Run();
  task_runner->RunUntilIdle();

  EXPECT_NE(nullptr, prefetch_request_factory()->FindGetOperationRequestByName(
                         operation_name));
  std::string path =
      url_fetcher_factory()->GetFetcherByID(0)->GetOriginalURL().path();
  EXPECT_THAT(path, HasSubstr(operation_name));
}

}  // namespace offline_pages
