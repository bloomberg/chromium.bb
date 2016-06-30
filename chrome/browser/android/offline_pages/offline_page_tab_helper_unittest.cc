// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_tab_helper.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/offline_page_utils.h"
#include "chrome/browser/android/offline_pages/test_offline_page_model_builder.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/offline_page_test_archiver.h"
#include "components/offline_pages/offline_page_types.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const GURL kTestPageUrl("http://test.org/page1");
const ClientId kTestClientId = ClientId(kBookmarkNamespace, "1234");
const int64_t kTestFileSize = 876543LL;
const char kRedirectResultHistogram[] = "OfflinePages.RedirectResult";
const char kTabId[] = "42";

class TestNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  TestNetworkChangeNotifier() : online_(true) {}

  net::NetworkChangeNotifier::ConnectionType GetCurrentConnectionType()
      const override {
    return online_ ? net::NetworkChangeNotifier::CONNECTION_UNKNOWN
                   : net::NetworkChangeNotifier::CONNECTION_NONE;
  }

  void set_online(bool online) { online_ = online; }

 private:
  bool online_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkChangeNotifier);
};

class TestDelegate : public OfflinePageTabHelper::Delegate {
 public:
  TestDelegate(bool has_tab_android, std::string tab_id)
      : has_tab_android_(has_tab_android), tab_id_(tab_id) {}
  ~TestDelegate() override {}

  // offline_pages::OfflinePageTabHelper::Delegate implementation:
  bool GetTabId(content::WebContents* web_contents,
                std::string* tab_id) const override {
    if (has_tab_android_)
      *tab_id = tab_id_;
    return has_tab_android_;
  }

 private:
  bool has_tab_android_;
  std::string tab_id_;
};

}  // namespace

class OfflinePageTabHelperTest :
    public ChromeRenderViewHostTestHarness,
    public OfflinePageTestArchiver::Observer,
    public base::SupportsWeakPtr<OfflinePageTabHelperTest> {
 public:
  OfflinePageTabHelperTest()
      : network_change_notifier_(new TestNetworkChangeNotifier()) {}
  ~OfflinePageTabHelperTest() override {}

  void SetUp() override;
  void TearDown() override;

  void RunUntilIdle();
  void SimulateHasNetworkConnectivity(bool has_connectivity);
  void StartLoad(const GURL& url);
  void FailLoad(const GURL& url);
  std::unique_ptr<OfflinePageTestArchiver> BuildArchiver(
      const GURL& url,
      const base::FilePath& file_name);
  void OnSavePageDone(SavePageResult result, int64_t offline_id);

  OfflinePageTabHelper* offline_page_tab_helper() const {
    return offline_page_tab_helper_;
  }

  const GURL& online_url() const { return offline_page_item_->url; }
  GURL offline_url() const { return offline_page_item_->GetOfflineURL(); }
  int64_t offline_id() const { return offline_page_item_->offline_id; }

  const base::HistogramTester& histograms() const { return histogram_tester_; }

 private:
  // OfflinePageTestArchiver::Observer implementation:
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override;

  void OnGetPageByOfflineIdDone(const OfflinePageItem* result);

  std::unique_ptr<TestNetworkChangeNotifier> network_change_notifier_;
  OfflinePageTabHelper* offline_page_tab_helper_;  // Not owned.

  std::unique_ptr<OfflinePageItem> offline_page_item_;

  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageTabHelperTest);
};

void OfflinePageTabHelperTest::SetUp() {
  // Enables offline pages feature.
  // TODO(jianli): Remove this once the feature is completely enabled.
  base::FeatureList::ClearInstanceForTesting();
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(
      offline_pages::kOfflineBookmarksFeature.name, "");
  base::FeatureList::SetInstance(std::move(feature_list));

  // Creates a test web contents.
  content::RenderViewHostTestHarness::SetUp();
  OfflinePageTabHelper::CreateForWebContents(web_contents());
  offline_page_tab_helper_ =
      OfflinePageTabHelper::FromWebContents(web_contents());
  offline_page_tab_helper_->SetDelegateForTesting(
      base::MakeUnique<TestDelegate>(true, kTabId));

  // Sets up the factory for testing.
  OfflinePageModelFactory::GetInstance()->SetTestingFactoryAndUse(
      browser_context(), BuildTestOfflinePageModel);
  RunUntilIdle();

  // Saves an offline page.
  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context());
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestPageUrl, base::FilePath(FILE_PATH_LITERAL("page1.mhtml"))));
  model->SavePage(
      kTestPageUrl, kTestClientId, std::move(archiver),
      base::Bind(&OfflinePageTabHelperTest::OnSavePageDone, AsWeakPtr()));
  RunUntilIdle();
}

