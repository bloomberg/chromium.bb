// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_model.h"

#include <stdint.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_storage.h"
#include "components/bookmarks/browser/bookmark_undo_delegate.h"
#include "components/bookmarks/browser/bookmark_undo_provider.h"
#include "components/bookmarks/test/test_bookmark_client.h"
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
const int64_t kTestPageBookmarkId1 = 1234LL;
const GURL kTestUrl2("http://other.page.com");
const GURL kTestUrl3("http://test.xyz");
const GURL kFileUrl("file:///foo");
const int64_t kTestPageBookmarkId2 = 5678LL;
const int64_t kTestPageBookmarkId3 = 42LL;
const int64_t kTestFileSize = 876543LL;

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
  void OfflinePageDeleted(int64_t bookmark_id) override;

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

  OfflinePageTestStore* GetStore();

  void SavePage(const GURL& url, int64_t bookmark_id);
  void SavePageWithArchiverResult(const GURL& url,
                                  int64_t bookmark_id,
                                  OfflinePageArchiver::ArchiverResult result);

  OfflinePageModel* model() { return model_.get(); }

  SavePageResult last_save_result() const {
    return last_save_result_;
  }

  DeletePageResult last_delete_result() const {
    return last_delete_result_;
  }

  int64_t last_deleted_bookmark_id() const { return last_deleted_bookmark_id_; }

  const base::FilePath& last_archiver_path() { return last_archiver_path_; }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  base::ScopedTempDir temp_dir_;

  scoped_ptr<OfflinePageModel> model_;
  SavePageResult last_save_result_;
  DeletePageResult last_delete_result_;
  base::FilePath last_archiver_path_;
  int64_t last_deleted_bookmark_id_;
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
  model_ = BuildModel(BuildStore());
  model_->AddObserver(this);
  PumpLoop();
}

void OfflinePageModelTest::TearDown() {
  model_->RemoveObserver(this);
  model_.reset();
  PumpLoop();
}

void OfflinePageModelTest::OfflinePageModelLoaded(OfflinePageModel* model) {
  ASSERT_EQ(model_.get(), model);
}

void OfflinePageModelTest::OfflinePageModelChanged(OfflinePageModel* model) {
  ASSERT_EQ(model_.get(), model);
}

void OfflinePageModelTest::OfflinePageDeleted(int64_t bookmark_id) {
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
      std::move(store), temp_dir_.path(), base::ThreadTaskRunnerHandle::Get()));
}

