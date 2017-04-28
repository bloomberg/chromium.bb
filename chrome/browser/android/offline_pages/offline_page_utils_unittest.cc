// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_utils.h"

#include <stdint.h>
#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/offline_page_tab_helper.h"
#include "chrome/browser/android/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/android/offline_pages/test_offline_page_model_builder.h"
#include "chrome/browser/android/offline_pages/test_request_coordinator_builder.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_pages/core/background/network_quality_provider_stub.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_test_archiver.h"
#include "components/offline_pages/core/offline_page_test_store.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

const GURL kTestPage1Url("http://test.org/page1");
const GURL kTestPage2Url("http://test.org/page2");
const GURL kTestPage3Url("http://test.org/page3");
const GURL kTestPage4Url("http://test.org/page4");
const int64_t kTestFileSize = 876543LL;
const char* kTestPage1ClientId = "1234";
const char* kTestPage2ClientId = "5678";
const char* kTestPage3ClientId = "7890";

void CheckDuplicateDownloadsCallback(
    OfflinePageUtils::DuplicateCheckResult* out_result,
    OfflinePageUtils::DuplicateCheckResult result) {
  DCHECK(out_result);
  *out_result = result;
}

void GetAllRequestsCallback(
    std::vector<std::unique_ptr<SavePageRequest>>* out_requests,
    std::vector<std::unique_ptr<SavePageRequest>> requests) {
  *out_requests = std::move(requests);
}

}  // namespace

class OfflinePageUtilsTest
    : public testing::Test,
      public OfflinePageTestArchiver::Observer,
      public base::SupportsWeakPtr<OfflinePageUtilsTest> {
 public:
  OfflinePageUtilsTest();
  ~OfflinePageUtilsTest() override;

  void SetUp() override;
  void RunUntilIdle();

  void SavePage(const GURL& url,
                const ClientId& client_id,
                std::unique_ptr<OfflinePageArchiver> archiver);

  // Return number of matches found.
  int FindRequestByNamespaceAndURL(const std::string& name_space,
                                   const GURL& url);

  // Necessary callbacks for the offline page model.
  void OnSavePageDone(SavePageResult result, int64_t offlineId);
  void OnClearAllDone();
  void OnExpirePageDone(bool success);
  void OnGetURLDone(const GURL& url);

  // OfflinePageTestArchiver::Observer implementation:
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override;

  TestingProfile* profile() { return &profile_; }
  content::WebContents* web_contents() const { return web_contents_.get(); }

  int64_t offline_id() const { return offline_id_; }

 private:
  void CreateOfflinePages();
  void CreateRequests();
  std::unique_ptr<OfflinePageTestArchiver> BuildArchiver(
      const GURL& url,
      const base::FilePath& file_name);

  content::TestBrowserThreadBundle browser_thread_bundle_;
  int64_t offline_id_;
  GURL url_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

OfflinePageUtilsTest::OfflinePageUtilsTest() = default;

OfflinePageUtilsTest::~OfflinePageUtilsTest() {}

void OfflinePageUtilsTest::SetUp() {
  // Enables offline pages feature.
  // TODO(jianli): Remove this once the feature is completely enabled.
  scoped_feature_list_.InitAndEnableFeature(
      offline_pages::kOfflineBookmarksFeature);

  // Create a test web contents.
  web_contents_.reset(content::WebContents::Create(
      content::WebContents::CreateParams(profile())));
  OfflinePageTabHelper::CreateForWebContents(web_contents_.get());

  // Set up the factory for testing.
  OfflinePageModelFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, BuildTestOfflinePageModel);
  RunUntilIdle();

  NetworkQualityProviderStub::SetUserData(
      &profile_, base::MakeUnique<NetworkQualityProviderStub>());
  RequestCoordinatorFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, BuildTestRequestCoordinator);
  RunUntilIdle();

  // Make sure to create offline pages and requests.
  CreateOfflinePages();
  CreateRequests();
}

void OfflinePageUtilsTest::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void OfflinePageUtilsTest::SavePage(
    const GURL& url,
    const ClientId& client_id,
    std::unique_ptr<OfflinePageArchiver> archiver) {
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = url;
  save_page_params.client_id = client_id;
  OfflinePageModelFactory::GetForBrowserContext(profile())->SavePage(
      save_page_params,
      std::move(archiver),
      base::Bind(&OfflinePageUtilsTest::OnSavePageDone, AsWeakPtr()));
  RunUntilIdle();
}

void OfflinePageUtilsTest::OnSavePageDone(SavePageResult result,
                                          int64_t offline_id) {
  offline_id_ = offline_id;
}

void OfflinePageUtilsTest::OnExpirePageDone(bool success) {
  // Result ignored here.
}

void OfflinePageUtilsTest::OnClearAllDone() {
  // Result ignored here.
}

void OfflinePageUtilsTest::OnGetURLDone(const GURL& url) {
  url_ = url;
}

void OfflinePageUtilsTest::SetLastPathCreatedByArchiver(
    const base::FilePath& file_path) {}

void OfflinePageUtilsTest::CreateOfflinePages() {
  // Create page 1.
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestPage1Url, base::FilePath(FILE_PATH_LITERAL("page1.mhtml"))));
  offline_pages::ClientId client_id;
  client_id.name_space = kDownloadNamespace;
  client_id.id = kTestPage1ClientId;
  SavePage(kTestPage1Url, client_id, std::move(archiver));

  // Create page 2.
  archiver = BuildArchiver(kTestPage2Url,
                           base::FilePath(FILE_PATH_LITERAL("page2.mhtml")));
  client_id.id = kTestPage2ClientId;
  SavePage(kTestPage2Url, client_id, std::move(archiver));
}

