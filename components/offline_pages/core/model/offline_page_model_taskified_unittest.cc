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
#include "components/offline_pages/core/model/offline_page_test_util.h"
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
using testing::ElementsAre;
using testing::Eq;
using testing::IsEmpty;
using testing::Pointee;
using testing::SaveArg;
using testing::UnorderedElementsAre;

namespace offline_pages {

using ArchiverResult = OfflinePageArchiver::ArchiverResult;

namespace {
const GURL kTestUrl("http://example.com");
const GURL kTestUrl2("http://other.page.com");
const GURL kTestUrlWithFragment("http://example.com#frag");
const GURL kTestUrl2WithFragment("http://other.page.com#frag");
const GURL kOtherUrl("http://foo");
const GURL kFileUrl("file:///foo");
const ClientId kTestClientId1(kDefaultNamespace, "1234");
const ClientId kTestClientId2(kDefaultNamespace, "5678");
const ClientId kTestUserRequestedClientId(kDownloadNamespace, "714");
const int64_t kTestFileSize = 876543LL;
const base::string16 kTestTitle = base::UTF8ToUTF16("a title");
const std::string kTestRequestOrigin("abc.xyz");
const std::string kEmptyRequestOrigin("");

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
  OfflinePageItemGenerator* page_generator() { return &generator_; }
  TaskQueue* task_queue() { return &model_->task_queue_; }
  const base::FilePath& temporary_dir_path() {
    return temporary_dir_.GetPath();
  }
  const base::FilePath& persistent_dir_path() {
    return persistent_dir_.GetPath();
  }

  const base::FilePath& last_path_created_by_archiver() {
    return last_path_created_by_archiver_;
  }
  bool observer_add_page_called() { return observer_add_page_called_; }
  const OfflinePageItem& last_added_page() { return last_added_page_; }
  bool observer_delete_page_called() { return observer_delete_page_called_; }
  const OfflinePageModel::DeletedPageInfo& last_deleted_page_info() {
    return last_deleted_page_info_;
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  std::unique_ptr<OfflinePageModelTaskified> model_;
  OfflinePageMetadataStoreTestUtil store_test_util_;
  OfflinePageItemGenerator generator_;
  base::ScopedTempDir temporary_dir_;
  base::ScopedTempDir persistent_dir_;

  base::FilePath last_path_created_by_archiver_;
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
  page_generator()->SetArchiveDirectory(temporary_dir_path());
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
  last_path_created_by_archiver_.clear();
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
  last_path_created_by_archiver_ = file_path;
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
  EXPECT_FALSE(task_queue()->HasPendingTasks());
  EXPECT_FALSE(task_queue()->HasRunningTask());
}

TEST_F(OfflinePageModelTaskifiedTest, SavePageSuccessful) {
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);
  int64_t offline_id = SavePageWithExpectedResult(
      kTestUrl, kTestClientId1, kTestUrl2, kEmptyRequestOrigin,
      std::move(archiver), SavePageResult::SUCCESS);

