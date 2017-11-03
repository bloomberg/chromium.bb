// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/offline_page_model_taskified.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_metadata_store_sql.h"
#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_test_archiver.h"
#include "components/offline_pages/core/offline_page_test_store.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::A;
using testing::An;
using testing::Eq;
using testing::SaveArg;

namespace offline_pages {

using ArchiverResult = OfflinePageArchiver::ArchiverResult;

namespace {
const GURL kTestUrl("http://example.com");
const GURL kTestUrl2("http://other.page.com");
const GURL kFileUrl("file:///foo");
const ClientId kTestClientId1(kDefaultNamespace, "1234");
const ClientId kTestClientId2(kDefaultNamespace, "5678");
const ClientId kTestUserRequestedClientId(kDownloadNamespace, "714");
const int64_t kTestFileSize = 876543LL;
const base::string16 kTestTitle = base::UTF8ToUTF16("a title");
const std::string kTestRequestOrigin("abc.xyz");
const std::string kEmptyRequestOrigin("");

int64_t GetFileCountInDir(const base::FilePath& dir) {
  base::FileEnumerator file_enumerator(dir, false, base::FileEnumerator::FILES);
  int64_t count = 0;
  for (base::FilePath path = file_enumerator.Next(); !path.empty();
       path = file_enumerator.Next()) {
    count++;
  }
  return count;
}

}  // namespace

class OfflinePageModelTaskifiedTest
    : public testing::Test,
      public OfflinePageModel::Observer,
      public OfflinePageTestArchiver::Observer,
      public base::SupportsWeakPtr<OfflinePageModelTaskifiedTest> {
 public:
  OfflinePageModelTaskifiedTest();
  ~OfflinePageModelTaskifiedTest() override;

  void SetUp() override;
  void TearDown() override;

  // Runs until all of the tasks that are not delayed are gone from the task
  // queue.
  void PumpLoop() { task_runner_->RunUntilIdle(); }
  void ResetResults();

  // OfflinePageModel::Observer implementation.
  void OfflinePageModelLoaded(OfflinePageModel* model) override;
  void OfflinePageAdded(OfflinePageModel* model,
                        const OfflinePageItem& added_page) override;
  void OfflinePageDeleted(
      const OfflinePageModel::DeletedPageInfo& page_info) override;

  // OfflinePageTestArchiver::Observer implementation.
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override;

  // Saves a page which will create the file and insert the corresponding
  // metadata into store. It relies on the implementation of
  // OfflinePageModel::SavePage.
  void SavePageWithCallback(const GURL& url,
                            const ClientId& client_id,
                            const GURL& original_url,
                            const std::string& request_origin,
                            std::unique_ptr<OfflinePageArchiver> archiver,
                            const SavePageCallback& callback);
  int64_t SavePageWithExpectedResult(
      const GURL& url,
      const ClientId& client_id,
      const GURL& original_url,
      const std::string& request_origin,
      std::unique_ptr<OfflinePageArchiver> archiver,
      SavePageResult expected_result);
  // Insert an offline page in to store, it does not rely on the model
  // implementation.
  void InsertPageIntoStore(const OfflinePageItem& offline_page);
  std::unique_ptr<OfflinePageTestArchiver> BuildArchiver(const GURL& url,
                                                         ArchiverResult result);
  void CheckTaskQueueIdle();

  // Getters for private fields.
  OfflinePageModelTaskified* model() { return model_.get(); }
  OfflinePageMetadataStoreSQL* store() { return store_test_util_.store(); }
  OfflinePageMetadataStoreTestUtil* store_test_util() {
    return &store_test_util_;
  }
  OfflinePageItemGenerator* generator() { return &generator_; }
  TaskQueue* model_task_queue() { return &model_->task_queue_; }
  const base::FilePath& temporary_dir_path() {
    return temporary_dir_.GetPath();
  }
  const base::FilePath& persistent_dir_path() {
    return persistent_dir_.GetPath();
  }

  const base::FilePath& last_archiver_path() { return last_archiver_path_; }
  bool observer_add_page_called() { return observer_add_page_called_; }
  const OfflinePageItem& last_added_page() { return last_added_page_; }
  bool observer_delete_page_called() { return observer_delete_page_called_; }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  std::unique_ptr<OfflinePageModelTaskified> model_;
  OfflinePageMetadataStoreTestUtil store_test_util_;
  OfflinePageItemGenerator generator_;
  base::ScopedTempDir temporary_dir_;
  base::ScopedTempDir persistent_dir_;

  base::FilePath last_archiver_path_;
  bool observer_add_page_called_;
  OfflinePageItem last_added_page_;
  bool observer_delete_page_called_;
  OfflinePageModel::DeletedPageInfo last_deleted_page_info_;
};

OfflinePageModelTaskifiedTest::OfflinePageModelTaskifiedTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_) {}

