// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_model.h"

#include <stdint.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
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
#include "components/offline_pages/offline_page_bookmark_bridge.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_test_archiver.h"
#include "components/offline_pages/offline_page_test_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using SavePageResult = offline_pages::OfflinePageModel::SavePageResult;
using DeletePageResult = offline_pages::OfflinePageModel::DeletePageResult;
using GetAllPagesResult = offline_pages::OfflinePageModel::GetAllPagesResult;

namespace offline_pages {

namespace {
const GURL kTestUrl("http://example.com");
const ClientId kTestPageBookmarkId1(BOOKMARK_NAMESPACE, "1234");
const GURL kTestUrl2("http://other.page.com");
const GURL kTestUrl3("http://test.xyz");
const GURL kFileUrl("file:///foo");
const ClientId kTestPageBookmarkId2(BOOKMARK_NAMESPACE, "5678");
const ClientId kTestPageBookmarkId3(BOOKMARK_NAMESPACE, "42");
const int64_t kTestFileSize = 876543LL;

bool URLSpecContains(std::string contains_value, const GURL& url) {
  std::string spec = url.spec();
  return spec.find(contains_value) != std::string::npos;
}

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
  void OfflinePageDeleted(int64_t offline_id,
                          const ClientId& client_id) override;

  // OfflinePageTestArchiver::Observer implementation.
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override;

  // OfflinePageModel callbacks.
  void OnGetAllPagesDone(const GetAllPagesResult& result);
  void OnSavePageDone(SavePageResult result, int64_t offline_id);
  void OnDeletePageDone(DeletePageResult result);
  void OnHasPagesDone(bool result);
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

  const GetAllPagesResult& GetAllPages();
  void SavePage(const GURL& url, ClientId client_id);
  void SavePageWithArchiverResult(const GURL& url,
                                  ClientId client_id,
                                  OfflinePageArchiver::ArchiverResult result);

  void DeletePage(int64_t offline_id,
                  const OfflinePageModel::DeletePageCallback& callback) {
    std::vector<int64_t> offline_ids;
    offline_ids.push_back(offline_id);
    model()->DeletePagesByOfflineId(offline_ids, callback);
  }

  bool HasPages(std::string name_space);

  OfflinePageModel* model() { return model_.get(); }

  int64_t last_save_offline_id() const { return last_save_offline_id_; }

  SavePageResult last_save_result() const {
    return last_save_result_;
  }

  DeletePageResult last_delete_result() const {
    return last_delete_result_;
  }

  int64_t last_deleted_offline_id() const { return last_deleted_offline_id_; }

  ClientId last_deleted_client_id() const { return last_deleted_client_id_; }

  const base::FilePath& last_archiver_path() { return last_archiver_path_; }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  base::ScopedTempDir temp_dir_;

