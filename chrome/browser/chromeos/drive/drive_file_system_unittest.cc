// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_file_system.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_test_util.h"
#include "chrome/browser/chromeos/drive/drive_webapps_registry.h"
#include "chrome/browser/chromeos/drive/fake_free_disk_space_getter.h"
#include "chrome/browser/chromeos/drive/file_system/remove_operation.h"
#include "chrome/browser/chromeos/drive/mock_directory_change_observer.h"
#include "chrome/browser/chromeos/drive/mock_drive_cache_observer.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/drive_uploader.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace drive {
namespace {

const char kSymLinkToDevNull[] = "/dev/null";

const int64 kLotsOfSpace = kMinFreeSpace * 10;

struct SearchResultPair {
  const char* path;
  const bool is_directory;
};

// Callback to DriveFileSystem::Search used in ContentSearch tests.
// Verifies returned vector of results and next feed url.
void DriveSearchCallback(
    MessageLoop* message_loop,
    const SearchResultPair* expected_results,
    size_t expected_results_size,
    const GURL& expected_next_feed,
    DriveFileError error,
    const GURL& next_feed,
    scoped_ptr<std::vector<SearchResultInfo> > results) {
  ASSERT_TRUE(results.get());
  ASSERT_EQ(expected_results_size, results->size());

  for (size_t i = 0; i < results->size(); i++) {
    EXPECT_EQ(base::FilePath(expected_results[i].path),
              results->at(i).path);
    EXPECT_EQ(expected_results[i].is_directory,
              results->at(i).entry_proto.file_info().is_directory());
  }

  EXPECT_EQ(expected_next_feed, next_feed);

  message_loop->Quit();
}

// Counts the number of files (not directories) in |entries|.
int CountFiles(const DriveEntryProtoVector& entries) {
  int num_files = 0;
  for (size_t i = 0; i < entries.size(); ++i) {
    if (!entries[i].file_info().is_directory())
      ++num_files;
  }
  return num_files;
}

}  // namespace

class DriveFileSystemTest : public testing::Test {
 protected:
  DriveFileSystemTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        expected_success_(true),
        // |root_feed_changestamp_| should be set to the largest changestamp in
        // account metadata feed. But we fake it by some non-zero positive
        // increasing value.  See |LoadFeed()|.
        root_feed_changestamp_(1) {
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);

    // The fake object will be manually deleted in TearDown().
    fake_drive_service_.reset(new google_apis::FakeDriveService);
    fake_drive_service_->LoadResourceListForWapi(
        "chromeos/gdata/root_feed.json");
    fake_drive_service_->LoadAccountMetadataForWapi(
        "chromeos/gdata/account_metadata.json");
    fake_drive_service_->LoadAppListForDriveApi("chromeos/drive/applist.json");