void OfflinePageModelTest::ResetModel() {
  model_->RemoveObserver(this);
  OfflinePageTestStore* old_store = GetStore();
  scoped_ptr<OfflinePageMetadataStore> new_store(
      new OfflinePageTestStore(*old_store));
  model_ = BuildModel(std::move(new_store));
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

void OfflinePageModelTest::SavePage(const GURL& url, int64_t bookmark_id) {
  SavePageWithArchiverResult(
      url,
      bookmark_id,
      OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED);
}

void OfflinePageModelTest::SavePageWithArchiverResult(
    const GURL& url,
    int64_t bookmark_id,
    OfflinePageArchiver::ArchiverResult result) {
  scoped_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(url, result));
  model()->SavePage(
      url, bookmark_id, std::move(archiver),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
}

TEST_F(OfflinePageModelTest, SavePageSuccessful) {
  EXPECT_FALSE(model()->HasOfflinePages());
  SavePage(kTestUrl, kTestPageBookmarkId1);
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
  SavePageWithArchiverResult(
      kTestUrl,
      kTestPageBookmarkId1,
      OfflinePageArchiver::ArchiverResult::ERROR_CANCELED);
  EXPECT_EQ(SavePageResult::CANCELLED, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineArchiverDeviceFull) {
  SavePageWithArchiverResult(
      kTestUrl,
      kTestPageBookmarkId1,
      OfflinePageArchiver::ArchiverResult::ERROR_DEVICE_FULL);
  EXPECT_EQ(SavePageResult::DEVICE_FULL, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineArchiverContentUnavailable) {
  SavePageWithArchiverResult(
      kTestUrl,
      kTestPageBookmarkId1,
      OfflinePageArchiver::ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
  EXPECT_EQ(SavePageResult::CONTENT_UNAVAILABLE, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineCreationFailed) {
  SavePageWithArchiverResult(
      kTestUrl,
      kTestPageBookmarkId1,
      OfflinePageArchiver::ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED);
  EXPECT_EQ(SavePageResult::ARCHIVE_CREATION_FAILED, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineArchiverReturnedWrongUrl) {
  scoped_ptr<OfflinePageTestArchiver> archiver(
      BuildArchiver(GURL("http://other.random.url.com"),
                    OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, std::move(archiver),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::ARCHIVE_CREATION_FAILED, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineCreationStoreWriteFailure) {
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::WRITE_FAILED);
  SavePage(kTestUrl, kTestPageBookmarkId1);
  EXPECT_EQ(SavePageResult::STORE_FAILURE, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageLocalFileFailed) {
  // Don't create archiver since it will not be needed for pages that are not
  // going to be saved.
  model()->SavePage(
      kFileUrl, kTestPageBookmarkId1, scoped_ptr<OfflinePageTestArchiver>(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::SKIPPED, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineArchiverTwoPages) {
  scoped_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  // archiver_ptr will be valid until after first PumpLoop() call after
  // CompleteCreateArchive() is called.
  OfflinePageTestArchiver* archiver_ptr = archiver.get();
  archiver_ptr->set_delayed(true);
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, std::move(archiver),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  EXPECT_TRUE(archiver_ptr->create_archive_called());

  // Request to save another page.
  SavePage(kTestUrl2, kTestPageBookmarkId2);

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
  SavePage(kTestUrl, kTestPageBookmarkId1);

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
  SavePage(kTestUrl, kTestPageBookmarkId1);

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
  scoped_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, std::move(archiver),
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
  SavePage(kTestUrl, kTestPageBookmarkId1);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(1u, store->GetAllPages().size());

  ResetResults();

  // Save another page.
  SavePage(kTestUrl2, kTestPageBookmarkId2);
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
  SavePage(kTestUrl, kTestPageBookmarkId1);

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
  SavePage(kTestUrl, kTestPageBookmarkId1);

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
  SavePage(kTestUrl, kTestPageBookmarkId1);

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
  scoped_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, std::move(archiver),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  scoped_ptr<OfflinePageTestArchiver> archiver2(BuildArchiver(
      kTestUrl2, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  model()->SavePage(
      kTestUrl2, kTestPageBookmarkId2, std::move(archiver2),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  scoped_ptr<OfflinePageTestArchiver> archiver3(BuildArchiver(
      kTestUrl3, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  model()->SavePage(
      kTestUrl3, kTestPageBookmarkId3, std::move(archiver3),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  EXPECT_EQ(3u, store->GetAllPages().size());

  // Delete multiple pages.
  std::vector<int64_t> ids_to_delete;
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
  SavePage(kTestUrl, kTestPageBookmarkId1);
  SavePage(kTestUrl2, kTestPageBookmarkId2);

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
  SavePage(kTestUrl, kTestPageBookmarkId1);

  OfflinePageTestStore* store = GetStore();
  GURL offline_url = store->last_saved_page().GetOfflineURL();

  SavePage(kTestUrl2, kTestPageBookmarkId2);

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
  SavePage(kTestUrl, kTestPageBookmarkId1);
  SavePage(kTestUrl2, kTestPageBookmarkId2);

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

  SavePage(kTestUrl, kTestPageBookmarkId1);
  GetStore()->UpdateLastAccessTime(kTestPageBookmarkId1,
                                   now - base::TimeDelta::FromDays(40));

  SavePage(kTestUrl2, kTestPageBookmarkId2);
  GetStore()->UpdateLastAccessTime(kTestPageBookmarkId2,
                                   now - base::TimeDelta::FromDays(31));

  SavePage(kTestUrl3, kTestPageBookmarkId3);
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
  SavePage(kTestUrl, kTestPageBookmarkId1);
  SavePage(kTestUrl2, kTestPageBookmarkId2);

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
  SavePage(kTestUrl2, kTestPageBookmarkId2);
  EXPECT_EQ(1UL, model()->GetAllPages().size());
  EXPECT_EQ(1UL, GetStore()->GetAllPages().size());
}

class OfflinePageModelBookmarkChangeTest :
    public OfflinePageModelTest,
    public bookmarks::BookmarkUndoDelegate {
 public:
  OfflinePageModelBookmarkChangeTest();
  ~OfflinePageModelBookmarkChangeTest() override;

  void SetUp() override;
  void TearDown() override;

  // bookmarks::BookmarkUndoDelegate implementation.
  void SetUndoProvider(bookmarks::BookmarkUndoProvider* provider) override;
  void OnBookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                             const bookmarks::BookmarkNode* parent,
                             int index,
                             scoped_ptr<bookmarks::BookmarkNode> node) override;

  const bookmarks::BookmarkNode* CreateBookmarkNode(const GURL& url);
  void UndoBookmarkRemoval();

  bookmarks::BookmarkModel* bookmark_model() const {
    return bookmark_model_.get();
  }

 private:
  scoped_ptr<bookmarks::BookmarkModel> bookmark_model_;
  bookmarks::BookmarkUndoProvider* bookmark_undo_provider_;
  const bookmarks::BookmarkNode* removed_bookmark_parent_;
  int removed_bookmark_index_;
  scoped_ptr<bookmarks::BookmarkNode> removed_bookmark_node_;
};

OfflinePageModelBookmarkChangeTest::OfflinePageModelBookmarkChangeTest()
    : OfflinePageModelTest(),
      bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()),
      bookmark_undo_provider_(nullptr),
      removed_bookmark_parent_(nullptr),
      removed_bookmark_index_(-1) {}

OfflinePageModelBookmarkChangeTest::~OfflinePageModelBookmarkChangeTest() {
}

void OfflinePageModelBookmarkChangeTest::SetUp() {
  OfflinePageModelTest::SetUp();
  model()->Start(bookmark_model_.get());
}

void OfflinePageModelBookmarkChangeTest::TearDown() {
  OfflinePageModelTest::TearDown();
  bookmark_model_.reset();
}

void OfflinePageModelBookmarkChangeTest::SetUndoProvider(
    bookmarks::BookmarkUndoProvider* provider) {
  bookmark_undo_provider_ = provider;
}

void OfflinePageModelBookmarkChangeTest::OnBookmarkNodeRemoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    int index,
    scoped_ptr<bookmarks::BookmarkNode> node) {
  removed_bookmark_parent_ = parent;
  removed_bookmark_index_ = index;
  removed_bookmark_node_ = std::move(node);
}

const bookmarks::BookmarkNode*
OfflinePageModelBookmarkChangeTest::CreateBookmarkNode(const GURL& url) {
  const bookmarks::BookmarkNode* bookmark_root =
      bookmark_model()->bookmark_bar_node();
  return bookmark_model()->AddURL(bookmark_root, 0, base::string16(), url);
}

void OfflinePageModelBookmarkChangeTest::UndoBookmarkRemoval() {
  bookmark_undo_provider_->RestoreRemovedNode(
      removed_bookmark_parent_, removed_bookmark_index_,
      std::move(removed_bookmark_node_));
  removed_bookmark_parent_ = nullptr;
  removed_bookmark_index_ = -1;
}

TEST_F(OfflinePageModelBookmarkChangeTest, ChangeBookmakeTitle) {
  // Creates a bookmark without offline copy.
  const bookmarks::BookmarkNode* bookmark_node = CreateBookmarkNode(kTestUrl);
  EXPECT_EQ(0UL, model()->GetAllPages().size());

  // Changing the bookmark title should have no effect.
  bookmark_model()->SetTitle(
      bookmark_node, base::string16(base::ASCIIToUTF16("foo")));
  PumpLoop();
  EXPECT_EQ(0UL, model()->GetAllPages().size());

  // Adds an offline copy for the bookmark.
  SavePage(kTestUrl, bookmark_node->id());
  EXPECT_EQ(1UL, model()->GetAllPages().size());

  // Changes the bookmark title. The offline copy was not affected.
  bookmark_model()->SetTitle(
      bookmark_node, base::string16(base::ASCIIToUTF16("bar")));
  PumpLoop();
  EXPECT_EQ(1UL, model()->GetAllPages().size());
}

TEST_F(OfflinePageModelBookmarkChangeTest, ChangeBookmakeURL) {
  // Creates a bookmark without offline copy.
  const bookmarks::BookmarkNode* bookmark_node = CreateBookmarkNode(kTestUrl2);
  EXPECT_EQ(0UL, model()->GetAllPages().size());

  // Changing the bookmark URL should have no effect. Chrome should not crash.
  // (http://crbug.com/560518)
  bookmark_model()->SetURL(bookmark_node, kTestUrl);
  PumpLoop();
  EXPECT_EQ(0UL, model()->GetAllPages().size());

  // Adds an offline copy for the bookmark.
  SavePage(kTestUrl, bookmark_node->id());
  EXPECT_EQ(1UL, model()->GetAllPages().size());

  // The offline copy should be removed upon the bookmark URL change.
  // (http://crbug.com/558929)
  bookmark_model()->SetURL(bookmark_node, kTestUrl2);
  PumpLoop();
  EXPECT_EQ(0UL, model()->GetAllPages().size());
}

TEST_F(OfflinePageModelBookmarkChangeTest, RemoveBookmark) {
  // Creates a bookmark without offline copy.
  const bookmarks::BookmarkNode* bookmark_node = CreateBookmarkNode(kTestUrl2);
  EXPECT_EQ(0UL, model()->GetAllPages().size());

  // Removing the bookmark should have no effect.
  bookmark_model()->Remove(bookmark_node);
  PumpLoop();
  EXPECT_EQ(0UL, model()->GetAllPages().size());

  // Creates a bookmark with offline copy.
  bookmark_node = CreateBookmarkNode(kTestUrl);
  SavePage(kTestUrl, bookmark_node->id());
  EXPECT_EQ(1UL, model()->GetAllPages().size());

  // The offline copy should also be removed upon the bookmark removal.
  bookmark_model()->Remove(bookmark_node);
  PumpLoop();
  EXPECT_EQ(0UL, model()->GetAllPages().size());
}

TEST_F(OfflinePageModelBookmarkChangeTest, UndoBookmarkRemoval) {
  // Enables undo support.
  bookmark_model()->SetUndoDelegate(this);

  // Creates a bookmark without offline copy.
  const bookmarks::BookmarkNode* bookmark_node = CreateBookmarkNode(kTestUrl2);
  EXPECT_EQ(0UL, model()->GetAllPages().size());

  // Removing the bookmark and undoing the removal should have no effect.
  bookmark_model()->Remove(bookmark_node);
  PumpLoop();
  UndoBookmarkRemoval();
  PumpLoop();
  EXPECT_EQ(0UL, model()->GetAllPages().size());

  // Creates a bookmark with offline copy.
  bookmark_node = CreateBookmarkNode(kTestUrl);
  SavePage(kTestUrl, bookmark_node->id());
  EXPECT_EQ(1UL, model()->GetAllPages().size());

  // The offline copy should also be removed upon the bookmark removal.
  bookmark_model()->Remove(bookmark_node);
  PumpLoop();
  EXPECT_EQ(0UL, model()->GetAllPages().size());

  // The offline copy should be restored upon the bookmark restore.
  UndoBookmarkRemoval();
  PumpLoop();
  EXPECT_EQ(1UL, model()->GetAllPages().size());
}

}  // namespace offline_pages