void OfflinePageTabHelperTest::TearDown() {
  content::RenderViewHostTestHarness::TearDown();
}

void OfflinePageTabHelperTest::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void OfflinePageTabHelperTest::SimulateHasNetworkConnectivity(bool online) {
  network_change_notifier_->set_online(online);
}

void OfflinePageTabHelperTest::StartLoad(const GURL& url) {
  controller().LoadURL(url, content::Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());
  content::RenderFrameHostTester::For(main_rfh())->
      SimulateNavigationStart(url);
}

void OfflinePageTabHelperTest::FailLoad(const GURL& url) {
  content::RenderFrameHostTester::For(main_rfh())->SimulateNavigationStart(url);
  // Set up the error code for the failed navigation.
  content::RenderFrameHostTester::For(main_rfh())->
      SimulateNavigationError(url, net::ERR_INTERNET_DISCONNECTED);
  content::RenderFrameHostTester::For(main_rfh())->
      SimulateNavigationErrorPageCommit();
  // Gives a chance to run delayed task to do redirection.
  RunUntilIdle();
}

std::unique_ptr<OfflinePageTestArchiver>
OfflinePageTabHelperTest::BuildArchiver(const GURL& url,
                                        const base::FilePath& file_name) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
      this, url, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
      kTestFileSize, base::ThreadTaskRunnerHandle::Get()));
  archiver->set_filename(file_name);
  return archiver;
}

void OfflinePageTabHelperTest::OnSavePageDone(SavePageResult result,
                                              int64_t offline_id) {
  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context());
  model->GetPageByOfflineId(offline_id,
      base::Bind(&OfflinePageTabHelperTest::OnGetPageByOfflineIdDone,
                 AsWeakPtr()));
}

void OfflinePageTabHelperTest::SetLastPathCreatedByArchiver(
    const base::FilePath& file_path) {}

void OfflinePageTabHelperTest::OnGetPageByOfflineIdDone(
    const OfflinePageItem* result) {
  DCHECK(result);
  offline_page_item_.reset(new OfflinePageItem(*result));
}

TEST_F(OfflinePageTabHelperTest, SwitchToOnlineFromOfflineOnNetwork) {
  SimulateHasNetworkConnectivity(true);

  StartLoad(offline_url());
  // Gives a chance to run delayed task to do redirection.
  RunUntilIdle();
  // Redirection will be done immediately on navigation start.
  EXPECT_EQ(online_url(), controller().GetPendingEntry()->GetURL());
  histograms().ExpectUniqueSample(
      kRedirectResultHistogram,
      static_cast<int>(OfflinePageTabHelper::RedirectResult::
          REDIRECTED_ON_CONNECTED_NETWORK),
      1);
}

TEST_F(OfflinePageTabHelperTest, SwitchToOfflineFromOnlineOnNoNetwork) {
  SimulateHasNetworkConnectivity(false);

  StartLoad(online_url());
  // Gives a chance to run delayed task to do redirection.
  RunUntilIdle();
  // Redirection will be done immediately on navigation start.
  EXPECT_EQ(offline_url(), controller().GetPendingEntry()->GetURL());
  histograms().ExpectUniqueSample(
      kRedirectResultHistogram,
      static_cast<int>(OfflinePageTabHelper::RedirectResult::
          REDIRECTED_ON_DISCONNECTED_NETWORK),
      1);
}