  EXPECT_EQ(1UL, test_util::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  auto saved_page_ptr = store_test_util()->GetPageByOfflineId(offline_id);
  ASSERT_TRUE(saved_page_ptr);

  EXPECT_EQ(kTestUrl, saved_page_ptr->url);
  EXPECT_EQ(kTestClientId1.id, saved_page_ptr->client_id.id);
  EXPECT_EQ(kTestClientId1.name_space, saved_page_ptr->client_id.name_space);
  EXPECT_EQ(last_path_created_by_archiver(), saved_page_ptr->file_path);
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

  EXPECT_EQ(1UL, test_util::GetFileCountInDirectory(temporary_dir_path()));
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

  EXPECT_EQ(1UL, test_util::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  auto saved_page_ptr = store_test_util()->GetPageByOfflineId(offline_id);
  ASSERT_TRUE(saved_page_ptr);

  EXPECT_EQ(kTestUrl, saved_page_ptr->url);
  EXPECT_EQ(kTestClientId1.id, saved_page_ptr->client_id.id);
  EXPECT_EQ(kTestClientId1.name_space, saved_page_ptr->client_id.name_space);
  EXPECT_EQ(last_path_created_by_archiver(), saved_page_ptr->file_path);
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

  EXPECT_EQ(1UL, test_util::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  base::FilePath saved_file_path1 = last_path_created_by_archiver();

  ResetResults();

  delayed_archiver_ptr->CompleteCreateArchive();
  // Pump loop so the first request can finish saving.
  PumpLoop();

  // Check that offline_id1 refers to the second save page request.
  EXPECT_EQ(2UL, test_util::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  base::FilePath saved_file_path2 = last_path_created_by_archiver();

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
  OfflinePageItem page = page_generator()->CreateItemWithTempFile();

  base::MockCallback<AddPageCallback> callback;
  EXPECT_CALL(callback, Run(An<AddPageResult>(), Eq(page.offline_id)));

  model()->AddPage(page, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
  EXPECT_TRUE(observer_add_page_called());
  EXPECT_EQ(last_added_page(), page);
}

TEST_F(OfflinePageModelTaskifiedTest, MarkPageAccessed) {
  OfflinePageItem page = page_generator()->CreateItem();
  InsertPageIntoStore(page);

  model()->MarkPageAccessed(page.offline_id);
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();

  auto accessed_page_ptr =
      store_test_util()->GetPageByOfflineId(page.offline_id);
  ASSERT_TRUE(accessed_page_ptr);
  EXPECT_EQ(1LL, accessed_page_ptr->access_count);
}

TEST_F(OfflinePageModelTaskifiedTest, GetAllPagesWhenStoreEmpty) {
  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(IsEmpty()));

  model()->GetAllPages(callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
}

// TODO(romax): remove these 'indicators for newly added tests' when migration
// is done.
// This test case is covered by DeletePageTaskTest::DeletePagesBy*.
TEST_F(OfflinePageModelTaskifiedTest, DISABLED_DeletePageSuccessful) {}

// This test case is covered by DeletePageTaskTest::DeletePagesByUrlPredicate.
TEST_F(OfflinePageModelTaskifiedTest,
       DISABLED_DeleteCachedPageByPredicateUserRequested) {}

// This test case is renamed to DeletePagesByUrlPredicate.
TEST_F(OfflinePageModelTaskifiedTest, DISABLED_DeleteCachedPageByPredicate) {}

// This test case is covered by DeletePageTaskTest::DeletePagesBy*NotFound.
TEST_F(OfflinePageModelTaskifiedTest, DISABLED_DeletePageNotFound) {}

// This test case is covered by
// DeletePageTaskTest::DeletePagesStoreFailureOnRemove.
TEST_F(OfflinePageModelTaskifiedTest, DISABLED_DeletePageStoreFailureOnRemove) {
}

// This test case is covered by DeletePageTaskTest::DeletePagesBy*.
TEST_F(OfflinePageModelTaskifiedTest, DISABLED_DeleteMultiplePages) {}

// These newly added tests are testing the API instead of results, which
// should be covered in DeletePagesTaskTest.

TEST_F(OfflinePageModelTaskifiedTest, DeletePagesByOfflineId) {
  OfflinePageItem page1 = page_generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = page_generator()->CreateItemWithTempFile();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);
  EXPECT_EQ(2UL, test_util::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(2LL, store_test_util()->GetPageCount());

  base::MockCallback<DeletePageCallback> callback;
  EXPECT_CALL(callback, Run(testing::A<DeletePageResult>()));
  CheckTaskQueueIdle();

  model()->DeletePagesByOfflineId({page1.offline_id}, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
  EXPECT_TRUE(observer_delete_page_called());
  EXPECT_EQ(last_deleted_page_info().offline_id, page1.offline_id);
  EXPECT_EQ(1UL, test_util::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  CheckTaskQueueIdle();
}

TEST_F(OfflinePageModelTaskifiedTest, DeletePagesByUrlPredicate) {
  page_generator()->SetNamespace(kDefaultNamespace);
  page_generator()->SetUrl(kTestUrl);
  OfflinePageItem page1 = page_generator()->CreateItemWithTempFile();
  page_generator()->SetUrl(kTestUrl2);
  OfflinePageItem page2 = page_generator()->CreateItemWithTempFile();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);
  EXPECT_EQ(2UL, test_util::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(2LL, store_test_util()->GetPageCount());

  base::MockCallback<DeletePageCallback> callback;
  EXPECT_CALL(callback, Run(testing::A<DeletePageResult>()));
  CheckTaskQueueIdle();

  UrlPredicate predicate =
      base::Bind([](const GURL& expected_url,
                    const GURL& url) -> bool { return url == expected_url; },
                 kTestUrl);

  model()->DeleteCachedPagesByURLPredicate(predicate, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
  EXPECT_TRUE(observer_delete_page_called());
  EXPECT_EQ(last_deleted_page_info().offline_id, page1.offline_id);
  EXPECT_EQ(1UL, test_util::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  CheckTaskQueueIdle();
}

TEST_F(OfflinePageModelTaskifiedTest, GetPageByOfflineId) {
  page_generator()->SetNamespace(kDefaultNamespace);
  page_generator()->SetUrl(kTestUrl);
  OfflinePageItem page = page_generator()->CreateItem();
  InsertPageIntoStore(page);

  base::MockCallback<SingleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(Pointee(Eq(page))));

  model()->GetPageByOfflineId(page.offline_id, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
}

TEST_F(OfflinePageModelTaskifiedTest, GetPagesByUrl_FinalUrl) {
  page_generator()->SetUrl(kTestUrl);
  OfflinePageItem page1 = page_generator()->CreateItem();
  InsertPageIntoStore(page1);
  page_generator()->SetUrl(kTestUrl2);
  OfflinePageItem page2 = page_generator()->CreateItem();
  InsertPageIntoStore(page2);

  // Search by kTestUrl.
  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(ElementsAre(page1)));
  model()->GetPagesByURL(kTestUrl, URLSearchMode::SEARCH_BY_FINAL_URL_ONLY,
                         callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());
  PumpLoop();

  // Search by kTestUrl2.
  EXPECT_CALL(callback, Run(ElementsAre(page2)));
  model()->GetPagesByURL(kTestUrl2, URLSearchMode::SEARCH_BY_FINAL_URL_ONLY,
                         callback.Get());
  PumpLoop();

  // Search by random url, which should return no pages.
  EXPECT_CALL(callback, Run(IsEmpty()));
  model()->GetPagesByURL(kOtherUrl, URLSearchMode::SEARCH_BY_FINAL_URL_ONLY,
                         callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());
  PumpLoop();
}

TEST_F(OfflinePageModelTaskifiedTest,
       GetPagesByUrl_FinalUrlWithFragmentStripped) {
  page_generator()->SetUrl(kTestUrl);
  OfflinePageItem page1 = page_generator()->CreateItem();
  InsertPageIntoStore(page1);
  page_generator()->SetUrl(kTestUrl2WithFragment);
  OfflinePageItem page2 = page_generator()->CreateItem();
  InsertPageIntoStore(page2);

  // Search by kTestUrlWithFragment.
  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(ElementsAre(page1)));
  model()->GetPagesByURL(kTestUrlWithFragment,
                         URLSearchMode::SEARCH_BY_FINAL_URL_ONLY,
                         callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());
  PumpLoop();

  // Search by kTestUrl2.
  EXPECT_CALL(callback, Run(ElementsAre(page2)));
  model()->GetPagesByURL(kTestUrl2, URLSearchMode::SEARCH_BY_FINAL_URL_ONLY,
                         callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());
  PumpLoop();

  // Search by kTestUrl2WithFragment.
  EXPECT_CALL(callback, Run(ElementsAre(page2)));
  model()->GetPagesByURL(kTestUrl2WithFragment,
                         URLSearchMode::SEARCH_BY_FINAL_URL_ONLY,
                         callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());
  PumpLoop();
}

TEST_F(OfflinePageModelTaskifiedTest, GetPagesByUrl_AllUrls) {
  page_generator()->SetUrl(kTestUrl);
  page_generator()->SetOriginalUrl(kTestUrl2);
  OfflinePageItem page1 = page_generator()->CreateItem();
  InsertPageIntoStore(page1);
  page_generator()->SetUrl(kTestUrl2);
  page_generator()->SetOriginalUrl(GURL());
  OfflinePageItem page2 = page_generator()->CreateItem();
  InsertPageIntoStore(page2);

  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(UnorderedElementsAre(page1, page2)));
  model()->GetPagesByURL(kTestUrl2, URLSearchMode::SEARCH_BY_ALL_URLS,
                         callback.Get());
  PumpLoop();
}

TEST_F(OfflinePageModelTaskifiedTest, GetOfflineIdsForClientId) {
  page_generator()->SetNamespace(kTestClientId1.name_space);
  page_generator()->SetId(kTestClientId1.id);
  OfflinePageItem page1 = page_generator()->CreateItem();
  OfflinePageItem page2 = page_generator()->CreateItem();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);

  base::MockCallback<MultipleOfflineIdCallback> callback;
  EXPECT_CALL(callback,
              Run(UnorderedElementsAre(page1.offline_id, page2.offline_id)));

  model()->GetOfflineIdsForClientId(kTestClientId1, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
}

TEST_F(OfflinePageModelTaskifiedTest, GetPagesByClientIds) {
  page_generator()->SetNamespace(kTestClientId1.name_space);
  page_generator()->SetId(kTestClientId1.id);
  OfflinePageItem page1 = page_generator()->CreateItem();
  OfflinePageItem page2 = page_generator()->CreateItem();
  page_generator()->SetNamespace(kTestUserRequestedClientId.name_space);
  OfflinePageItem page3 = page_generator()->CreateItem();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);
  InsertPageIntoStore(page3);

  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(UnorderedElementsAre(page1, page2)));

  model()->GetPagesByClientIds({kTestClientId1}, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
}

TEST_F(OfflinePageModelTaskifiedTest, GetPagesByRequestOrigin) {
  page_generator()->SetRequestOrigin(kTestRequestOrigin);
  OfflinePageItem page1 = page_generator()->CreateItem();
  page_generator()->SetRequestOrigin(kEmptyRequestOrigin);
  OfflinePageItem page2 = page_generator()->CreateItem();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);

  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(ElementsAre(page1)));

  model()->GetPagesByRequestOrigin(kTestRequestOrigin, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
}

TEST_F(OfflinePageModelTaskifiedTest, DeletePagesByClientIds) {
  page_generator()->SetNamespace(kTestClientId1.name_space);
  page_generator()->SetId(kTestClientId1.id);
  OfflinePageItem page1 = page_generator()->CreateItemWithTempFile();
  page_generator()->SetId(kTestClientId2.id);
  OfflinePageItem page2 = page_generator()->CreateItemWithTempFile();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);
  EXPECT_EQ(2UL, test_util::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(2LL, store_test_util()->GetPageCount());

  base::MockCallback<DeletePageCallback> callback;
  EXPECT_CALL(callback, Run(testing::A<DeletePageResult>()));
  CheckTaskQueueIdle();

  model()->DeletePagesByClientIds({page1.client_id}, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
  EXPECT_TRUE(observer_delete_page_called());
  EXPECT_EQ(last_deleted_page_info().client_id, page1.client_id);
  EXPECT_EQ(1UL, test_util::GetFileCountInDirectory(temporary_dir_path()));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  CheckTaskQueueIdle();
}

TEST_F(OfflinePageModelTaskifiedTest, GetPagesByNamespace) {
  page_generator()->SetNamespace(kDefaultNamespace);
  OfflinePageItem page1 = page_generator()->CreateItem();
  OfflinePageItem page2 = page_generator()->CreateItem();
  page_generator()->SetNamespace(kDownloadNamespace);
  OfflinePageItem page3 = page_generator()->CreateItem();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);
  InsertPageIntoStore(page3);

  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(UnorderedElementsAre(page1, page2)));

  model()->GetPagesByNamespace(kDefaultNamespace, callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
}

TEST_F(OfflinePageModelTaskifiedTest, GetPagesRemovedOnCacheReset) {
  page_generator()->SetNamespace(kDefaultNamespace);
  OfflinePageItem page1 = page_generator()->CreateItem();
  OfflinePageItem page2 = page_generator()->CreateItem();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);
  page_generator()->SetNamespace(kDownloadNamespace);
  OfflinePageItem page3 = page_generator()->CreateItem();
  InsertPageIntoStore(page3);

  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(UnorderedElementsAre(page1, page2)));

  model()->GetPagesRemovedOnCacheReset(callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
}

TEST_F(OfflinePageModelTaskifiedTest, GetPagesSupportedByDownloads) {
  page_generator()->SetNamespace(kDownloadNamespace);
  OfflinePageItem page1 = page_generator()->CreateItem();
  OfflinePageItem page2 = page_generator()->CreateItem();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);
  page_generator()->SetNamespace(kDefaultNamespace);
  OfflinePageItem page3 = page_generator()->CreateItem();
  InsertPageIntoStore(page3);

  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(UnorderedElementsAre(page1, page2)));

  model()->GetPagesSupportedByDownloads(callback.Get());
  EXPECT_TRUE(task_queue()->HasRunningTask());

  PumpLoop();
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
  // Save a temporary page.
  auto archiver = BuildArchiver(kTestUrl, ArchiverResult::SUCCESSFULLY_CREATED);
  int64_t temporary_id = SavePageWithExpectedResult(
      kTestUrl, kTestClientId1, GURL(), kEmptyRequestOrigin,
      std::move(archiver), SavePageResult::SUCCESS);

  // Save a persistent page.
  archiver = BuildArchiver(kTestUrl2, ArchiverResult::SUCCESSFULLY_CREATED);
  int64_t persistent_id = SavePageWithExpectedResult(
      kTestUrl2, kTestUserRequestedClientId, GURL(), kEmptyRequestOrigin,
      std::move(archiver), SavePageResult::SUCCESS);

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
  page_generator()->SetArchiveDirectory(temporary_dir_path());
  page_generator()->SetNamespace(kDefaultNamespace);
  page_generator()->SetUrl(kTestUrl);
  OfflinePageItem page1 = page_generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = page_generator()->CreateItemWithTempFile();
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
  page_generator()->SetNamespace(kDefaultNamespace);
  page_generator()->SetUrl(kTestUrl);
  OfflinePageItem page1 = page_generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = page_generator()->CreateItemWithTempFile();
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

TEST_F(OfflinePageModelTaskifiedTest, GetAllPages) {
  OfflinePageItem page1 = page_generator()->CreateItem();
  OfflinePageItem page2 = page_generator()->CreateItem();
  InsertPageIntoStore(page1);
  InsertPageIntoStore(page2);

  base::MockCallback<MultipleOfflinePageItemCallback> callback;
  EXPECT_CALL(callback, Run(UnorderedElementsAre(page1, page2)));
  model()->GetAllPages(callback.Get());
  PumpLoop();
}

}  // namespace offline_pages
