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
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_metadata_store.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using SavePageResult = offline_pages::OfflinePageModel::SavePageResult;
using LoadResult = offline_pages::OfflinePageModel::LoadResult;
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
  ~OfflinePageTestStore() override;

  // OfflinePageMetadataStore overrides:
  void Load(const LoadCallback& callback) override;
  void AddOfflinePage(const OfflinePageItem& offline_page,
                      const UpdateCallback& callback) override;
  void RemoveOfflinePage(const GURL& page_url,
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

  DISALLOW_COPY_AND_ASSIGN(OfflinePageTestStore);
};

OfflinePageTestStore::OfflinePageTestStore(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner),
      scenario_(TestScenario::SUCCESSFUL) {
}

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

void OfflinePageTestStore::AddOfflinePage(const OfflinePageItem& offline_page,
                                          const UpdateCallback& callback) {
  last_saved_page_ = offline_page;
  bool result = scenario_ != TestScenario::WRITE_FAILED;
  if (result) {
    offline_pages_.push_back(offline_page);
  }
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, result));
}

void OfflinePageTestStore::RemoveOfflinePage(const GURL& page_url,
                                             const UpdateCallback& callback) {
  bool result = false;
  if (scenario_ != TestScenario::REMOVE_FAILED) {
    for (auto iter = offline_pages_.begin();
         iter != offline_pages_.end(); ++iter) {
      if (iter->url == page_url) {
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
      public base::SupportsWeakPtr<OfflinePageModelTest> {
 public:
  OfflinePageModelTest();
  ~OfflinePageModelTest() override;

  void SetUp() override;

  // OfflinePageModel callbacks.
  void OnSavePageDone(SavePageResult result);
  void OnLoadAllPagesDone(LoadResult result,
                          const std::vector<OfflinePageItem>& offline_pages);
  void OnDeletePageDone(DeletePageResult result);

  scoped_ptr<OfflinePageTestArchiver> BuildArchiver(
      const GURL& url,
      OfflinePageArchiver::ArchiverResult result);
  scoped_ptr<OfflinePageMetadataStore> BuildStore();
  scoped_ptr<OfflinePageModel> BuildModel();

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

  LoadResult last_load_result() const {
    return last_load_result_;
  }

  DeletePageResult last_delete_result() const {
    return last_delete_result_;
  }

  const std::vector<OfflinePageItem>& last_loaded_pages() const {
    return last_loaded_pages_;
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
  LoadResult last_load_result_;
  DeletePageResult last_delete_result_;
  std::vector<OfflinePageItem> last_loaded_pages_;
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
      last_load_result_(LoadResult::CANCELLED),
      last_delete_result_(DeletePageResult::CANCELLED) {
}

OfflinePageModelTest::~OfflinePageModelTest() {
}

void OfflinePageModelTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  model_ = BuildModel().Pass();
}

void OfflinePageModelTest::OnSavePageDone(
    OfflinePageModel::SavePageResult result) {
  run_loop_->Quit();
  last_save_result_ = result;
}

void OfflinePageModelTest::OnLoadAllPagesDone(
    LoadResult result,
    const std::vector<OfflinePageItem>& offline_pages) {
  run_loop_->Quit();
  last_load_result_ = result;
  last_loaded_pages_ = offline_pages;
}

void OfflinePageModelTest::OnDeletePageDone(DeletePageResult result) {
  run_loop_->Quit();
  last_delete_result_ = result;
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

scoped_ptr<OfflinePageModel> OfflinePageModelTest::BuildModel() {
  return scoped_ptr<OfflinePageModel>(
      new OfflinePageModel(BuildStore().Pass(), task_runner()));
}

void OfflinePageModelTest::PumpLoop() {
  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();
}

void OfflinePageModelTest::ResetResults() {
  last_save_result_ = SavePageResult::CANCELLED;
  last_load_result_ = LoadResult::CANCELLED;
  last_delete_result_ = DeletePageResult::CANCELLED;
  last_loaded_pages_.clear();
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

  model()->LoadAllPages(base::Bind(&OfflinePageModelTest::OnLoadAllPagesDone,
                                   AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(LoadResult::SUCCESS, last_load_result());
  EXPECT_EQ(1UL, last_loaded_pages().size());
  EXPECT_EQ(kTestUrl, last_loaded_pages()[0].url);
  EXPECT_EQ(kTestPageBookmarkId1, last_loaded_pages()[0].bookmark_id);
  EXPECT_EQ(archiver_path, last_loaded_pages()[0].file_path);
  EXPECT_EQ(kTestFileSize, last_loaded_pages()[0].file_size);
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

  model()->LoadAllPages(base::Bind(&OfflinePageModelTest::OnLoadAllPagesDone,
                                   AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(LoadResult::SUCCESS, last_load_result());
  EXPECT_EQ(2UL, last_loaded_pages().size());
  EXPECT_EQ(kTestUrl2, last_loaded_pages()[0].url);
  EXPECT_EQ(kTestPageBookmarkId2, last_loaded_pages()[0].bookmark_id);
  EXPECT_EQ(archiver_path2, last_loaded_pages()[0].file_path);
  EXPECT_EQ(kTestFileSize, last_loaded_pages()[0].file_size);
  EXPECT_EQ(kTestUrl, last_loaded_pages()[1].url);
  EXPECT_EQ(kTestPageBookmarkId1, last_loaded_pages()[1].bookmark_id);
  EXPECT_EQ(archiver_path, last_loaded_pages()[1].file_path);
  EXPECT_EQ(kTestFileSize, last_loaded_pages()[1].file_size);
}

TEST_F(OfflinePageModelTest, LoadAllPagesStoreEmpty) {
  model()->LoadAllPages(base::Bind(&OfflinePageModelTest::OnLoadAllPagesDone,
                                   AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(LoadResult::SUCCESS, last_load_result());
  EXPECT_EQ(0UL, last_loaded_pages().size());
}

TEST_F(OfflinePageModelTest, LoadAllPagesStoreFailure) {
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::LOAD_FAILED);
  model()->LoadAllPages(base::Bind(&OfflinePageModelTest::OnLoadAllPagesDone,
                                   AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(LoadResult::STORE_FAILURE, last_load_result());
  EXPECT_EQ(0UL, last_loaded_pages().size());
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
  model()->DeletePage(kTestUrl,
                      base::Bind(&OfflinePageModelTest::OnDeletePageDone,
                                 AsWeakPtr()));

  PumpLoop();

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  ASSERT_EQ(1u, store->offline_pages().size());
  EXPECT_EQ(kTestUrl2, store->offline_pages()[0].url);

  // Delete another page.
  model()->DeletePage(kTestUrl2,
                      base::Bind(&OfflinePageModelTest::OnDeletePageDone,
                                 AsWeakPtr()));

  ResetResults();

  PumpLoop();

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_result());
  EXPECT_EQ(0u, store->offline_pages().size());
}

TEST_F(OfflinePageModelTest, DeletePageStoreFailureOnLoad) {
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::LOAD_FAILED);
  model()->DeletePage(kTestUrl,
                      base::Bind(&OfflinePageModelTest::OnDeletePageDone,
                                 AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(DeletePageResult::STORE_FAILURE, last_delete_result());
}

TEST_F(OfflinePageModelTest, DeletePageNotFound) {
  model()->DeletePage(kTestUrl,
                      base::Bind(&OfflinePageModelTest::OnDeletePageDone,
                                 AsWeakPtr()));
  PumpLoop();
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
  model()->DeletePage(kTestUrl,
                      base::Bind(&OfflinePageModelTest::OnDeletePageDone,
                                 AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(DeletePageResult::STORE_FAILURE, last_delete_result());
}

}  // namespace offline_pages
