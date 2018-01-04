// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signin_partition_manager.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {
namespace login {

namespace {
constexpr char kEmbedderUrl[] = "http://www.whatever.com/";
}  // namespace

class SigninPartitionManagerTest : public ChromeRenderViewHostTestHarness {
 protected:
  SigninPartitionManagerTest() {}
  ~SigninPartitionManagerTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    signin_browser_context_ = base::MakeUnique<TestingProfile>();

    signin_ui_web_contents_ = base::WrapUnique<content::WebContents>(
        content::WebContentsTester::CreateTestWebContents(
            GetSigninProfile(),
            content::SiteInstance::Create(GetSigninProfile())));

    GURL url(kEmbedderUrl);
    content::WebContentsTester::For(signin_ui_web_contents())
        ->NavigateAndCommit(url);

    GetSigninPartitionManager()->SetClearStoragePartitionTaskForTesting(
        base::Bind(&SigninPartitionManagerTest::ClearStoragePartitionTask,
                   base::Unretained(this)));
  }

  void TearDown() override {
    signin_ui_web_contents_.reset();

    signin_browser_context_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  Profile* GetSigninProfile() {
    return signin_browser_context_->GetOffTheRecordProfile();
  }

  SigninPartitionManager* GetSigninPartitionManager() {
    return SigninPartitionManager::Factory::GetForBrowserContext(
        GetSigninProfile());
  }

  content::WebContents* signin_ui_web_contents() {
    return signin_ui_web_contents_.get();
  }

  void ExpectOneClearPartitionTask(
      content::StoragePartition* storage_partition) {
    EXPECT_EQ(1u, pending_clear_tasks_.size());
    if (pending_clear_tasks_.size() > 0) {
      EXPECT_EQ(storage_partition, pending_clear_tasks_[0].first);
    }
  }

  void FinishAllClearPartitionTasks() {
    for (auto& task : pending_clear_tasks_) {
      std::move(task.second).Run();
    }
    pending_clear_tasks_.clear();
  }

 private:
  void ClearStoragePartitionTask(content::StoragePartition* partition,
                                 base::OnceClosure clear_done_closure) {
    pending_clear_tasks_.push_back({partition, std::move(clear_done_closure)});
  }

  std::unique_ptr<TestingProfile> signin_browser_context_;

  // Web contents of the sign-in UI, embedder of the signin-frame webview.
  std::unique_ptr<content::WebContents> signin_ui_web_contents_;

  std::vector<std::pair<content::StoragePartition*, base::OnceClosure>>
      pending_clear_tasks_;

  DISALLOW_COPY_AND_ASSIGN(SigninPartitionManagerTest);
};

TEST_F(SigninPartitionManagerTest, TestSubsequentAttempts) {
  // First sign-in attempt
  GetSigninPartitionManager()->StartSigninSession(signin_ui_web_contents());
  std::string signin_partition_name_1 =
      GetSigninPartitionManager()->GetCurrentStoragePartitionName();
  auto* signin_partition_1 =
      GetSigninPartitionManager()->GetCurrentStoragePartition();
  EXPECT_FALSE(signin_partition_name_1.empty());

  // Second sign-in attempt
  GetSigninPartitionManager()->StartSigninSession(signin_ui_web_contents());
  std::string signin_partition_name_2 =
      GetSigninPartitionManager()->GetCurrentStoragePartitionName();
  auto* signin_partition_2 =
      GetSigninPartitionManager()->GetCurrentStoragePartition();
  EXPECT_FALSE(signin_partition_name_2.empty());

  // Make sure that the StoragePartition has not been re-used.
  EXPECT_NE(signin_partition_name_1, signin_partition_name_2);
  EXPECT_NE(signin_partition_1, signin_partition_2);

  // Make sure that the first StoragePartition has been cleared automatically.
  ExpectOneClearPartitionTask(signin_partition_1);
  FinishAllClearPartitionTasks();

  // Make sure that the second StoragePartition will be cleared when we
  // explicitly close the current sign-in session.
  bool closure_called = false;
  base::RepeatingClosure partition_cleared_closure = base::BindRepeating(
      [](bool* closure_called_ptr) { *closure_called_ptr = true; },
      &closure_called);

  GetSigninPartitionManager()->CloseCurrentSigninSession(
      partition_cleared_closure);
  EXPECT_FALSE(closure_called);

  ExpectOneClearPartitionTask(signin_partition_2);
  FinishAllClearPartitionTasks();

  EXPECT_TRUE(closure_called);
}

}  // namespace login
}  // namespace chromeos
