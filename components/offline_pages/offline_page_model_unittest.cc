// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_model.h"

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_metadata_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using SavePageResult = offline_pages::OfflinePageModel::SavePageResult;
using DeletePageResult = offline_pages::OfflinePageModel::DeletePageResult;

namespace offline_pages {

namespace {
const GURL kTestUrl("http://example.com");
const int64 kTestPageBookmarkId1 = 1234LL;
const GURL kTestUrl2("http://other.page.com");
const int64 kTestPageBookmarkId2 = 5678LL;
const int64 kTestFileSize = 876543LL;

class OfflinePageTestStore : public OfflinePageMetadataStore {
 public:
  enum class TestScenario {
    SUCCESSFUL,
    WRITE_FAILED,
    LOAD_FAILED,
    REMOVE_FAILED,
  };

  explicit OfflinePageTestStore(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  explicit OfflinePageTestStore(const OfflinePageTestStore& other_store);
  ~OfflinePageTestStore() override;

  // OfflinePageMetadataStore overrides:
  void Load(const LoadCallback& callback) override;
  void AddOrUpdateOfflinePage(const OfflinePageItem& offline_page,
                              const UpdateCallback& callback) override;
  void RemoveOfflinePages(const std::vector<int64>& bookmark_ids,
                          const UpdateCallback& callback) override;
  const OfflinePageItem& last_saved_page() const { return last_saved_page_; }

  void set_test_scenario(TestScenario scenario) { scenario_ = scenario; };

  const std::vector<OfflinePageItem>& offline_pages() const {
    return offline_pages_;
  }

 private:
  OfflinePageItem last_saved_page_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  TestScenario scenario_;

  std::vector<OfflinePageItem> offline_pages_;

  DISALLOW_ASSIGN(OfflinePageTestStore);
};

OfflinePageTestStore::OfflinePageTestStore(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner),
      scenario_(TestScenario::SUCCESSFUL) {
}

OfflinePageTestStore::OfflinePageTestStore(
    const OfflinePageTestStore& other_store)
    : task_runner_(other_store.task_runner_),
      scenario_(other_store.scenario_),
      offline_pages_(other_store.offline_pages_) {}

OfflinePageTestStore::~OfflinePageTestStore() {
}

void OfflinePageTestStore::Load(const LoadCallback& callback) {
  if (scenario_ != TestScenario::LOAD_FAILED) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(callback, true, offline_pages_));
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(callback, false, std::vector<OfflinePageItem>()));
  }
}

void OfflinePageTestStore::AddOrUpdateOfflinePage(
    const OfflinePageItem& offline_page, const UpdateCallback& callback) {
  last_saved_page_ = offline_page;
  bool result = scenario_ != TestScenario::WRITE_FAILED;
  if (result) {
    offline_pages_.push_back(offline_page);
  }
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, result));
}

void OfflinePageTestStore::RemoveOfflinePages(
    const std::vector<int64>& bookmark_ids,
    const UpdateCallback& callback) {
  ASSERT_FALSE(bookmark_ids.empty());
  bool result = false;
  if (scenario_ != TestScenario::REMOVE_FAILED) {
    for (auto iter = offline_pages_.begin();
         iter != offline_pages_.end(); ++iter) {
      if (iter->bookmark_id == bookmark_ids[0]) {
        offline_pages_.erase(iter);
        result = true;
        break;
      }
    }
  }

  task_runner_->PostTask(FROM_HERE, base::Bind(callback, result));
}

}  // namespace

class OfflinePageModelTest;

