// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include "base/files/file_path.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_job_info.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kOrigin[] = "https://example.com/";
const char kResource[] = "https://example.com/resource.html";
const char kTag[] = "TestRequestTag";
constexpr int64_t kServiceWorkerRegistrationId = 9001;

}  // namespace

namespace content {

class BackgroundFetchDataManagerTest : public testing::Test {
 protected:
  BackgroundFetchDataManagerTest()
      : helper_(base::FilePath()),
        background_fetch_context_(new BackgroundFetchContext(
            &browser_context_,
            BrowserContext::GetDefaultStoragePartition(&browser_context_),
            helper_.context_wrapper())) {}

  BackgroundFetchDataManager* GetDataManager() const {
    return background_fetch_context_->GetDataManagerForTesting();
  }

 private:
  TestBrowserThreadBundle browser_thread_bundle_;
  TestBrowserContext browser_context_;
  EmbeddedWorkerTestHelper helper_;

  scoped_refptr<BackgroundFetchContext> background_fetch_context_;
};

TEST_F(BackgroundFetchDataManagerTest, AddRequest) {
  // Create a BackgroundFetchJobInfo and a set of BackgroundFetchRequestInfos.
  std::vector<BackgroundFetchRequestInfo> request_infos;
  std::vector<std::string> request_guids;
  for (int i = 0; i < 10; i++) {
    BackgroundFetchRequestInfo request_info(GURL(kResource), kTag);
    request_guids.push_back(request_info.guid());
    request_infos.push_back(std::move(request_info));
  }
  BackgroundFetchJobInfo job_info(kTag, url::Origin(GURL(kOrigin)),
                                  kServiceWorkerRegistrationId);

  // Initialize a BackgroundFetchJobData with the constructed requests.
  BackgroundFetchDataManager* data_manager = GetDataManager();
  BackgroundFetchJobData* job_data =
      data_manager->CreateRequest(job_info, request_infos);

  // Get all of the fetch requests from the BackgroundFetchJobData.
  for (int i = 0; i < 10; i++) {
    EXPECT_FALSE(job_data->IsComplete());
    ASSERT_TRUE(job_data->HasRequestsRemaining());
    const BackgroundFetchRequestInfo& request_info =
        job_data->GetNextBackgroundFetchRequestInfo();
    EXPECT_EQ(request_info.tag(), kTag);
  }

  // At this point, all the fetches have been started, but none finished.
  EXPECT_FALSE(job_data->HasRequestsRemaining());
  EXPECT_FALSE(job_data->IsComplete());

  // Complete all buy one of the fetch requests.
  for (int i = 0; i < 9; i++) {
    EXPECT_FALSE(
        job_data->BackgroundFetchRequestInfoComplete(request_guids[i]));
    EXPECT_FALSE(job_data->IsComplete());
  }

  // Complete the final fetch request.
  EXPECT_FALSE(job_data->BackgroundFetchRequestInfoComplete(request_guids[9]));
  EXPECT_TRUE(job_data->IsComplete());
}

}  // namespace content
