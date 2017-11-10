// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetch_importer_impl.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_pages/core/prefetch/test_prefetch_dispatcher.h"
#include "components/offline_pages/core/stub_offline_page_model.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const int64_t kTestOfflineID = 111;
const int64_t kTestOfflineIDFailedToAdd = 223344;
const GURL kTestURL("http://sample.org");
const GURL kTestFinalURL("https://sample.org/foo");
const ClientId kTestClientID("Foo", "C56A4180-65AA-42EC-A945-5FD21DEC0538");
const base::string16 kTestTitle = base::ASCIIToUTF16("Hello");
const base::FilePath kTestFilePath(FILE_PATH_LITERAL("foo"));
const int64_t kTestFileSize = 88888;

class TestOfflinePageModel : public StubOfflinePageModel {
 public:
  TestOfflinePageModel() { ignore_result(archive_dir_.CreateUniqueTempDir()); }
  ~TestOfflinePageModel() override = default;

  void AddPage(const OfflinePageItem& page,
               const AddPageCallback& callback) override {
    page_added_ = page.offline_id != kTestOfflineIDFailedToAdd;
    if (page_added_)
      last_added_page_ = page;
    callback.Run(
        page_added_ ? AddPageResult::SUCCESS : AddPageResult::STORE_FAILURE,
        page.offline_id);
  }

  const base::FilePath& GetArchiveDirectory(
      const std::string& name_space) const override {
    return archive_dir_.GetPath();
  }

  bool page_added() const { return page_added_; }
  const OfflinePageItem& last_added_page() const { return last_added_page_; }

 private:
  base::ScopedTempDir archive_dir_;
  bool page_added_ = false;
  OfflinePageItem last_added_page_;

  DISALLOW_COPY_AND_ASSIGN(TestOfflinePageModel);
};

std::unique_ptr<KeyedService> BuildTestOfflinePageModel(
    content::BrowserContext* context) {
  return std::make_unique<TestOfflinePageModel>();
}

}  // namespace

class PrefetchImporterImplTest : public testing::Test {
 public:
  PrefetchImporterImplTest() = default;
  ~PrefetchImporterImplTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    profile_ = std::make_unique<TestingProfile>();

    OfflinePageModelFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), BuildTestOfflinePageModel);
    RunUntilIdle();
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  void ImportArchive(int64_t offline_id, const base::FilePath& file_path) {
    PrefetchImporterImpl importer(dispatcher(), profile(),
                                  background_task_runner());

    PrefetchArchiveInfo archive;
    archive.offline_id = offline_id;
    archive.client_id = kTestClientID;
    archive.url = kTestURL;
    archive.final_archived_url = kTestFinalURL;
    archive.title = kTestTitle;
    archive.file_path = file_path;
    archive.file_size = kTestFileSize;
    importer.ImportArchive(archive);
    RunUntilIdle();
  }

  scoped_refptr<base::SequencedTaskRunner> background_task_runner() {
    return base::ThreadTaskRunnerHandle::Get();
  }
  base::FilePath temp_dir_path() const { return temp_dir_.GetPath(); }
  Profile* profile() { return profile_.get(); }
  TestPrefetchDispatcher* dispatcher() { return &dispatcher_; }

  TestOfflinePageModel* offline_page_model() {
    return static_cast<TestOfflinePageModel*>(
        OfflinePageModelFactory::GetForBrowserContext(profile()));
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  TestPrefetchDispatcher dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchImporterImplTest);
};

TEST_F(PrefetchImporterImplTest, ImportSuccess) {
  base::FilePath path;
  base::CreateTemporaryFileInDir(temp_dir_path(), &path);
  ImportArchive(kTestOfflineID, path);

  ASSERT_EQ(1u, dispatcher()->import_results.size());
  EXPECT_EQ(kTestOfflineID, dispatcher()->import_results[0].first);
  EXPECT_TRUE(dispatcher()->import_results[0].second);

  EXPECT_TRUE(offline_page_model()->page_added());
  EXPECT_EQ(kTestOfflineID, offline_page_model()->last_added_page().offline_id);
  EXPECT_EQ(kTestClientID, offline_page_model()->last_added_page().client_id);
  EXPECT_EQ(kTestFinalURL, offline_page_model()->last_added_page().url);
  EXPECT_EQ(kTestURL, offline_page_model()->last_added_page().original_url);
  EXPECT_EQ(kTestTitle, offline_page_model()->last_added_page().title);
  EXPECT_EQ(kTestFileSize, offline_page_model()->last_added_page().file_size);
}

TEST_F(PrefetchImporterImplTest, MoveFileError) {
  ImportArchive(kTestOfflineID, kTestFilePath);

  ASSERT_EQ(1u, dispatcher()->import_results.size());
  EXPECT_EQ(kTestOfflineID, dispatcher()->import_results[0].first);
  EXPECT_FALSE(dispatcher()->import_results[0].second);

  EXPECT_FALSE(offline_page_model()->page_added());
}

TEST_F(PrefetchImporterImplTest, AddPageError) {
  base::FilePath path;
  base::CreateTemporaryFileInDir(temp_dir_path(), &path);
  ImportArchive(kTestOfflineIDFailedToAdd, path);

  ASSERT_EQ(1u, dispatcher()->import_results.size());
  EXPECT_EQ(kTestOfflineIDFailedToAdd, dispatcher()->import_results[0].first);
  EXPECT_FALSE(dispatcher()->import_results[0].second);

  EXPECT_FALSE(offline_page_model()->page_added());
}

}  // namespace offline_pages
