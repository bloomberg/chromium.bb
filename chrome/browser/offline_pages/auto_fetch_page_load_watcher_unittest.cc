// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/auto_fetch_page_load_watcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/bind_test_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_pages/core/background/request_coordinator_stub_taco.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
using content::NavigationSimulator;

GURL TestURL() {
  return GURL("http://www.url.com");
}

SavePageRequest TestRequest(int64_t id, const GURL& url = TestURL()) {
  return SavePageRequest(id, url,
                         ClientId(kAutoAsyncNamespace, std::to_string(id)),
                         base::Time::Now(), false);
}

// For access to protected methods.
class TestAutoFetchPageLoadWatcher : public AutoFetchPageLoadWatcher {
 public:
  explicit TestAutoFetchPageLoadWatcher(RequestCoordinator* request_coordinator)
      : AutoFetchPageLoadWatcher(request_coordinator) {}
  void RemoveRequests(const std::vector<int64_t>& request_ids) override {
    removed_ids_ = request_ids;
  }
  std::vector<int64_t> removed_ids() const { return removed_ids_; }

  std::map<GURL, std::vector<int64_t>>* live_auto_fetch_requests() {
    return &live_auto_fetch_requests_;
  }

  using AutoFetchPageLoadWatcher::HandlePageNavigation;
  using AutoFetchPageLoadWatcher::ObserverInitialize;

 private:
  std::vector<int64_t> removed_ids_;
};

// Tests AutoFetchPageLoadWatcher in a realistic way by simulating navigations.
class AutoFetchPageLoadWatcherNavigationTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutoFetchPageLoadWatcherNavigationTest() = default;
  ~AutoFetchPageLoadWatcherNavigationTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    RequestCoordinatorFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), request_coordinator_taco_.FactoryFunction());

    AutoFetchPageLoadWatcher::CreateForWebContents(web_contents());
  }

  std::unique_ptr<NavigationSimulator> CreateNavigation(const GURL& url) {
    return NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  }

  RequestCoordinator* request_coordinator() {
    return request_coordinator_taco_.request_coordinator();
  }

  void AddAutoSavePageRequest() {
    RequestCoordinator::SavePageLaterParams params;
    params.url = TestURL();
    params.client_id = ClientId(kAutoAsyncNamespace, "request1");
    request_coordinator()->SavePageLater(params, base::DoNothing());
  }

  std::vector<GURL> RequestsInQueue() {
    std::vector<GURL> request_urls;
    request_coordinator()->GetAllRequests(base::BindLambdaForTesting(
        [&](std::vector<std::unique_ptr<SavePageRequest>> requests) {
          for (const auto& request : requests) {
            request_urls.push_back(request->url());
          }
        }));
    RunUntilIdle();
    return request_urls;
  }

  void RunUntilIdle() { thread_bundle()->RunUntilIdle(); }

 private:
  RequestCoordinatorStubTaco request_coordinator_taco_;

  DISALLOW_COPY_AND_ASSIGN(AutoFetchPageLoadWatcherNavigationTest);
};

// Navigation results in an error page, and has no effect on
// AutoFetchPageLoadWatcher.
TEST_F(AutoFetchPageLoadWatcherNavigationTest, NavigateToErrorPage) {
  AddAutoSavePageRequest();
  RunUntilIdle();

  std::unique_ptr<NavigationSimulator> simulator = CreateNavigation(TestURL());
  simulator->Start();
  simulator->Fail(net::ERR_TIMED_OUT);
  simulator->CommitErrorPage();

  std::vector<GURL> expected_requests{TestURL()};
  EXPECT_EQ(expected_requests, RequestsInQueue());
}

// Successful navigation results in cancellation of request.
TEST_F(AutoFetchPageLoadWatcherNavigationTest, NavigateAndCancel) {
  AddAutoSavePageRequest();
  RunUntilIdle();

  std::unique_ptr<NavigationSimulator> simulator = CreateNavigation(TestURL());
  simulator->Start();
  simulator->Commit();

  std::vector<GURL> expected_requests{};
  EXPECT_EQ(expected_requests, RequestsInQueue());
}

TEST_F(AutoFetchPageLoadWatcherNavigationTest, NavigateToDifferentURL) {
  AddAutoSavePageRequest();
  RunUntilIdle();

  std::unique_ptr<NavigationSimulator> simulator =
      CreateNavigation(GURL("http://www.different.com"));
  simulator->Start();
  simulator->Commit();

  std::vector<GURL> expected_requests{TestURL()};
  EXPECT_EQ(expected_requests, RequestsInQueue());
}