  scoped_ptr<OfflinePageModel> model_;
  GetAllPagesResult all_pages_;
  SavePageResult last_save_result_;
  int64_t last_save_offline_id_;
  DeletePageResult last_delete_result_;
  base::FilePath last_archiver_path_;
  int64_t last_deleted_offline_id_;
  ClientId last_deleted_client_id_;
  bool last_has_pages_result_;
};

OfflinePageModelTest::OfflinePageModelTest()
    : task_runner_(new base::TestMockTimeTaskRunner),
      task_runner_handle_(task_runner_),
      last_save_result_(SavePageResult::CANCELLED),
      last_save_offline_id_(-1),
      last_delete_result_(DeletePageResult::CANCELLED),
      last_deleted_offline_id_(-1) {}

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

void OfflinePageModelTest::OfflinePageDeleted(int64_t offline_id,
                                              const ClientId& client_id) {
  last_deleted_offline_id_ = offline_id;
  last_deleted_client_id_ = client_id;
}

void OfflinePageModelTest::SetLastPathCreatedByArchiver(
    const base::FilePath& file_path) {
  last_archiver_path_ = file_path;
}

void OfflinePageModelTest::OnSavePageDone(
    OfflinePageModel::SavePageResult result,
    int64_t offline_id) {
  last_save_result_ = result;
  last_save_offline_id_ = offline_id;
}

void OfflinePageModelTest::OnGetAllPagesDone(
    const OfflinePageModel::GetAllPagesResult& result) {
  all_pages_ = result;
}

void OfflinePageModelTest::OnDeletePageDone(DeletePageResult result) {
  last_delete_result_ = result;
}

void OfflinePageModelTest::OnHasPagesDone(bool result) {
  last_has_pages_result_ = result;
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

void OfflinePageModelTest::SavePage(const GURL& url, ClientId client_id) {
  SavePageWithArchiverResult(
      url, client_id,
      OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED);
}

void OfflinePageModelTest::SavePageWithArchiverResult(
    const GURL& url,
    ClientId client_id,
    OfflinePageArchiver::ArchiverResult result) {
  scoped_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(url, result));
  model()->SavePage(
      url, client_id, std::move(archiver),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
}

const std::vector<OfflinePageItem>& OfflinePageModelTest::GetAllPages() {
  model()->GetAllPages(
      base::Bind(&OfflinePageModelTest::OnGetAllPagesDone, AsWeakPtr()));
  PumpLoop();
  return all_pages_;
}

bool OfflinePageModelTest::HasPages(std::string name_space) {
  model()->HasPages(
      name_space,
      base::Bind(&OfflinePageModelTest::OnHasPagesDone, AsWeakPtr()));
  PumpLoop();
  return last_has_pages_result_;
}

TEST_F(OfflinePageModelTest, SavePageSuccessful) {
  EXPECT_FALSE(HasPages(BOOKMARK_NAMESPACE));
  SavePage(kTestUrl, kTestPageBookmarkId1);
  EXPECT_TRUE(HasPages(BOOKMARK_NAMESPACE));

  OfflinePageTestStore* store = GetStore();
  EXPECT_EQ(kTestUrl, store->last_saved_page().url);
  EXPECT_EQ(kTestPageBookmarkId1.id, store->last_saved_page().client_id.id);
  EXPECT_EQ(kTestPageBookmarkId1.name_space,
            store->last_saved_page().client_id.name_space);
  // Save last_archiver_path since it will be referred to later.
  base::FilePath archiver_path = last_archiver_path();
  EXPECT_EQ(archiver_path, store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  ResetResults();

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  EXPECT_EQ(1UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  EXPECT_EQ(kTestPageBookmarkId1.id, offline_pages[0].client_id.id);
  EXPECT_EQ(kTestPageBookmarkId1.name_space,
            offline_pages[0].client_id.name_space);
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
  EXPECT_EQ(kTestPageBookmarkId2, store->last_saved_page().client_id);
  base::FilePath archiver_path2 = last_archiver_path();
  EXPECT_EQ(archiver_path2, store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());

  ResetResults();

  archiver_ptr->CompleteCreateArchive();
  // After this pump loop archiver_ptr is invalid.
  PumpLoop();

  EXPECT_EQ(kTestUrl, store->last_saved_page().url);
  EXPECT_EQ(kTestPageBookmarkId1, store->last_saved_page().client_id);
  base::FilePath archiver_path = last_archiver_path();
  EXPECT_EQ(archiver_path, store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());

  ResetResults();

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  EXPECT_EQ(2UL, offline_pages.size());
  // Offline IDs are random, so the order of the pages is also random
  // So load in the right page for the validation below.
  const OfflinePageItem* page1;
  const OfflinePageItem* page2;
  if (offline_pages[0].client_id == kTestPageBookmarkId1) {
    page1 = &offline_pages[0];
    page2 = &offline_pages[1];
  } else {
    page1 = &offline_pages[1];
    page2 = &offline_pages[0];
  }

  EXPECT_EQ(kTestUrl, page1->url);
  EXPECT_EQ(kTestPageBookmarkId1, page1->client_id);
  EXPECT_EQ(archiver_path, page1->file_path);
  EXPECT_EQ(kTestFileSize, page1->file_size);
  EXPECT_EQ(0, page1->access_count);
  EXPECT_EQ(0, page1->flags);
  EXPECT_EQ(kTestUrl2, page2->url);
  EXPECT_EQ(kTestPageBookmarkId2, page2->client_id);
  EXPECT_EQ(archiver_path2, page2->file_path);
  EXPECT_EQ(kTestFileSize, page2->file_size);
  EXPECT_EQ(0, page2->access_count);
  EXPECT_EQ(0, page2->flags);
}

TEST_F(OfflinePageModelTest, MarkPageAccessed) {
  SavePage(kTestUrl, kTestPageBookmarkId1);

  // This will increase access_count by one.
  model()->MarkPageAccessed(last_save_offline_id());
  PumpLoop();

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  EXPECT_EQ(1UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  EXPECT_EQ(kTestPageBookmarkId1, offline_pages[0].client_id);
  EXPECT_EQ(kTestFileSize, offline_pages[0].file_size);
  EXPECT_EQ(1, offline_pages[0].access_count);
}

TEST_F(OfflinePageModelTest, MarkPageForDeletion) {
  SavePage(kTestUrl, kTestPageBookmarkId1);

  GURL offline_url = GetAllPages().begin()->GetOfflineURL();

  // Delete the page with undo tiggerred.
  model()->MarkPageForDeletion(
      last_save_offline_id(),
      base::Bind(&OfflinePageModelTest::OnDeletePageDone, AsWeakPtr()));
  PumpLoop();

  // GetAllPages will not return the page that is marked for deletion.
  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();
  EXPECT_EQ(0UL, offline_pages.size());

  EXPECT_FALSE(HasPages(BOOKMARK_NAMESPACE));
  EXPECT_EQ(nullptr, model()->GetPageByOnlineURL(kTestUrl));
  EXPECT_EQ(nullptr, model()->GetPageByOfflineId(last_save_offline_id()));
  EXPECT_EQ(nullptr, model()->GetPageByOfflineURL(offline_url));

  // Undo the deletion.
  model()->UndoPageDeletion(last_save_offline_id());
  PumpLoop();

  // GetAllPages will now return the restored page.
  const std::vector<OfflinePageItem>& offline_pages_after_undo = GetAllPages();
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
      last_save_offline_id(),
      base::Bind(&OfflinePageModelTest::OnDeletePageDone, AsWeakPtr()));
  PumpLoop();

  EXPECT_EQ(1UL, GetStore()->GetAllPages().size());

  // Fast forward to trigger the page deletion.
  FastForwardBy(OfflinePageModel::GetFinalDeletionDelayForTesting());

  EXPECT_EQ(0UL, GetStore()->GetAllPages().size());
}

TEST_F(OfflinePageModelTest, SavePageAfterMarkingPageForDeletion) {
  scoped_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, std::move(archiver),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  // Mark the page for deletion.
  model()->MarkPageForDeletion(
      last_save_offline_id(),
      base::Bind(&OfflinePageModelTest::OnDeletePageDone, AsWeakPtr()));
  PumpLoop();

  EXPECT_EQ(1UL, GetStore()->GetAllPages().size());

  // Re-save the same page.
  scoped_ptr<OfflinePageTestArchiver> archiver2(BuildArchiver(
      kTestUrl, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, std::move(archiver2),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));

  // Fast forward to trigger the page cleanup.
  FastForwardBy(OfflinePageModel::GetFinalDeletionDelayForTesting());

  // The re-saved page should still exist.
  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();
  ASSERT_EQ(1UL, offline_pages.size());
  EXPECT_EQ(kTestUrl, offline_pages[0].url);
  EXPECT_EQ(kTestPageBookmarkId1, offline_pages[0].client_id);
  EXPECT_EQ(kTestFileSize, offline_pages[0].file_size);
  EXPECT_EQ(0, offline_pages[0].access_count);
}

TEST_F(OfflinePageModelTest, GetAllPagesStoreEmpty) {
  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  EXPECT_EQ(0UL, offline_pages.size());
}

TEST_F(OfflinePageModelTest, GetAllPagesStoreFailure) {
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::LOAD_FAILED);
  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();

  EXPECT_EQ(0UL, offline_pages.size());
}

TEST_F(OfflinePageModelTest, DeletePageSuccessful) {
  OfflinePageTestStore* store = GetStore();

  // Save one page.
  SavePage(kTestUrl, kTestPageBookmarkId1);
  int64_t offline1 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(1u, store->GetAllPages().size());

  ResetResults();

  // Save another page.
  SavePage(kTestUrl2, kTestPageBookmarkId2);
  int64_t offline2 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(2u, store->GetAllPages().size());

  ResetResults();

  // Delete one page.
  DeletePage(offline1,
             base::Bind(&OfflinePageModelTest::OnDeletePageDone, AsWeakPtr()));

  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline1);
  EXPECT_EQ(last_deleted_client_id(), kTestPageBookmarkId1);
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  ASSERT_EQ(1u, store->GetAllPages().size());
  EXPECT_EQ(kTestUrl2, store->GetAllPages()[0].url);

  // Delete another page.
  DeletePage(offline2,
             base::Bind(&OfflinePageModelTest::OnDeletePageDone, AsWeakPtr()));

  ResetResults();

  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline2);
  EXPECT_EQ(last_deleted_client_id(), kTestPageBookmarkId2);
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  EXPECT_EQ(0u, store->GetAllPages().size());
}

TEST_F(OfflinePageModelTest, DeletePageByPredicate) {
  OfflinePageTestStore* store = GetStore();

  // Save one page.
  SavePage(kTestUrl, kTestPageBookmarkId1);
  int64_t offline1 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(1u, store->GetAllPages().size());

  ResetResults();

  // Save another page.
  SavePage(kTestUrl2, kTestPageBookmarkId2);
  int64_t offline2 = last_save_offline_id();
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
  EXPECT_EQ(2u, store->GetAllPages().size());

  ResetResults();

  // Delete the second page.
  model()->DeletePagesByURLPredicate(
      base::Bind(&URLSpecContains, "page.com"),
      base::Bind(&OfflinePageModelTest::OnDeletePageDone, AsWeakPtr()));

  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline2);
  EXPECT_EQ(last_deleted_client_id(), kTestPageBookmarkId2);
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  ASSERT_EQ(1u, store->GetAllPages().size());
  EXPECT_EQ(kTestUrl, store->GetAllPages()[0].url);