TEST_F(OfflinePageTabHelperTest, TestCurrentOfflinePage) {
  SimulateHasNetworkConnectivity(false);

  StartLoad(online_url());
  // Gives a chance to run delayed task to do redirection.
  RunUntilIdle();

  const OfflinePageItem* item =
      OfflinePageUtils::GetOfflinePageFromWebContents(web_contents());
  EXPECT_EQ(offline_url(), item->GetOfflineURL());
  EXPECT_EQ(online_url(), item->url);

  SimulateHasNetworkConnectivity(true);
  StartLoad(offline_url());
  RunUntilIdle();
  item = OfflinePageUtils::GetOfflinePageFromWebContents(web_contents());
  EXPECT_EQ(nullptr, item);
}

TEST_F(OfflinePageTabHelperTest, SwitchToOfflineFromOnlineOnError) {
  SimulateHasNetworkConnectivity(true);

  StartLoad(online_url());
  RunUntilIdle();
  EXPECT_EQ(online_url(), controller().GetPendingEntry()->GetURL());

  // Redirection will be done immediately on navigation end with error.
  FailLoad(online_url());
  EXPECT_EQ(offline_url(), controller().GetPendingEntry()->GetURL());

  histograms().ExpectUniqueSample(
      kRedirectResultHistogram,
      static_cast<int>(OfflinePageTabHelper::RedirectResult::
          REDIRECTED_ON_FLAKY_NETWORK),
      1);
}

TEST_F(OfflinePageTabHelperTest, NewNavigationCancelsPendingRedirects) {
  SimulateHasNetworkConnectivity(false);

  StartLoad(online_url());
  const GURL unsaved_url("http://test.org/page2");

  // We should have a pending task that will do the redirect.
  ASSERT_TRUE(offline_page_tab_helper()->weak_ptr_factory_.HasWeakPtrs());
  ASSERT_EQ(online_url(), controller().GetPendingEntry()->GetURL());

  // Should cancel pending tasks for previous URL.
  StartLoad(unsaved_url);

  // Gives a chance to run delayed task to do redirection.
  RunUntilIdle();

  // Redirection should be cancelled so we should still navigate to
  // |unsaved_url|.
  EXPECT_EQ(unsaved_url, controller().GetPendingEntry()->GetURL());

  // Should report attempt of redirect, but the page not found.
  histograms().ExpectUniqueSample(
      kRedirectResultHistogram,
      static_cast<int>(OfflinePageTabHelper::RedirectResult::
          PAGE_NOT_FOUND_ON_DISCONNECTED_NETWORK),
      1);
}

// This test saves 3 pages (one in setup and 2 in test). The most appropriate
// test is related to |kTabId|, as it is saved in the latest moment and can be
// used in the current tab.
TEST_F(OfflinePageTabHelperTest, SelectBestPageForCurrentTab) {
  // Saves an offline page.
  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context());
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestPageUrl, base::FilePath(FILE_PATH_LITERAL("page2.mhtml"))));

  // We expect this copy to be used later.
  ClientId client_id(kLastNNamespace, kTabId);
  model->SavePage(
      kTestPageUrl, client_id, std::move(archiver),
      base::Bind(&OfflinePageTabHelperTest::OnSavePageDone, AsWeakPtr()));
  RunUntilIdle();
  const int64_t expected_offline_id = offline_id();
  const GURL expected_offline_url = offline_url();

  archiver = BuildArchiver(kTestPageUrl,
                           base::FilePath(FILE_PATH_LITERAL("page3.html")));
  client_id.id = "39";
  model->SavePage(
      kTestPageUrl, client_id, std::move(archiver),
      base::Bind(&OfflinePageTabHelperTest::OnSavePageDone, AsWeakPtr()));
  RunUntilIdle();

  SimulateHasNetworkConnectivity(false);
  StartLoad(kTestPageUrl);
  // Gives a chance to run delayed task to do redirection.
  RunUntilIdle();

  const OfflinePageItem* item =
      OfflinePageUtils::GetOfflinePageFromWebContents(web_contents());
  EXPECT_EQ(expected_offline_id, item->offline_id);
  EXPECT_EQ(expected_offline_url, item->GetOfflineURL());
  EXPECT_EQ(kLastNNamespace, item->client_id.name_space);
  EXPECT_EQ(kTabId, item->client_id.id);
}
}  // namespace offline_pages