OfflinePageModelTaskifiedTest::~OfflinePageModelTaskifiedTest() {}

void OfflinePageModelTaskifiedTest::SetUp() {
  store_test_util()->BuildStoreInMemory();
  ASSERT_TRUE(temporary_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(persistent_dir_.CreateUniqueTempDir());
  auto archive_manager = base::MakeUnique<ArchiveManager>(
      temporary_dir_path(), persistent_dir_path(),
      base::ThreadTaskRunnerHandle::Get());
  model_ = base::MakeUnique<OfflinePageModelTaskified>(
      store_test_util()->ReleaseStore(), std::move(archive_manager),
      base::ThreadTaskRunnerHandle::Get());
  model_->AddObserver(this);
  ResetResults();
  PumpLoop();
  CheckTaskQueueIdle();
  EXPECT_EQ(0UL, model_->pending_archivers_.size());
}

void OfflinePageModelTaskifiedTest::TearDown() {
  store_test_util_.DeleteStore();
  if (temporary_dir_.IsValid()) {
    if (!temporary_dir_.Delete())
      DLOG(ERROR) << "temporary_dir_ not created";
  }
  if (persistent_dir_.IsValid()) {
    if (!persistent_dir_.Delete())
      DLOG(ERROR) << "persistent_dir not created";
  }
  EXPECT_EQ(0UL, model_->pending_archivers_.size());
  CheckTaskQueueIdle();
  model_->RemoveObserver(this);
  model_.reset();
  PumpLoop();
}

void OfflinePageModelTaskifiedTest::ResetResults() {
  last_archiver_path_.clear();
  observer_add_page_called_ = false;
  observer_delete_page_called_ = false;
}

void OfflinePageModelTaskifiedTest::OfflinePageModelLoaded(
    OfflinePageModel* model) {}

void OfflinePageModelTaskifiedTest::OfflinePageAdded(
    OfflinePageModel* model,
    const OfflinePageItem& added_page) {
  observer_add_page_called_ = true;
  last_added_page_ = added_page;
}

void OfflinePageModelTaskifiedTest::OfflinePageDeleted(
    const OfflinePageModel::DeletedPageInfo& page_info) {
  observer_delete_page_called_ = true;
  last_deleted_page_info_ = page_info;
}

void OfflinePageModelTaskifiedTest::SetLastPathCreatedByArchiver(
    const base::FilePath& file_path) {
  last_archiver_path_ = file_path;
}

void OfflinePageModelTaskifiedTest::SavePageWithCallback(
    const GURL& url,
    const ClientId& client_id,
    const GURL& original_url,
    const std::string& request_origin,
    std::unique_ptr<OfflinePageArchiver> archiver,
    const SavePageCallback& callback) {
  OfflinePageModel::SavePageParams save_page_params;
  save_page_params.url = url;
  save_page_params.client_id = client_id;
  save_page_params.original_url = original_url;
  save_page_params.request_origin = request_origin;
  save_page_params.is_background = false;
  model()->SavePage(save_page_params, std::move(archiver), callback);
  PumpLoop();
}

int64_t OfflinePageModelTaskifiedTest::SavePageWithExpectedResult(
    const GURL& url,
    const ClientId& client_id,
    const GURL& original_url,
    const std::string& request_origin,
    std::unique_ptr<OfflinePageArchiver> archiver,
    SavePageResult expected_result) {
  int64_t offline_id = OfflinePageModel::kInvalidOfflineId;
  base::MockCallback<SavePageCallback> callback;
  EXPECT_CALL(callback, Run(Eq(expected_result), A<int64_t>()))
      .WillOnce(SaveArg<1>(&offline_id));
  SavePageWithCallback(url, client_id, original_url, request_origin,
                       std::move(archiver), callback.Get());
  if (expected_result == SavePageResult::SUCCESS) {
    EXPECT_NE(OfflinePageModel::kInvalidOfflineId, offline_id);
  }
  return offline_id;
}

void OfflinePageModelTaskifiedTest::InsertPageIntoStore(
    const OfflinePageItem& offline_page) {
  store_test_util()->InsertItem(offline_page);
}

std::unique_ptr<OfflinePageTestArchiver>
OfflinePageModelTaskifiedTest::BuildArchiver(const GURL& url,
                                             ArchiverResult result) {
  return base::MakeUnique<OfflinePageTestArchiver>(
      this, url, result, kTestTitle, kTestFileSize,
      base::ThreadTaskRunnerHandle::Get());
}

void OfflinePageModelTaskifiedTest::CheckTaskQueueIdle() {
  EXPECT_FALSE(model_task_queue()->HasPendingTasks());
  EXPECT_FALSE(model_task_queue()->HasRunningTask());
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageSuccessful) {
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);
  int64_t offline_id = SavePageWithExpectedResult(
      kTestUrl, kTestClientId1, kTestUrl2, kEmptyRequestOrigin,
      std::move(archiver), SavePageResult::SUCCESS);

  EXPECT_EQ(1LL, GetFileCountInDir(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  auto saved_page_ptr = store_test_util()->GetPageByOfflineId(offline_id);
  ASSERT_TRUE(saved_page_ptr);

  EXPECT_EQ(kTestUrl, saved_page_ptr->url);
  EXPECT_EQ(kTestClientId1.id, saved_page_ptr->client_id.id);
  EXPECT_EQ(kTestClientId1.name_space, saved_page_ptr->client_id.name_space);
  EXPECT_EQ(last_archiver_path(), saved_page_ptr->file_path);
  EXPECT_EQ(kTestFileSize, saved_page_ptr->file_size);
  EXPECT_EQ(0, saved_page_ptr->access_count);
  EXPECT_EQ(0, saved_page_ptr->flags);
  EXPECT_EQ(kTestTitle, saved_page_ptr->title);
  EXPECT_EQ(kTestUrl2, saved_page_ptr->original_url);
  EXPECT_EQ("", saved_page_ptr->request_origin);
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageSuccessfulWithSameOriginalUrl) {
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);

  // Pass the original URL same as the final URL.
  int64_t offline_id = SavePageWithExpectedResult(
      kTestUrl, kTestClientId1, kTestUrl, kEmptyRequestOrigin,
      std::move(archiver), SavePageResult::SUCCESS);

  EXPECT_EQ(1LL, GetFileCountInDir(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  auto saved_page_ptr = store_test_util()->GetPageByOfflineId(offline_id);
  ASSERT_TRUE(saved_page_ptr);

  EXPECT_EQ(kTestUrl, saved_page_ptr->url);
  // The original URL should be empty.
  EXPECT_TRUE(saved_page_ptr->original_url.is_empty());
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageSuccessfulWithRequestOrigin) {
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);
  int64_t offline_id = SavePageWithExpectedResult(
      kTestUrl, kTestClientId1, kTestUrl2, kTestRequestOrigin,
      std::move(archiver), SavePageResult::SUCCESS);

  EXPECT_EQ(1LL, GetFileCountInDir(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  auto saved_page_ptr = store_test_util()->GetPageByOfflineId(offline_id);
  ASSERT_TRUE(saved_page_ptr);

  EXPECT_EQ(kTestUrl, saved_page_ptr->url);
  EXPECT_EQ(kTestClientId1.id, saved_page_ptr->client_id.id);
  EXPECT_EQ(kTestClientId1.name_space, saved_page_ptr->client_id.name_space);
  EXPECT_EQ(last_archiver_path(), saved_page_ptr->file_path);
  EXPECT_EQ(kTestFileSize, saved_page_ptr->file_size);
  EXPECT_EQ(0, saved_page_ptr->access_count);
  EXPECT_EQ(0, saved_page_ptr->flags);
  EXPECT_EQ(kTestTitle, saved_page_ptr->title);
  EXPECT_EQ(kTestUrl2, saved_page_ptr->original_url);
  EXPECT_EQ(kTestRequestOrigin, saved_page_ptr->request_origin);
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageOfflineArchiverCancelled) {
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::ERROR_CANCELED);
  SavePageWithExpectedResult(kTestUrl, kTestClientId1, kTestUrl2,
                             kEmptyRequestOrigin, std::move(archiver),
                             SavePageResult::CANCELLED);
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageOfflineArchiverDeviceFull) {
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::ERROR_DEVICE_FULL);
  SavePageWithExpectedResult(kTestUrl, kTestClientId1, kTestUrl2,
                             kEmptyRequestOrigin, std::move(archiver),
                             SavePageResult::DEVICE_FULL);
}

TEST_F(OfflinePageModelTaskifiedTest,
       SavePageOfflineArchiverContentUnavailable) {
  auto archiver =
      BuildArchiver(kTestUrl, ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
  SavePageWithExpectedResult(kTestUrl, kTestClientId1, kTestUrl2,
                             kEmptyRequestOrigin, std::move(archiver),
                             SavePageResult::CONTENT_UNAVAILABLE);
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageOfflineCreationFailed) {
  auto archiver =
      BuildArchiver(kTestUrl, ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED);
  SavePageWithExpectedResult(kTestUrl, kTestClientId1, kTestUrl2,
                             kEmptyRequestOrigin, std::move(archiver),
                             SavePageResult::ARCHIVE_CREATION_FAILED);
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageOfflineArchiverReturnedWrongUrl) {
  auto archiver = BuildArchiver(GURL("http://other.random.url.com"),
                                ArchiverResult::SUCCESSFULLY_CREATED);
  SavePageWithExpectedResult(kTestUrl, kTestClientId1, kTestUrl2,
                             kEmptyRequestOrigin, std::move(archiver),
                             SavePageResult::ARCHIVE_CREATION_FAILED);
}

// This test is disabled since it's lacking the ability of mocking store failure
// in store_test_utils. https://crbug.com/781023
// TODO(romax): reenable the test once the above issue is resolved.
TEST_F(OfflinePageModelTaskifiedTest,
       DISABLED_SavePageOfflineCreationStoreWriteFailure) {}

TEST_F(OfflinePageModelTaskifiedTest, SavePageLocalFileFailed) {
  SavePageWithExpectedResult(
      kFileUrl, kTestClientId1, kTestUrl2, kEmptyRequestOrigin,
      std::unique_ptr<OfflinePageTestArchiver>(), SavePageResult::SKIPPED);
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageOfflineArchiverTwoPages) {
  // This test case is for the scenario that there are two save page requests
  // but the first one is slower during archive creation (set_delayed in the
  // test case) so the second request will finish first.
  // offline_id1 will be id of the first completed request.
  // offline_id2 will be id of the second completed request.
  int64_t offline_id1;
  int64_t offline_id2;

  base::MockCallback<SavePageCallback> callback;
  EXPECT_CALL(callback, Run(Eq(SavePageResult::SUCCESS), A<int64_t>()))
      .Times(2)
      .WillOnce(SaveArg<1>(&offline_id1))
      .WillOnce(SaveArg<1>(&offline_id2));

  // delayed_archiver_ptr will be valid until after first PumpLoop() call after
  // CompleteCreateArchive() is called. Keeping the raw pointer because the
  // ownership is transferring to the model.
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);
  OfflinePageTestArchiver* delayed_archiver_ptr = archiver.get();
  delayed_archiver_ptr->set_delayed(true);
  SavePageWithCallback(kTestUrl, kTestClientId1, GURL(), kEmptyRequestOrigin,
                       std::move(archiver), callback.Get());

  // Request to save another page, with request origin.
  archiver = BuildArchiver(kTestUrl2, ArchiverResult::SUCCESSFULLY_CREATED);
  SavePageWithCallback(kTestUrl2, kTestClientId2, GURL(), kTestRequestOrigin,
                       std::move(archiver), callback.Get());

  EXPECT_EQ(1LL, GetFileCountInDir(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  base::FilePath saved_file_path1 = last_archiver_path();

  ResetResults();

  delayed_archiver_ptr->CompleteCreateArchive();
  // Pump loop so the first request can finish saving.
  PumpLoop();

  // Check that offline_id1 refers to the second save page request.
  EXPECT_EQ(2LL, GetFileCountInDir(temporary_dir_path()));
  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  base::FilePath saved_file_path2 = last_archiver_path();

  auto saved_page_ptr1 = store_test_util()->GetPageByOfflineId(offline_id1);
  auto saved_page_ptr2 = store_test_util()->GetPageByOfflineId(offline_id2);
  ASSERT_TRUE(saved_page_ptr1);
  ASSERT_TRUE(saved_page_ptr2);

  EXPECT_EQ(kTestUrl2, saved_page_ptr1->url);
  EXPECT_EQ(kTestClientId2, saved_page_ptr1->client_id);
  EXPECT_EQ(saved_file_path1, saved_page_ptr1->file_path);
  EXPECT_EQ(kTestFileSize, saved_page_ptr1->file_size);
  EXPECT_EQ(kTestRequestOrigin, saved_page_ptr1->request_origin);

  EXPECT_EQ(kTestUrl, saved_page_ptr2->url);
  EXPECT_EQ(kTestClientId1, saved_page_ptr2->client_id);
  EXPECT_EQ(saved_file_path2, saved_page_ptr2->file_path);
  EXPECT_EQ(kTestFileSize, saved_page_ptr2->file_size);
}

TEST_F(OfflinePageModelTaskifiedTest, AddPage) {
  // Creates a fresh page.
  generator()->SetArchiveDirectory(temporary_dir_path());
  OfflinePageItem page = generator()->CreateItemWithTempFile();

  base::MockCallback<AddPageCallback> callback;
  EXPECT_CALL(callback, Run(An<AddPageResult>(), Eq(page.offline_id)));

  model()->AddPage(page, callback.Get());
  EXPECT_TRUE(model_task_queue()->HasRunningTask());

  PumpLoop();
  EXPECT_TRUE(observer_add_page_called());
  EXPECT_EQ(last_added_page(), page);
}

TEST_F(OfflinePageModelTaskifiedTest, MarkPageAccessed) {
  OfflinePageItem page = generator()->CreateItem();
  InsertPageIntoStore(page);

  model()->MarkPageAccessed(page.offline_id);
  EXPECT_TRUE(model_task_queue()->HasRunningTask());

  PumpLoop();

  auto accessed_page_ptr =
      store_test_util()->GetPageByOfflineId(page.offline_id);
  ASSERT_TRUE(accessed_page_ptr);
  EXPECT_EQ(1LL, accessed_page_ptr->access_count);
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_CheckPagesSavedInSeparateDirs \
  DISABLED_CheckPagesSavedInSeparateDirs
#else
#define MAYBE_CheckPagesSavedInSeparateDirs CheckPagesSavedInSeparateDirs
#endif
TEST_F(OfflinePageModelTaskifiedTest, MAYBE_CheckPagesSavedInSeparateDirs) {
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);
  int64_t temporary_id;
  int64_t persistent_id;

  base::MockCallback<SavePageCallback> callback;
  EXPECT_CALL(callback, Run(Eq(SavePageResult::SUCCESS), A<int64_t>()))
      .Times(2)
      .WillOnce(SaveArg<1>(&temporary_id))
      .WillOnce(SaveArg<1>(&persistent_id));

  // Save a temporary page.
  SavePageWithCallback(kTestUrl, kTestClientId1, GURL(), kEmptyRequestOrigin,
                       std::move(archiver), callback.Get());

  // Save a persistent page.
  archiver = BuildArchiver(kTestUrl2, ArchiverResult::SUCCESSFULLY_CREATED);
  SavePageWithCallback(kTestUrl2, kTestUserRequestedClientId, GURL(),
                       kEmptyRequestOrigin, std::move(archiver),
                       callback.Get());

  std::unique_ptr<OfflinePageItem> temporary_page =
      store_test_util()->GetPageByOfflineId(temporary_id);
  std::unique_ptr<OfflinePageItem> persistent_page =
      store_test_util()->GetPageByOfflineId(persistent_id);

  ASSERT_TRUE(temporary_page);
  ASSERT_TRUE(persistent_page);

  base::FilePath temporary_page_path = temporary_page->file_path;
  base::FilePath persistent_page_path = persistent_page->file_path;

  EXPECT_TRUE(temporary_dir_path().IsParent(temporary_page_path));
  EXPECT_TRUE(persistent_dir_path().IsParent(persistent_page_path));
  EXPECT_NE(temporary_page_path.DirName(), persistent_page_path.DirName());
}

// This test is disabled since it's lacking the ability of mocking store failure
// in store_test_utils. https://crbug.com/781023
// TODO(romax): reenable the test once the above issue is resolved.
TEST_F(OfflinePageModelTaskifiedTest,
       DISABLED_ClearCachedPagesTriggeredWhenSaveFailed) {
  // After a save failed, only PostClearCachedPagesTask will be triggered.
  generator()->SetArchiveDirectory(temporary_dir_path());
  generator()->SetNamespace(kDefaultNamespace);
  generator()->SetUrl(kTestUrl);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);

  ResetResults();

  base::MockCallback<SavePageCallback> callback;
  EXPECT_CALL(callback, Run(Eq(SavePageResult::ERROR_PAGE), A<int64_t>()));

  std::unique_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED));
  OfflinePageTestArchiver* archiver_ptr = archiver.get();

  SavePageWithCallback(kTestUrl, kTestClientId1, kTestUrl2, kEmptyRequestOrigin,
                       std::move(archiver), callback.Get());
  // The archiver will not be erased before PumpLoop().
  ASSERT_TRUE(archiver_ptr);
  EXPECT_TRUE(archiver_ptr->create_archive_called());

  PumpLoop();
  EXPECT_FALSE(observer_add_page_called());
  EXPECT_FALSE(observer_delete_page_called());
}

TEST_F(OfflinePageModelTaskifiedTest, ExtraActionTriggeredWhenSaveSuccess) {
  // After a save successfully saved, both RemovePagesWithSameUrlInSameNamespace
  // and PostClearCachedPagesTask will be triggered.
  // Add pages that have the same namespace and url directly into store, in
  // order to avoid triggering the removal.
  // The 'default' namespace has a limit of 1 per url.
  generator()->SetArchiveDirectory(temporary_dir_path());
  generator()->SetNamespace(kDefaultNamespace);
  generator()->SetUrl(kTestUrl);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);

  ResetResults();

  std::unique_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED));
  SavePageWithExpectedResult(kTestUrl, kTestClientId1, kTestUrl2,
                             kEmptyRequestOrigin, std::move(archiver),
                             SavePageResult::SUCCESS);

  EXPECT_TRUE(observer_add_page_called());
  EXPECT_TRUE(observer_delete_page_called());
}

TEST_F(OfflinePageModelTaskifiedTest, GetArchiveDirectory) {
  base::FilePath temporary_dir =
      model()->GetArchiveDirectory(kDefaultNamespace);
  EXPECT_EQ(temporary_dir_path(), temporary_dir);
  base::FilePath persistent_dir =
      model()->GetArchiveDirectory(kDownloadNamespace);
  EXPECT_EQ(persistent_dir_path(), persistent_dir);
}

}  // namespace offline_pages