  ResetResults();

  // Delete the first page.
  model()->DeletePagesByURLPredicate(
      base::Bind(&URLSpecContains, "example.com"),
      base::Bind(&OfflinePageModelTest::OnDeletePageDone, AsWeakPtr()));

  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline1);
  EXPECT_EQ(last_deleted_client_id(), kTestPageBookmarkId1);
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  EXPECT_EQ(0u, store->GetAllPages().size());
}

TEST_F(OfflinePageModelTest, DeletePageNotFound) {
  DeletePage(1234LL,
             base::Bind(&OfflinePageModelTest::OnDeletePageDone, AsWeakPtr()));
  EXPECT_EQ(DeletePageResult::NOT_FOUND, last_delete_result());

  ResetResults();

  model()->DeletePagesByURLPredicate(
      base::Bind(&URLSpecContains, "page.com"),
      base::Bind(&OfflinePageModelTest::OnDeletePageDone, AsWeakPtr()));
  EXPECT_EQ(DeletePageResult::NOT_FOUND, last_delete_result());
}

TEST_F(OfflinePageModelTest, DeletePageStoreFailureOnRemove) {
  // Save a page.
  SavePage(kTestUrl, kTestPageBookmarkId1);
  int64_t offline_id = last_save_offline_id();
  ResetResults();

  // Try to delete this page.
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::REMOVE_FAILED);
  DeletePage(offline_id,
             base::Bind(&OfflinePageModelTest::OnDeletePageDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(DeletePageResult::STORE_FAILURE, last_delete_result());
}

TEST_F(OfflinePageModelTest, DetectThatOfflineCopyIsMissing) {
  // Save a page.
  SavePage(kTestUrl, kTestPageBookmarkId1);
  int64_t offline_id = last_save_offline_id();

  ResetResults();

  const OfflinePageItem* page = model()->GetPageByOfflineId(offline_id);

  // Delete the offline copy of the page and check the metadata.
  base::DeleteFile(page->file_path, false);
  model()->CheckForExternalFileDeletion();
  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline_id);
  EXPECT_EQ(0UL, GetAllPages().size());
}

