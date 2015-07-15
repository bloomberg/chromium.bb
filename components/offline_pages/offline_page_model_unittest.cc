// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_model.h"

#include "base/bind.h"
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

namespace offline_pages {

namespace {
const char kTestUrl[] = "http://example.com";
const base::string16 kTestPageTitle = base::ASCIIToUTF16("Test Page Title");
const base::FilePath::CharType kTestFilePath[] =
    FILE_PATH_LITERAL("/archive_dir/offline_page.mhtml");
const int64 kTestFileSize = 876543LL;

class OfflinePageTestStore : public OfflinePageMetadataStore {
 public:
  enum class TestScenario {
    SUCCESSFUL,
    WRITE_FAILED,
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

 private:
  OfflinePageItem last_saved_page_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  TestScenario scenario_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageTestStore);
};

OfflinePageTestStore::OfflinePageTestStore(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner), scenario_(TestScenario::SUCCESSFUL) {
}

OfflinePageTestStore::~OfflinePageTestStore() {
}

void OfflinePageTestStore::Load(const LoadCallback& callback) {
}

void OfflinePageTestStore::AddOfflinePage(const OfflinePageItem& offline_page,
                                          const UpdateCallback& callback) {
  last_saved_page_ = offline_page;
  bool result = scenario_ != TestScenario::WRITE_FAILED;
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, result));
}

void OfflinePageTestStore::RemoveOfflinePage(const GURL& page_url,
                                             const UpdateCallback& callback) {
}

class OfflinePageTestArchiver : public OfflinePageArchiver {
 public:
  OfflinePageTestArchiver(
      const GURL& url,
      const base::string16& title,
      ArchiverResult result,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~OfflinePageTestArchiver() override;

  // OfflinePageArchiver implementation:
  void CreateArchive(const CreateArchiveCallback& callback) override;

  void CompleteCreateArchive();

  void set_delayed(bool delayed) { delayed_ = delayed; }

  bool create_archive_called() const { return create_archive_called_; }

 private:
  GURL url_;
  base::string16 title_;
  ArchiverResult result_;
  bool create_archive_called_;
  bool delayed_;
  CreateArchiveCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  DISALLOW_COPY_AND_ASSIGN(OfflinePageTestArchiver);
};

OfflinePageTestArchiver::OfflinePageTestArchiver(
    const GURL& url,
    const base::string16& title,
    ArchiverResult result,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : url_(url),
      title_(title),
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
  base::FilePath file_path(kTestFilePath);
  task_runner_->PostTask(
      FROM_HERE, base::Bind(callback_, this, result_, url_, title_, file_path,
                            kTestFileSize));
}

}  // namespace

class OfflinePageModelTest
    : public testing::Test,
      public base::SupportsWeakPtr<OfflinePageModelTest> {
 public:
  OfflinePageModelTest();
  ~OfflinePageModelTest() override;

  void SetUp() override;

  // OfflinePageModel callbacks.
  void OnSavePageDone(SavePageResult result);

  scoped_ptr<OfflinePageMetadataStore> BuildStore();
  scoped_ptr<OfflinePageModel> BuildModel();

  // Utility methods.
  void PumpLoop();

  OfflinePageModel::SavePageResult last_save_result() const {
    return last_save_result_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return message_loop_.task_runner();
  }

  OfflinePageModel* model() { return model_.get(); }

  OfflinePageTestStore* GetStore();

 private:
  base::MessageLoop message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;

  scoped_ptr<OfflinePageModel> model_;
  OfflinePageModel::SavePageResult last_save_result_;
};

OfflinePageModelTest::OfflinePageModelTest() {
}

OfflinePageModelTest::~OfflinePageModelTest() {
}

void OfflinePageModelTest::SetUp() {
  model_ = BuildModel().Pass();
}

void OfflinePageModelTest::OnSavePageDone(
    OfflinePageModel::SavePageResult result) {
  run_loop_->Quit();
  last_save_result_ = result;
}

scoped_ptr<OfflinePageMetadataStore> OfflinePageModelTest::BuildStore() {
  return scoped_ptr<OfflinePageMetadataStore>(
      new OfflinePageTestStore(message_loop_.task_runner()));
}

scoped_ptr<OfflinePageModel> OfflinePageModelTest::BuildModel() {
  return scoped_ptr<OfflinePageModel>(
      new OfflinePageModel(BuildStore().Pass(), message_loop_.task_runner()));
}

void OfflinePageModelTest::PumpLoop() {
  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();
}

OfflinePageTestStore* OfflinePageModelTest::GetStore() {
  return static_cast<OfflinePageTestStore*>(model()->GetStoreForTesting());
}

