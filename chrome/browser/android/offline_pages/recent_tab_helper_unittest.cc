// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/recent_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/android/offline_pages/test_offline_page_model_builder.h"
#include "chrome/browser/android/offline_pages/test_request_coordinator_builder.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_test_archiver.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const GURL kTestPageUrl("http://mystery.site/foo.html");
const GURL kTestPageUrlOther("http://crazy.site/foo_other.html");
const int kTabId = 153;
}  // namespace

class TestDelegate: public RecentTabHelper::Delegate {
 public:
  const size_t kArchiveSizeToReport = 1234;

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

  void set_archive_result(
      offline_pages::OfflinePageArchiver::ArchiverResult result) {
    archive_result_ = result;
  }

  void set_archive_size(int64_t size) { archive_size_ = size; }

 private:
  OfflinePageTestArchiver::Observer* observer_;  // observer owns this.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  int tab_id_;
  bool tab_id_result_;

  // These values can be updated so that new OfflinePageTestArchiver instances
  // will return different results.
  offline_pages::OfflinePageArchiver::ArchiverResult archive_result_ =
      offline_pages::OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED;
  int64_t archive_size_ = kArchiveSizeToReport;
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

  // Returns a OfflinePageItem pointer from |all_pages| that matches the
  // provided |offline_id|. If a match is not found returns nullptr.
  const OfflinePageItem* FindPageForOfflineId(int64_t offline_id) {
    for (const OfflinePageItem& page : all_pages_) {
      if (page.offline_id == offline_id)
        return &page;
    }
    return nullptr;
  }

  scoped_refptr<base::TestMockTimeTaskRunner>& task_runner() {
    return task_runner_;
  }

  size_t page_added_count() { return page_added_count_; }
  size_t model_removed_count() { return model_removed_count_; }