void OfflinePageUtilsTest::CreateRequests() {
  RequestCoordinator* request_coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(profile());

  RequestCoordinator::SavePageLaterParams params;
  params.url = kTestPage3Url;
  params.client_id =
      offline_pages::ClientId(kDownloadNamespace, kTestPage3ClientId);
  request_coordinator->SavePageLater(params);
  RunUntilIdle();
}

std::unique_ptr<OfflinePageTestArchiver> OfflinePageUtilsTest::BuildArchiver(
    const GURL& url,
    const base::FilePath& file_name) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
      this, url, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
      base::string16(), kTestFileSize, base::ThreadTaskRunnerHandle::Get()));
  archiver->set_filename(file_name);
  return archiver;
}

int OfflinePageUtilsTest::FindRequestByNamespaceAndURL(
    const std::string& name_space,
    const GURL& url) {
  RequestCoordinator* request_coordinator =
      RequestCoordinatorFactory::GetForBrowserContext(profile());
  std::vector<std::unique_ptr<SavePageRequest>> requests;
  request_coordinator->GetAllRequests(
      base::Bind(&GetAllRequestsCallback, base::Unretained(&requests)));
  RunUntilIdle();

  int matches = 0;
  for (auto& request : requests) {
    if (request->url() == url &&
        request->client_id().name_space == name_space) {
      matches++;
    }
  }
  return matches;
}

TEST_F(OfflinePageUtilsTest, CheckDuplicateDownloads) {
  OfflinePageUtils::DuplicateCheckResult result =
      OfflinePageUtils::DuplicateCheckResult::NOT_FOUND;

  // The duplicate page should be found for this.
  OfflinePageUtils::CheckDuplicateDownloads(
      profile(), kTestPage1Url,
      base::Bind(&CheckDuplicateDownloadsCallback, base::Unretained(&result)));
  RunUntilIdle();
  EXPECT_EQ(OfflinePageUtils::DuplicateCheckResult::DUPLICATE_PAGE_FOUND,
            result);

  // The duplicate request should be found for this.
  OfflinePageUtils::CheckDuplicateDownloads(
      profile(), kTestPage3Url,
      base::Bind(&CheckDuplicateDownloadsCallback, base::Unretained(&result)));
  RunUntilIdle();
  EXPECT_EQ(OfflinePageUtils::DuplicateCheckResult::DUPLICATE_REQUEST_FOUND,
            result);

  // No duplicate should be found for this.
  OfflinePageUtils::CheckDuplicateDownloads(
      profile(), kTestPage4Url,
      base::Bind(&CheckDuplicateDownloadsCallback, base::Unretained(&result)));
  RunUntilIdle();
  EXPECT_EQ(OfflinePageUtils::DuplicateCheckResult::NOT_FOUND, result);
}

TEST_F(OfflinePageUtilsTest, ScheduleDownload) {
  // Pre-check.
  ASSERT_EQ(0, FindRequestByNamespaceAndURL(kDownloadNamespace, kTestPage1Url));
  ASSERT_EQ(1, FindRequestByNamespaceAndURL(kDownloadNamespace, kTestPage3Url));
  ASSERT_EQ(0, FindRequestByNamespaceAndURL(kDownloadNamespace, kTestPage4Url));

  // Re-downloading a page with duplicate page found.
  OfflinePageUtils::ScheduleDownload(
      web_contents(), kDownloadNamespace, kTestPage1Url,
      OfflinePageUtils::DownloadUIActionFlags::NONE);
  RunUntilIdle();
  EXPECT_EQ(1, FindRequestByNamespaceAndURL(kDownloadNamespace, kTestPage1Url));

  // Re-downloading a page with duplicate request found.
  OfflinePageUtils::ScheduleDownload(
      web_contents(), kDownloadNamespace, kTestPage3Url,
      OfflinePageUtils::DownloadUIActionFlags::NONE);
  RunUntilIdle();
  EXPECT_EQ(2, FindRequestByNamespaceAndURL(kDownloadNamespace, kTestPage3Url));

  // Downloading a page with no duplicate found.
  OfflinePageUtils::ScheduleDownload(
      web_contents(), kDownloadNamespace, kTestPage4Url,
      OfflinePageUtils::DownloadUIActionFlags::NONE);
  RunUntilIdle();
  EXPECT_EQ(1, FindRequestByNamespaceAndURL(kDownloadNamespace, kTestPage4Url));
}

TEST_F(OfflinePageUtilsTest, EqualsIgnoringFragment) {
  EXPECT_TRUE(OfflinePageUtils::EqualsIgnoringFragment(
      GURL("http://example.com/"), GURL("http://example.com/")));
  EXPECT_TRUE(OfflinePageUtils::EqualsIgnoringFragment(
      GURL("http://example.com/"), GURL("http://example.com/#test")));
  EXPECT_TRUE(OfflinePageUtils::EqualsIgnoringFragment(
      GURL("http://example.com/#test"), GURL("http://example.com/")));
  EXPECT_TRUE(OfflinePageUtils::EqualsIgnoringFragment(
      GURL("http://example.com/#test"), GURL("http://example.com/#test2")));
  EXPECT_FALSE(OfflinePageUtils::EqualsIgnoringFragment(
      GURL("http://example.com/"), GURL("http://test.com/#test")));
}

}  // namespace offline_pages