    fake_free_disk_space_getter_.reset(new FakeFreeDiskSpaceGetter);

    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());

    cache_.reset(new DriveCache(DriveCache::GetCacheRootPath(profile_.get()),
                                blocking_task_runner_,
                                fake_free_disk_space_getter_.get()));

    uploader_.reset(new google_apis::DriveUploader(fake_drive_service_.get()));
    drive_webapps_registry_.reset(new DriveWebAppsRegistry);


    mock_cache_observer_.reset(new StrictMock<MockDriveCacheObserver>);
    cache_->AddObserver(mock_cache_observer_.get());

    mock_directory_observer_.reset(new StrictMock<MockDirectoryChangeObserver>);

    cache_->RequestInitializeForTesting();
    google_apis::test_util::RunBlockingPoolTask();

    SetUpResourceMetadataAndFileSystem();
  }

  void SetUpResourceMetadataAndFileSystem() {
    resource_metadata_.reset(new DriveResourceMetadata(
        fake_drive_service_->GetRootResourceId(),
        cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_META),
        blocking_task_runner_));

    file_system_.reset(new DriveFileSystem(profile_.get(),
                                           cache_.get(),
                                           fake_drive_service_.get(),
                                           uploader_.get(),
                                           drive_webapps_registry_.get(),
                                           resource_metadata_.get(),
                                           blocking_task_runner_));
    file_system_->AddObserver(mock_directory_observer_.get());
    file_system_->Initialize();

    DriveFileError error = DRIVE_FILE_ERROR_FAILED;
    resource_metadata_->Initialize(
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    ASSERT_EQ(DRIVE_FILE_OK, error);
  }

  virtual void TearDown() OVERRIDE {
    ASSERT_TRUE(file_system_);
    file_system_.reset();
    fake_drive_service_.reset();
    cache_.reset();
    profile_.reset(NULL);
  }

  // Loads test json file as root ("/drive") element.
  bool LoadRootFeedDocument() {
    DriveFileError error = DRIVE_FILE_ERROR_FAILED;
    file_system_->change_list_loader()->LoadFromServerIfNeeded(
        DirectoryFetchInfo(),
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    return error == DRIVE_FILE_OK;
  }

  bool LoadChangeFeed(const std::string& filename) {
    return LoadFeed(filename, true);
  }

  bool LoadFeed(const std::string& filename, bool is_delta_feed) {
    if (!test_util::LoadChangeFeed(filename,
                                   file_system_->change_list_loader(),
                                   is_delta_feed,
                                   fake_drive_service_->GetRootResourceId(),
                                   root_feed_changestamp_)) {
      return false;
    }
    root_feed_changestamp_++;
    return true;
  }

  void AddDirectoryFromFile(const base::FilePath& directory_path,
                            const std::string& filename) {
    scoped_ptr<Value> atom = google_apis::test_util::LoadJSONFile(filename);
    ASSERT_TRUE(atom.get());
    ASSERT_TRUE(atom->GetType() == Value::TYPE_DICTIONARY);

    DictionaryValue* dict_value = NULL;
    Value* entry_value = NULL;
    ASSERT_TRUE(atom->GetAsDictionary(&dict_value));
    ASSERT_TRUE(dict_value->Get("entry", &entry_value));

    DictionaryValue* entry_dict = NULL;
    ASSERT_TRUE(entry_value->GetAsDictionary(&entry_dict));

    // Tweak entry title to match the last segment of the directory path
    // (new directory name).
    std::vector<base::FilePath::StringType> dir_parts;
    directory_path.GetComponents(&dir_parts);
    entry_dict->SetString("title.$t", dir_parts[dir_parts.size() - 1]);

    DriveFileError error = DRIVE_FILE_ERROR_FAILED;
    file_system_->CreateDirectory(
        directory_path,
        false,  // is_exclusive
        false,  // is_recursive
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(DRIVE_FILE_OK, error);
  }

  bool RemoveEntry(const base::FilePath& file_path) {
    DriveFileError error = DRIVE_FILE_ERROR_FAILED;
    file_system_->Remove(
        file_path, false,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    return error == DRIVE_FILE_OK;
  }

  // Gets entry info by path synchronously.
  scoped_ptr<DriveEntryProto> GetEntryInfoByPathSync(
      const base::FilePath& file_path) {
    DriveFileError error = DRIVE_FILE_ERROR_FAILED;
    scoped_ptr<DriveEntryProto> entry_proto;
    file_system_->GetEntryInfoByPath(
        file_path,
        google_apis::test_util::CreateCopyResultCallback(&error, &entry_proto));
    google_apis::test_util::RunBlockingPoolTask();

    return entry_proto.Pass();
  }

  // Gets directory info by path synchronously.
  scoped_ptr<DriveEntryProtoVector> ReadDirectoryByPathSync(
      const base::FilePath& file_path) {
    DriveFileError error = DRIVE_FILE_ERROR_FAILED;
    bool unused_hide_hosted_documents;
    scoped_ptr<DriveEntryProtoVector> entries;
    file_system_->ReadDirectoryByPath(
        file_path,
        google_apis::test_util::CreateCopyResultCallback(
            &error, &unused_hide_hosted_documents, &entries));
    google_apis::test_util::RunBlockingPoolTask();

    return entries.Pass();
  }

  // Returns true if an entry exists at |file_path|.
  bool EntryExists(const base::FilePath& file_path) {
    return GetEntryInfoByPathSync(file_path).get();
  }


  // Gets the resource ID of |file_path|. Returns an empty string if not found.
  std::string GetResourceIdByPath(const base::FilePath& file_path) {
    scoped_ptr<DriveEntryProto> entry_proto =
        GetEntryInfoByPathSync(file_path);
    if (entry_proto.get())
      return entry_proto->resource_id();
    else
      return "";
  }

  // Helper function to call GetCacheEntry from origin thread.
  bool GetCacheEntryFromOriginThread(const std::string& resource_id,
                                     const std::string& md5,
                                     DriveCacheEntry* cache_entry) {
    bool result = false;
    cache_->GetCacheEntry(resource_id, md5,
                          google_apis::test_util::CreateCopyResultCallback(
                              &result, cache_entry));
    google_apis::test_util::RunBlockingPoolTask();
    return result;
  }

  // Returns true if the cache entry exists for the given resource ID and MD5.
  bool CacheEntryExists(const std::string& resource_id,
                        const std::string& md5) {
    DriveCacheEntry cache_entry;
    return GetCacheEntryFromOriginThread(resource_id, md5, &cache_entry);
  }

  // Flag for specifying the timestamp of the test filesystem cache.
  enum SaveTestFileSystemParam {
    USE_OLD_TIMESTAMP,
    USE_SERVER_TIMESTAMP,
  };

  // Saves a file representing a filesystem with directories:
  // drive, drive/Dir1, drive/Dir1/SubDir2
  // and files
  // drive/File1, drive/Dir1/File2, drive/Dir1/SubDir2/File3.
  // If |use_up_to_date_timestamp| is true, sets the changestamp to 654321,
  // equal to that of "account_metadata.json" test data, indicating the cache is
  // holding the latest file system info.
  bool SaveTestFileSystem(SaveTestFileSystemParam param) {
    // Destroy the existing resource metadata to close DB.
    resource_metadata_.reset();

    const std::string root_resource_id =
        fake_drive_service_->GetRootResourceId();
    scoped_ptr<DriveResourceMetadata, test_util::DestroyHelperForTests>
        resource_metadata(new DriveResourceMetadata(
            root_resource_id,
            cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_META),
            blocking_task_runner_));

    DriveFileError error = DRIVE_FILE_ERROR_FAILED;
    resource_metadata->Initialize(
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    if (error != DRIVE_FILE_OK)
      return false;

    resource_metadata->SetLargestChangestamp(
        param == USE_SERVER_TIMESTAMP ? 654321 : 1,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    if (error != DRIVE_FILE_OK)
      return false;

    // drive/File1
    DriveEntryProto file1;
    file1.set_title("File1");
    file1.set_resource_id("resource_id:File1");
    file1.set_parent_resource_id(root_resource_id);
    file1.set_upload_url("http://resumable-edit-media/1");
    file1.mutable_file_specific_info()->set_file_md5("md5");
    file1.mutable_file_info()->set_is_directory(false);
    file1.mutable_file_info()->set_size(1048576);
    base::FilePath file_path;
    resource_metadata->AddEntry(
        file1,
        google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
    google_apis::test_util::RunBlockingPoolTask();
    if (error != DRIVE_FILE_OK)
      return false;

    // drive/Dir1
    DriveEntryProto dir1;
    dir1.set_title("Dir1");
    dir1.set_resource_id("resource_id:Dir1");
    dir1.set_parent_resource_id(root_resource_id);
    dir1.set_upload_url("http://resumable-create-media/2");
    dir1.mutable_file_info()->set_is_directory(true);
    resource_metadata->AddEntry(
        dir1,
        google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
    google_apis::test_util::RunBlockingPoolTask();
    if (error != DRIVE_FILE_OK)
      return false;

    // drive/Dir1/File2
    DriveEntryProto file2;
    file2.set_title("File2");
    file2.set_resource_id("resource_id:File2");
    file2.set_parent_resource_id(dir1.resource_id());
    file2.set_upload_url("http://resumable-edit-media/2");
    file2.mutable_file_specific_info()->set_file_md5("md5");
    file2.mutable_file_info()->set_is_directory(false);
    file2.mutable_file_info()->set_size(555);
    resource_metadata->AddEntry(
        file2,
        google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
    google_apis::test_util::RunBlockingPoolTask();
    if (error != DRIVE_FILE_OK)
      return false;

    // drive/Dir1/SubDir2
    DriveEntryProto dir2;
    dir2.set_title("SubDir2");
    dir2.set_resource_id("resource_id:SubDir2");
    dir2.set_parent_resource_id(dir1.resource_id());
    dir2.set_upload_url("http://resumable-create-media/3");
    dir2.mutable_file_info()->set_is_directory(true);
    resource_metadata->AddEntry(
        dir2,
        google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
    google_apis::test_util::RunBlockingPoolTask();
    if (error != DRIVE_FILE_OK)
      return false;

    // drive/Dir1/SubDir2/File3
    DriveEntryProto file3;
    file3.set_title("File3");
    file3.set_resource_id("resource_id:File3");
    file3.set_parent_resource_id(dir2.resource_id());
    file3.set_upload_url("http://resumable-edit-media/3");
    file3.mutable_file_specific_info()->set_file_md5("md5");
    file3.mutable_file_info()->set_is_directory(false);
    file3.mutable_file_info()->set_size(12345);
    resource_metadata->AddEntry(
        file3,
        google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
    google_apis::test_util::RunBlockingPoolTask();
    if (error != DRIVE_FILE_OK)
      return false;

    // Write resource metadata.
    resource_metadata->MaybeSave();
    google_apis::test_util::RunBlockingPoolTask();

    // Recreate resource metadata.
    SetUpResourceMetadataAndFileSystem();

    return true;
  }

  // Verifies that |file_path| is a valid JSON file for the hosted document
  // associated with |entry| (i.e. |url| and |resource_id| match).
  void VerifyHostedDocumentJSONFile(const DriveEntryProto& entry_proto,
                                    const base::FilePath& file_path) {
    std::string error;
    JSONFileValueSerializer serializer(file_path);
    scoped_ptr<Value> value(serializer.Deserialize(NULL, &error));
    ASSERT_TRUE(value.get()) << "Parse error " << file_path.value()
                             << ": " << error;
    DictionaryValue* dict_value = NULL;
    ASSERT_TRUE(value->GetAsDictionary(&dict_value));

    std::string edit_url, resource_id;
    EXPECT_TRUE(dict_value->GetString("url", &edit_url));
    EXPECT_TRUE(dict_value->GetString("resource_id", &resource_id));

    EXPECT_EQ(entry_proto.file_specific_info().alternate_url(),
              edit_url);
    EXPECT_EQ(entry_proto.resource_id(), resource_id);
  }

  MessageLoopForUI message_loop_;
  // The order of the test threads is important, do not change the order.
  // See also content/browser/browser_thread_impl.cc.
  content::TestBrowserThread ui_thread_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<DriveCache, test_util::DestroyHelperForTests> cache_;
  scoped_ptr<google_apis::DriveUploaderInterface> uploader_;
  scoped_ptr<DriveFileSystem> file_system_;
  scoped_ptr<google_apis::FakeDriveService> fake_drive_service_;
  scoped_ptr<DriveWebAppsRegistry> drive_webapps_registry_;
  scoped_ptr<DriveResourceMetadata, test_util::DestroyHelperForTests>
      resource_metadata_;
  scoped_ptr<FakeFreeDiskSpaceGetter> fake_free_disk_space_getter_;
  scoped_ptr<StrictMock<MockDriveCacheObserver> > mock_cache_observer_;
  scoped_ptr<StrictMock<MockDirectoryChangeObserver> > mock_directory_observer_;

  bool expected_success_;
  std::string expected_file_extension_;
  int root_feed_changestamp_;
};

void AsyncInitializationCallback(
    int* counter,
    int expected_counter,
    MessageLoop* message_loop,
    DriveFileError error,
    bool hide_hosted_documents,
    scoped_ptr<DriveEntryProtoVector> entries) {
  ASSERT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entries.get());
  ASSERT_FALSE(entries->empty());

  (*counter)++;
  if (*counter >= expected_counter)
    message_loop->Quit();
}

TEST_F(DriveFileSystemTest, DuplicatedAsyncInitialization) {
  // The root directory will be loaded that triggers the event.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  int counter = 0;
  ReadDirectoryWithSettingCallback callback = base::Bind(
      &AsyncInitializationCallback,
      &counter,
      2,
      &message_loop_);

  file_system_->ReadDirectoryByPath(
      base::FilePath(FILE_PATH_LITERAL("drive")), callback);
  file_system_->ReadDirectoryByPath(
      base::FilePath(FILE_PATH_LITERAL("drive")), callback);
  message_loop_.Run();  // Wait to get our result
  EXPECT_EQ(2, counter);

  // ReadDirectoryByPath() was called twice, but the account metadata should
  // only be loaded once. In the past, there was a bug that caused it to be
  // loaded twice.
  EXPECT_EQ(1, fake_drive_service_->about_resource_load_count());
  // On the other hand, the resource list could be loaded twice. One for
  // just the directory contents, and one for the entire resource list.
  //
  // The |callback| function gets called back soon after the directory content
  // is loaded, and the full resource load is done in background asynchronously.
  // So it depends on timing whether we receive the full resource load request
  // at this point.
  EXPECT_TRUE(fake_drive_service_->resource_list_load_count() == 1 ||
              fake_drive_service_->resource_list_load_count() == 2)
      << ": " << fake_drive_service_->resource_list_load_count();
}

TEST_F(DriveFileSystemTest, GetRootEntry) {
  const base::FilePath kFilePath = base::FilePath(FILE_PATH_LITERAL("drive"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ(fake_drive_service_->GetRootResourceId(), entry->resource_id());

  // Getting the root entry should not cause the resource load to happen.
  EXPECT_EQ(0, fake_drive_service_->about_resource_load_count());
  EXPECT_EQ(0, fake_drive_service_->resource_list_load_count());
}

TEST_F(DriveFileSystemTest, GetNonRootEntry) {
  const base::FilePath kFilePath =
      base::FilePath(FILE_PATH_LITERAL("drive/whatever.txt"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  // The entry should not exist as the resource metadata only contains the
  // root entry now.
  ASSERT_FALSE(entry.get());

  // The resource load should happen because non-root entry is requested.
  EXPECT_EQ(1, fake_drive_service_->about_resource_load_count());
  EXPECT_EQ(1, fake_drive_service_->resource_list_load_count());
}

TEST_F(DriveFileSystemTest, SearchRootDirectory) {
  ASSERT_TRUE(LoadRootFeedDocument());

  const base::FilePath kFilePath = base::FilePath(FILE_PATH_LITERAL("drive"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ(fake_drive_service_->GetRootResourceId(), entry->resource_id());

  // The changestamp should be propagated to the root directory.
  EXPECT_EQ(fake_drive_service_->largest_changestamp(),
            entry->directory_specific_info().changestamp());
}

TEST_F(DriveFileSystemTest, SearchExistingFile) {
  ASSERT_TRUE(LoadRootFeedDocument());

  const base::FilePath kFilePath = base::FilePath(
      FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file:2_file_resource_id", entry->resource_id());
}

TEST_F(DriveFileSystemTest, SearchExistingDocument) {
  ASSERT_TRUE(LoadRootFeedDocument());

  const base::FilePath kFilePath = base::FilePath(
      FILE_PATH_LITERAL("drive/Document 1.gdoc"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("document:5_document_resource_id", entry->resource_id());
}

TEST_F(DriveFileSystemTest, SearchNonExistingFile) {
  ASSERT_TRUE(LoadRootFeedDocument());

  const base::FilePath kFilePath = base::FilePath(
      FILE_PATH_LITERAL("drive/nonexisting.file"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_FALSE(entry.get());
}

TEST_F(DriveFileSystemTest, SearchEncodedFileNames) {
  ASSERT_TRUE(LoadRootFeedDocument());

  const base::FilePath kFilePath1 = base::FilePath(
      FILE_PATH_LITERAL("drive/Slash / in file 1.txt"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath1);
  ASSERT_FALSE(entry.get());

  const base::FilePath kFilePath2 = base::FilePath::FromUTF8Unsafe(
      "drive/Slash \xE2\x88\x95 in file 1.txt");
  entry = GetEntryInfoByPathSync(kFilePath2);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file:slash_file_resource_id", entry->resource_id());

  const base::FilePath kFilePath3 = base::FilePath::FromUTF8Unsafe(
      "drive/Slash \xE2\x88\x95 in directory/Slash SubDir File.txt");
  entry = GetEntryInfoByPathSync(kFilePath3);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file:slash_subdir_file", entry->resource_id());
}

TEST_F(DriveFileSystemTest, SearchEncodedFileNamesLoadingRoot) {
  ASSERT_TRUE(LoadRootFeedDocument());

  const base::FilePath kFilePath1 = base::FilePath(
      FILE_PATH_LITERAL("drive/Slash / in file 1.txt"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath1);
  ASSERT_FALSE(entry.get());

  const base::FilePath kFilePath2 = base::FilePath::FromUTF8Unsafe(
      "drive/Slash \xE2\x88\x95 in file 1.txt");
  entry = GetEntryInfoByPathSync(kFilePath2);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file:slash_file_resource_id", entry->resource_id());

  const base::FilePath kFilePath3 = base::FilePath::FromUTF8Unsafe(
      "drive/Slash \xE2\x88\x95 in directory/Slash SubDir File.txt");
  entry = GetEntryInfoByPathSync(kFilePath3);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file:slash_subdir_file", entry->resource_id());
}

TEST_F(DriveFileSystemTest, SearchDuplicateNames) {
  ASSERT_TRUE(LoadRootFeedDocument());

  const base::FilePath kFilePath1 = base::FilePath(
      FILE_PATH_LITERAL("drive/Duplicate Name.txt"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath1);
  ASSERT_TRUE(entry.get());
  const std::string resource_id1 = entry->resource_id();

  const base::FilePath kFilePath2 = base::FilePath(
      FILE_PATH_LITERAL("drive/Duplicate Name (2).txt"));
  entry = GetEntryInfoByPathSync(kFilePath2);
  ASSERT_TRUE(entry.get());
  const std::string resource_id2 = entry->resource_id();

  // The entries are de-duped non-deterministically, so we shouldn't rely on the
  // names matching specific resource ids.
  const std::string file3_resource_id = "file:3_file_resource_id";
  const std::string file4_resource_id = "file:4_file_resource_id";
  EXPECT_TRUE(file3_resource_id == resource_id1 ||
              file3_resource_id == resource_id2);
  EXPECT_TRUE(file4_resource_id == resource_id1 ||
              file4_resource_id == resource_id2);
}

TEST_F(DriveFileSystemTest, SearchExistingDirectory) {
  ASSERT_TRUE(LoadRootFeedDocument());

  const base::FilePath kFilePath = base::FilePath(
      FILE_PATH_LITERAL("drive/Directory 1"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  ASSERT_EQ("folder:1_folder_resource_id", entry->resource_id());

  // The changestamp should be propagated to the directory.
  EXPECT_EQ(fake_drive_service_->largest_changestamp(),
            entry->directory_specific_info().changestamp());
}

TEST_F(DriveFileSystemTest, SearchInSubdir) {
  ASSERT_TRUE(LoadRootFeedDocument());

  const base::FilePath kFilePath = base::FilePath(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  ASSERT_EQ("file:subdirectory_file_1_id", entry->resource_id());
}

// Check the reconstruction of the directory structure from only the root feed.
TEST_F(DriveFileSystemTest, SearchInSubSubdir) {
  ASSERT_TRUE(LoadRootFeedDocument());

  const base::FilePath kFilePath = base::FilePath(
      FILE_PATH_LITERAL("drive/Directory 1/Sub Directory Folder/"
                        "Sub Sub Directory Folder"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  ASSERT_EQ("folder:sub_sub_directory_folder_id", entry->resource_id());
}

TEST_F(DriveFileSystemTest, ReadDirectoryByPath_Root) {
  // The root directory will be loaded that triggers the event.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  // ReadDirectoryByPath() should kick off the resource list loading.
  scoped_ptr<DriveEntryProtoVector> entries(
      ReadDirectoryByPathSync(base::FilePath::FromUTF8Unsafe("drive")));
  // The root directory should be read correctly.
  ASSERT_TRUE(entries.get());
  EXPECT_EQ(8U, entries->size());
}

TEST_F(DriveFileSystemTest, ReadDirectoryByPath_NonRootDirectory) {
  // ReadDirectoryByPath() should kick off the resource list loading.
  scoped_ptr<DriveEntryProtoVector> entries(
      ReadDirectoryByPathSync(
          base::FilePath::FromUTF8Unsafe("drive/Directory 1")));
  // The non root directory should also be read correctly.
  // There was a bug (crbug.com/181487), which broke this behavior.
  // Make sure this is fixed.
  ASSERT_TRUE(entries.get());
  EXPECT_EQ(3U, entries->size());
}

TEST_F(DriveFileSystemTest, FilePathTests) {
  ASSERT_TRUE(LoadRootFeedDocument());

  EXPECT_TRUE(
      EntryExists(base::FilePath(FILE_PATH_LITERAL("drive/File 1.txt"))));
  EXPECT_TRUE(
      EntryExists(base::FilePath(FILE_PATH_LITERAL("drive/Directory 1"))));
  EXPECT_TRUE(EntryExists(
      base::FilePath(
          FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"))));
}

TEST_F(DriveFileSystemTest, ChangeFeed_AddAndDeleteFileInRoot) {
  ASSERT_TRUE(LoadRootFeedDocument());

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(2);

  ASSERT_TRUE(LoadChangeFeed("chromeos/gdata/delta_file_added_in_root.json"));
  EXPECT_TRUE(
      EntryExists(base::FilePath(FILE_PATH_LITERAL("drive/Added file.gdoc"))));

  ASSERT_TRUE(LoadChangeFeed("chromeos/gdata/delta_file_deleted_in_root.json"));
  EXPECT_FALSE(
      EntryExists(base::FilePath(FILE_PATH_LITERAL("drive/Added file.gdoc"))));
}


TEST_F(DriveFileSystemTest, ChangeFeed_AddAndDeleteFileFromExistingDirectory) {
  ASSERT_TRUE(LoadRootFeedDocument());

  EXPECT_TRUE(
      EntryExists(base::FilePath(FILE_PATH_LITERAL("drive/Directory 1"))));

  // Add file to an existing directory.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  ASSERT_TRUE(
      LoadChangeFeed("chromeos/gdata/delta_file_added_in_directory.json"));
  EXPECT_TRUE(EntryExists(base::FilePath(
      FILE_PATH_LITERAL("drive/Directory 1/Added file.gdoc"))));

  // Remove that file from the directory.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  ASSERT_TRUE(
      LoadChangeFeed("chromeos/gdata/delta_file_deleted_in_directory.json"));
  EXPECT_TRUE(
      EntryExists(base::FilePath(FILE_PATH_LITERAL("drive/Directory 1"))));
  EXPECT_FALSE(EntryExists(base::FilePath(
      FILE_PATH_LITERAL("drive/Directory 1/Added file.gdoc"))));
}

TEST_F(DriveFileSystemTest, ChangeFeed_AddFileToNewDirectory) {
  ASSERT_TRUE(LoadRootFeedDocument());
  // Add file to a new directory.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive/New Directory"))))).Times(1);

  ASSERT_TRUE(
      LoadChangeFeed("chromeos/gdata/delta_file_added_in_new_directory.json"));

  EXPECT_TRUE(
      EntryExists(base::FilePath(FILE_PATH_LITERAL("drive/New Directory"))));
  EXPECT_TRUE(EntryExists(base::FilePath(
      FILE_PATH_LITERAL("drive/New Directory/File in new dir.gdoc"))));
}

TEST_F(DriveFileSystemTest, ChangeFeed_AddFileToNewButDeletedDirectory) {
  ASSERT_TRUE(LoadRootFeedDocument());

  // This feed contains the following updates:
  // 1) A new PDF file is added to a new directory
  // 2) but the new directory is marked "deleted" (i.e. moved to Trash)
  // Hence, the PDF file should be just ignored.
  ASSERT_TRUE(LoadChangeFeed(
      "chromeos/gdata/delta_file_added_in_new_but_deleted_directory.json"));
}

TEST_F(DriveFileSystemTest, ChangeFeed_DirectoryMovedFromRootToDirectory) {
  ASSERT_TRUE(LoadRootFeedDocument());

  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder/Sub Sub Directory Folder"))));

  // This will move "Directory 1" from "drive/" to "drive/Directory 2/".
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive/Directory 2"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive/Directory 2/Directory 1")))))
      .Times(1);
  ASSERT_TRUE(LoadChangeFeed(
      "chromeos/gdata/delta_dir_moved_from_root_to_directory.json"));

  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2"))));
  EXPECT_FALSE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2/Directory 1"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2/Directory 1/SubDirectory File 1.txt"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2/Directory 1/Sub Directory Folder"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2/Directory 1/Sub Directory Folder/"
      "Sub Sub Directory Folder"))));
}

TEST_F(DriveFileSystemTest, ChangeFeed_FileMovedFromDirectoryToRoot) {
  ASSERT_TRUE(LoadRootFeedDocument());

  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder/Sub Sub Directory Folder"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  ASSERT_TRUE(LoadChangeFeed(
      "chromeos/gdata/delta_file_moved_from_directory_to_root.json"));

  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder/Sub Sub Directory Folder"))));
  EXPECT_FALSE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/SubDirectory File 1.txt"))));
}

TEST_F(DriveFileSystemTest, ChangeFeed_FileRenamedInDirectory) {
  ASSERT_TRUE(LoadRootFeedDocument());

  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  ASSERT_TRUE(LoadChangeFeed(
      "chromeos/gdata/delta_file_renamed_in_directory.json"));

  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_FALSE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/New SubDirectory File 1.txt"))));
}

TEST_F(DriveFileSystemTest, CachedFeedLoading) {
  ASSERT_TRUE(SaveTestFileSystem(USE_OLD_TIMESTAMP));
  // Tests that cached data can be loaded even if the server is not reachable.
  fake_drive_service_->set_offline(true);

  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL("drive/File1"))));
  EXPECT_TRUE(EntryExists(base::FilePath(FILE_PATH_LITERAL("drive/Dir1"))));
  EXPECT_TRUE(
      EntryExists(base::FilePath(FILE_PATH_LITERAL("drive/Dir1/File2"))));
  EXPECT_TRUE(
      EntryExists(base::FilePath(FILE_PATH_LITERAL("drive/Dir1/SubDir2"))));
  EXPECT_TRUE(EntryExists(
      base::FilePath(FILE_PATH_LITERAL("drive/Dir1/SubDir2/File3"))));
}

TEST_F(DriveFileSystemTest, CachedFeedLoadingThenServerFeedLoading) {
  ASSERT_TRUE(SaveTestFileSystem(USE_SERVER_TIMESTAMP));

  // Kicks loading of cached file system and query for server update.
  EXPECT_TRUE(ReadDirectoryByPathSync(util::GetDriveMyDriveRootPath()));

  // SaveTestFileSystem and "account_metadata.json" have the same changestamp,
  // so no request for new feeds (i.e., call to GetResourceList) should happen.
  EXPECT_EQ(1, fake_drive_service_->about_resource_load_count());
  EXPECT_EQ(0, fake_drive_service_->resource_list_load_count());


  // Since the file system has verified that it holds the latest snapshot,
  // it should change its state to INITIALIZED, which admits periodic refresh.
  // To test it, call CheckForUpdates and verify it does try to check updates.
  file_system_->CheckForUpdates();
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(2, fake_drive_service_->about_resource_load_count());
}

TEST_F(DriveFileSystemTest, OfflineCachedFeedLoading) {
  ASSERT_TRUE(SaveTestFileSystem(USE_OLD_TIMESTAMP));

  // Make GetResourceList fail for simulating offline situation. This will leave
  // the file system "loaded from cache, but not synced with server" state.
  fake_drive_service_->set_offline(true);

  // Kicks loading of cached file system and query for server update.
  EXPECT_TRUE(ReadDirectoryByPathSync(util::GetDriveMyDriveRootPath()));
  // Loading of account metadata should not happen as it's offline.
  EXPECT_EQ(0, fake_drive_service_->account_metadata_load_count());

  // Since the file system has at least succeeded to load cached snapshot,
  // the file system should be able to start periodic refresh.
  // To test it, call CheckForUpdates and verify it does try to check
  // updates, which will cause directory changes.
  fake_drive_service_->set_offline(false);

  file_system_->CheckForUpdates();
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(_))
      .Times(AtLeast(1));

  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(1, fake_drive_service_->about_resource_load_count());
  EXPECT_EQ(1, fake_drive_service_->resource_list_load_count());
}

TEST_F(DriveFileSystemTest, TransferFileFromLocalToRemote_RegularFile) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  ASSERT_TRUE(LoadRootFeedDocument());

  // We'll add a file to the Drive root directory.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  // Prepare a local file.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath local_src_file_path =
      temp_dir.path().AppendASCII("local.txt");
  const std::string kContent = "hello";
  file_util::WriteFile(local_src_file_path, kContent.data(), kContent.size());

  // Confirm that the remote file does not exist.
  const base::FilePath remote_dest_file_path(
      FILE_PATH_LITERAL("drive/remote.txt"));
  EXPECT_FALSE(EntryExists(remote_dest_file_path));

  // Transfer the local file to Drive.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  file_system_->TransferFileFromLocalToRemote(
      local_src_file_path,
      remote_dest_file_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, error);

  // Now the remote file should exist.
  EXPECT_TRUE(EntryExists(remote_dest_file_path));
}

TEST_F(DriveFileSystemTest, TransferFileFromLocalToRemote_HostedDocument) {
  ASSERT_TRUE(LoadRootFeedDocument());

  // Prepare a local file, which is a json file of a hosted document, which
  // matches "Document 1" in root_feed.json.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath local_src_file_path =
      temp_dir.path().AppendASCII("local.gdoc");
  const std::string kEditUrl =
      "https://3_document_self_link/document:5_document_resource_id";
  const std::string kResourceId = "document:5_document_resource_id";
  const std::string kContent =
      base::StringPrintf("{\"url\": \"%s\", \"resource_id\": \"%s\"}",
                         kEditUrl.c_str(), kResourceId.c_str());
  file_util::WriteFile(local_src_file_path, kContent.data(), kContent.size());

  // Confirm that the remote file does not exist.
  const base::FilePath remote_dest_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/Document 1.gdoc"));
  EXPECT_FALSE(EntryExists(remote_dest_file_path));

  // We'll add a file to the Drive root and then move to "Directory 1".
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);

  // We'll copy a hosted document using CopyHostedDocument.
  // ".gdoc" suffix should be stripped when copying.
  scoped_ptr<base::Value> value =
      google_apis::test_util::LoadJSONFile(
          "chromeos/gdata/uploaded_document.json");
  scoped_ptr<google_apis::ResourceEntry> resource_entry =
      google_apis::ResourceEntry::ExtractAndParse(*value);


  // Transfer the local file to Drive.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  file_system_->TransferFileFromLocalToRemote(
      local_src_file_path,
      remote_dest_file_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, error);

  // Now the remote file should exist.
  EXPECT_TRUE(EntryExists(remote_dest_file_path));
}

TEST_F(DriveFileSystemTest, TransferFileFromRemoteToLocal_RegularFile) {
  ASSERT_TRUE(LoadRootFeedDocument());

  // The transfered file is cached and the change of "offline available"
  // attribute is notified.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath local_dest_file_path =
      temp_dir.path().AppendASCII("local_copy.txt");

  base::FilePath remote_src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> file = GetEntryInfoByPathSync(
      remote_src_file_path);
  const int64 file_size = file->file_info().size();

  // Pretend we have enough space.
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      file_size + kMinFreeSpace);

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  file_system_->TransferFileFromRemoteToLocal(
      remote_src_file_path,
      local_dest_file_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, error);

  // The content is "x"s of the file size.
  base::FilePath cache_file_path;
  cache_->GetFile(file->resource_id(),
                  file->file_specific_info().file_md5(),
                  google_apis::test_util::CreateCopyResultCallback(
                      &error, &cache_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  const std::string kExpectedContent = "xxxxxxxxxx";
  std::string cache_file_data;
  EXPECT_TRUE(file_util::ReadFileToString(cache_file_path, &cache_file_data));
  EXPECT_EQ(kExpectedContent, cache_file_data);

  std::string local_dest_file_data;
  EXPECT_TRUE(file_util::ReadFileToString(local_dest_file_path,
                                          &local_dest_file_data));
  EXPECT_EQ(kExpectedContent, local_dest_file_data);
}

TEST_F(DriveFileSystemTest, TransferFileFromRemoteToLocal_HostedDocument) {
  ASSERT_TRUE(LoadRootFeedDocument());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath local_dest_file_path =
      temp_dir.path().AppendASCII("local_copy.txt");
  base::FilePath remote_src_file_path(
      FILE_PATH_LITERAL("drive/Document 1.gdoc"));
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  file_system_->TransferFileFromRemoteToLocal(
      remote_src_file_path,
      local_dest_file_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, error);

  scoped_ptr<DriveEntryProto> entry_proto = GetEntryInfoByPathSync(
      remote_src_file_path);
  ASSERT_TRUE(entry_proto.get());
  VerifyHostedDocumentJSONFile(*entry_proto, local_dest_file_path);
}

TEST_F(DriveFileSystemTest, CopyNotExistingFile) {
  base::FilePath src_file_path(FILE_PATH_LITERAL("drive/Dummy file.txt"));
  base::FilePath dest_file_path(FILE_PATH_LITERAL("drive/Test.log"));

  ASSERT_TRUE(LoadRootFeedDocument());

  EXPECT_FALSE(EntryExists(src_file_path));

  DriveFileError error = DRIVE_FILE_OK;
  file_system_->Copy(
      src_file_path,
      dest_file_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_FALSE(EntryExists(dest_file_path));
}

TEST_F(DriveFileSystemTest, CopyFileToNonExistingDirectory) {
  base::FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  base::FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Dummy"));
  base::FilePath dest_file_path(FILE_PATH_LITERAL("drive/Dummy/Test.log"));

  ASSERT_TRUE(LoadRootFeedDocument());

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<DriveEntryProto> src_entry_proto = GetEntryInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_entry_proto.get());
  std::string src_file_path_resource_id =
      src_entry_proto->resource_id();
  EXPECT_FALSE(src_entry_proto->edit_url().empty());

  EXPECT_FALSE(EntryExists(dest_parent_path));

  DriveFileError error = DRIVE_FILE_OK;
  file_system_->Move(
      src_file_path,
      dest_file_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);

  EXPECT_TRUE(EntryExists(src_file_path));
  EXPECT_FALSE(EntryExists(dest_parent_path));
  EXPECT_FALSE(EntryExists(dest_file_path));
}

// Test the case where the parent of |dest_file_path| is an existing file,
// not a directory.
TEST_F(DriveFileSystemTest, CopyFileToInvalidPath) {
  base::FilePath src_file_path(FILE_PATH_LITERAL("drive/Document 1.gdoc"));
  base::FilePath dest_parent_path(
      FILE_PATH_LITERAL("drive/Duplicate Name.txt"));
  base::FilePath dest_file_path(FILE_PATH_LITERAL(
      "drive/Duplicate Name.txt/Document 1.gdoc"));

  ASSERT_TRUE(LoadRootFeedDocument());

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<DriveEntryProto> src_entry_proto = GetEntryInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_entry_proto.get());
  std::string src_file_resource_id =
      src_entry_proto->resource_id();
  EXPECT_FALSE(src_entry_proto->edit_url().empty());

  ASSERT_TRUE(EntryExists(dest_parent_path));
  scoped_ptr<DriveEntryProto> dest_entry_proto = GetEntryInfoByPathSync(
      dest_parent_path);
  ASSERT_TRUE(dest_entry_proto.get());

  DriveFileError error = DRIVE_FILE_OK;
  file_system_->Copy(
      src_file_path,
      dest_file_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_A_DIRECTORY, error);

  EXPECT_TRUE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(dest_parent_path));

  EXPECT_FALSE(EntryExists(dest_file_path));
}

TEST_F(DriveFileSystemTest, RenameFile) {
  const base::FilePath src_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
  const base::FilePath src_parent_path(FILE_PATH_LITERAL("drive/Directory 1"));
  const base::FilePath dest_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/Test.log"));

  ASSERT_TRUE(LoadRootFeedDocument());

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<DriveEntryProto> src_entry_proto = GetEntryInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_entry_proto.get());
  std::string src_file_resource_id =
      src_entry_proto->resource_id();

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  file_system_->Move(
      src_file_path,
      dest_file_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(dest_file_path));
  EXPECT_EQ(src_file_resource_id, GetResourceIdByPath(dest_file_path));
}

TEST_F(DriveFileSystemTest, MoveFileFromRootToSubDirectory) {
  base::FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  base::FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Directory 1"));
  base::FilePath dest_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/Test.log"));

  ASSERT_TRUE(LoadRootFeedDocument());

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<DriveEntryProto> src_entry_proto = GetEntryInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_entry_proto.get());
  std::string src_file_resource_id =
      src_entry_proto->resource_id();
  EXPECT_FALSE(src_entry_proto->edit_url().empty());

  ASSERT_TRUE(EntryExists(dest_parent_path));
  scoped_ptr<DriveEntryProto> dest_parent_proto = GetEntryInfoByPathSync(
      dest_parent_path);
  ASSERT_TRUE(dest_parent_proto.get());
  ASSERT_TRUE(dest_parent_proto->file_info().is_directory());
  EXPECT_FALSE(dest_parent_proto->download_url().empty());

  // Expect notification for both source and destination directories.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  file_system_->Move(
      src_file_path,
      dest_file_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(dest_file_path));
  EXPECT_EQ(src_file_resource_id, GetResourceIdByPath(dest_file_path));
}

TEST_F(DriveFileSystemTest, MoveFileFromSubDirectoryToRoot) {
  base::FilePath src_parent_path(FILE_PATH_LITERAL("drive/Directory 1"));
  base::FilePath src_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
  base::FilePath dest_file_path(FILE_PATH_LITERAL("drive/Test.log"));

  ASSERT_TRUE(LoadRootFeedDocument());

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<DriveEntryProto> src_entry_proto = GetEntryInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_entry_proto.get());
  std::string src_file_resource_id =
      src_entry_proto->resource_id();
  EXPECT_FALSE(src_entry_proto->edit_url().empty());

  ASSERT_TRUE(EntryExists(src_parent_path));
  scoped_ptr<DriveEntryProto> src_parent_proto = GetEntryInfoByPathSync(
      src_parent_path);
  ASSERT_TRUE(src_parent_proto.get());
  ASSERT_TRUE(src_parent_proto->file_info().is_directory());
  EXPECT_FALSE(src_parent_proto->download_url().empty());

  // Expect notification for both source and destination directories.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  file_system_->Move(
      src_file_path,
      dest_file_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  EXPECT_FALSE(EntryExists(src_file_path));
  ASSERT_TRUE(EntryExists(dest_file_path));
  EXPECT_EQ(src_file_resource_id, GetResourceIdByPath(dest_file_path));
}

TEST_F(DriveFileSystemTest, MoveFileBetweenSubDirectories) {
  base::FilePath src_parent_path(FILE_PATH_LITERAL("drive/Directory 1"));
  base::FilePath src_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
  base::FilePath dest_parent_path(FILE_PATH_LITERAL("drive/New Folder 1"));
  base::FilePath dest_file_path(
      FILE_PATH_LITERAL("drive/New Folder 1/Test.log"));
  base::FilePath interim_file_path(FILE_PATH_LITERAL("drive/Test.log"));

  ASSERT_TRUE(LoadRootFeedDocument());

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  AddDirectoryFromFile(dest_parent_path,
                       "chromeos/gdata/directory_entry_atom.json");

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<DriveEntryProto> src_entry_proto = GetEntryInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_entry_proto.get());
  std::string src_file_resource_id =
      src_entry_proto->resource_id();
  EXPECT_FALSE(src_entry_proto->edit_url().empty());

  ASSERT_TRUE(EntryExists(src_parent_path));
  scoped_ptr<DriveEntryProto> src_parent_proto = GetEntryInfoByPathSync(
      src_parent_path);
  ASSERT_TRUE(src_parent_proto.get());
  ASSERT_TRUE(src_parent_proto->file_info().is_directory());
  EXPECT_FALSE(src_parent_proto->download_url().empty());

  ASSERT_TRUE(EntryExists(dest_parent_path));
  scoped_ptr<DriveEntryProto> dest_parent_proto = GetEntryInfoByPathSync(
      dest_parent_path);
  ASSERT_TRUE(dest_parent_proto.get());
  ASSERT_TRUE(dest_parent_proto->file_info().is_directory());
  EXPECT_FALSE(dest_parent_proto->download_url().empty());

  EXPECT_FALSE(EntryExists(interim_file_path));

  // Expect notification for both source and destination directories.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive/New Folder 1"))))).Times(1);

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  file_system_->Move(
      src_file_path,
      dest_file_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_FALSE(EntryExists(interim_file_path));

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(dest_file_path));
  EXPECT_EQ(src_file_resource_id, GetResourceIdByPath(dest_file_path));
}

TEST_F(DriveFileSystemTest, MoveNotExistingFile) {
  base::FilePath src_file_path(FILE_PATH_LITERAL("drive/Dummy file.txt"));
  base::FilePath dest_file_path(FILE_PATH_LITERAL("drive/Test.log"));

  ASSERT_TRUE(LoadRootFeedDocument());

  EXPECT_FALSE(EntryExists(src_file_path));

  DriveFileError error = DRIVE_FILE_OK;
  file_system_->Move(
      src_file_path,
      dest_file_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_FALSE(EntryExists(dest_file_path));
}

TEST_F(DriveFileSystemTest, MoveFileToNonExistingDirectory) {
  base::FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  base::FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Dummy"));
  base::FilePath dest_file_path(FILE_PATH_LITERAL("drive/Dummy/Test.log"));

  ASSERT_TRUE(LoadRootFeedDocument());

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<DriveEntryProto> src_entry_proto = GetEntryInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_entry_proto.get());
  std::string src_file_resource_id =
      src_entry_proto->resource_id();
  EXPECT_FALSE(src_entry_proto->edit_url().empty());

  EXPECT_FALSE(EntryExists(dest_parent_path));

  DriveFileError error = DRIVE_FILE_OK;
  file_system_->Move(
      src_file_path,
      dest_file_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);

  EXPECT_FALSE(EntryExists(dest_parent_path));
  EXPECT_FALSE(EntryExists(dest_file_path));
}

// Test the case where the parent of |dest_file_path| is a existing file,
// not a directory.
TEST_F(DriveFileSystemTest, MoveFileToInvalidPath) {
  base::FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  base::FilePath dest_parent_path(
      FILE_PATH_LITERAL("drive/Duplicate Name.txt"));
  base::FilePath dest_file_path(FILE_PATH_LITERAL(
      "drive/Duplicate Name.txt/Test.log"));

  ASSERT_TRUE(LoadRootFeedDocument());

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<DriveEntryProto> src_entry_proto = GetEntryInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_entry_proto.get());
  std::string src_file_resource_id =
      src_entry_proto->resource_id();
  EXPECT_FALSE(src_entry_proto->edit_url().empty());

  ASSERT_TRUE(EntryExists(dest_parent_path));
  scoped_ptr<DriveEntryProto> dest_parent_proto = GetEntryInfoByPathSync(
      dest_parent_path);
  ASSERT_TRUE(dest_parent_proto.get());

  DriveFileError error = DRIVE_FILE_OK;
  file_system_->Move(
      src_file_path,
      dest_file_path,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_A_DIRECTORY, error);

  EXPECT_TRUE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(dest_parent_path));
  EXPECT_FALSE(EntryExists(dest_file_path));
}