TEST_F(OfflinePageModelTest, DetectThatOfflineCopyIsMissingAfterLoad) {
  // Save a page.
  SavePage(kTestUrl, kTestPageBookmarkId1);
  PumpLoop();
  int64_t offline_id = last_save_offline_id();

  ResetResults();

  const OfflinePageItem* page = model()->GetPageByOfflineId(offline_id);
  // Delete the offline copy of the page and check the metadata.
  base::DeleteFile(page->file_path, false);
  // Reseting the model should trigger the metadata consistency check as well.
  ResetModel();
  PumpLoop();

  EXPECT_EQ(last_deleted_offline_id(), offline_id);
  EXPECT_EQ(0UL, GetAllPages().size());
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
  int64_t offline1 = last_save_offline_id();

  scoped_ptr<OfflinePageTestArchiver> archiver2(BuildArchiver(
      kTestUrl2, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  model()->SavePage(
      kTestUrl2, kTestPageBookmarkId2, std::move(archiver2),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();
  int64_t offline2 = last_save_offline_id();

  scoped_ptr<OfflinePageTestArchiver> archiver3(BuildArchiver(
      kTestUrl3, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED));
  model()->SavePage(
      kTestUrl3, kTestPageBookmarkId3, std::move(archiver3),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

  EXPECT_EQ(3u, store->GetAllPages().size());

  // Delete multiple pages.
  std::vector<int64_t> ids_to_delete;
  ids_to_delete.push_back(offline2);
  ids_to_delete.push_back(offline1);
  ids_to_delete.push_back(23434LL);  // Non-existent ID.
  model()->DeletePagesByOfflineId(
      ids_to_delete,
      base::Bind(&OfflinePageModelTest::OnDeletePageDone, AsWeakPtr()));
  PumpLoop();

  // Success is expected if at least one page is deleted successfully.
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  EXPECT_EQ(1u, store->GetAllPages().size());
}

TEST_F(OfflinePageModelTest, GetPageByOfflineId) {
  SavePage(kTestUrl, kTestPageBookmarkId1);
  int64_t offline1 = last_save_offline_id();
  SavePage(kTestUrl2, kTestPageBookmarkId2);
  int64_t offline2 = last_save_offline_id();

  const OfflinePageItem* page = model()->GetPageByOfflineId(offline1);
  ASSERT_TRUE(page);
  EXPECT_EQ(kTestUrl, page->url);
  EXPECT_EQ(kTestPageBookmarkId1, page->client_id);
  EXPECT_EQ(kTestFileSize, page->file_size);

  page = model()->GetPageByOfflineId(offline2);
  ASSERT_TRUE(page);
  EXPECT_EQ(kTestUrl2, page->url);
  EXPECT_EQ(kTestPageBookmarkId2, page->client_id);
  EXPECT_EQ(kTestFileSize, page->file_size);

  page = model()->GetPageByOfflineId(-42);
  EXPECT_FALSE(page);
}

TEST_F(OfflinePageModelTest, GetPageByOfflineURL) {
  SavePage(kTestUrl, kTestPageBookmarkId1);
  int64_t offline1 = last_save_offline_id();

  OfflinePageTestStore* store = GetStore();
  GURL offline_url = store->last_saved_page().GetOfflineURL();

  SavePage(kTestUrl2, kTestPageBookmarkId2);

  GURL offline_url2 = store->last_saved_page().GetOfflineURL();
  int64_t offline2 = last_save_offline_id();

  const OfflinePageItem* page = model()->GetPageByOfflineURL(offline_url2);
  EXPECT_TRUE(page);
  EXPECT_EQ(kTestUrl2, page->url);
  EXPECT_EQ(kTestPageBookmarkId2, page->client_id);
  EXPECT_EQ(offline2, page->offline_id);

  page = model()->GetPageByOfflineURL(offline_url);
  EXPECT_TRUE(page);
  EXPECT_EQ(kTestUrl, page->url);
  EXPECT_EQ(kTestPageBookmarkId1, page->client_id);
  EXPECT_EQ(offline1, page->offline_id);

  page = model()->GetPageByOfflineURL(GURL("http://foo"));
  EXPECT_FALSE(page);
}

TEST_F(OfflinePageModelTest, GetPageByOnlineURL) {
  SavePage(kTestUrl, kTestPageBookmarkId1);
  SavePage(kTestUrl2, kTestPageBookmarkId2);

  const OfflinePageItem* page = model()->GetPageByOnlineURL(kTestUrl2);
  EXPECT_TRUE(page);
  EXPECT_EQ(kTestUrl2, page->url);
  EXPECT_EQ(kTestPageBookmarkId2, page->client_id);

  page = model()->GetPageByOnlineURL(kTestUrl);
  EXPECT_TRUE(page);
  EXPECT_EQ(kTestUrl, page->url);
  EXPECT_EQ(kTestPageBookmarkId1, page->client_id);

  page = model()->GetPageByOnlineURL(GURL("http://foo"));
  EXPECT_FALSE(page);
}

// Test that model returns pages that are older than 30 days as candidates for
// clean up, hence the numbers in time delta.
TEST_F(OfflinePageModelTest, GetPagesToCleanUp) {
  base::Time now = base::Time::Now();

  SavePage(kTestUrl, kTestPageBookmarkId1);
  GetStore()->UpdateLastAccessTime(last_save_offline_id(),
                                   now - base::TimeDelta::FromDays(40));

  SavePage(kTestUrl2, kTestPageBookmarkId2);
  GetStore()->UpdateLastAccessTime(last_save_offline_id(),
                                   now - base::TimeDelta::FromDays(31));

  SavePage(kTestUrl3, kTestPageBookmarkId3);
  GetStore()->UpdateLastAccessTime(last_save_offline_id(),
                                   now - base::TimeDelta::FromDays(29));

  ResetModel();

  // Only page_1 and page_2 are expected to be picked up by the model as page_3
  // has not been in the store long enough.
  std::vector<OfflinePageItem> pages_to_clean_up = model()->GetPagesToCleanUp();
  // Offline IDs are random, so the order of the pages is also random
  // So load in the right page for the validation below.
  const OfflinePageItem* page1;
  const OfflinePageItem* page2;
  if (pages_to_clean_up[0].client_id == kTestPageBookmarkId1) {
    page1 = &pages_to_clean_up[0];
    page2 = &pages_to_clean_up[1];
  } else {
    page1 = &pages_to_clean_up[1];
    page2 = &pages_to_clean_up[0];
  }

  EXPECT_EQ(2UL, pages_to_clean_up.size());
  EXPECT_EQ(kTestUrl, page1->url);
  EXPECT_EQ(kTestPageBookmarkId1, page1->client_id);
  EXPECT_EQ(kTestUrl2, page2->url);
  EXPECT_EQ(kTestPageBookmarkId2, page2->client_id);
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

  const std::vector<OfflinePageItem>& offline_pages = GetAllPages();
  EXPECT_EQ(2UL, offline_pages.size());
  EXPECT_EQ(2UL, GetStore()->GetAllPages().size());
  base::FilePath archiver_path = offline_pages[0].file_path;
  EXPECT_TRUE(base::PathExists(archiver_path));

  // ClearAll should delete all the files and wipe out both cache and store.
  model()->ClearAll(
      base::Bind(&OfflinePageModelTest::OnClearAllDone, AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(0UL, GetAllPages().size());
  EXPECT_EQ(0UL, GetStore()->GetAllPages().size());
  EXPECT_FALSE(base::PathExists(archiver_path));

  // The model should reload the store after the reset. All model operations
  // should continue to work.
  SavePage(kTestUrl2, kTestPageBookmarkId2);
  EXPECT_EQ(1UL, GetAllPages().size());
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
  bookmark_model_->AddObserver(
      new OfflinePageBookmarkBridge(model(), bookmark_model()));
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
  EXPECT_EQ(0UL, GetAllPages().size());

  // Changing the bookmark title should have no effect.
  bookmark_model()->SetTitle(
      bookmark_node, base::string16(base::ASCIIToUTF16("foo")));
  PumpLoop();
  EXPECT_EQ(0UL, GetAllPages().size());

  // Adds an offline copy for the bookmark.
  SavePage(kTestUrl, ClientId(BOOKMARK_NAMESPACE,
                              base::Int64ToString(bookmark_node->id())));
  EXPECT_EQ(1UL, GetAllPages().size());

  // Changes the bookmark title. The offline copy was not affected.
  bookmark_model()->SetTitle(
      bookmark_node, base::string16(base::ASCIIToUTF16("bar")));
  PumpLoop();
  EXPECT_EQ(1UL, GetAllPages().size());
}

TEST_F(OfflinePageModelBookmarkChangeTest, ChangeBookmakeURL) {
  // Creates a bookmark without offline copy.
  const bookmarks::BookmarkNode* bookmark_node = CreateBookmarkNode(kTestUrl2);
  EXPECT_EQ(0UL, GetAllPages().size());

  // Changing the bookmark URL should have no effect. Chrome should not crash.
  // (http://crbug.com/560518)
  bookmark_model()->SetURL(bookmark_node, kTestUrl);
  PumpLoop();
  EXPECT_EQ(0UL, GetAllPages().size());

  // Adds an offline copy for the bookmark.
  SavePage(kTestUrl, ClientId(BOOKMARK_NAMESPACE,
                              base::Int64ToString(bookmark_node->id())));
  EXPECT_EQ(1UL, GetAllPages().size());

  // The offline copy should be removed upon the bookmark URL change.
  // (http://crbug.com/558929)
  bookmark_model()->SetURL(bookmark_node, kTestUrl2);
  PumpLoop();
  EXPECT_EQ(0UL, GetAllPages().size());
}

TEST_F(OfflinePageModelBookmarkChangeTest, RemoveBookmark) {
  // Creates a bookmark without offline copy.
  const bookmarks::BookmarkNode* bookmark_node = CreateBookmarkNode(kTestUrl2);
  EXPECT_EQ(0UL, GetAllPages().size());

  // Removing the bookmark should have no effect.
  bookmark_model()->Remove(bookmark_node);
  PumpLoop();
  EXPECT_EQ(0UL, GetAllPages().size());

  // Creates a bookmark with offline copy.
  bookmark_node = CreateBookmarkNode(kTestUrl);
  ClientId client_id =
      ClientId(BOOKMARK_NAMESPACE, base::Int64ToString(bookmark_node->id()));
  SavePage(kTestUrl, client_id);
  EXPECT_EQ(1UL, GetAllPages().size());

  // The offline copy should also be removed upon the bookmark removal.
  bookmark_model()->Remove(bookmark_node);
  PumpLoop();
  EXPECT_EQ(client_id, last_deleted_client_id());
  EXPECT_EQ(0UL, GetAllPages().size());
}

TEST_F(OfflinePageModelBookmarkChangeTest, UndoBookmarkRemoval) {
  // Enables undo support.
  bookmark_model()->SetUndoDelegate(this);

  // Creates a bookmark without offline copy.
  const bookmarks::BookmarkNode* bookmark_node = CreateBookmarkNode(kTestUrl2);
  EXPECT_EQ(0UL, GetAllPages().size());

  // Removing the bookmark and undoing the removal should have no effect.
  bookmark_model()->Remove(bookmark_node);
  PumpLoop();
  UndoBookmarkRemoval();
  PumpLoop();
  EXPECT_EQ(0UL, GetAllPages().size());

  // Creates a bookmark with offline copy.
  bookmark_node = CreateBookmarkNode(kTestUrl);
  SavePage(kTestUrl, ClientId(BOOKMARK_NAMESPACE,
                              base::Int64ToString(bookmark_node->id())));
  EXPECT_EQ(1UL, GetAllPages().size());

  // The offline copy should also be removed upon the bookmark removal.
  bookmark_model()->Remove(bookmark_node);
  PumpLoop();
  EXPECT_EQ(0UL, GetAllPages().size());

  // The offline copy should be restored upon the bookmark restore.
  UndoBookmarkRemoval();
  PumpLoop();
  EXPECT_EQ(1UL, GetAllPages().size());
}

TEST_F(OfflinePageModelTest, SaveRetrieveMultipleClientIds) {
  EXPECT_FALSE(HasPages(BOOKMARK_NAMESPACE));
  SavePage(kTestUrl, kTestPageBookmarkId1);
  int64_t offline1 = last_save_offline_id();
  EXPECT_TRUE(HasPages(BOOKMARK_NAMESPACE));

  SavePage(kTestUrl, kTestPageBookmarkId1);
  int64_t offline2 = last_save_offline_id();

  EXPECT_NE(offline1, offline2);

  const std::vector<int64_t> ids =
      model()->GetOfflineIdsForClientId(kTestPageBookmarkId1);

  EXPECT_EQ(2UL, ids.size());

  std::set<int64_t> id_set;
  for (size_t i = 0; i < ids.size(); i++) {
    id_set.insert(ids[i]);
  }

  EXPECT_TRUE(id_set.find(offline1) != id_set.end());
  EXPECT_TRUE(id_set.find(offline2) != id_set.end());
}

TEST(CommandLineFlagsTest, OfflineBookmarks) {
  // Disabled by default.
  EXPECT_FALSE(offline_pages::IsOfflineBookmarksEnabled());

  // Check if feature is correctly enabled by command-line flag.
  base::FeatureList::ClearInstanceForTesting();
  scoped_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(
      offline_pages::kOfflineBookmarksFeature.name, "");
  base::FeatureList::SetInstance(std::move(feature_list));
  EXPECT_TRUE(offline_pages::IsOfflineBookmarksEnabled());
}

TEST(CommandLineFlagsTest, OffliningRecentPages) {
  // Enable offline bookmarks feature first.
  // TODO(dimich): once offline pages are enabled by default, remove this.
  base::FeatureList::ClearInstanceForTesting();
  scoped_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(
      offline_pages::kOfflineBookmarksFeature.name, "");
  base::FeatureList::SetInstance(std::move(feature_list));

  // This feature is still disabled by default.
  EXPECT_FALSE(offline_pages::IsOffliningRecentPagesEnabled());

  // Check if feature is correctly enabled by command-line flag.
  base::FeatureList::ClearInstanceForTesting();
  scoped_ptr<base::FeatureList> feature_list2(new base::FeatureList);
  feature_list2->InitializeFromCommandLine(
      std::string(offline_pages::kOfflineBookmarksFeature.name) + "," +
          offline_pages::kOffliningRecentPagesFeature.name,
      "");
  base::FeatureList::SetInstance(std::move(feature_list2));
  EXPECT_TRUE(offline_pages::IsOffliningRecentPagesEnabled());
}

TEST(CommandLineFlagsTest, OfflinePagesBackgroundLoading) {
  // Enable offline bookmarks feature first.
  // TODO(dimich): once offline pages are enabled by default, remove this.
  base::FeatureList::ClearInstanceForTesting();
  scoped_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(
      offline_pages::kOfflineBookmarksFeature.name, "");
  base::FeatureList::SetInstance(std::move(feature_list));

  // This feature is still disabled by default.
  EXPECT_FALSE(offline_pages::IsOfflinePagesBackgroundLoadingEnabled());

  // Check if feature is correctly enabled by command-line flag.
  base::FeatureList::ClearInstanceForTesting();
  scoped_ptr<base::FeatureList> feature_list2(new base::FeatureList);
  feature_list2->InitializeFromCommandLine(
      std::string(offline_pages::kOfflineBookmarksFeature.name) + "," +
          offline_pages::kOfflinePagesBackgroundLoadingFeature.name,
      "");
  base::FeatureList::SetInstance(std::move(feature_list2));
  EXPECT_TRUE(offline_pages::IsOfflinePagesBackgroundLoadingEnabled());
}

}  // namespace offline_pages
