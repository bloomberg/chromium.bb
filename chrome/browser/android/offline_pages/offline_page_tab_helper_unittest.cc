// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_tab_helper.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/test_offline_page_model_builder.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/offline_page_test_archiver.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const GURL kTestPageUrl("http://test.org/page1");
const ClientId kTestPageBookmarkId = ClientId(BOOKMARK_NAMESPACE, "1234");
const int64_t kTestFileSize = 876543LL;

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
  void CommitLoad(const GURL& url);
  void FailLoad(const GURL& url);

  OfflinePageTabHelper* offline_page_tab_helper() const {
    return offline_page_tab_helper_;
  }

  int64_t offline_id() const { return offline_id_; }

 private:
  // OfflinePageTestArchiver::Observer implementation:
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override;

  std::unique_ptr<OfflinePageTestArchiver> BuildArchiver(
      const GURL& url,
      const base::FilePath& file_name);
  void OnSavePageDone(OfflinePageModel::SavePageResult result,
                      int64_t offline_id);

  std::unique_ptr<TestNetworkChangeNotifier> network_change_notifier_;
  OfflinePageTabHelper* offline_page_tab_helper_;  // Not owned.

  int64_t offline_id_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageTabHelperTest);
};

void OfflinePageTabHelperTest::SetUp() {
  // Enables offline pages feature.
  // TODO(jianli): Remove this once the feature is completely enabled.
  base::FeatureList::ClearInstanceForTesting();
  scoped_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(
      offline_pages::kOfflineBookmarksFeature.name, "");
  base::FeatureList::SetInstance(std::move(feature_list));

  // Creates a test web contents.
  content::RenderViewHostTestHarness::SetUp();
  OfflinePageTabHelper::CreateForWebContents(web_contents());
  offline_page_tab_helper_ =
      OfflinePageTabHelper::FromWebContents(web_contents());

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
      kTestPageUrl, kTestPageBookmarkId, std::move(archiver),
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
}

void OfflinePageTabHelperTest::CommitLoad(const GURL& url) {
  int entry_id = controller().GetPendingEntry()->GetUniqueID();
  content::RenderFrameHostTester::For(main_rfh())
      ->SendNavigate(0, entry_id, true, url);
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

void OfflinePageTabHelperTest::SetLastPathCreatedByArchiver(
    const base::FilePath& file_path) {
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

void OfflinePageTabHelperTest::OnSavePageDone(
    OfflinePageModel::SavePageResult result,
    int64_t offline_id) {
  offline_id_ = offline_id;
}

TEST_F(OfflinePageTabHelperTest, SwitchToOnlineFromOffline) {
  SimulateHasNetworkConnectivity(true);

  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context());
  const OfflinePageItem* page = model->GetPageByOfflineId(offline_id());
  GURL offline_url = page->GetOfflineURL();
  GURL online_url = page->url;

  StartLoad(offline_url);
  CommitLoad(online_url);
  EXPECT_EQ(online_url, controller().GetLastCommittedEntry()->GetURL());
}

TEST_F(OfflinePageTabHelperTest, SwitchToOfflineFromOnline) {
  SimulateHasNetworkConnectivity(false);

  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(browser_context());
  const OfflinePageItem* page = model->GetPageByOfflineId(offline_id());
  GURL offline_url = page->GetOfflineURL();
  GURL online_url = page->url;

  StartLoad(online_url);
  EXPECT_EQ(online_url, controller().GetPendingEntry()->GetURL());

  FailLoad(online_url);
  EXPECT_EQ(offline_url, controller().GetPendingEntry()->GetURL());
}

}  // namespace offline_pages
