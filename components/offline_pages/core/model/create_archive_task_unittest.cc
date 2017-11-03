// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/create_archive_task.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_test_archiver.h"
#include "components/offline_pages/core/offline_page_test_store.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/test_task.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

using ArchiverResult = OfflinePageArchiver::ArchiverResult;
using SavePageParams = OfflinePageModel::SavePageParams;

namespace {
const char kTestClientNamespace[] = "default";
const GURL kTestUrl("http://example.com");
const GURL kTestUrl2("http://other.page.com");
const GURL kFileUrl("file:///foo");
const ClientId kTestClientId1(kTestClientNamespace, "1234");
const int64_t kTestFileSize = 876543LL;
const base::string16 kTestTitle = base::UTF8ToUTF16("a title");
const std::string kRequestOrigin("abc.xyz");
}  // namespace

class CreateArchiveTaskTest
    : public testing::Test,
      public OfflinePageTestArchiver::Observer,
      public base::SupportsWeakPtr<CreateArchiveTaskTest> {
 public:
  CreateArchiveTaskTest();
  ~CreateArchiveTaskTest() override;

  void SetUp() override;

  // OfflinePageTestArchiver::Observer implementation.
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override;

  void PumpLoop();
  void ResetResults();
  void OnCreateArchiveDone(OfflinePageItem offline_page,
                           OfflinePageArchiver* archiver,
                           ArchiverResult result,
                           const GURL& saved_url,
                           const base::FilePath& file_path,
                           const base::string16& title,
                           int64_t file_size,
                           const std::string& file_hash);
  std::unique_ptr<OfflinePageTestArchiver> BuildArchiver(const GURL& url,
                                                         ArchiverResult result);
  void CreateArchiveWithParams(const SavePageParams& save_page_params,
                               OfflinePageArchiver* archiver);

  void CreateArchiveWithArchiver(const GURL& gurl,
                                 const ClientId& client_id,
                                 const GURL& original_url,
                                 const std::string& request_origin,
                                 OfflinePageArchiver* archiver);

  void CreateArchiveWithResult(const GURL& gurl,
                               const ClientId& client_id,
                               const GURL& original_url,
                               const std::string& request_origin,
                               ArchiverResult expected_result);

  const base::FilePath& archives_dir() { return temp_dir_.GetPath(); }
  const OfflinePageItem& last_page_of_archive() {
    return last_page_of_archive_;
  }
  OfflinePageArchiver* last_saved_archiver() { return last_saved_archiver_; }
  ArchiverResult last_create_archive_result() {
    return last_create_archive_result_;
  }
  const base::FilePath& last_archiver_path() { return last_archiver_path_; }
  const GURL& last_saved_url() { return last_saved_url_; }
  const base::FilePath& last_saved_file_path() { return last_saved_file_path_; }
  const base::string16& last_saved_title() { return last_saved_title_; }
  int64_t last_saved_file_size() { return last_saved_file_size_; }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  base::ScopedTempDir temp_dir_;

  OfflinePageItem last_page_of_archive_;
  OfflinePageArchiver* last_saved_archiver_;
  ArchiverResult last_create_archive_result_;
  base::FilePath last_archiver_path_;
  GURL last_saved_url_;
  base::FilePath last_saved_file_path_;
  base::string16 last_saved_title_;
  int64_t last_saved_file_size_;
  // Owning a task to prevent it being destroyed in the heap when calling
  // CreateArchiveWithParams, which will lead to a heap-use-after-free on
  // trybots.
  std::unique_ptr<CreateArchiveTask> task_;
};

CreateArchiveTaskTest::CreateArchiveTaskTest()
    : task_runner_(new base::TestSimpleTaskRunner()),
      task_runner_handle_(task_runner_) {}

CreateArchiveTaskTest::~CreateArchiveTaskTest() {}

void CreateArchiveTaskTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
}

void CreateArchiveTaskTest::SetLastPathCreatedByArchiver(
    const base::FilePath& file_path) {
  last_archiver_path_ = file_path;
}

void CreateArchiveTaskTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void CreateArchiveTaskTest::ResetResults() {
  last_create_archive_result_ = ArchiverResult::ERROR_CANCELED;
  last_page_of_archive_ = OfflinePageItem();
  last_saved_url_ = GURL();
  last_saved_file_path_ = base::FilePath();
  last_saved_title_ = base::string16();
  last_saved_file_size_ = 0;
  last_archiver_path_.clear();
}
void CreateArchiveTaskTest::OnCreateArchiveDone(OfflinePageItem offline_page,
                                                OfflinePageArchiver* archiver,
                                                ArchiverResult result,
                                                const GURL& saved_url,
                                                const base::FilePath& file_path,
                                                const base::string16& title,
                                                int64_t file_size,
                                                const std::string& file_hash) {
  last_page_of_archive_ = offline_page;
  last_saved_archiver_ = archiver;
  last_create_archive_result_ = result;
  last_saved_url_ = saved_url;
  last_saved_file_path_ = file_path;
  last_saved_title_ = title;
  last_saved_file_size_ = file_size;
}

std::unique_ptr<OfflinePageTestArchiver> CreateArchiveTaskTest::BuildArchiver(
    const GURL& url,
    ArchiverResult result) {
  return std::unique_ptr<OfflinePageTestArchiver>(
      new OfflinePageTestArchiver(this, url, result, kTestTitle, kTestFileSize,
                                  base::ThreadTaskRunnerHandle::Get()));
}

void CreateArchiveTaskTest::CreateArchiveWithParams(
    const SavePageParams& save_page_params,
    OfflinePageArchiver* archiver) {
  task_ = base::MakeUnique<CreateArchiveTask>(
      archives_dir(), save_page_params, archiver,
      base::Bind(&CreateArchiveTaskTest::OnCreateArchiveDone, AsWeakPtr()));
  task_->Run();
  PumpLoop();
  // Check if the archiver is the same with the one in the callback.
  EXPECT_EQ(archiver, last_saved_archiver());
}

void CreateArchiveTaskTest::CreateArchiveWithArchiver(
    const GURL& gurl,
    const ClientId& client_id,
    const GURL& original_url,
    const std::string& request_origin,
    OfflinePageArchiver* archiver) {
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = gurl;
  save_page_params.client_id = client_id;
  save_page_params.original_url = original_url;
  save_page_params.is_background = false;
  save_page_params.request_origin = request_origin;
  CreateArchiveWithParams(save_page_params, archiver);
}

void CreateArchiveTaskTest::CreateArchiveWithResult(
    const GURL& gurl,
    const ClientId& client_id,
    const GURL& original_url,
    const std::string& request_origin,
    ArchiverResult expected_result) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(gurl, expected_result));
  CreateArchiveWithArchiver(gurl, client_id, original_url, request_origin,
                            archiver.get());
}

TEST_F(CreateArchiveTaskTest, CreateArchiveSuccessful) {
  CreateArchiveWithResult(
      kTestUrl, kTestClientId1, kTestUrl2, "",
      OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED);

  // Check the last result to be successful.
  EXPECT_EQ(ArchiverResult::SUCCESSFULLY_CREATED, last_create_archive_result());
  const OfflinePageItem& offline_page = last_page_of_archive();

  // The values will be set during archive creation.
  EXPECT_EQ(kTestUrl, offline_page.url);
  EXPECT_NE(OfflinePageModel::kInvalidOfflineId, offline_page.offline_id);
  EXPECT_EQ(kTestClientId1, offline_page.client_id);
  EXPECT_EQ(kTestUrl2, offline_page.original_url);
  EXPECT_EQ("", offline_page.request_origin);

  // The values that will not be set during archive creation, but will be set to
  // default value in the constructor of OfflinePageItem.
  EXPECT_EQ(0, offline_page.access_count);
  EXPECT_EQ(0, offline_page.flags);

  // The values that will not be set during archive creation, but will be in the
  // CreateArchiveTaskCallback.
  EXPECT_EQ(last_archiver_path(), last_saved_file_path());
  EXPECT_EQ(kTestFileSize, last_saved_file_size());
  EXPECT_EQ(kTestTitle, last_saved_title());
}

TEST_F(CreateArchiveTaskTest, CreateArchiveSuccessfulWithSameOriginalURL) {
  // Pass the original URL same as the final URL, the original will be empty in
  // this case.
  CreateArchiveWithResult(
      kTestUrl, kTestClientId1, kTestUrl, "",
      OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED);

  // Check the last result to be successful.
  EXPECT_EQ(ArchiverResult::SUCCESSFULLY_CREATED, last_create_archive_result());
  const OfflinePageItem& offline_page = last_page_of_archive();

  // The original URL should be empty.
  EXPECT_TRUE(offline_page.original_url.is_empty());

  // The values will be set during archive creation.
  EXPECT_EQ(kTestUrl, offline_page.url);
  EXPECT_NE(OfflinePageModel::kInvalidOfflineId, offline_page.offline_id);
  EXPECT_EQ(kTestClientId1, offline_page.client_id);
  EXPECT_EQ("", offline_page.request_origin);

  // The values that will not be set during archive creation, but will be set to
  // default value in the constructor of OfflinePageItem.
  EXPECT_EQ(0, offline_page.access_count);
  EXPECT_EQ(0, offline_page.flags);

  // The values that will not be set during archive creation, but will be in the
  // CreateArchiveTaskCallback.
  EXPECT_EQ(last_archiver_path(), last_saved_file_path());
  EXPECT_EQ(kTestFileSize, last_saved_file_size());
  EXPECT_EQ(kTestTitle, last_saved_title());
}

