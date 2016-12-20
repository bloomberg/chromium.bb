// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/downloads/resource_throttle.h"

#include "base/run_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/offline_page_utils.h"
#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/android/offline_pages/test_offline_page_model_builder.h"
#include "chrome/browser/android/offline_pages/test_request_coordinator_builder.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_test_archiver.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const GURL kTestPageUrl("http://mystery.site/foo.html");
const char kClientNamespace[] = "download";

}  // namespace

namespace offline_pages {
namespace downloads {

class ResourceThrottleTest : public ChromeRenderViewHostTestHarness {
 public:
  ResourceThrottleTest();
  ~ResourceThrottleTest() override;

  void SetUp() override;

  // Runs default thread.
  void RunUntilIdle();

  const std::vector<std::unique_ptr<SavePageRequest>>& last_requests() const {
    return last_requests_;
  }

  // Callback for getting requests.
  void GetRequestsDone(GetRequestsResult result,
                       std::vector<std::unique_ptr<SavePageRequest>> requests);

 private:
  GetRequestsResult last_get_requests_result_;
  std::vector<std::unique_ptr<SavePageRequest>> last_requests_;
};

ResourceThrottleTest::ResourceThrottleTest() {}

ResourceThrottleTest::~ResourceThrottleTest() {}

void ResourceThrottleTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  OfflinePageModelFactory::GetInstance()->SetTestingFactoryAndUse(
      browser_context(), BuildTestOfflinePageModel);
  RunUntilIdle();
  RequestCoordinatorFactory::GetInstance()->SetTestingFactoryAndUse(
      browser_context(), BuildTestRequestCoordinator);
  RunUntilIdle();
}

void ResourceThrottleTest::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void ResourceThrottleTest::GetRequestsDone(
    GetRequestsResult result,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  last_get_requests_result_ = result;
  last_requests_ = std::move(requests);
}

TEST_F(ResourceThrottleTest, StartOfflinePageDownload) {
  OfflinePageUtils::StartOfflinePageDownload(browser_context(), kTestPageUrl);
  RequestCoordinator* request_coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(browser_context());

  request_coordinator->queue()->GetRequests(base::Bind(
      &ResourceThrottleTest::GetRequestsDone, base::Unretained(this)));

  // Wait for callbacks to finish, both request queue and offliner.
  RunUntilIdle();

  // Check the request queue is as expected.
  EXPECT_EQ(1UL, last_requests().size());
  EXPECT_EQ(kTestPageUrl, last_requests().at(0)->url());
  EXPECT_EQ(kClientNamespace, last_requests().at(0)->client_id().name_space);
}

}  // namespace downloads
}  // namespace offline_pages
