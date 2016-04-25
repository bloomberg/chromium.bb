// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_utils.h"

#include <stdint.h>
#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/test_offline_page_model_builder.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/offline_page_test_archiver.h"
#include "components/offline_pages/offline_page_test_store.h"
#include "components/offline_pages/proto/offline_pages.pb.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

const GURL kTestPage1Url("http://test.org/page1");
const GURL kTestPage2Url("http://test.org/page2");
const GURL kTestPage3Url("http://test.org/page3");
const int64_t kTestFileSize = 876543LL;
const char* kTestPage1ClientId = "1234";
const char* kTestPage2ClientId = "5678";
const int64_t kTestPage1BookmarkId = 1234;
const int64_t kTestPage2BookmarkId = 5678;

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

  // Necessary callbacks for the offline page model.
  void OnSavePageDone(OfflinePageModel::SavePageResult result,
                      int64_t offlineId);
  void OnClearAllDone();

  // OfflinePageTestArchiver::Observer implementation:
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override;

  // Offline page URL for the first page.
  const GURL& offline_url_page_1() const { return offline_url_page_1_; }
  // Offline page URL for the second page.
  const GURL& offline_url_page_2() const { return offline_url_page_2_; }
  // Offline page URL not related to any page.
  const GURL& offline_url_missing() const { return offline_url_missing_; }

  TestingProfile* profile() { return &profile_; }

  int64_t offline_id() const { return offline_id_; }

 private:
  void CreateOfflinePages();
  std::unique_ptr<OfflinePageTestArchiver> BuildArchiver(
      const GURL& url,
      const base::FilePath& file_name);

  GURL offline_url_page_1_;
  GURL offline_url_page_2_;
  GURL offline_url_missing_;

  int64_t offline_id_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  TestingProfile profile_;
};

OfflinePageUtilsTest::OfflinePageUtilsTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {}

OfflinePageUtilsTest::~OfflinePageUtilsTest() {}

void OfflinePageUtilsTest::SetUp() {
  // Enables offline pages feature.
  // TODO(jianli): Remove this once the feature is completely enabled.
  base::FeatureList::ClearInstanceForTesting();
  scoped_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(
      offline_pages::kOfflineBookmarksFeature.name, "");
  base::FeatureList::SetInstance(std::move(feature_list));

  // Set up the factory for testing.
  OfflinePageModelFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, BuildTestOfflinePageModel);
  RunUntilIdle();

  // Make sure the store contains the right offline pages before the load
  // happens.
  CreateOfflinePages();
}

void OfflinePageUtilsTest::RunUntilIdle() {
  task_runner_->RunUntilIdle();
}

void OfflinePageUtilsTest::OnSavePageDone(
    OfflinePageModel::SavePageResult result,
    int64_t offline_id) {
  offline_id_ = offline_id;
}

void OfflinePageUtilsTest::OnClearAllDone() {
  // Result ignored here.
}

void OfflinePageUtilsTest::SetLastPathCreatedByArchiver(
    const base::FilePath& file_path) {}

void OfflinePageUtilsTest::CreateOfflinePages() {
  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(profile());

  // Create page 1.
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestPage1Url, base::FilePath(FILE_PATH_LITERAL("page1.mhtml"))));
  offline_pages::ClientId client_id;
  client_id.name_space = kBookmarkNamespace;
  client_id.id = kTestPage1ClientId;
  model->SavePage(
      kTestPage1Url, client_id, std::move(archiver),
      base::Bind(&OfflinePageUtilsTest::OnSavePageDone, AsWeakPtr()));
  RunUntilIdle();
  int64_t offline1 = offline_id();

  client_id.id = kTestPage2ClientId;
  // Create page 2.
  archiver = BuildArchiver(kTestPage2Url,
                           base::FilePath(FILE_PATH_LITERAL("page2.mhtml")));
  model->SavePage(
      kTestPage2Url, client_id, std::move(archiver),
      base::Bind(&OfflinePageUtilsTest::OnSavePageDone, AsWeakPtr()));
  RunUntilIdle();
  int64_t offline2 = offline_id();

  // Make a copy of local paths of the two pages stored in the model.
  offline_url_page_1_ = model->GetPageByOfflineId(offline1)->GetOfflineURL();
  offline_url_page_2_ = model->GetPageByOfflineId(offline2)->GetOfflineURL();
  // Create a file path that is not associated with any offline page.
  offline_url_missing_ = net::FilePathToFileURL(
      profile()
          ->GetPath()
          .Append(chrome::kOfflinePageArchviesDirname)
          .Append(FILE_PATH_LITERAL("missing_file.mhtml")));
}