TEST_F(CreateArchiveTaskTest, CreateArchiveSuccessfulWithRequestOrigin) {
  CreateArchiveWithResult(
      kTestUrl, kTestClientId1, kTestUrl2, kRequestOrigin,
      OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED);

  // Check the last result to be successful.
  EXPECT_EQ(ArchiverResult::SUCCESSFULLY_CREATED, last_create_archive_result());
  const OfflinePageItem& offline_page = last_page_of_archive();

  // The values will be set during archive creation.
  EXPECT_EQ(kTestUrl, offline_page.url);
  EXPECT_NE(OfflinePageModel::kInvalidOfflineId, offline_page.offline_id);
  EXPECT_EQ(kTestClientId1, offline_page.client_id);
  EXPECT_EQ(kTestUrl2, offline_page.original_url);
  EXPECT_EQ(kRequestOrigin, offline_page.request_origin);

  // The values that will not be set during archive creation, but will be set to
  // default value in the constructor of OfflinePageItem.
  EXPECT_EQ(0, offline_page.access_count);
  EXPECT_EQ(0, offline_page.flags);

  // The values that will not be set during archive creation, but will be in the
  // CreateArchiveTaskCallback.
  EXPECT_EQ(last_archiver_path(), last_saved_file_path());
  EXPECT_EQ(kTestFileSize, last_saved_file_size());
  EXPECT_EQ(kTestTitle, last_saved_title());
}

TEST_F(CreateArchiveTaskTest, CreateArchiveWithArchiverCanceled) {
  CreateArchiveWithResult(kTestUrl, kTestClientId1, GURL(), "",
                          OfflinePageArchiver::ArchiverResult::ERROR_CANCELED);
  EXPECT_EQ(ArchiverResult::ERROR_CANCELED, last_create_archive_result());

  // The values will be set during archive creation.
  const OfflinePageItem& offline_page = last_page_of_archive();
  EXPECT_EQ(kTestUrl, offline_page.url);
  EXPECT_NE(OfflinePageModel::kInvalidOfflineId, offline_page.offline_id);
  EXPECT_EQ(kTestClientId1, offline_page.client_id);
  EXPECT_EQ(GURL(), offline_page.original_url);
  EXPECT_EQ("", offline_page.request_origin);
}

TEST_F(CreateArchiveTaskTest, CreateArchiveWithArchiverDeviceFull) {
  CreateArchiveWithResult(
      kTestUrl, kTestClientId1, GURL(), "",
      OfflinePageArchiver::ArchiverResult::ERROR_DEVICE_FULL);
  EXPECT_EQ(ArchiverResult::ERROR_DEVICE_FULL, last_create_archive_result());

  // The values will be set during archive creation.
  const OfflinePageItem& offline_page = last_page_of_archive();
  EXPECT_EQ(kTestUrl, offline_page.url);
  EXPECT_NE(OfflinePageModel::kInvalidOfflineId, offline_page.offline_id);
  EXPECT_EQ(kTestClientId1, offline_page.client_id);
  EXPECT_EQ(GURL(), offline_page.original_url);
  EXPECT_EQ("", offline_page.request_origin);
}