TEST_F(DriveFileSystemTest, RemoveEntries) {
  ASSERT_TRUE(LoadRootFeedDocument());

  base::FilePath nonexisting_file(FILE_PATH_LITERAL("drive/Dummy file.txt"));
  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  base::FilePath dir_in_root(FILE_PATH_LITERAL("drive/Directory 1"));
  base::FilePath file_in_subdir(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));

  ASSERT_TRUE(EntryExists(file_in_root));
  scoped_ptr<DriveEntryProto> file_in_root_proto = GetEntryInfoByPathSync(
      file_in_root);
  ASSERT_TRUE(file_in_root_proto.get());

  ASSERT_TRUE(EntryExists(dir_in_root));
  scoped_ptr<DriveEntryProto> dir_in_root_proto = GetEntryInfoByPathSync(
      dir_in_root);
  ASSERT_TRUE(dir_in_root_proto.get());
  ASSERT_TRUE(dir_in_root_proto->file_info().is_directory());

  ASSERT_TRUE(EntryExists(file_in_subdir));
  scoped_ptr<DriveEntryProto> file_in_subdir_proto = GetEntryInfoByPathSync(
      file_in_subdir);
  ASSERT_TRUE(file_in_subdir_proto.get());

  // Once for file in root and once for file...
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(2);

  // Remove first file in root.
  EXPECT_TRUE(RemoveEntry(file_in_root));
  EXPECT_FALSE(EntryExists(file_in_root));
  EXPECT_TRUE(EntryExists(dir_in_root));
  EXPECT_TRUE(EntryExists(file_in_subdir));

  // Remove directory.
  EXPECT_TRUE(RemoveEntry(dir_in_root));
  EXPECT_FALSE(EntryExists(file_in_root));
  EXPECT_FALSE(EntryExists(dir_in_root));
  EXPECT_FALSE(EntryExists(file_in_subdir));

  // Try removing file in already removed subdirectory.
  EXPECT_FALSE(RemoveEntry(file_in_subdir));

  // Try removing non-existing file.
  EXPECT_FALSE(RemoveEntry(nonexisting_file));

  // Try removing root file element.
  EXPECT_FALSE(RemoveEntry(base::FilePath(FILE_PATH_LITERAL("drive"))));

  // Need this to ensure OnDirectoryChanged() is run.
  google_apis::test_util::RunBlockingPoolTask();
}

