// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_model.h"

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_test_archiver.h"
#include "components/offline_pages/offline_page_test_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using SavePageResult = offline_pages::OfflinePageModel::SavePageResult;
using DeletePageResult = offline_pages::OfflinePageModel::DeletePageResult;

namespace offline_pages {

namespace {
const GURL kTestUrl("http://example.com");
const int64 kTestPageBookmarkId1 = 1234LL;
const GURL kTestUrl2("http://other.page.com");
const GURL kTestUrl3("http://test.xyz");
const GURL kFileUrl("file:///foo");
const int64 kTestPageBookmarkId2 = 5678LL;
const int64 kTestPageBookmarkId3 = 42LL;
const int64 kTestFileSize = 876543LL;

}  // namespace

class OfflinePageModelTest
    : public testing::Test,
      public OfflinePageModel::Observer,
      public OfflinePageTestArchiver::Observer,
      public base::SupportsWeakPtr<OfflinePageModelTest> {
 public:
  OfflinePageModelTest();
  ~OfflinePageModelTest() override;

  void SetUp() override;
  void TearDown() override;

  // OfflinePageModel::Observer implementation.
  void OfflinePageModelLoaded(OfflinePageModel* model) override;
  void OfflinePageModelChanged(OfflinePageModel* model) override;
  void OfflinePageDeleted(int64 bookmark_id) override;

  // OfflinePageTestArchiver::Observer implementation.
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override;

  // OfflinePageModel callbacks.
  void OnSavePageDone(SavePageResult result);
  void OnDeletePageDone(DeletePageResult result);
  void OnClearAllDone();

  // OfflinePageMetadataStore callbacks.
  void OnStoreUpdateDone(bool /* success */);

  scoped_ptr<OfflinePageTestArchiver> BuildArchiver(
      const GURL& url,
      OfflinePageArchiver::ArchiverResult result);
  scoped_ptr<OfflinePageMetadataStore> BuildStore();
  scoped_ptr<OfflinePageModel> BuildModel(
      scoped_ptr<OfflinePageMetadataStore> store);
  void ResetModel();

  // Utility methods.
  // Runs until all of the tasks that are not delayed are gone from the task
  // queue.
  void PumpLoop();
  // Fast-forwards virtual time by |delta|, causing tasks with a remaining
  // delay less than or equal to |delta| to be executed.
  void FastForwardBy(base::TimeDelta delta);
  void ResetResults();

  OfflinePageModel* model() { return model_.get(); }

  OfflinePageTestStore* GetStore();

  SavePageResult last_save_result() const {
    return last_save_result_;
  }

  DeletePageResult last_delete_result() const {
    return last_delete_result_;
  }

  int64 last_deleted_bookmark_id() const {
    return last_deleted_bookmark_id_;
  }

  const base::FilePath& last_archiver_path() { return last_archiver_path_; }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  base::ScopedTempDir temp_dir_;

  scoped_ptr<OfflinePageModel> model_;
  SavePageResult last_save_result_;
  DeletePageResult last_delete_result_;
  base::FilePath last_archiver_path_;
  int64 last_deleted_bookmark_id_;
};

OfflinePageModelTest::OfflinePageModelTest()
    : task_runner_(new base::TestMockTimeTaskRunner),
      task_runner_handle_(task_runner_),
      last_save_result_(SavePageResult::CANCELLED),
      last_delete_result_(DeletePageResult::CANCELLED),
      last_deleted_bookmark_id_(-1) {
}

OfflinePageModelTest::~OfflinePageModelTest() {
}

void OfflinePageModelTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  model_ = BuildModel(BuildStore().Pass()).Pass();
  model_->AddObserver(this);
  PumpLoop();
}

void OfflinePageModelTest::TearDown() {
  model_->RemoveObserver(this);
  PumpLoop();
}

void OfflinePageModelTest::OfflinePageModelLoaded(OfflinePageModel* model) {
  ASSERT_EQ(model_.get(), model);
}

void OfflinePageModelTest::OfflinePageModelChanged(OfflinePageModel* model) {
  ASSERT_EQ(model_.get(), model);
}

void OfflinePageModelTest::OfflinePageDeleted(int64 bookmark_id) {
  last_deleted_bookmark_id_ = bookmark_id;
}

void OfflinePageModelTest::SetLastPathCreatedByArchiver(
    const base::FilePath& file_path) {
  last_archiver_path_ = file_path;
}