class OfflinePageTestArchiver : public OfflinePageArchiver {
 public:
  OfflinePageTestArchiver(
      OfflinePageModelTest* test,
      const GURL& url,
      const base::FilePath& archiver_dir,
      ArchiverResult result,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~OfflinePageTestArchiver() override;

  // OfflinePageArchiver implementation:
  void CreateArchive(const CreateArchiveCallback& callback) override;

  void CompleteCreateArchive();

  void set_delayed(bool delayed) { delayed_ = delayed; }

  bool create_archive_called() const { return create_archive_called_; }

 private:
  OfflinePageModelTest* test_;  // Outlive OfflinePageTestArchiver.
  GURL url_;
  base::FilePath archiver_dir_;
  ArchiverResult result_;
  bool create_archive_called_;
  bool delayed_;
  CreateArchiveCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  DISALLOW_COPY_AND_ASSIGN(OfflinePageTestArchiver);
};

class OfflinePageModelTest
    : public testing::Test,
      public OfflinePageModel::Observer,
      public base::SupportsWeakPtr<OfflinePageModelTest> {
 public:
  OfflinePageModelTest();
  ~OfflinePageModelTest() override;

  void SetUp() override;
  void TearDown() override;

  // OfflinePageModel::Observer implementation.
  void OfflinePageModelLoaded(OfflinePageModel* model) override;
  void OfflinePageModelChanged(OfflinePageModel* model) override;

  // OfflinePageModel callbacks.
  void OnSavePageDone(SavePageResult result);
  void OnDeletePageDone(DeletePageResult result);

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
  void PumpLoop();
  void ResetResults();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return message_loop_.task_runner();
  }

  OfflinePageModel* model() { return model_.get(); }

  OfflinePageTestStore* GetStore();

  SavePageResult last_save_result() const {
    return last_save_result_;
  }

  DeletePageResult last_delete_result() const {
    return last_delete_result_;
  }

  const base::FilePath& last_archiver_path() { return last_archiver_path_; }
  void set_last_archiver_path(const base::FilePath& last_archiver_path) {
    last_archiver_path_ = last_archiver_path;
  }

 private:
  base::MessageLoop message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;
  base::ScopedTempDir temp_dir_;

  scoped_ptr<OfflinePageModel> model_;
  SavePageResult last_save_result_;
  DeletePageResult last_delete_result_;
  base::FilePath last_archiver_path_;
};

OfflinePageTestArchiver::OfflinePageTestArchiver(
    OfflinePageModelTest* test,
    const GURL& url,
    const base::FilePath& archiver_dir,
    ArchiverResult result,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : test_(test),
      url_(url),
      archiver_dir_(archiver_dir),
      result_(result),
      create_archive_called_(false),
      delayed_(false),
      task_runner_(task_runner) {
}

OfflinePageTestArchiver::~OfflinePageTestArchiver() {
  EXPECT_TRUE(create_archive_called_);
}

void OfflinePageTestArchiver::CreateArchive(
    const CreateArchiveCallback& callback) {
  create_archive_called_ = true;
  callback_ = callback;
  if (!delayed_)
    CompleteCreateArchive();
}

void OfflinePageTestArchiver::CompleteCreateArchive() {
  DCHECK(!callback_.is_null());
  base::FilePath archiver_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(archiver_dir_, &archiver_path));
  test_->set_last_archiver_path(archiver_path);
  task_runner_->PostTask(FROM_HERE, base::Bind(callback_, this, result_, url_,
                                               archiver_path, kTestFileSize));
}

OfflinePageModelTest::OfflinePageModelTest()
    : last_save_result_(SavePageResult::CANCELLED),
      last_delete_result_(DeletePageResult::CANCELLED) {
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
}

void OfflinePageModelTest::OfflinePageModelLoaded(OfflinePageModel* model) {
  ASSERT_EQ(model_.get(), model);
  run_loop_->Quit();
}

void OfflinePageModelTest::OfflinePageModelChanged(OfflinePageModel* model) {
  ASSERT_EQ(model_.get(), model);
}

void OfflinePageModelTest::OnSavePageDone(
    OfflinePageModel::SavePageResult result) {
  run_loop_->Quit();
  last_save_result_ = result;
}

void OfflinePageModelTest::OnDeletePageDone(DeletePageResult result) {
  run_loop_->Quit();
  last_delete_result_ = result;
}

void OfflinePageModelTest::OnStoreUpdateDone(bool /* success - ignored */) {
  run_loop_->Quit();
}

scoped_ptr<OfflinePageTestArchiver> OfflinePageModelTest::BuildArchiver(
    const GURL& url,
    OfflinePageArchiver::ArchiverResult result) {
  return scoped_ptr<OfflinePageTestArchiver>(new OfflinePageTestArchiver(
      this, url, temp_dir_.path(), result, task_runner()));
}