TEST_F(CreateArchiveTaskTest, CreateArchiveWithArchiverContentUnavailable) {
  CreateArchiveWithResult(
      kTestUrl, kTestClientId1, GURL(), "",
      OfflinePageArchiver::ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
  EXPECT_EQ(ArchiverResult::ERROR_CONTENT_UNAVAILABLE,
            last_create_archive_result());

  // The values will be set during archive creation.
  const OfflinePageItem& offline_page = last_page_of_archive();
  EXPECT_EQ(kTestUrl, offline_page.url);
  EXPECT_NE(OfflinePageModel::kInvalidOfflineId, offline_page.offline_id);
  EXPECT_EQ(kTestClientId1, offline_page.client_id);
  EXPECT_EQ(GURL(), offline_page.original_url);
  EXPECT_EQ("", offline_page.request_origin);
}

TEST_F(CreateArchiveTaskTest, CreateArchiveWithCreationFailed) {
  CreateArchiveWithResult(
      kTestUrl, kTestClientId1, GURL(), "",
      OfflinePageArchiver::ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED);
  EXPECT_EQ(ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED,
            last_create_archive_result());

  // The values will be set during archive creation.
  const OfflinePageItem& offline_page = last_page_of_archive();
  EXPECT_EQ(kTestUrl, offline_page.url);
  EXPECT_NE(OfflinePageModel::kInvalidOfflineId, offline_page.offline_id);
  EXPECT_EQ(kTestClientId1, offline_page.client_id);
  EXPECT_EQ(GURL(), offline_page.original_url);
  EXPECT_EQ("", offline_page.request_origin);
}

TEST_F(CreateArchiveTaskTest, CreateArchiveWithArchiverReturnedWrongUrl) {
  GURL test_url("http://other.random.url.com");
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      test_url, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  CreateArchiveWithArchiver(kTestUrl, kTestClientId1, GURL(), "",
                            archiver.get());

  // Since the task will not judge the result even if the |saved_url| in the
  // callback is not the same with the SavePageParams.url, so the result will be
  // SUCCESSFULLY_CREATED, but the |last_saved_url()| will be |test_url|. The
  // creator of the task will be responsible to detect this failure case.
  EXPECT_EQ(ArchiverResult::SUCCESSFULLY_CREATED, last_create_archive_result());
  EXPECT_EQ(test_url, last_saved_url());

  // The values will be set during archive creation.
  const OfflinePageItem& offline_page = last_page_of_archive();
  EXPECT_EQ(kTestUrl, offline_page.url);
  EXPECT_NE(OfflinePageModel::kInvalidOfflineId, offline_page.offline_id);
  EXPECT_EQ(kTestClientId1, offline_page.client_id);
  EXPECT_EQ(GURL(), offline_page.original_url);
  EXPECT_EQ("", offline_page.request_origin);
}

TEST_F(CreateArchiveTaskTest, CreateArchiveLocalFileFailed) {
  // Don't create archiver since it will not be needed for pages that are not
  // going to be saved.
  CreateArchiveWithArchiver(kFileUrl, kTestClientId1, GURL(), "", nullptr);
  EXPECT_EQ(ArchiverResult::ERROR_SKIPPED, last_create_archive_result());

  const OfflinePageItem& offline_page = last_page_of_archive();
  EXPECT_EQ(kFileUrl, offline_page.url);
  EXPECT_EQ(OfflinePageModel::kInvalidOfflineId, offline_page.offline_id);
  EXPECT_EQ(kTestClientId1, offline_page.client_id);
  EXPECT_EQ(GURL(), offline_page.original_url);
  EXPECT_EQ("", offline_page.request_origin);
}

TEST_F(CreateArchiveTaskTest, CreateArchiveFailedWithNullptr) {
  // Passing in a nullptr intentionally for test.
  CreateArchiveWithArchiver(kTestUrl, kTestClientId1, GURL(), "", nullptr);
  EXPECT_EQ(ArchiverResult::ERROR_CONTENT_UNAVAILABLE,
            last_create_archive_result());

  const OfflinePageItem& offline_page = last_page_of_archive();
  EXPECT_EQ(kTestUrl, offline_page.url);
  EXPECT_EQ(OfflinePageModel::kInvalidOfflineId, offline_page.offline_id);
  EXPECT_EQ(kTestClientId1, offline_page.client_id);
  EXPECT_EQ(GURL(), offline_page.original_url);
  EXPECT_EQ("", offline_page.request_origin);
}

TEST_F(CreateArchiveTaskTest, CreateArchiveInBackground) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  OfflinePageTestArchiver* archiver_ptr = archiver.get();

  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = kTestUrl;
  save_page_params.client_id = kTestClientId1;
  save_page_params.is_background = true;
  save_page_params.use_page_problem_detectors = false;

  CreateArchiveWithParams(save_page_params, archiver_ptr);

  EXPECT_TRUE(archiver_ptr->create_archive_called());
  // |remove_popup_overlay| should be turned on on background mode.
  EXPECT_TRUE(archiver_ptr->create_archive_params().remove_popup_overlay);
}

}  // namespace offline_pages