void OfflinePageModelTest::OnSavePageDone(
    OfflinePageModel::SavePageResult result) {
  last_save_result_ = result;
}

void OfflinePageModelTest::OnDeletePageDone(DeletePageResult result) {
  last_delete_result_ = result;
}

void OfflinePageModelTest::OnClearAllDone() {
  PumpLoop();
}

void OfflinePageModelTest::OnStoreUpdateDone(bool /* success - ignored */) {
}

scoped_ptr<OfflinePageTestArchiver> OfflinePageModelTest::BuildArchiver(
    const GURL& url,
    OfflinePageArchiver::ArchiverResult result) {
  return scoped_ptr<OfflinePageTestArchiver>(new OfflinePageTestArchiver(
      this, url, result, kTestFileSize, base::ThreadTaskRunnerHandle::Get()));
}

scoped_ptr<OfflinePageMetadataStore> OfflinePageModelTest::BuildStore() {
  return scoped_ptr<OfflinePageMetadataStore>(
      new OfflinePageTestStore(base::ThreadTaskRunnerHandle::Get()));
}

scoped_ptr<OfflinePageModel> OfflinePageModelTest::BuildModel(
    scoped_ptr<OfflinePageMetadataStore> store) {
  return scoped_ptr<OfflinePageModel>(new OfflinePageModel(
      store.Pass(), temp_dir_.path(), base::ThreadTaskRunnerHandle::Get()));
}

void OfflinePageModelTest::ResetModel() {
  model_->RemoveObserver(this);
  OfflinePageTestStore* old_store = GetStore();
  scoped_ptr<OfflinePageMetadataStore> new_store(
      new OfflinePageTestStore(*old_store));
  model_ = BuildModel(new_store.Pass()).Pass();
  model_->AddObserver(this);
  PumpLoop();
}

void OfflinePageModelTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void OfflinePageModelTest::FastForwardBy(base::TimeDelta delta) {
  task_runner_->FastForwardBy(delta);
}

void OfflinePageModelTest::ResetResults() {
  last_save_result_ = SavePageResult::CANCELLED;
  last_delete_result_ = DeletePageResult::CANCELLED;
  last_archiver_path_.clear();
}

OfflinePageTestStore* OfflinePageModelTest::GetStore() {
  return static_cast<OfflinePageTestStore*>(model()->GetStoreForTesting());
}