TEST_F(DriveFileSystemTest, CreateDirectory) {
  ASSERT_TRUE(LoadRootFeedDocument());

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  // Create directory in root.
  base::FilePath dir_path(FILE_PATH_LITERAL("drive/New Folder 1"));
  EXPECT_FALSE(EntryExists(dir_path));
  AddDirectoryFromFile(dir_path, "chromeos/gdata/directory_entry_atom.json");
  EXPECT_TRUE(EntryExists(dir_path));

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive/New Folder 1"))))).Times(1);

  // Create directory in a sub directory.
  base::FilePath subdir_path(
      FILE_PATH_LITERAL("drive/New Folder 1/New Folder 2"));
  EXPECT_FALSE(EntryExists(subdir_path));
  AddDirectoryFromFile(subdir_path,
                       "chromeos/gdata/directory_entry_atom2.json");
  EXPECT_TRUE(EntryExists(subdir_path));
}

// Create a directory through the document service
TEST_F(DriveFileSystemTest, CreateDirectoryWithService) {
  ASSERT_TRUE(LoadRootFeedDocument());
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  file_system_->CreateDirectory(
      base::FilePath(FILE_PATH_LITERAL("drive/Sample Directory Title")),
      false,  // is_exclusive
      true,  // is_recursive
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, error);
}

