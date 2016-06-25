// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/recent_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
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

namespace {
const GURL kTestPageUrl("http://mystery.site/foo.html");
const GURL kTestPageUrlOther("http://crazy.site/foo_other.html");
const int kTabId = 153;
}

class TestDelegate: public RecentTabHelper::Delegate {
 public:
  explicit TestDelegate(
      OfflinePageTestArchiver::Observer* observer,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      int tab_id,
      bool tab_id_result);
  ~TestDelegate() override {}

  std::unique_ptr<OfflinePageArchiver> CreatePageArchiver(
        content::WebContents* web_contents) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override;
    // There is no expectations that tab_id is always present.
  bool GetTabId(content::WebContents* web_contents, int* tab_id) override;

 private:
  OfflinePageTestArchiver::Observer* observer_;  // observer owns this.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  int tab_id_;
  bool tab_id_result_;
};

class RecentTabHelperTest
    : public ChromeRenderViewHostTestHarness,
      public OfflinePageModel::Observer,
      public OfflinePageTestArchiver::Observer {
 public:
  RecentTabHelperTest();
  ~RecentTabHelperTest() override {}

  void SetUp() override;
  const std::vector<OfflinePageItem>& GetAllPages();

  void FailLoad(const GURL& url);

  // Runs default thread.
  void RunUntilIdle();
  // Moves forward the snapshot controller's task runner.
  void FastForwardSnapshotController();

  RecentTabHelper* recent_tab_helper() const { return recent_tab_helper_; }

  OfflinePageModel* model() const { return model_; }

  const std::vector<OfflinePageItem>& all_pages() { return all_pages_; }

  scoped_refptr<base::TestMockTimeTaskRunner>& task_runner() {
    return task_runner_;
  }

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

TestDelegate::TestDelegate(
    OfflinePageTestArchiver::Observer* observer,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    int tab_id,
    bool tab_id_result)
    : observer_(observer),
      task_runner_(task_runner),
      tab_id_(tab_id),
      tab_id_result_(tab_id_result) {
}

std::unique_ptr<OfflinePageArchiver> TestDelegate::CreatePageArchiver(
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

scoped_refptr<base::SingleThreadTaskRunner> TestDelegate::GetTaskRunner() {
  return task_runner_;
}
  // There is no expectations that tab_id is always present.
bool TestDelegate::GetTabId(content::WebContents* web_contents, int* tab_id) {
  *tab_id = tab_id_;
  return tab_id_result_;
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

  recent_tab_helper_->SetDelegate(base::MakeUnique<TestDelegate>(
      this, task_runner(), kTabId, true));

  model_ = OfflinePageModelFactory::GetForBrowserContext(browser_context());
  model_->AddObserver(this);
}

void RecentTabHelperTest::FailLoad(const GURL& url) {
  controller().LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());
  content::RenderFrameHostTester::For(main_rfh())->SimulateNavigationStart(url);
  content::RenderFrameHostTester::For(main_rfh())->
      SimulateNavigationError(url, net::ERR_INTERNET_DISCONNECTED);
  content::RenderFrameHostTester::For(main_rfh())->
      SimulateNavigationErrorPageCommit();
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
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  RunUntilIdle();
  EXPECT_TRUE(model()->is_loaded());
  GetAllPages();
  EXPECT_EQ(1U, all_pages().size());
  EXPECT_EQ(kTestPageUrl, all_pages()[0].url);
}

TEST_F(RecentTabHelperTest, NoTabIdNoCapture) {
  // Create delegate that returns 'false' as TabId retrieval result.
  recent_tab_helper()->SetDelegate(base::MakeUnique<TestDelegate>(
      this, task_runner(), kTabId, false));

  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  RunUntilIdle();
  EXPECT_TRUE(model()->is_loaded());
  GetAllPages();
  // No page shodul be captured.
  EXPECT_EQ(0U, all_pages().size());
}

// Should end up with 1 page.
TEST_F(RecentTabHelperTest, TwoCapturesSameUrl) {
  NavigateAndCommit(kTestPageUrl);
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
  EXPECT_EQ(kTestPageUrl, all_pages()[0].url);

  // Triggers snapshot immediately;
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  RunUntilIdle();
  EXPECT_EQ(2U, model_changed_count());
  EXPECT_EQ(1U, model_removed_count());
  // the same page should be simply overridden.
  GetAllPages();
  EXPECT_EQ(1U, all_pages().size());
  EXPECT_EQ(kTestPageUrl, all_pages()[0].url);
}

// Should end up with 1 page.
TEST_F(RecentTabHelperTest, TwoCapturesDifferentUrls) {
  NavigateAndCommit(kTestPageUrl);
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
  EXPECT_EQ(kTestPageUrl, all_pages()[0].url);

  NavigateAndCommit(kTestPageUrlOther);
  // Triggers snapshot immediately;
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  RunUntilIdle();
  EXPECT_EQ(2U, model_changed_count());
  EXPECT_EQ(1U, model_removed_count());
  // the same page should be simply overridden.
  GetAllPages();
  EXPECT_EQ(1U, all_pages().size());
  EXPECT_EQ(kTestPageUrlOther, all_pages()[0].url);
}

TEST_F(RecentTabHelperTest, NoCaptureOnErrorPage) {
  FailLoad(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  RunUntilIdle();
  EXPECT_TRUE(model()->is_loaded());
  GetAllPages();
  EXPECT_EQ(0U, all_pages().size());
}

}  // namespace offline_pages