scoped_ptr<OfflinePageMetadataStore> OfflinePageModelTest::BuildStore() {
  return scoped_ptr<OfflinePageMetadataStore>(
      new OfflinePageTestStore(task_runner()));
}

scoped_ptr<OfflinePageModel> OfflinePageModelTest::BuildModel(
    scoped_ptr<OfflinePageMetadataStore> store) {
  return scoped_ptr<OfflinePageModel>(
      new OfflinePageModel(store.Pass(), task_runner()));
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
  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();
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
  model()->SavePage(
      kTestUrl, kTestPageBookmarkId1, archiver.Pass(),
      base::Bind(&OfflinePageModelTest::OnSavePageDone, AsWeakPtr()));
  PumpLoop();

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
  base::RunLoop().RunUntilIdle();

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

  // Delete the page with undo tiggerred.
  model()->MarkPageForDeletion(
      kTestPageBookmarkId1,
      base::Bind(&OfflinePageModelTest::OnDeletePageDone, AsWeakPtr()));
  PumpLoop();

  // GetAllPages will not return the page that is marked for deletion.
  const std::vector<OfflinePageItem>& offline_pages = model()->GetAllPages();
  EXPECT_EQ(0UL, offline_pages.size());

  // Undo the deletion.
  model()->UndoPageDeletion(kTestPageBookmarkId1);
  base::RunLoop().RunUntilIdle();

  // GetAllPages will now return the restored page.
  const std::vector<OfflinePageItem>& offline_pages_after_undo =
      model()->GetAllPages();
  EXPECT_EQ(1UL, offline_pages_after_undo.size());
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
  EXPECT_EQ(1u, store->offline_pages().size());

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
  EXPECT_EQ(2u, store->offline_pages().size());

  ResetResults();

  // Delete one page.
  model()->DeletePageByBookmarkId(
      kTestPageBookmarkId1, base::Bind(&OfflinePageModelTest::OnDeletePageDone,
                                       AsWeakPtr()));

  PumpLoop();

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  ASSERT_EQ(1u, store->offline_pages().size());
  EXPECT_EQ(kTestUrl2, store->offline_pages()[0].url);

  // Delete another page.
  model()->DeletePageByBookmarkId(
      kTestPageBookmarkId2, base::Bind(&OfflinePageModelTest::OnDeletePageDone,
                                       AsWeakPtr()));

  ResetResults();

  PumpLoop();

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  EXPECT_EQ(0u, store->offline_pages().size());
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

// Test that model returns pages that are older than 30 days as candidates for
// clean up, hence the numbers in time delta.
TEST_F(OfflinePageModelTest, GetPagesToCleanUp) {
  base::Time now = base::Time::Now();
  base::Time forty_days_ago = now - base::TimeDelta::FromDays(40);
  OfflinePageItem page_1(
      GURL(kTestUrl), kTestPageBookmarkId1,
      base::FilePath(FILE_PATH_LITERAL("/test/location/page1.mhtml")),
      kTestFileSize, forty_days_ago);
  GetStore()->AddOrUpdateOfflinePage(
      page_1,
      base::Bind(&OfflinePageModelTest::OnStoreUpdateDone, AsWeakPtr()));
  PumpLoop();

  OfflinePageItem page_2(
      GURL(kTestUrl2), kTestPageBookmarkId2,
      base::FilePath(FILE_PATH_LITERAL("/test/location/page2.mhtml")),
      kTestFileSize, forty_days_ago);
  page_2.last_access_time = now - base::TimeDelta::FromDays(31);
  GetStore()->AddOrUpdateOfflinePage(
      page_2,
      base::Bind(&OfflinePageModelTest::OnStoreUpdateDone, AsWeakPtr()));
  PumpLoop();

  OfflinePageItem page_3(
      GURL("http://test.xyz"), 42,
      base::FilePath(FILE_PATH_LITERAL("/test/location/page3.mhtml")),
      kTestFileSize, forty_days_ago);
  page_3.last_access_time = now - base::TimeDelta::FromDays(29);
  GetStore()->AddOrUpdateOfflinePage(
      page_3,
      base::Bind(&OfflinePageModelTest::OnStoreUpdateDone, AsWeakPtr()));
  PumpLoop();

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

}  // namespace offline_pages