  // OfflinePageModel::Observer
  void OfflinePageModelLoaded(OfflinePageModel* model) override { }
  void OfflinePageAdded(OfflinePageModel* model,
                        const OfflinePageItem& added_page) override {
    page_added_count_++;
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
  size_t page_added_count_;
  size_t model_removed_count_;
  std::vector<OfflinePageItem> all_pages_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::test::ScopedFeatureList scoped_feature_list_;

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
  std::unique_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
      observer_, web_contents->GetLastCommittedURL(), archive_result_,
      base::string16(), kArchiveSizeToReport,
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
      page_added_count_(0),
      model_removed_count_(0),
      task_runner_(new base::TestMockTimeTaskRunner),
      weak_ptr_factory_(this) {}

void RecentTabHelperTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  scoped_feature_list_.InitAndEnableFeature(kOffliningRecentPagesFeature);
  // Sets up the factories for testing.
  OfflinePageModelFactory::GetInstance()->SetTestingFactoryAndUse(
      browser_context(), BuildTestOfflinePageModel);
  RunUntilIdle();
  RequestCoordinatorFactory::GetInstance()->SetTestingFactoryAndUse(
      browser_context(), BuildTestRequestCoordinator);
  RunUntilIdle();

  RecentTabHelper::CreateForWebContents(web_contents());
  recent_tab_helper_ = RecentTabHelper::FromWebContents(web_contents());

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

// Checks the test setup.
TEST_F(RecentTabHelperTest, Basic) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.Init();
  EXPECT_NE(nullptr, recent_tab_helper());
}

// Loads a page and verifies that a snapshot is created.
TEST_F(RecentTabHelperTest, SimpleCapture) {
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_TRUE(model()->is_loaded());
  GetAllPages();
  ASSERT_EQ(1U, all_pages().size());
  EXPECT_EQ(kTestPageUrl, all_pages()[0].url);
}

// Checks that WebContents with no tab IDs are properly ignored.
TEST_F(RecentTabHelperTest, NoTabIdNoCapture) {
  // Create delegate that returns 'false' as TabId retrieval result.
  recent_tab_helper()->SetDelegate(base::MakeUnique<TestDelegate>(
      this, task_runner(), kTabId, false));

  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_TRUE(model()->is_loaded());
  GetAllPages();
  // No page should be captured.
  ASSERT_EQ(0U, all_pages().size());
}

// Triggers two snapshot captures during a single page load. Should end up with
// one offline page (the 2nd snapshot should be kept).
TEST_F(RecentTabHelperTest, TwoCapturesSamePageLoad) {
  NavigateAndCommit(kTestPageUrl);
  // Triggers snapshot after a time delay.
  recent_tab_helper()->DocumentAvailableInMainFrame();
  RunUntilIdle();
  EXPECT_TRUE(model()->is_loaded());
  EXPECT_EQ(0U, page_added_count());
  // Move the snapshot controller's time forward so it gets past timeouts.
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  GetAllPages();
  ASSERT_EQ(1U, all_pages().size());
  EXPECT_EQ(kTestPageUrl, all_pages()[0].url);
  int64_t first_offline_id = all_pages()[0].offline_id;

  // Triggers snapshot after a time delay.
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  // Move the snapshot controller's time forward so it gets past timeouts.
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_EQ(2U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  // the same page should be simply overridden.
  GetAllPages();
  ASSERT_EQ(1U, all_pages().size());
  EXPECT_EQ(kTestPageUrl, all_pages()[0].url);
  EXPECT_NE(first_offline_id, all_pages()[0].offline_id);
}

// Triggers two snapshot captures during a single page load, where the 2nd one
// fails. Should end up with one offline page (the 1st, successful snapshot
// should be kept).
// TODO(carlosk): re-enable once https://crbug.com/655697 is fixed, again.
TEST_F(RecentTabHelperTest, DISABLED_TwoCapturesSamePageLoadSecondFails) {
  NavigateAndCommit(kTestPageUrl);
  // Triggers snapshot after a time delay.
  recent_tab_helper()->DocumentAvailableInMainFrame();
  RunUntilIdle();
  EXPECT_TRUE(model()->is_loaded());
  EXPECT_EQ(0U, page_added_count());
  // Move the snapshot controller's time forward so it gets past timeouts.
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  GetAllPages();
  ASSERT_EQ(1U, all_pages().size());
  EXPECT_EQ(kTestPageUrl, all_pages()[0].url);
  int64_t first_offline_id = all_pages()[0].offline_id;

  // Sets a new delegate that will make the second snapshot fail.
  TestDelegate* failing_delegate =
      new TestDelegate(this, task_runner(), kTabId, true);
  failing_delegate->set_archive_size(-1);
  failing_delegate->set_archive_result(
      offline_pages::OfflinePageArchiver::ArchiverResult::
          ERROR_ARCHIVE_CREATION_FAILED);
  recent_tab_helper()->SetDelegate(
      std::unique_ptr<TestDelegate>(failing_delegate));

  // Triggers snapshot after a time delay.
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  // Move the snapshot controller's time forward so it gets past timeouts.
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  // The exact same page should still be available.
  GetAllPages();
  ASSERT_EQ(1U, all_pages().size());
  EXPECT_EQ(kTestPageUrl, all_pages()[0].url);
  EXPECT_EQ(first_offline_id, all_pages()[0].offline_id);
}

// Triggers two snapshot captures for two different page loads for the same URL.
// Should end up with one offline page (snapshot from the 2nd load).
TEST_F(RecentTabHelperTest, TwoCapturesDifferentPageLoadsSameUrl) {
  NavigateAndCommit(kTestPageUrl);
  // Triggers snapshot after a time delay.
  recent_tab_helper()->DocumentAvailableInMainFrame();
  RunUntilIdle();
  EXPECT_TRUE(model()->is_loaded());
  EXPECT_EQ(0U, page_added_count());
  // Move the snapshot controller's time forward so it gets past timeouts.
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  GetAllPages();
  ASSERT_EQ(1U, all_pages().size());
  EXPECT_EQ(kTestPageUrl, all_pages()[0].url);
  int64_t first_offline_id = all_pages()[0].offline_id;

  NavigateAndCommit(kTestPageUrl);
  // Triggers snapshot after a time delay.
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  // Move the snapshot controller's time forward so it gets past timeouts.
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_EQ(2U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  // the same page should be simply overridden.
  GetAllPages();
  ASSERT_EQ(1U, all_pages().size());
  EXPECT_EQ(kTestPageUrl, all_pages()[0].url);
  EXPECT_NE(first_offline_id, all_pages()[0].offline_id);
}

// Triggers two snapshot captures for two different page loads for the same URL,
// where the 2nd one fails. Should end up with no offline pages (privacy driven
// decision).
TEST_F(RecentTabHelperTest, TwoCapturesDifferentPageLoadsSameUrlSecondFails) {
  NavigateAndCommit(kTestPageUrl);
  // Triggers snapshot after a time delay.
  recent_tab_helper()->DocumentAvailableInMainFrame();
  RunUntilIdle();
  EXPECT_TRUE(model()->is_loaded());
  EXPECT_EQ(0U, page_added_count());
  // Move the snapshot controller's time forward so it gets past timeouts.
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  GetAllPages();
  ASSERT_EQ(1U, all_pages().size());
  EXPECT_EQ(kTestPageUrl, all_pages()[0].url);

  // Sets a new delegate that will make the second snapshot fail.
  TestDelegate* failing_delegate =
      new TestDelegate(this, task_runner(), kTabId, true);
  failing_delegate->set_archive_size(-1);
  failing_delegate->set_archive_result(
      offline_pages::OfflinePageArchiver::ArchiverResult::
          ERROR_ARCHIVE_CREATION_FAILED);
  recent_tab_helper()->SetDelegate(
      std::unique_ptr<TestDelegate>(failing_delegate));

  NavigateAndCommit(kTestPageUrl);
  // Triggers snapshot after a time delay.
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  // Move the snapshot controller's time forward so it gets past timeouts.
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  // the same page should be simply overridden.
  GetAllPages();
  ASSERT_EQ(0U, all_pages().size());
}

// Triggers two snapshot captures for two different page loads and URLs. Should
// end up with one offline page.
TEST_F(RecentTabHelperTest, TwoCapturesDifferentPageLoadsAndUrls) {
  NavigateAndCommit(kTestPageUrl);
  // Triggers snapshot after a time delay.
  recent_tab_helper()->DocumentAvailableInMainFrame();
  RunUntilIdle();
  EXPECT_TRUE(model()->is_loaded());
  EXPECT_EQ(0U, page_added_count());
  // Move the snapshot controller's time forward so it gets past timeouts.
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_EQ(1U, page_added_count());
  EXPECT_EQ(0U, model_removed_count());
  GetAllPages();
  ASSERT_EQ(1U, all_pages().size());
  EXPECT_EQ(kTestPageUrl, all_pages()[0].url);

  NavigateAndCommit(kTestPageUrlOther);
  // Triggers snapshot after a time delay.
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  // Move the snapshot controller's time forward so it gets past timeouts.
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_EQ(2U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  // the same page should be simply overridden.
  GetAllPages();
  ASSERT_EQ(1U, all_pages().size());
  EXPECT_EQ(kTestPageUrlOther, all_pages()[0].url);
}

// Triggers two snapshot captures via the download after a page was loaded page
// and saved twice by last_n. Should end up with three offline pages: two from
// downloads (which shouldn't replace each other) and one from last_n.
TEST_F(RecentTabHelperTest, TwoDownloadCapturesInARowSamePage) {
  NavigateAndCommit(kTestPageUrl);
  // Executes a regular load with snapshots taken by last_n.
  recent_tab_helper()->DocumentAvailableInMainFrame();
  RunUntilIdle();
  FastForwardSnapshotController();
  RunUntilIdle();
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  RunUntilIdle();
  // Checks that two last_n snapshots were created but only one was kept.
  EXPECT_EQ(2U, page_added_count());
  EXPECT_EQ(1U, model_removed_count());
  GetAllPages();
  ASSERT_EQ(1U, all_pages().size());
  EXPECT_EQ(kTestPageUrl, all_pages()[0].url);
  int64_t first_offline_id = all_pages()[0].offline_id;

  // Request the 1st offline download.
  const int64_t second_offline_id = first_offline_id + 1;
  recent_tab_helper()->ObserveAndDownloadCurrentPage(
      ClientId("download", "id2"), second_offline_id);
  RunUntilIdle();
  GetAllPages();
  // Checks that both the previous last_n and download snapshots are present.
  ASSERT_EQ(2U, all_pages().size());
  EXPECT_NE(nullptr, FindPageForOfflineId(first_offline_id));
  const OfflinePageItem* second_page = FindPageForOfflineId(second_offline_id);
  ASSERT_NE(nullptr, second_page);
  EXPECT_EQ(kTestPageUrl, second_page->url);
  EXPECT_EQ("download", second_page->client_id.name_space);
  EXPECT_EQ("id2", second_page->client_id.id);

  // Request the 2nd offline download.
  const int64_t third_offline_id = first_offline_id + 2;
  recent_tab_helper()->ObserveAndDownloadCurrentPage(
      ClientId("download", "id3"), third_offline_id);
  RunUntilIdle();
  GetAllPages();
  ASSERT_EQ(3U, all_pages().size());
  // Checks that the previous last_n and download snapshots are still present
  // and the new downloaded one was added.
  EXPECT_NE(nullptr, FindPageForOfflineId(first_offline_id));
  EXPECT_NE(nullptr, FindPageForOfflineId(second_offline_id));
  const OfflinePageItem* third_page = FindPageForOfflineId(third_offline_id);
  ASSERT_NE(nullptr, third_page);
  EXPECT_EQ(kTestPageUrl, third_page->url);
  EXPECT_EQ("download", third_page->client_id.name_space);
  EXPECT_EQ("id3", third_page->client_id.id);
}

// Simulates an error (disconnection) during the load of a page. Should end up
// with no offline pages.
TEST_F(RecentTabHelperTest, NoCaptureOnErrorPage) {
  FailLoad(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_TRUE(model()->is_loaded());
  GetAllPages();
  ASSERT_EQ(0U, all_pages().size());
}

// Checks that no snapshots are created if the Offline Page Cache feature is
// disabled.
TEST_F(RecentTabHelperTest, FeatureNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.Init();
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_TRUE(model()->is_loaded());
  GetAllPages();
  // No page should be captured.
  ASSERT_EQ(0U, all_pages().size());
}

// Simulates a download request to offline the current page. Should end up with
// one offline pages.
TEST_F(RecentTabHelperTest, DownloadRequest) {
  NavigateAndCommit(kTestPageUrl);
  recent_tab_helper()->ObserveAndDownloadCurrentPage(
      ClientId("download", "id1"), 153L);
  recent_tab_helper()->DocumentOnLoadCompletedInMainFrame();
  FastForwardSnapshotController();
  RunUntilIdle();
  EXPECT_TRUE(model()->is_loaded());
  GetAllPages();
  ASSERT_EQ(1U, all_pages().size());
  const OfflinePageItem& page = all_pages()[0];
  EXPECT_EQ(kTestPageUrl, page.url);
  EXPECT_EQ("download", page.client_id.name_space);
  EXPECT_EQ("id1", page.client_id.id);
  EXPECT_EQ(153L, page.offline_id);
}

}  // namespace offline_pages