std::unique_ptr<OfflinePageTestArchiver> OfflinePageUtilsTest::BuildArchiver(
    const GURL& url,
    const base::FilePath& file_name) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
      this, url, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
      kTestFileSize, base::ThreadTaskRunnerHandle::Get()));
  archiver->set_filename(file_name);
  return archiver;
}

TEST_F(OfflinePageUtilsTest, MightBeOfflineURL) {
  // URL is invalid.
  EXPECT_FALSE(OfflinePageUtils::MightBeOfflineURL(GURL("/test.mhtml")));
  // Scheme is not file.
  EXPECT_FALSE(OfflinePageUtils::MightBeOfflineURL(GURL("http://test.com/")));
  // Does not end with .mhtml.
  EXPECT_FALSE(OfflinePageUtils::MightBeOfflineURL(GURL("file:///test.txt")));
  // Might still be an offline page.
  EXPECT_TRUE(OfflinePageUtils::MightBeOfflineURL(GURL("file:///test.mhtml")));
}

TEST_F(OfflinePageUtilsTest, GetOfflineURLForOnlineURL) {
  EXPECT_EQ(offline_url_page_1(), OfflinePageUtils::GetOfflineURLForOnlineURL(
                                      profile(), kTestPage1Url));
  EXPECT_EQ(offline_url_page_2(), OfflinePageUtils::GetOfflineURLForOnlineURL(
                                      profile(), kTestPage2Url));
  EXPECT_EQ(GURL(), OfflinePageUtils::GetOfflineURLForOnlineURL(
                        profile(), GURL(kTestPage3Url)));
}

TEST_F(OfflinePageUtilsTest, GetOnlineURLForOfflineURL) {
  EXPECT_EQ(kTestPage1Url, OfflinePageUtils::GetOnlineURLForOfflineURL(
                               profile(), offline_url_page_1()));
  EXPECT_EQ(kTestPage2Url, OfflinePageUtils::GetOnlineURLForOfflineURL(
                               profile(), offline_url_page_2()));
  EXPECT_EQ(GURL::EmptyGURL(), OfflinePageUtils::GetOnlineURLForOfflineURL(
                                   profile(), offline_url_missing()));
}

TEST_F(OfflinePageUtilsTest, GetBookmarkIdForOfflineURL) {
  EXPECT_EQ(kTestPage1BookmarkId, OfflinePageUtils::GetBookmarkIdForOfflineURL(
                                      profile(), offline_url_page_1()));
  EXPECT_EQ(kTestPage2BookmarkId, OfflinePageUtils::GetBookmarkIdForOfflineURL(
                                      profile(), offline_url_page_2()));
  EXPECT_EQ(-1, OfflinePageUtils::GetBookmarkIdForOfflineURL(
                    profile(), offline_url_missing()));
}

TEST_F(OfflinePageUtilsTest, IsOfflinePage) {
  EXPECT_TRUE(OfflinePageUtils::IsOfflinePage(profile(), offline_url_page_1()));
  EXPECT_TRUE(OfflinePageUtils::IsOfflinePage(profile(), offline_url_page_2()));
  EXPECT_FALSE(
      OfflinePageUtils::IsOfflinePage(profile(), offline_url_missing()));
  EXPECT_FALSE(OfflinePageUtils::IsOfflinePage(profile(), kTestPage1Url));
  EXPECT_FALSE(OfflinePageUtils::IsOfflinePage(profile(), kTestPage2Url));
}

TEST_F(OfflinePageUtilsTest, HasOfflinePageForOnlineURL) {
  EXPECT_TRUE(
      OfflinePageUtils::HasOfflinePageForOnlineURL(profile(), kTestPage1Url));
  EXPECT_TRUE(
      OfflinePageUtils::HasOfflinePageForOnlineURL(profile(), kTestPage2Url));
  EXPECT_FALSE(
      OfflinePageUtils::HasOfflinePageForOnlineURL(profile(), kTestPage3Url));
}

}  // namespace offline_pages