TEST_F(OfflinePageModelTest, SavePageSuccessful) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  EXPECT_FALSE(model()->HasOfflinePages());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_TRUE(model()->HasOfflinePages());

  OfflinePageTestStore* store = GetStore();
  EXPECT_EQ(kTestUrl, store->last_saved_page().url);
  EXPECT_EQ(kTestPageBookmarkId1, store->last_saved_page().bookmark_id);
  // Save last_archiver_path since it will be referred to later.
  base::FilePath archiver_path = last_archiver_path();
  EXPECT_EQ(archiver_path, store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  ResetResults();

  const std::vector<OfflinePageItem>& offline_pages = model()->GetAllPages();

  EXPECT_EQ(1UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  EXPECT_EQ(kTestPageBookmarkId1, offline_pages[0].bookmark_id);
  EXPECT_EQ(archiver_path, offline_pages[0].file_path);
  EXPECT_EQ(kTestFileSize, offline_pages[0].file_size);
  EXPECT_EQ(0, offline_pages[0].access_count);
  EXPECT_EQ(0, offline_pages[0].flags);
}

TEST_F(OfflinePageModelTest, SavePageOfflineArchiverCancelled) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::ERROR_CANCELED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::CANCELLED, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineArchiverDeviceFull) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::ERROR_DEVICE_FULL)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::DEVICE_FULL, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineArchiverContentUnavailable) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(
          kTestUrl,
          OfflinePageArchiver::ArchiverResult::ERROR_CONTENT_UNAVAILABLE)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::CONTENT_UNAVAILABLE, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineCreationFailed) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(
          kTestUrl,
          OfflinePageArchiver::ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::ARCHIVE_CREATION_FAILED, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineArchiverReturnedWrongUrl) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(GURL("http://other.random.url.com"),
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::ARCHIVE_CREATION_FAILED, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineCreationStoreWriteFailure) {
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::WRITE_FAILED);
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::STORE_FAILURE, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageLocalFileFailed) {
  model()->SavePage(
      kFileUrl, kTestPageBookmarkId1, scoped_ptr<OfflinePageTestArchiver>(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::SKIPPED, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineArchiverTwoPages) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  // archiver_ptr will be valid until after first PumpLoop() call after
  // CompleteCreateArchive() is called.
  OfflinePageTestArchiver* archiver_ptr = archiver.get();
  archiver_ptr->set_delayed(true);
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  EXPECT_TRUE(archiver_ptr->create_archive_called());

  // Request to save another page.
  scoped_ptr<OfflinePageTestArchiver> archiver2(
      BuildArchiver(kTestUrl2,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl2, kTestPageBookmarkId2, archiver2.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  OfflinePageTestStore* store = GetStore();

  EXPECT_EQ(kTestUrl2, store->last_saved_page().url);
  EXPECT_EQ(kTestPageBookmarkId2, store->last_saved_page().bookmark_id);
  base::FilePath archiver_path2 = last_archiver_path();
  EXPECT_EQ(archiver_path2, store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());

  ResetResults();

  archiver_ptr->CompleteCreateArchive();
  // After this pump loop archiver_ptr is invalid.
  PumpLoop();

  EXPECT_EQ(kTestUrl, store->last_saved_page().url);
  EXPECT_EQ(kTestPageBookmarkId1, store->last_saved_page().bookmark_id);
  base::FilePath archiver_path = last_archiver_path();
  EXPECT_EQ(archiver_path, store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());

  ResetResults();

  const std::vector<OfflinePageItem>& offline_pages = model()->GetAllPages();

  EXPECT_EQ(2UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  EXPECT_EQ(kTestPageBookmarkId1, offline_pages[0].bookmark_id);
  EXPECT_EQ(archiver_path, offline_pages[0].file_path);
  EXPECT_EQ(kTestFileSize, offline_pages[0].file_size);
  EXPECT_EQ(0, offline_pages[0].access_count);
  EXPECT_EQ(0, offline_pages[0].flags);
  EXPECT_EQ(kTestUrl2, offline_pages[1].url);
  EXPECT_EQ(kTestPageBookmarkId2, offline_pages[1].bookmark_id);
  EXPECT_EQ(archiver_path2, offline_pages[1].file_path);
  EXPECT_EQ(kTestFileSize, offline_pages[1].file_size);
  EXPECT_EQ(0, offline_pages[1].access_count);
  EXPECT_EQ(0, offline_pages[1].flags);
}

TEST_F(OfflinePageModelTest, MarkPageAccessed) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  // This will increase access_count by one.
  model()->MarkPageAccessed(kTestPageBookmarkId1);
  PumpLoop();

  const std::vector<OfflinePageItem>& offline_pages = model()->GetAllPages();

  EXPECT_EQ(1UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  EXPECT_EQ(kTestPageBookmarkId1, offline_pages[0].bookmark_id);
  EXPECT_EQ(kTestFileSize, offline_pages[0].file_size);
  EXPECT_EQ(1, offline_pages[0].access_count);
}

TEST_F(OfflinePageModelTest, MarkPageForDeletion) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  GURL offline_url = model()->GetAllPages().begin()->GetOfflineURL();

  // Delete the page with undo tiggerred.
  model()->MarkPageForDeletion(
      kTestPageBookmarkId1,
      base::Bind(&OfflinePageModelTest::OnDeletePageDone, AsWeakPtr()));
  PumpLoop();

  // GetAllPages will not return the page that is marked for deletion.
  const std::vector<OfflinePageItem>& offline_pages = model()->GetAllPages();
  EXPECT_EQ(0UL, offline_pages.size());

  EXPECT_FALSE(model()->HasOfflinePages());
  EXPECT_EQ(nullptr, model()->GetPageByOnlineURL(kTestUrl));
  EXPECT_EQ(nullptr, model()->GetPageByBookmarkId(kTestPageBookmarkId1));
  EXPECT_EQ(nullptr, model()->GetPageByOfflineURL(offline_url));

  // Undo the deletion.
  model()->UndoPageDeletion(kTestPageBookmarkId1);
  PumpLoop();

  // GetAllPages will now return the restored page.
  const std::vector<OfflinePageItem>& offline_pages_after_undo =
      model()->GetAllPages();
  EXPECT_EQ(1UL, offline_pages_after_undo.size());
}

TEST_F(OfflinePageModelTest, FinalizePageDeletion) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  // Mark the page for deletion.
  model()->MarkPageForDeletion(
      kTestPageBookmarkId1,
      base::Bind(&OfflinePageModelTest::OnDeletePageDone, AsWeakPtr()));
  PumpLoop();

  EXPECT_EQ(1UL, GetStore()->GetAllPages().size());

  // Fast forward to trigger the page deletion.
  FastForwardBy(OfflinePageModel::GetFinalDeletionDelayForTesting());

  EXPECT_EQ(0UL, GetStore()->GetAllPages().size());
}

TEST_F(OfflinePageModelTest, GetAllPagesStoreEmpty) {
  const std::vector<OfflinePageItem>& offline_pages = model()->GetAllPages();

  EXPECT_EQ(0UL, offline_pages.size());
}

TEST_F(OfflinePageModelTest, GetAllPagesStoreFailure) {
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::LOAD_FAILED);
  const std::vector<OfflinePageItem>& offline_pages = model()->GetAllPages();

  EXPECT_EQ(0UL, offline_pages.size());
}

TEST_F(OfflinePageModelTest, DeletePageSuccessful) {
  OfflinePageTestStore* store = GetStore();

  // Save one page.
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(1u, store->GetAllPages().size());

  ResetResults();

  // Save another page.
  scoped_ptr<OfflinePageTestArchiver> archiver2(
      BuildArchiver(kTestUrl2,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl2, kTestPageBookmarkId2, archiver2.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(2u, store->GetAllPages().size());

  ResetResults();

  // Delete one page.
  model()->DeletePageByBookmarkId(
      kTestPageBookmarkId1, base::Bind(&OfflinePageModelTest::OnDeletePageDone,
                                       AsWeakPtr()));

  PumpLoop();

  EXPECT_EQ(last_deleted_bookmark_id(), kTestPageBookmarkId1);
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  ASSERT_EQ(1u, store->GetAllPages().size());
  EXPECT_EQ(kTestUrl2, store->GetAllPages()[0].url);

  // Delete another page.
  model()->DeletePageByBookmarkId(
      kTestPageBookmarkId2, base::Bind(&OfflinePageModelTest::OnDeletePageDone,
                                       AsWeakPtr()));

  ResetResults();

  PumpLoop();

  EXPECT_EQ(last_deleted_bookmark_id(), kTestPageBookmarkId2);
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  EXPECT_EQ(0u, store->GetAllPages().size());
}

TEST_F(OfflinePageModelTest, DeletePageNotFound) {
  model()->DeletePageByBookmarkId(
      kTestPageBookmarkId1, base::Bind(&OfflinePageModelTest::OnDeletePageDone,
                                       AsWeakPtr()));
  EXPECT_EQ(DeletePageResult::NOT_FOUND, last_delete_result());
}

TEST_F(OfflinePageModelTest, DeletePageStoreFailureOnRemove) {
  // Save a page.
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  ResetResults();

  // Try to delete this page.
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::REMOVE_FAILED);
  model()->DeletePageByBookmarkId(
      kTestPageBookmarkId1, base::Bind(&OfflinePageModelTest::OnDeletePageDone,
                                       AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(DeletePageResult::STORE_FAILURE, last_delete_result());
}

TEST_F(OfflinePageModelTest, DetectThatOfflineCopyIsMissing) {
  // Save a page.
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  ResetResults();

  const OfflinePageItem* page =
      model()->GetPageByBookmarkId(kTestPageBookmarkId1);
  // Delete the offline copy of the page and check the metadata.
  base::DeleteFile(page->file_path, false);
  model()->CheckForExternalFileDeletion();
  PumpLoop();

  EXPECT_EQ(last_deleted_bookmark_id(), kTestPageBookmarkId1);
  EXPECT_EQ(0UL, model()->GetAllPages().size());
}

TEST_F(OfflinePageModelTest, DetectThatOfflineCopyIsMissingAfterLoad) {
  // Save a page.
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  ResetResults();

  const OfflinePageItem* page =
      model()->GetPageByBookmarkId(kTestPageBookmarkId1);
  // Delete the offline copy of the page and check the metadata.
  base::DeleteFile(page->file_path, false);
  // Reseting the model should trigger the metadata consistency check as well.
  ResetModel();
  PumpLoop();

  EXPECT_EQ(last_deleted_bookmark_id(), kTestPageBookmarkId1);
  EXPECT_EQ(0UL, model()->GetAllPages().size());
}

TEST_F(OfflinePageModelTest, DeleteMultiplePages) {
  OfflinePageTestStore* store = GetStore();

  // Save 3 pages.
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  scoped_ptr<OfflinePageTestArchiver> archiver2(
      BuildArchiver(kTestUrl2,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl2, kTestPageBookmarkId2, archiver2.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  scoped_ptr<OfflinePageTestArchiver> archiver3(
      BuildArchiver(kTestUrl3,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl3, kTestPageBookmarkId3, archiver3.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  EXPECT_EQ(3u, store->GetAllPages().size());

  // Delete multiple pages.
  std::vector<int64> ids_to_delete;
  ids_to_delete.push_back(kTestPageBookmarkId2);
  ids_to_delete.push_back(kTestPageBookmarkId1);
  ids_to_delete.push_back(23434LL);  // Non-existent ID.
  model()->DeletePagesByBookmarkId(
      ids_to_delete, base::Bind(&OfflinePageModelTest::OnDeletePageDone,
                                AsWeakPtr()));
  PumpLoop();

  // Success is expected if at least one page is deleted successfully.
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  EXPECT_EQ(1u, store->GetAllPages().size());
}

TEST_F(OfflinePageModelTest, GetPageByBookmarkId) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  scoped_ptr<OfflinePageTestArchiver> archiver2(
      BuildArchiver(kTestUrl2,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl2, kTestPageBookmarkId2, archiver2.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  const OfflinePageItem* page =
      model()->GetPageByBookmarkId(kTestPageBookmarkId1);
  ASSERT_TRUE(page);
  EXPECT_EQ(kTestUrl, page->url);
  EXPECT_EQ(kTestPageBookmarkId1, page->bookmark_id);
  EXPECT_EQ(kTestFileSize, page->file_size);

  page = model()->GetPageByBookmarkId(kTestPageBookmarkId2);
  ASSERT_TRUE(page);
  EXPECT_EQ(kTestUrl2, page->url);
  EXPECT_EQ(kTestPageBookmarkId2, page->bookmark_id);
  EXPECT_EQ(kTestFileSize, page->file_size);

  page = model()->GetPageByBookmarkId(-42);
  EXPECT_FALSE(page);
}

TEST_F(OfflinePageModelTest, GetPageByOfflineURL) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  OfflinePageTestStore* store = GetStore();
  GURL offline_url = store->last_saved_page().GetOfflineURL();

  scoped_ptr<OfflinePageTestArchiver> archiver2(
      BuildArchiver(kTestUrl2,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl2, kTestPageBookmarkId2, archiver2.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  GURL offline_url2 = store->last_saved_page().GetOfflineURL();

  const OfflinePageItem* page = model()->GetPageByOfflineURL(offline_url2);
  EXPECT_TRUE(page);
  EXPECT_EQ(kTestUrl2, page->url);
  EXPECT_EQ(kTestPageBookmarkId2, page->bookmark_id);

  page = model()->GetPageByOfflineURL(offline_url);
  EXPECT_TRUE(page);
  EXPECT_EQ(kTestUrl, page->url);
  EXPECT_EQ(kTestPageBookmarkId1, page->bookmark_id);

  page = model()->GetPageByOfflineURL(GURL("http://foo"));
  EXPECT_FALSE(page);
}

TEST_F(OfflinePageModelTest, GetPageByOnlineURL) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  scoped_ptr<OfflinePageTestArchiver> archiver2(
      BuildArchiver(kTestUrl2,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl2, kTestPageBookmarkId2, archiver2.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  const OfflinePageItem* page = model()->GetPageByOnlineURL(kTestUrl2);
  EXPECT_TRUE(page);
  EXPECT_EQ(kTestUrl2, page->url);
  EXPECT_EQ(kTestPageBookmarkId2, page->bookmark_id);

  page = model()->GetPageByOnlineURL(kTestUrl);
  EXPECT_TRUE(page);
  EXPECT_EQ(kTestUrl, page->url);
  EXPECT_EQ(kTestPageBookmarkId1, page->bookmark_id);

  page = model()->GetPageByOnlineURL(GURL("http://foo"));
  EXPECT_FALSE(page);
}

// Test that model returns pages that are older than 30 days as candidates for
// clean up, hence the numbers in time delta.
TEST_F(OfflinePageModelTest, GetPagesToCleanUp) {
  base::Time now = base::Time::Now();

  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  GetStore()->UpdateLastAccessTime(kTestPageBookmarkId1,
                                   now - base::TimeDelta::FromDays(40));

  scoped_ptr<OfflinePageTestArchiver> archiver2(
      BuildArchiver(kTestUrl2,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl2, kTestPageBookmarkId2, archiver2.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  GetStore()->UpdateLastAccessTime(kTestPageBookmarkId2,
                                   now - base::TimeDelta::FromDays(31));


  scoped_ptr<OfflinePageTestArchiver> archiver3(
      BuildArchiver(kTestUrl3,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl3, kTestPageBookmarkId3, archiver3.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  GetStore()->UpdateLastAccessTime(kTestPageBookmarkId3,
                                   now - base::TimeDelta::FromDays(29));

  ResetModel();

  // Only page_1 and page_2 are expected to be picked up by the model as page_3
  // has not been in the store long enough.
  std::vector<OfflinePageItem> pages_to_clean_up = model()->GetPagesToCleanUp();
  EXPECT_EQ(2UL, pages_to_clean_up.size());
  EXPECT_EQ(kTestUrl, pages_to_clean_up[0].url);
  EXPECT_EQ(kTestPageBookmarkId1, pages_to_clean_up[0].bookmark_id);
  EXPECT_EQ(kTestUrl2, pages_to_clean_up[1].url);
  EXPECT_EQ(kTestPageBookmarkId2, pages_to_clean_up[1].bookmark_id);
}

TEST_F(OfflinePageModelTest, CanSavePage) {
  EXPECT_TRUE(OfflinePageModel::CanSavePage(GURL("http://foo")));
  EXPECT_TRUE(OfflinePageModel::CanSavePage(GURL("https://foo")));
  EXPECT_FALSE(OfflinePageModel::CanSavePage(GURL("file:///foo")));
  EXPECT_FALSE(OfflinePageModel::CanSavePage(GURL("data:image/png;base64,ab")));
  EXPECT_FALSE(OfflinePageModel::CanSavePage(GURL("chrome://version")));
  EXPECT_FALSE(OfflinePageModel::CanSavePage(GURL("chrome-native://newtab/")));
}

TEST_F(OfflinePageModelTest, ClearAll) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  scoped_ptr<OfflinePageTestArchiver> archiver2(
      BuildArchiver(kTestUrl2,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl2, kTestPageBookmarkId2, archiver2.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  const std::vector<OfflinePageItem>& offline_pages = model()->GetAllPages();
  EXPECT_EQ(2UL, offline_pages.size());
  EXPECT_EQ(2UL, GetStore()->GetAllPages().size());
  base::FilePath archiver_path = offline_pages[0].file_path;
  EXPECT_TRUE(base::PathExists(archiver_path));

  // ClearAll should delete all the files and wipe out both cache and store.
  model()->ClearAll(
      base::Bind(&OfflinePageModelTest::OnClearAllDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(0UL, model()->GetAllPages().size());
  EXPECT_EQ(0UL, GetStore()->GetAllPages().size());
  EXPECT_FALSE(base::PathExists(archiver_path));

  // The model should reload the store after the reset. All model operations
  // should continue to work.
  scoped_ptr<OfflinePageTestArchiver> archiver3(
      BuildArchiver(kTestUrl2,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl2, kTestPageBookmarkId2, archiver3.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(1UL, model()->GetAllPages().size());
  EXPECT_EQ(1UL, GetStore()->GetAllPages().size());
}

TEST_F(OfflinePageModelTest, BookmarkNodeChangesUrl) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(kTestUrl,
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED)
          .Pass());
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  EXPECT_EQ(1UL, model()->GetAllPages().size());

  bookmarks::BookmarkNode bookmark_node(kTestPageBookmarkId1, kTestUrl2);
  model()->BookmarkNodeChanged(nullptr, &bookmark_node);
  PumpLoop();

  // Offline copy should be removed. Chrome should not crash.
  // http://crbug.com/558929
  EXPECT_EQ(0UL, model()->GetAllPages().size());

  // Chrome should not crash when a bookmark with no offline copy is changed.
  // http://crbug.com/560518
  bookmark_node.set_url(kTestUrl);
  model()->BookmarkNodeChanged(nullptr, &bookmark_node);
  PumpLoop();
  EXPECT_EQ(0UL, model()->GetAllPages().size());
}

}  // namespace offline_pages
