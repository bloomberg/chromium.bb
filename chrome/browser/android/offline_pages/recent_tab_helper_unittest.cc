// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/recent_tab_helper.h"

#include "base/run_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/test_offline_page_model_builder.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/offline_page_test_archiver.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class TestArchiveFactoryImpl : public RecentTabHelper::TestArchiveFactory {
 public:
  explicit TestArchiveFactoryImpl(OfflinePageTestArchiver::Observer* observer);
  ~TestArchiveFactoryImpl() override {}

  std::unique_ptr<OfflinePageArchiver> CreateArchiver(
        content::WebContents* web_contents) override;
 private:
  OfflinePageTestArchiver::Observer* observer_;  // observer owns this.
};

class RecentTabHelperTest
    : public ChromeRenderViewHostTestHarness,
      public OfflinePageModel::Observer,
      public OfflinePageTestArchiver::Observer {
 public:
  RecentTabHelperTest();
  ~RecentTabHelperTest() override {}

  void SetUp() override;
  void SetUpTestArchiver(const GURL& url);
  void SetUpMockTaskRunner();
  const std::vector<OfflinePageItem>& GetAllPages();

  // Runs default thread.
  void RunUntilIdle();
  // Moves forward the snapshot controller's task runner.
  void FastForwardSnapshotController();

  RecentTabHelper* recent_tab_helper() const { return recent_tab_helper_; }
  OfflinePageModel* model() const { return model_; }
  const std::vector<OfflinePageItem>& all_pages() { return all_pages_; }
  size_t model_changed_count() { return model_changed_count_; }
  size_t model_removed_count() { return model_removed_count_; }

  // OfflinePageModel::Observer
  void OfflinePageModelLoaded(OfflinePageModel* model) override { }
  void OfflinePageModelChanged(OfflinePageModel* model) override {
    model_changed_count_++;
  }
  void OfflinePageDeleted(int64_t, const offline_pages::ClientId&) override {
    model_removed_count_++;
  }

  // OfflinePageTestArchiver::Observer
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override {}

 private:
  void OnGetAllPagesDone(const std::vector<OfflinePageItem>& result);

  RecentTabHelper* recent_tab_helper_;  // Owned by WebContents.
  OfflinePageModel* model_;  // Keyed service
  size_t model_changed_count_;
  size_t model_removed_count_;
  std::vector<OfflinePageItem> all_pages_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  base::WeakPtrFactory<RecentTabHelperTest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RecentTabHelperTest);
};

TestArchiveFactoryImpl::TestArchiveFactoryImpl(
    OfflinePageTestArchiver::Observer* observer)
    : observer_(observer) {
}

std::unique_ptr<OfflinePageArchiver> TestArchiveFactoryImpl::CreateArchiver(
    content::WebContents* web_contents) {
  const size_t kArchiveSizeToReport = 1234;
  std::unique_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
    observer_,
    web_contents->GetLastCommittedURL(),
    offline_pages::OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
    kArchiveSizeToReport,
    base::ThreadTaskRunnerHandle::Get()));
  return std::move(archiver);
}

RecentTabHelperTest::RecentTabHelperTest()
    : recent_tab_helper_(nullptr),
      model_(nullptr),
      model_changed_count_(0),
      model_removed_count_(0),
      task_runner_(new base::TestMockTimeTaskRunner),
      weak_ptr_factory_(this) {
}

void RecentTabHelperTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();
  // Sets up the factory for testing.
  OfflinePageModelFactory::GetInstance()->SetTestingFactoryAndUse(
      browser_context(), BuildTestOfflinePageModel);
  RunUntilIdle();

  RecentTabHelper::CreateForWebContents(web_contents());
  recent_tab_helper_ =
      RecentTabHelper::FromWebContents(web_contents());
  recent_tab_helper_->SetTaskRunnerForTest(task_runner_);

  std::unique_ptr<RecentTabHelper::TestArchiveFactory> test_archive_factory(
      new TestArchiveFactoryImpl(this));
  recent_tab_helper_->SetArchiveFactoryForTest(std::move(test_archive_factory));


  model_ = OfflinePageModelFactory::GetForBrowserContext(browser_context());
  model_->AddObserver(this);
}

const std::vector<OfflinePageItem>& RecentTabHelperTest::GetAllPages() {
  model()->GetAllPages(
      base::Bind(&RecentTabHelperTest::OnGetAllPagesDone,
                 weak_ptr_factory_.GetWeakPtr()));
  RunUntilIdle();
  return all_pages_;
}

void RecentTabHelperTest::OnGetAllPagesDone(
    const std::vector<OfflinePageItem>& result) {
  all_pages_ = result;
}

void RecentTabHelperTest::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void RecentTabHelperTest::FastForwardSnapshotController() {
  const size_t kLongDelayMs = 100*1000;
  task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(kLongDelayMs));
}

TEST_F(RecentTabHelperTest, Basic) {
  EXPECT_NE(nullptr, recent_tab_helper());
}

TEST_F(RecentTabHelperTest, SimpleCapture) {
  const GURL url("http://mystery.site/foo.html");
  NavigateAndCommit(url);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  RunUntilIdle();
  EXPECT_TRUE(model()->is_loaded());
  GetAllPages();
  EXPECT_EQ(1U, all_pages().size());
  EXPECT_EQ(url, all_pages()[0].url);
}

TEST_F(RecentTabHelperTest, TwoCaptures) {
  const GURL url("http://mystery.site/foo.html");
  NavigateAndCommit(url);
  // Triggers snapshot after a time delay.
  recent_tab_helper()->DocumentAvailableInMainFrame();
  RunUntilIdle();
  EXPECT_TRUE(model()->is_loaded());
  EXPECT_EQ(0U, model_changed_count());
  // Move the snapshot controller's time forward so it gets past timeouts.
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_EQ(1U, model_changed_count());
  EXPECT_EQ(0U, model_removed_count());
  GetAllPages();
  EXPECT_EQ(1U, all_pages().size());
  EXPECT_EQ(url, all_pages()[0].url);

  // Triggers snapshot immediately;
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  RunUntilIdle();
  EXPECT_EQ(2U, model_changed_count());
  EXPECT_EQ(1U, model_removed_count());
  // the same page should be simply overridden.
  GetAllPages();
  EXPECT_EQ(1U, all_pages().size());
  EXPECT_EQ(url, all_pages()[0].url);
}

}  // namespace offline_pages