TEST_F(AutoFetchPageLoadWatcherNavigationTest, RedirectToAndCancel) {
  AddAutoSavePageRequest();
  RunUntilIdle();

  std::unique_ptr<NavigationSimulator> simulator =
      CreateNavigation(GURL("http://different.com"));
  simulator->Start();
  simulator->Redirect(TestURL());
  simulator->Commit();

  std::vector<GURL> expected_requests{};
  EXPECT_EQ(expected_requests, RequestsInQueue());
}

TEST_F(AutoFetchPageLoadWatcherNavigationTest, RedirectFromAndCancel) {
  AddAutoSavePageRequest();
  RunUntilIdle();

  std::unique_ptr<NavigationSimulator> simulator = CreateNavigation(TestURL());

  simulator->Start();
  simulator->Redirect(GURL("http://different.com"));
  simulator->Commit();

  std::vector<GURL> expected_requests{};
  EXPECT_EQ(expected_requests, RequestsInQueue());
}

// Tests some details of AutoFetchPageLoadWatcher
class AutoFetchPageLoadWatcherTest : public testing::Test {
 public:
  AutoFetchPageLoadWatcherTest() {}
  ~AutoFetchPageLoadWatcherTest() override {}

  void SetUp() override {
    testing::Test::SetUp();

    taco_.CreateRequestCoordinator();
  }

  RequestCoordinator* request_coordinator() {
    return taco_.request_coordinator();
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_ =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();
  base::ThreadTaskRunnerHandle handle_{task_runner_};

  RequestCoordinatorStubTaco taco_;
};

// Simulate navigation to a URL prior to ObserverInitialize. Verify the
// RemoveRequests is called.
TEST_F(AutoFetchPageLoadWatcherTest, NavigateBeforeObserverInitialize) {
  TestAutoFetchPageLoadWatcher tab_helper(request_coordinator());
  tab_helper.HandlePageNavigation(TestURL());
  std::vector<std::unique_ptr<SavePageRequest>> all_requests;
  all_requests.push_back(std::make_unique<SavePageRequest>(TestRequest(1)));
  all_requests.push_back(std::make_unique<SavePageRequest>(
      TestRequest(2, GURL("http://different.com"))));
  tab_helper.ObserverInitialize(std::move(all_requests));

  std::vector<int64_t> expected_requests{1};
  EXPECT_EQ(expected_requests, tab_helper.removed_ids());
}

TEST_F(AutoFetchPageLoadWatcherTest, OnCompletedNoRequest) {
  TestAutoFetchPageLoadWatcher tab_helper(request_coordinator());
  tab_helper.ObserverInitialize({});

  tab_helper.OnCompleted(TestRequest(1),
                         RequestNotifier::BackgroundSavePageResult());

  // Nothing happens, just verify there is no crash.
  SUCCEED();
}

TEST_F(AutoFetchPageLoadWatcherTest, OnCompletedOneRequestWithURL) {
  TestAutoFetchPageLoadWatcher tab_helper(request_coordinator());
  tab_helper.ObserverInitialize({});
  std::map<GURL, std::vector<int64_t>>* requests =
      tab_helper.live_auto_fetch_requests();

  tab_helper.OnAdded(TestRequest(1));
  ASSERT_EQ(1ul, requests->count(TestURL()));

  tab_helper.OnCompleted(TestRequest(1),
                         RequestNotifier::BackgroundSavePageResult());

  EXPECT_EQ(0ul, requests->count(TestURL()));
}

// Verify multiple requests with the same URL are handled as expected.
TEST_F(AutoFetchPageLoadWatcherTest, OnCompletedMultipleRequestsWithURL) {
  TestAutoFetchPageLoadWatcher tab_helper(request_coordinator());
  tab_helper.ObserverInitialize({});

  // Three requests with the same URL.
  tab_helper.OnAdded(TestRequest(1));
  tab_helper.OnAdded(TestRequest(2));
  tab_helper.OnAdded(TestRequest(3));

  // Only one is completed.
  tab_helper.OnCompleted(TestRequest(2),
                         RequestNotifier::BackgroundSavePageResult());

  std::vector<int64_t> expected_requests = {1, 3};
  EXPECT_EQ(expected_requests,
            (*tab_helper.live_auto_fetch_requests())[TestURL()]);
}

}  // namespace
}  // namespace offline_pages
