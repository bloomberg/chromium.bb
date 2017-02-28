// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/recent_tabs/recent_tabs_ui_adapter_delegate.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/offline_pages/core/background/request_coordinator_stub_taco.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/downloads/download_ui_adapter.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
static const char kTestGuid1[] = "1";
static const char kTestGuid2[] = "2";
static const char kTestBadGuid[] = "ccccccc-cccc-0ccc-0ccc-ccccccccccc0";
static const ClientId kTestClientIdOtherNamespace(kAsyncNamespace, kTestGuid1);
static const ClientId kTestClientIdOtherGuid(kLastNNamespace, kTestBadGuid);
static const ClientId kTestClientId1(kLastNNamespace, kTestGuid1);
static const ClientId kTestClientId2(kLastNNamespace, kTestGuid2);
}  // namespace

// Creates mock versions for OfflinePageModel, RequestCoordinator and their
// dependencies, then passes them to DownloadUIAdapter for testing.
// Note that initially the OfflienPageModel is not "loaded". PumpLoop() will
// load it, firing ItemsLoaded callback to the Adapter. Hence some tests
// start from PumpLoop() right away if they don't need to test this.
class RecentTabsUIAdapterDelegateTest : public testing::Test {
 public:
  RecentTabsUIAdapterDelegateTest();
  ~RecentTabsUIAdapterDelegateTest() override;

  // Runs until all of the tasks that are not delayed are gone from the task
  // queue.
  void PumpLoop();

  RequestCoordinator* request_coordinator() {
    return request_coordinator_taco_->request_coordinator();
  }

  StubOfflinePageModel model;
  RecentTabsUIAdapterDelegate* adapter_delegate;
  std::unique_ptr<DownloadUIAdapter> adapter;

 private:
  std::unique_ptr<RequestCoordinatorStubTaco> request_coordinator_taco_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

RecentTabsUIAdapterDelegateTest::RecentTabsUIAdapterDelegateTest()
    : task_runner_(new base::TestMockTimeTaskRunner()),
      task_runner_handle_(task_runner_) {
  request_coordinator_taco_ = base::MakeUnique<RequestCoordinatorStubTaco>();
  request_coordinator_taco_->CreateRequestCoordinator();

  auto delegate = base::MakeUnique<RecentTabsUIAdapterDelegate>(&model);
  adapter_delegate = delegate.get();

  adapter = base::MakeUnique<DownloadUIAdapter>(
      &model, request_coordinator_taco_->request_coordinator(),
      std::move(delegate));
}

RecentTabsUIAdapterDelegateTest::~RecentTabsUIAdapterDelegateTest() = default;

void RecentTabsUIAdapterDelegateTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

TEST_F(RecentTabsUIAdapterDelegateTest, TabIdFromClientId) {
  EXPECT_EQ(1, RecentTabsUIAdapterDelegate::TabIdFromClientId(kTestClientId1));
  EXPECT_EQ(2, RecentTabsUIAdapterDelegate::TabIdFromClientId(kTestClientId2));
}

TEST_F(RecentTabsUIAdapterDelegateTest, IsVisibleInUI) {
  EXPECT_FALSE(adapter_delegate->IsVisibleInUI(kTestClientIdOtherNamespace));
  EXPECT_TRUE(adapter_delegate->IsVisibleInUI(kTestClientId1));
  EXPECT_TRUE(adapter_delegate->IsVisibleInUI(kTestClientId2));
}

TEST_F(RecentTabsUIAdapterDelegateTest, IsTemporarilyHiddenInUI) {
  adapter_delegate->RegisterTab(3);
  adapter_delegate->RegisterTab(4);

  EXPECT_TRUE(adapter_delegate->IsTemporarilyHiddenInUI(kTestClientId1));
  EXPECT_TRUE(adapter_delegate->IsTemporarilyHiddenInUI(kTestClientId2));

  adapter_delegate->RegisterTab(1);
  EXPECT_FALSE(adapter_delegate->IsTemporarilyHiddenInUI(kTestClientId1));
  EXPECT_TRUE(adapter_delegate->IsTemporarilyHiddenInUI(kTestClientId2));

  adapter_delegate->RegisterTab(2);
  EXPECT_FALSE(adapter_delegate->IsTemporarilyHiddenInUI(kTestClientId1));
  EXPECT_FALSE(adapter_delegate->IsTemporarilyHiddenInUI(kTestClientId2));

  adapter_delegate->UnregisterTab(2);
  EXPECT_FALSE(adapter_delegate->IsTemporarilyHiddenInUI(kTestClientId1));
  EXPECT_TRUE(adapter_delegate->IsTemporarilyHiddenInUI(kTestClientId2));
}

}  // namespace offline_pages