TEST_F(DriveFileSystemTest, GetFileByPath_FromGData_EnoughSpace) {
  ASSERT_TRUE(LoadRootFeedDocument());

  // The transfered file is cached and the change of "offline available"
  // attribute is notified.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(file_in_root));
  const int64 file_size = entry_proto->file_info().size();

  // Pretend we have enough space.
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      file_size + kMinFreeSpace);

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  base::FilePath file_path;
  std::string mime_type;
  DriveFileType file_type;
  file_system_->GetFileByPath(file_in_root,
                              google_apis::test_util::CreateCopyResultCallback(
                                  &error, &file_path, &mime_type, &file_type));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(REGULAR_FILE, file_type);
}

TEST_F(DriveFileSystemTest, GetFileByPath_FromGData_NoSpaceAtAll) {
  ASSERT_TRUE(LoadRootFeedDocument());

  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(file_in_root));

  // Pretend we have no space at all.
  fake_free_disk_space_getter_->set_fake_free_disk_space(0);

  DriveFileError error = DRIVE_FILE_OK;
  base::FilePath file_path;
  std::string mime_type;
  DriveFileType file_type;
  file_system_->GetFileByPath(file_in_root,
                              google_apis::test_util::CreateCopyResultCallback(
                                  &error, &file_path, &mime_type, &file_type));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_ERROR_NO_SPACE, error);
}