TEST_F(OfflinePageModelTest, SavePageSuccessful) {
  GURL page_url = GURL(kTestUrl);
  scoped_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
      page_url, kTestPageTitle,
      OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
      task_runner()));
  model()->SavePage(page_url, archiver.Pass(),
                    base::Bind(&OfflinePageModelTest::OnSavePageDone,
                               AsWeakPtr()));
  PumpLoop();

  OfflinePageTestStore* store = GetStore();
  EXPECT_EQ(page_url, store->last_saved_page().url);
  EXPECT_EQ(kTestPageTitle, store->last_saved_page().title);
  EXPECT_EQ(base::FilePath(kTestFilePath), store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineArchiverCancelled) {
  GURL page_url = GURL(kTestUrl);
  scoped_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
      page_url, kTestPageTitle,
      OfflinePageArchiver::ArchiverResult::ERROR_CANCELED, task_runner()));
  model()->SavePage(page_url, archiver.Pass(),
                    base::Bind(&OfflinePageModelTest::OnSavePageDone,
                               AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::CANCELLED, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineArchiverDeviceFull) {
  GURL page_url = GURL(kTestUrl);
  scoped_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
      page_url, kTestPageTitle,
      OfflinePageArchiver::ArchiverResult::ERROR_DEVICE_FULL, task_runner()));
  model()->SavePage(page_url, archiver.Pass(),
                    base::Bind(&OfflinePageModelTest::OnSavePageDone,
                               AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::DEVICE_FULL, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineArchiverContentUnavailable) {
  GURL page_url = GURL(kTestUrl);
  scoped_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
      page_url, kTestPageTitle,
      OfflinePageArchiver::ArchiverResult::ERROR_CONTENT_UNAVAILABLE,
      task_runner()));
  model()->SavePage(page_url, archiver.Pass(),
                    base::Bind(&OfflinePageModelTest::OnSavePageDone,
                               AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::CONTENT_UNAVAILABLE, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineCreationFailed) {
  GURL page_url = GURL(kTestUrl);
  scoped_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
      page_url, kTestPageTitle,
      OfflinePageArchiver::ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED,
      task_runner()));
  model()->SavePage(page_url, archiver.Pass(),
                    base::Bind(&OfflinePageModelTest::OnSavePageDone,
                               AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::ARCHIVE_CREATION_FAILED, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineArchiverReturnedWrongUrl) {
  GURL page_url = GURL(kTestUrl);
  scoped_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
      GURL("http://other.random.url.com"), kTestPageTitle,
      OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
      task_runner()));
  model()->SavePage(page_url, archiver.Pass(),
                    base::Bind(&OfflinePageModelTest::OnSavePageDone,
                               AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::ARCHIVE_CREATION_FAILED, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineCreationStoreWriteFailure) {
  GURL page_url = GURL(kTestUrl);
  GetStore()->set_test_scenario(
      OfflinePageTestStore::TestScenario::WRITE_FAILED);
  scoped_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
      page_url, kTestPageTitle,
      OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
      task_runner()));
  model()->SavePage(page_url, archiver.Pass(),
                    base::Bind(&OfflinePageModelTest::OnSavePageDone,
                               AsWeakPtr()));
  PumpLoop();
  EXPECT_EQ(SavePageResult::STORE_FAILURE, last_save_result());
}

TEST_F(OfflinePageModelTest, SavePageOfflineArchiverTwoPages) {
  GURL page_url = GURL(kTestUrl);
  scoped_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
      page_url, kTestPageTitle,
      OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
      task_runner()));
  // archiver_ptr will be valid until after first PumpLoop() call after
  // CompleteCreateArchive() is called.
  OfflinePageTestArchiver* archiver_ptr = archiver.get();
  archiver_ptr->set_delayed(true);
  model()->SavePage(page_url, archiver.Pass(),
                    base::Bind(&OfflinePageModelTest::OnSavePageDone,
                               AsWeakPtr()));
  EXPECT_TRUE(archiver_ptr->create_archive_called());

  // Request to save another page.
  GURL page_url2 = GURL("http://other.page.com");
  base::string16 title2 = base::ASCIIToUTF16("Other page title");
  scoped_ptr<OfflinePageTestArchiver> archiver2(new OfflinePageTestArchiver(
      page_url2, title2,
      OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
      task_runner()));
  model()->SavePage(page_url2, archiver2.Pass(),
                    base::Bind(&OfflinePageModelTest::OnSavePageDone,
                               AsWeakPtr()));
  PumpLoop();

  OfflinePageTestStore* store = GetStore();

  EXPECT_EQ(page_url2, store->last_saved_page().url);
  EXPECT_EQ(title2, store->last_saved_page().title);
  EXPECT_EQ(base::FilePath(kTestFilePath), store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());

  archiver_ptr->CompleteCreateArchive();
  // After this pump loop archiver_ptr is invalid.
  PumpLoop();

  EXPECT_EQ(page_url, store->last_saved_page().url);
  EXPECT_EQ(kTestPageTitle, store->last_saved_page().title);
  EXPECT_EQ(base::FilePath(kTestFilePath), store->last_saved_page().file_path);
  EXPECT_EQ(kTestFileSize, store->last_saved_page().file_size);
  EXPECT_EQ(SavePageResult::SUCCESS, last_save_result());
}

}  // namespace offline_pages