TEST_F(DriveFileSystemTest, GetFileByPath_FromGData_NoEnoughSpaceButCanFreeUp) {
  ASSERT_TRUE(LoadRootFeedDocument());

  // The transfered file is cached and the change of "offline available"
  // attribute is notified.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(file_in_root));
  const int64 file_size = entry_proto->file_info().size();

  // Pretend we have no space first (checked before downloading a file),
  // but then start reporting we have space. This is to emulate that
  // the disk space was freed up by removing temporary files.
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      file_size + kMinFreeSpace);
  fake_free_disk_space_getter_->set_fake_free_disk_space(0);
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      file_size + kMinFreeSpace);
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      file_size + kMinFreeSpace);

  // Store something of the file size in the temporary cache directory.
  const std::string content(file_size, 'x');
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath tmp_file =
      temp_dir.path().AppendASCII("something.txt");
  ASSERT_EQ(file_size,
            file_util::WriteFile(tmp_file, content.data(), content.size()));

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  cache_->Store("<resource_id>", "<md5>", tmp_file,
                DriveCache::FILE_OPERATION_COPY,
                google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(CacheEntryExists("<resource_id>", "<md5>"));

  base::FilePath file_path;
  std::string mime_type;
  DriveFileType file_type;
  file_system_->GetFileByPath(file_in_root,
                              google_apis::test_util::CreateCopyResultCallback(
                                  &error, &file_path, &mime_type, &file_type));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(REGULAR_FILE, file_type);

  // The cache entry should be removed in order to free up space.
  ASSERT_FALSE(CacheEntryExists("<resource_id>", "<md5>"));
}

TEST_F(DriveFileSystemTest, GetFileByPath_FromGData_EnoughSpaceButBecomeFull) {
  ASSERT_TRUE(LoadRootFeedDocument());

  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(file_in_root));
  const int64 file_size = entry_proto->file_info().size();

  // Pretend we have enough space first (checked before downloading a file),
  // but then start reporting we have not enough space. This is to emulate that
  // the disk space becomes full after the file is downloaded for some reason
  // (ex. the actual file was larger than the expected size).
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      file_size + kMinFreeSpace);
  fake_free_disk_space_getter_->set_fake_free_disk_space(kMinFreeSpace - 1);
  fake_free_disk_space_getter_->set_fake_free_disk_space(kMinFreeSpace - 1);

  DriveFileError error = DRIVE_FILE_OK;
  base::FilePath file_path;
  std::string mime_type;
  DriveFileType file_type;
  file_system_->GetFileByPath(file_in_root,
                              google_apis::test_util::CreateCopyResultCallback(
                                  &error, &file_path, &mime_type, &file_type));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_ERROR_NO_SPACE, error);
}

TEST_F(DriveFileSystemTest, GetFileByPath_FromCache) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  ASSERT_TRUE(LoadRootFeedDocument());

  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(file_in_root));

  // Store something as cached version of this file.
  DriveFileError error = DRIVE_FILE_OK;
  cache_->Store(entry_proto->resource_id(),
                entry_proto->file_specific_info().file_md5(),
                google_apis::test_util::GetTestFilePath(
                    "chromeos/gdata/root_feed.json"),
                DriveCache::FILE_OPERATION_COPY,
                google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  base::FilePath file_path;
  std::string mime_type;
  DriveFileType file_type;
  file_system_->GetFileByPath(file_in_root,
                              google_apis::test_util::CreateCopyResultCallback(
                                  &error, &file_path, &mime_type, &file_type));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(REGULAR_FILE, file_type);
}

TEST_F(DriveFileSystemTest, GetFileByPath_HostedDocument) {
  ASSERT_TRUE(LoadRootFeedDocument());

  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/Document 1.gdoc"));
  scoped_ptr<DriveEntryProto> src_entry_proto =
      GetEntryInfoByPathSync(file_in_root);
  ASSERT_TRUE(src_entry_proto.get());

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  base::FilePath file_path;
  std::string mime_type;
  DriveFileType file_type;
  file_system_->GetFileByPath(file_in_root,
                              google_apis::test_util::CreateCopyResultCallback(
                                  &error, &file_path, &mime_type, &file_type));
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(HOSTED_DOCUMENT, file_type);
  EXPECT_FALSE(file_path.empty());

  ASSERT_TRUE(src_entry_proto.get());
  VerifyHostedDocumentJSONFile(*src_entry_proto, file_path);
}

TEST_F(DriveFileSystemTest, GetFileByResourceId) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  // The transfered file is cached and the change of "offline available"
  // attribute is notified.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  ASSERT_TRUE(LoadRootFeedDocument());

  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(file_in_root));

  DriveFileError error = DRIVE_FILE_OK;
  base::FilePath file_path;
  std::string mime_type;
  DriveFileType file_type;
  file_system_->GetFileByResourceId(
      entry_proto->resource_id(),
      DriveClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &mime_type, &file_type),
      google_apis::GetContentCallback());
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(REGULAR_FILE, file_type);
}

TEST_F(DriveFileSystemTest, CancelGetFile) {
  base::FilePath cancel_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  file_system_->CancelGetFile(cancel_file_path);
  EXPECT_EQ(cancel_file_path, fake_drive_service_->last_cancelled_file());
}

TEST_F(DriveFileSystemTest, GetFileByResourceId_FromCache) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  ASSERT_TRUE(LoadRootFeedDocument());

  base::FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(file_in_root));

  // Store something as cached version of this file.
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  cache_->Store(entry_proto->resource_id(),
                entry_proto->file_specific_info().file_md5(),
                google_apis::test_util::GetTestFilePath(
                    "chromeos/gdata/root_feed.json"),
                DriveCache::FILE_OPERATION_COPY,
                google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  // The file is obtained from the cache.
  // Hence the downloading should work even if the drive service is offline.
  fake_drive_service_->set_offline(true);

  base::FilePath file_path;
  std::string mime_type;
  DriveFileType file_type;
  file_system_->GetFileByResourceId(
      entry_proto->resource_id(),
      DriveClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(
          &error, &file_path, &mime_type, &file_type),
      google_apis::GetContentCallback());
  google_apis::test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(REGULAR_FILE, file_type);
}

TEST_F(DriveFileSystemTest, UpdateFileByResourceId_PersistentFile) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  ASSERT_TRUE(LoadRootFeedDocument());

  // This is a file defined in root_feed.json.
  const base::FilePath kFilePath(FILE_PATH_LITERAL("drive/File 1.txt"));
  const std::string kResourceId("file:2_file_resource_id");
  const std::string kMd5("3b4382ebefec6e743578c76bbd0575ce");

  // Pin the file so it'll be store in "persistent" directory.
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(kResourceId, kMd5)).Times(1);
  DriveFileError error = DRIVE_FILE_OK;
  cache_->Pin(kResourceId, kMd5,
              google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  // First store a file to cache.
  cache_->Store(kResourceId,
                kMd5,
                // Anything works.
                google_apis::test_util::GetTestFilePath(
                    "chromeos/gdata/root_feed.json"),
                DriveCache::FILE_OPERATION_COPY,
                google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  // Add the dirty bit.
  cache_->MarkDirty(kResourceId, kMd5,
                    google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  // Commit the dirty bit.
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(kResourceId)).Times(1);
  cache_->CommitDirty(kResourceId, kMd5,
                      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  // We'll notify the directory change to the observer upon completion.
  EXPECT_CALL(*mock_directory_observer_,
              OnDirectoryChanged(Eq(util::GetDriveMyDriveRootPath())))
      .Times(1);

  // Check the number of files in the root directory. We'll compare the
  // number after updating a file.
  scoped_ptr<DriveEntryProtoVector> root_directory_entries(
      ReadDirectoryByPathSync(base::FilePath::FromUTF8Unsafe("drive")));
  ASSERT_TRUE(root_directory_entries.get());
  const int num_files_in_root = CountFiles(*root_directory_entries);

  // The callback will be called upon completion of
  // UpdateFileByResourceId().
  file_system_->UpdateFileByResourceId(
      kResourceId,
      DriveClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  // Make sure that the number of files did not change (i.e. we updated an
  // existing file, rather than adding a new file. The number of files
  // increases if we don't handle the file update right).
  EXPECT_EQ(num_files_in_root, CountFiles(*root_directory_entries));
}

TEST_F(DriveFileSystemTest, UpdateFileByResourceId_NonexistentFile) {
  ASSERT_TRUE(LoadRootFeedDocument());

  // This is nonexistent in root_feed.json.
  const base::FilePath kFilePath(FILE_PATH_LITERAL("drive/Nonexistent.txt"));
  const std::string kResourceId("file:nonexistent_resource_id");
  const std::string kMd5("nonexistent_md5");

  // The callback will be called upon completion of
  // UpdateFileByResourceId().
  DriveFileError error = DRIVE_FILE_OK;
  file_system_->UpdateFileByResourceId(
      kResourceId,
      DriveClientContext(USER_INITIATED),
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
}

TEST_F(DriveFileSystemTest, ContentSearch) {
  ASSERT_TRUE(LoadRootFeedDocument());

  const SearchResultPair kExpectedResults[] = {
    { "drive/Directory 1/Sub Directory Folder/Sub Sub Directory Folder",
      true },
    { "drive/Directory 1/Sub Directory Folder", true },
    { "drive/Directory 1/SubDirectory File 1.txt", false },
    { "drive/Directory 1", true },
    { "drive/Directory 2", true },
  };

  SearchCallback callback = base::Bind(&DriveSearchCallback,
      &message_loop_,
      kExpectedResults, ARRAYSIZE_UNSAFE(kExpectedResults),
      GURL());

  file_system_->Search("Directory", false, GURL(), callback);
  message_loop_.Run();  // Wait to get our result.
}

TEST_F(DriveFileSystemTest, ContentSearchWithNewEntry) {
  ASSERT_TRUE(LoadRootFeedDocument());

  // Create a new directory in the drive service.
  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<google_apis::ResourceEntry> resource_entry;
  fake_drive_service_->AddNewDirectory(
      fake_drive_service_->GetRootResourceId(),  // Add to the root directory.
      "New Directory 1!",
      google_apis::test_util::CreateCopyResultCallback(
          &error, &resource_entry));
  message_loop_.RunUntilIdle();

  // As the result of the first Search(), only entries in the current file
  // system snapshot are expected to be returned (i.e. "New Directory 1!"
  // shouldn't be included in the search result even though it matches
  // "Directory 1".
  const SearchResultPair kExpectedResults[] = {
    { "drive/Directory 1", true }
  };

  // At the same time, unknown entry should trigger delta feed request.
  // This will cause notification to directory observers (e.g., File Browser)
  // so that they can request search again.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(_))
      .Times(AtLeast(1));

  SearchCallback callback = base::Bind(&DriveSearchCallback,
      &message_loop_,
      kExpectedResults, ARRAYSIZE_UNSAFE(kExpectedResults),
      GURL());

  file_system_->Search("\"Directory 1\"", false, GURL(), callback);
  // Make sure all the delayed tasks to complete.
  // message_loop_.Run() can return before the delta feed processing finishes.
  google_apis::test_util::RunBlockingPoolTask();
}

TEST_F(DriveFileSystemTest, ContentSearchEmptyResult) {
  ASSERT_TRUE(LoadRootFeedDocument());

  const SearchResultPair* expected_results = NULL;

  SearchCallback callback = base::Bind(&DriveSearchCallback,
      &message_loop_, expected_results, 0u, GURL());

  file_system_->Search("\"no-match query\"", false, GURL(), callback);
  message_loop_.Run();  // Wait to get our result.
}

TEST_F(DriveFileSystemTest, GetAvailableSpace) {
  DriveFileError error = DRIVE_FILE_OK;
  int64 bytes_total;
  int64 bytes_used;
  file_system_->GetAvailableSpace(
      google_apis::test_util::CreateCopyResultCallback(
          &error, &bytes_total, &bytes_used));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(GG_LONGLONG(6789012345), bytes_used);
  EXPECT_EQ(GG_LONGLONG(9876543210), bytes_total);
}

TEST_F(DriveFileSystemTest, RefreshDirectory) {
  ASSERT_TRUE(LoadRootFeedDocument());

  // We'll notify the directory change to the observer.
  EXPECT_CALL(*mock_directory_observer_,
      OnDirectoryChanged(Eq(util::GetDriveMyDriveRootPath()))).Times(1);

  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  file_system_->RefreshDirectory(
      util::GetDriveMyDriveRootPath(),
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
}

TEST_F(DriveFileSystemTest, OpenAndCloseFile) {
  ASSERT_TRUE(LoadRootFeedDocument());

  // The transfered file is cached and the change of "offline available"
  // attribute is notified.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(base::FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  const base::FilePath kFileInRoot(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(kFileInRoot));
  const int64 file_size = entry_proto->file_info().size();
  const std::string& file_resource_id =
      entry_proto->resource_id();
  const std::string& file_md5 = entry_proto->file_specific_info().file_md5();

  // A dirty file is created on close.
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(file_resource_id))
      .Times(1);

  // Pretend we have enough space.
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      file_size + kMinFreeSpace);

  // Open kFileInRoot ("drive/File 1.txt").
  DriveFileError error = DRIVE_FILE_ERROR_FAILED;
  base::FilePath file_path;
  file_system_->OpenFile(
      kFileInRoot,
      google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
  google_apis::test_util::RunBlockingPoolTask();
  const base::FilePath opened_file_path = file_path;

  // Verify that the file was properly opened.
  EXPECT_EQ(DRIVE_FILE_OK, error);

  // Try to open the already opened file.
  file_system_->OpenFile(
      kFileInRoot,
      google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
  google_apis::test_util::RunBlockingPoolTask();

  // It must fail.
  EXPECT_EQ(DRIVE_FILE_ERROR_IN_USE, error);

  // Verify that the file contents match the expected contents.
  // The content is "x"s of the file size.
  const std::string kExpectedContent = "xxxxxxxxxx";
  std::string cache_file_data;
  EXPECT_TRUE(file_util::ReadFileToString(opened_file_path, &cache_file_data));
  EXPECT_EQ(kExpectedContent, cache_file_data);

  DriveCacheEntry cache_entry;
  EXPECT_TRUE(GetCacheEntryFromOriginThread(file_resource_id, file_md5,
                                            &cache_entry));
  EXPECT_TRUE(cache_entry.is_present());
  EXPECT_TRUE(cache_entry.is_dirty());
  EXPECT_TRUE(cache_entry.is_persistent());

  base::FilePath cache_file_path;
  cache_->GetFile(file_resource_id, file_md5,
                  google_apis::test_util::CreateCopyResultCallback(
                      &error, &cache_file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);
  EXPECT_EQ(cache_file_path, opened_file_path);

  // Close kFileInRoot ("drive/File 1.txt").
  file_system_->CloseFile(
      kFileInRoot,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  // Verify that the file was properly closed.
  EXPECT_EQ(DRIVE_FILE_OK, error);

  // Verify that the cache state was changed as expected.
  EXPECT_TRUE(GetCacheEntryFromOriginThread(file_resource_id, file_md5,
                                            &cache_entry));
  EXPECT_TRUE(cache_entry.is_present());
  EXPECT_TRUE(cache_entry.is_dirty());
  EXPECT_TRUE(cache_entry.is_persistent());

  // Try to close the same file twice.
  file_system_->CloseFile(
      kFileInRoot,
      google_apis::test_util::CreateCopyResultCallback(&error));
  google_apis::test_util::RunBlockingPoolTask();

  // It must fail.
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, error);
}

// TODO(satorux): Testing if WebAppsRegistry is loaded here is awkward. We
// should move this to change_list_loader_unittest.cc. crbug.com/161703
TEST_F(DriveFileSystemTest, WebAppsRegistryIsLoaded) {
  ASSERT_TRUE(SaveTestFileSystem(USE_SERVER_TIMESTAMP));

  // No apps should be found as the webapps registry is empty.
  ScopedVector<DriveWebAppInfo> apps;
  drive_webapps_registry_->GetWebAppsForFile(
      base::FilePath::FromUTF8Unsafe("foo.exe"),
      "" /* mime_type */,
      &apps);
  EXPECT_TRUE(apps.empty());

  // Kicks loading of cached file system and query for server update. This
  // will cause GetAccountMetadata() to be called, to check the server-side
  // changestamp, and the webapps registry will be loaded at the same time.
  EXPECT_TRUE(ReadDirectoryByPathSync(util::GetDriveMyDriveRootPath()));

  // An app for foo.exe should now be found, as the registry was loaded.
  drive_webapps_registry_->GetWebAppsForFile(
      base::FilePath(FILE_PATH_LITERAL("foo.exe")),
      "" /* mime_type */,
      &apps);
  EXPECT_EQ(1U, apps.size());
}

}   // namespace drive
