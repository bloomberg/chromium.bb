// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_file_system.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_api_parser.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/chromeos/gdata/drive_function_remove.h"
#include "chrome/browser/chromeos/gdata/drive_test_util.h"
#include "chrome/browser/chromeos/gdata/drive_uploader.h"
#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"
#include "chrome/browser/chromeos/gdata/mock_directory_change_observer.h"
#include "chrome/browser/chromeos/gdata/mock_drive_cache_observer.h"
#include "chrome/browser/chromeos/gdata/mock_drive_service.h"
#include "chrome/browser/chromeos/gdata/mock_drive_uploader.h"
#include "chrome/browser/chromeos/gdata/mock_drive_web_apps_registry.h"
#include "chrome/browser/chromeos/gdata/mock_free_disk_space_getter.h"
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

namespace gdata {
namespace {

const char kSymLinkToDevNull[] = "/dev/null";

const int64 kLotsOfSpace = kMinFreeSpace * 10;

struct SearchResultPair {
  const char* path;
  const bool is_directory;
};

// Callback to DriveFileSystem::Search used in ContentSearch tests.
// Verifies returned vector of results.
void DriveSearchCallback(
    MessageLoop* message_loop,
    const SearchResultPair* expected_results,
    size_t expected_results_size,
    DriveFileError error,
    const GURL& next_feed,
    scoped_ptr<std::vector<SearchResultInfo> > results) {
  ASSERT_TRUE(results.get());
  ASSERT_EQ(expected_results_size, results->size());

  for (size_t i = 0; i < results->size(); i++) {
    EXPECT_EQ(FilePath(expected_results[i].path),
              results->at(i).path);
    EXPECT_EQ(expected_results[i].is_directory,
              results->at(i).is_directory);
  }

  message_loop->Quit();
}

// Action used to set mock expectations for
// DriveServiceInterface::GetDocumentEntry().
ACTION_P2(MockGetDocumentEntry, status, value) {
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(arg1, status, base::Passed(value)));
}

// Action used to set mock expectations for
// DriveUploaderInterface::UploadExistingFile().
ACTION_P4(MockUploadExistingFile,
          error, drive_path, local_file_path, document_entry) {
  scoped_ptr<DocumentEntry> scoped_document_entry(document_entry);
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(arg5,
                 error,
                 drive_path,
                 local_file_path,
                 base::Passed(&scoped_document_entry)));

  const int kUploadId = 123;
  return kUploadId;
}

// Action used to set mock expectations for
// DriveUploaderInterface::UploadNewFile().
ACTION(MockUploadNewFile) {
  scoped_ptr<base::Value> value =
      test_util::LoadJSONFile("gdata/uploaded_file.json");
  scoped_ptr<DocumentEntry> document_entry(
      DocumentEntry::ExtractAndParse(*value));

  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(arg7,
                 DRIVE_FILE_OK,
                 arg1,
                 arg2,
                 base::Passed(&document_entry)));

  const int kUploadId = 123;
  return kUploadId;
}

// Action used to set mock expectations for
// DriveFileSystem::CopyDocument().
ACTION_P2(MockCopyDocument, status, value) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg2, status, base::Passed(value)));
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
        io_thread_(content::BrowserThread::IO),
        cache_(NULL),
        file_system_(NULL),
        mock_drive_service_(NULL),
        mock_webapps_registry_(NULL),
        num_callback_invocations_(0),
        expected_error_(DRIVE_FILE_OK),
        expected_cache_state_(0),
        expected_sub_dir_type_(DriveCache::CACHE_TYPE_META),
        expected_success_(true),
        expect_outgoing_symlink_(false),
        root_feed_changestamp_(0) {
  }

  virtual void SetUp() OVERRIDE {
    io_thread_.StartIOThread();

    profile_.reset(new TestingProfile);

    callback_helper_ = new CallbackHelper;

    // Allocate and keep a pointer to the mock, and inject it into the
    // DriveFileSystem object, which will own the mock object.
    mock_drive_service_ = new StrictMock<MockDriveService>;

    EXPECT_CALL(*mock_drive_service_, Initialize(profile_.get())).Times(1);

    // Likewise, this will be owned by DriveFileSystem.
    mock_free_disk_space_checker_ = new StrictMock<MockFreeDiskSpaceGetter>;
    SetFreeDiskSpaceGetterForTesting(mock_free_disk_space_checker_);

    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());

    cache_ = DriveCache::CreateDriveCacheOnUIThread(
        DriveCache::GetCacheRootPath(profile_.get()), blocking_task_runner_);

    mock_uploader_.reset(new StrictMock<MockDriveUploader>);
    mock_webapps_registry_.reset(new StrictMock<MockDriveWebAppsRegistry>);

    ASSERT_FALSE(file_system_);
    file_system_ = new DriveFileSystem(profile_.get(),
                                       cache_,
                                       mock_drive_service_,
                                       mock_uploader_.get(),
                                       mock_webapps_registry_.get(),
                                       blocking_task_runner_);

    mock_cache_observer_.reset(new StrictMock<MockDriveCacheObserver>);
    cache_->AddObserver(mock_cache_observer_.get());

    mock_directory_observer_.reset(new StrictMock<MockDirectoryChangeObserver>);
    file_system_->AddObserver(mock_directory_observer_.get());

    file_system_->Initialize();
    cache_->RequestInitializeOnUIThreadForTesting();
    test_util::RunBlockingPoolTask();
  }

  virtual void TearDown() OVERRIDE {
    ASSERT_TRUE(file_system_);
    EXPECT_CALL(*mock_drive_service_, CancelAll()).Times(1);
    delete file_system_;
    file_system_ = NULL;
    delete mock_drive_service_;
    mock_drive_service_ = NULL;
    SetFreeDiskSpaceGetterForTesting(NULL);
    cache_->DestroyOnUIThread();
    // The cache destruction requires to post a task to the blocking pool.
    test_util::RunBlockingPoolTask();

    profile_.reset(NULL);
  }

  // Loads test json file as root ("/drive") element.
  void LoadRootFeedDocument(const std::string& filename) {
    LoadChangeFeed(filename, 0);
  }

  void LoadChangeFeed(const std::string& filename,
                      int largest_changestamp) {
    test_util::LoadChangeFeed(filename,
                              file_system_,
                              largest_changestamp,
                              root_feed_changestamp_);
    root_feed_changestamp_++;
  }

  void AddDirectoryFromFile(const FilePath& directory_path,
                            const std::string& filename) {
    scoped_ptr<Value> atom = test_util::LoadJSONFile(filename);
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
    std::vector<FilePath::StringType> dir_parts;
    directory_path.GetComponents(&dir_parts);
    entry_dict->SetString("title.$t", dir_parts[dir_parts.size() - 1]);

    DriveFileError error;
    DriveFileSystem::CreateDirectoryParams params(directory_path,
                                                  directory_path,
                                                  false,  // is_exclusive
                                                  false,  // is_recursive
        base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));
    file_system_->AddNewDirectory(params, HTTP_SUCCESS, atom.Pass());
    test_util::RunBlockingPoolTask();
    EXPECT_EQ(DRIVE_FILE_OK, error);
  }

  bool RemoveEntry(const FilePath& file_path) {
    DriveFileError error;
    EXPECT_CALL(*mock_drive_service_, DeleteDocument(_, _)).Times(AnyNumber());
    file_system_->remove_function_->Remove(
        file_path, false,
        base::Bind(&test_util::CopyErrorCodeFromFileOperationCallback, &error));

    test_util::RunBlockingPoolTask();
    return error == DRIVE_FILE_OK;
  }

  FilePath GetCachePathForFile(const std::string& resource_id,
                               const std::string& md5) {
    return cache_->GetCacheFilePath(resource_id,
                                    md5,
                                    DriveCache::CACHE_TYPE_TMP,
                                    DriveCache::CACHED_FILE_FROM_SERVER);
  }

  // Gets entry info by path synchronously.
  scoped_ptr<DriveEntryProto> GetEntryInfoByPathSync(
      const FilePath& file_path) {
    file_system_->GetEntryInfoByPath(
        file_path,
        base::Bind(&CallbackHelper::GetEntryInfoCallback,
                   callback_helper_.get()));
    test_util::RunBlockingPoolTask();

    return callback_helper_->entry_proto_.Pass();
  }

  // Gets directory info by path synchronously.
  scoped_ptr<DriveEntryProtoVector> ReadDirectoryByPathSync(
      const FilePath& file_path) {
    file_system_->ReadDirectoryByPath(
        file_path,
        base::Bind(&CallbackHelper::ReadDirectoryCallback,
                   callback_helper_.get()));
    test_util::RunBlockingPoolTask();

    return callback_helper_->directory_entries_.Pass();
  }

  // Returns true if an entry exists at |file_path|.
  bool EntryExists(const FilePath& file_path) {
    return GetEntryInfoByPathSync(file_path).get();
  }


  // Gets the resource ID of |file_path|. Returns an empty string if not found.
  std::string GetResourceIdByPath(const FilePath& file_path) {
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
    blocking_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DriveFileSystemTest::GetCacheEntryFromOriginThreadInternal,
                   base::Unretained(this),
                   resource_id,
                   md5,
                   cache_entry,
                   &result));
    test_util::RunBlockingPoolTask();
    return result;
  }

  // Used to implement GetCacheEntry.
  void GetCacheEntryFromOriginThreadInternal(
      const std::string& resource_id,
      const std::string& md5,
      DriveCacheEntry* cache_entry,
      bool* result) {
    *result = cache_->GetCacheEntry(resource_id, md5, cache_entry);
  }

  // Returns true if the cache entry exists for the given resource ID and MD5.
  bool CacheEntryExists(const std::string& resource_id,
                        const std::string& md5) {
    DriveCacheEntry cache_entry;
    return GetCacheEntryFromOriginThread(resource_id, md5, &cache_entry);
  }

  // Returns true if the cache file exists for the given resource ID and MD5.
  bool CacheFileExists(const std::string& resource_id,
                       const std::string& md5) {
    const FilePath file_path = cache_->GetCacheFilePath(
        resource_id,
        md5,
        DriveCache::CACHE_TYPE_TMP,
        DriveCache::CACHED_FILE_FROM_SERVER);
    return file_util::PathExists(file_path);
  }

  void TestStoreToCache(
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& source_path,
      DriveFileError expected_error,
      int expected_cache_state,
      DriveCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    cache_->StoreOnUIThread(
        resource_id, md5, source_path,
        DriveCache::FILE_OPERATION_COPY,
        base::Bind(&DriveFileSystemTest::VerifyCacheFileState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void TestPin(
      const std::string& resource_id,
      const std::string& md5,
      DriveFileError expected_error,
      int expected_cache_state,
      DriveCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    cache_->PinOnUIThread(
        resource_id, md5,
        base::Bind(&DriveFileSystemTest::VerifyCacheFileState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void TestMarkDirty(
      const std::string& resource_id,
      const std::string& md5,
      DriveFileError expected_error,
      int expected_cache_state,
      DriveCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = false;

    cache_->MarkDirtyOnUIThread(
        resource_id,
        md5,
        base::Bind(&DriveFileSystemTest::VerifyMarkDirty,
                   base::Unretained(this),
                   resource_id,
                   md5));

    test_util::RunBlockingPoolTask();
  }

  void VerifyMarkDirty(const std::string& resource_id,
                       const std::string& md5,
                       DriveFileError error,
                       const FilePath& cache_file_path) {
    VerifyCacheFileState(error, resource_id, md5);

    // Verify filename of |cache_file_path|.
    if (error == DRIVE_FILE_OK) {
      FilePath base_name = cache_file_path.BaseName();
      EXPECT_EQ(util::EscapeCacheFileName(resource_id) +
                FilePath::kExtensionSeparator +
                "local",
                base_name.value());
    } else {
      EXPECT_TRUE(cache_file_path.empty());
    }
  }

  void TestCommitDirty(
      const std::string& resource_id,
      const std::string& md5,
      DriveFileError expected_error,
      int expected_cache_state,
      DriveCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = true;

    cache_->CommitDirtyOnUIThread(
        resource_id, md5,
        base::Bind(&DriveFileSystemTest::VerifyCacheFileState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  // Verify the file identified by |resource_id| and |md5| is in the expected
  // cache state after |OpenFile|, that is, marked dirty and has no outgoing
  // symlink, etc.
  void VerifyCacheStateAfterOpenFile(DriveFileError error,
                                     const std::string& resource_id,
                                     const std::string& md5,
                                     const FilePath& cache_file_path) {
    expected_error_ = DRIVE_FILE_OK;
    expected_cache_state_ = (test_util::TEST_CACHE_STATE_PRESENT |
                             test_util::TEST_CACHE_STATE_DIRTY |
                             test_util::TEST_CACHE_STATE_PERSISTENT);
    expected_sub_dir_type_ = DriveCache::CACHE_TYPE_PERSISTENT;
    expect_outgoing_symlink_ = false;
    VerifyMarkDirty(resource_id, md5, error, cache_file_path);
  }

  // Verify the file identified by |resource_id| and |md5| is in the expected
  // cache state after |CloseFile|, that is, marked dirty and has an outgoing
  // symlink, etc.
  void VerifyCacheStateAfterCloseFile(DriveFileError error,
                                      const std::string& resource_id,
                                      const std::string& md5) {
    expected_error_ = DRIVE_FILE_OK;
    expected_cache_state_ = (test_util::TEST_CACHE_STATE_PRESENT |
                             test_util::TEST_CACHE_STATE_DIRTY |
                             test_util::TEST_CACHE_STATE_PERSISTENT);
    expected_sub_dir_type_ = DriveCache::CACHE_TYPE_PERSISTENT;
    expect_outgoing_symlink_ = true;
    VerifyCacheFileState(error, resource_id, md5);
  }

  void VerifyCacheFileState(DriveFileError error,
                            const std::string& resource_id,
                            const std::string& md5) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

    // Verify cache map.
    DriveCacheEntry cache_entry;
    const bool cache_entry_found =
        GetCacheEntryFromOriginThread(resource_id, md5, &cache_entry);
    if (test_util::ToCacheEntry(expected_cache_state_).is_present() ||
        test_util::ToCacheEntry(expected_cache_state_).is_pinned()) {
      ASSERT_TRUE(cache_entry_found);
      EXPECT_TRUE(test_util::CacheStatesEqual(
          test_util::ToCacheEntry(expected_cache_state_),
          cache_entry));
      EXPECT_EQ(expected_sub_dir_type_,
                DriveCache::GetSubDirectoryType(cache_entry));
    } else {
      EXPECT_FALSE(cache_entry_found);
    }

    // Verify actual cache file.
    FilePath dest_path = cache_->GetCacheFilePath(
        resource_id,
        md5,
        test_util::ToCacheEntry(expected_cache_state_).is_pinned() ||
        test_util::ToCacheEntry(expected_cache_state_).is_dirty() ?
                DriveCache::CACHE_TYPE_PERSISTENT :
                DriveCache::CACHE_TYPE_TMP,
        test_util::ToCacheEntry(expected_cache_state_).is_dirty() ?
            DriveCache::CACHED_FILE_LOCALLY_MODIFIED :
            DriveCache::CACHED_FILE_FROM_SERVER);
    bool exists = file_util::PathExists(dest_path);
    if (test_util::ToCacheEntry(expected_cache_state_).is_present())
      EXPECT_TRUE(exists);
    else
      EXPECT_FALSE(exists);

    // Verify symlink in pinned dir.
    FilePath symlink_path = cache_->GetCacheFilePath(
        resource_id,
        std::string(),
        DriveCache::CACHE_TYPE_PINNED,
        DriveCache::CACHED_FILE_FROM_SERVER);
    // Check that pin symlink exists, without dereferencing to target path.
    exists = file_util::IsLink(symlink_path);
    if (test_util::ToCacheEntry(expected_cache_state_).is_pinned()) {
      EXPECT_TRUE(exists);
      FilePath target_path;
      EXPECT_TRUE(file_util::ReadSymbolicLink(symlink_path, &target_path));
      if (test_util::ToCacheEntry(expected_cache_state_).is_present())
        EXPECT_EQ(dest_path, target_path);
      else
        EXPECT_EQ(kSymLinkToDevNull, target_path.value());
    } else {
      EXPECT_FALSE(exists);
    }

    // Verify symlink in outgoing dir.
    symlink_path = cache_->GetCacheFilePath(
        resource_id,
        std::string(),
        DriveCache::CACHE_TYPE_OUTGOING,
        DriveCache::CACHED_FILE_FROM_SERVER);
    // Check that outgoing symlink exists, without dereferencing to target path.
    exists = file_util::IsLink(symlink_path);
    if (expect_outgoing_symlink_ &&
        test_util::ToCacheEntry(expected_cache_state_).is_dirty()) {
      EXPECT_TRUE(exists);
      FilePath target_path;
      EXPECT_TRUE(file_util::ReadSymbolicLink(symlink_path, &target_path));
      EXPECT_TRUE(target_path.value() != kSymLinkToDevNull);
      if (test_util::ToCacheEntry(expected_cache_state_).is_present())
        EXPECT_EQ(dest_path, target_path);
    } else {
      EXPECT_FALSE(exists);
    }
  }

  void SetExpectationsForGetDocumentEntry(scoped_ptr<base::Value>* document,
                                          const std::string& resource_id) {
    EXPECT_CALL(*mock_drive_service_, GetDocumentEntry(resource_id, _))
        .WillOnce(MockGetDocumentEntry(gdata::HTTP_SUCCESS, document));
  }

  // Loads serialized proto file from GCache, and makes sure the root
  // filesystem has a root at 'drive'
  void TestLoadMetadataFromCache() {
    file_system_->LoadRootFeedFromCacheForTesting();
    test_util::RunBlockingPoolTask();
  }

  // Creates a proto file representing a filesystem with directories:
  // drive, drive/Dir1, drive/Dir1/SubDir2
  // and files
  // drive/File1, drive/Dir1/File2, drive/Dir1/SubDir2/File3.
  // Sets the changestamp to 654321, equal to that of "account_metadata.json"
  // test data, indicating the cache is holding the latest file system info.
  void SaveTestFileSystem() {
    DriveRootDirectoryProto root;
    root.set_version(kProtoVersion);
    root.set_largest_changestamp(654321);
    DriveDirectoryProto* root_dir = root.mutable_drive_directory();
    DriveEntryProto* dir_base = root_dir->mutable_drive_entry();
    PlatformFileInfoProto* platform_info = dir_base->mutable_file_info();
    dir_base->set_title("drive");
    dir_base->set_resource_id(kDriveRootDirectoryResourceId);
    dir_base->set_upload_url("http://resumable-create-media/1");
    platform_info->set_is_directory(true);

    // drive/File1
    DriveEntryProto* file = root_dir->add_child_files();
    file->set_title("File1");
    file->set_resource_id("resource_id:File1");
    file->set_upload_url("http://resumable-edit-media/1");
    file->mutable_file_specific_info()->set_file_md5("md5");
    platform_info = file->mutable_file_info();
    platform_info->set_is_directory(false);
    platform_info->set_size(1048576);

    // drive/Dir1
    DriveDirectoryProto* dir1 = root_dir->add_child_directories();
    dir_base = dir1->mutable_drive_entry();
    dir_base->set_title("Dir1");
    dir_base->set_resource_id("resource_id:Dir1");
    dir_base->set_upload_url("http://resumable-create-media/2");
    platform_info = dir_base->mutable_file_info();
    platform_info->set_is_directory(true);

    // drive/Dir1/File2
    file = dir1->add_child_files();
    file->set_title("File2");
    file->set_resource_id("resource_id:File2");
    file->set_upload_url("http://resumable-edit-media/2");
    file->mutable_file_specific_info()->set_file_md5("md5");
    platform_info = file->mutable_file_info();
    platform_info->set_is_directory(false);
    platform_info->set_size(555);

    // drive/Dir1/SubDir2
    DriveDirectoryProto* dir2 = dir1->add_child_directories();
    dir_base = dir2->mutable_drive_entry();
    dir_base->set_title("SubDir2");
    dir_base->set_resource_id("resource_id:SubDir2");
    dir_base->set_upload_url("http://resumable-create-media/3");
    platform_info = dir_base->mutable_file_info();
    platform_info->set_is_directory(true);

    // drive/Dir1/SubDir2/File3
    file = dir2->add_child_files();
    file->set_title("File3");
    file->set_resource_id("resource_id:File3");
    file->set_upload_url("http://resumable-edit-media/3");
    file->mutable_file_specific_info()->set_file_md5("md5");
    platform_info = file->mutable_file_info();
    platform_info->set_is_directory(false);
    platform_info->set_size(12345);

    // Write this proto out to GCache/vi/meta/file_system.pb
    std::string serialized_proto;
    ASSERT_TRUE(root.SerializeToString(&serialized_proto));
    ASSERT_TRUE(!serialized_proto.empty());

    FilePath cache_dir_path = profile_->GetPath().Append(
        FILE_PATH_LITERAL("GCache/v1/meta/"));
    ASSERT_TRUE(file_util::CreateDirectory(cache_dir_path));
    const int file_size = static_cast<int>(serialized_proto.length());
    ASSERT_EQ(file_util::WriteFile(cache_dir_path.Append("file_system.pb"),
        serialized_proto.data(), file_size), file_size);
  }

  // Verifies that |file_path| is a valid JSON file for the hosted document
  // associated with |entry| (i.e. |url| and |resource_id| match).
  void VerifyHostedDocumentJSONFile(const DriveEntryProto& entry_proto,
                                    const FilePath& file_path) {
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

  // This is used as a helper for registering callbacks that need to be
  // RefCountedThreadSafe, and a place where we can fetch results from various
  // operations.
  class CallbackHelper
    : public base::RefCountedThreadSafe<CallbackHelper> {
   public:
    CallbackHelper()
        : last_error_(DRIVE_FILE_OK),
          quota_bytes_total_(0),
          quota_bytes_used_(0),
          entry_proto_(NULL) {}

    virtual void GetFileCallback(DriveFileError error,
                                 const FilePath& file_path,
                                 const std::string& mime_type,
                                 DriveFileType file_type) {
      last_error_ = error;
      download_path_ = file_path;
      mime_type_ = mime_type;
      file_type_ = file_type;
    }

    virtual void FileOperationCallback(DriveFileError error) {
      DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

      last_error_ = error;
    }

    virtual void GetAvailableSpaceCallback(DriveFileError error,
                                           int64 bytes_total,
                                           int64 bytes_used) {
      last_error_ = error;
      quota_bytes_total_ = bytes_total;
      quota_bytes_used_ = bytes_used;
    }

    virtual void OpenFileCallback(DriveFileError error,
                                  const FilePath& file_path) {
      last_error_ = error;
      opened_file_path_ = file_path;
      MessageLoop::current()->Quit();
    }

    virtual void CloseFileCallback(DriveFileError error) {
      last_error_ = error;
      MessageLoop::current()->Quit();
    }

    virtual void GetEntryInfoCallback(
        DriveFileError error,
        scoped_ptr<DriveEntryProto> entry_proto) {
      last_error_ = error;
      entry_proto_ = entry_proto.Pass();
    }

    virtual void ReadDirectoryCallback(
        DriveFileError error,
        bool /* hide_hosted_documents */,
        scoped_ptr<DriveEntryProtoVector> entries) {
      last_error_ = error;
      directory_entries_ = entries.Pass();
    }

    DriveFileError last_error_;
    FilePath download_path_;
    FilePath opened_file_path_;
    std::string mime_type_;
    DriveFileType file_type_;
    int64 quota_bytes_total_;
    int64 quota_bytes_used_;
    scoped_ptr<DriveEntryProto> entry_proto_;
    scoped_ptr<DriveEntryProtoVector> directory_entries_;

   protected:
    virtual ~CallbackHelper() {}

   private:
    friend class base::RefCountedThreadSafe<CallbackHelper>;
  };

  // Copy the result from FindFirstMissingParentDirectory().
  static void CopyResultFromFindFirstMissingParentDirectory(
      DriveFileSystem::FindFirstMissingParentDirectoryResult* out_result,
      const DriveFileSystem::FindFirstMissingParentDirectoryResult& result) {
    DCHECK(out_result);
    *out_result = result;
  }

  MessageLoopForUI message_loop_;
  // The order of the test threads is important, do not change the order.
  // See also content/browser/browser_thread_impl.cc.
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<CallbackHelper> callback_helper_;
  DriveCache* cache_;
  scoped_ptr<StrictMock<MockDriveUploader> > mock_uploader_;
  DriveFileSystem* file_system_;
  StrictMock<MockDriveService>* mock_drive_service_;
  scoped_ptr<StrictMock<MockDriveWebAppsRegistry> > mock_webapps_registry_;
  StrictMock<MockFreeDiskSpaceGetter>* mock_free_disk_space_checker_;
  scoped_ptr<StrictMock<MockDriveCacheObserver> > mock_cache_observer_;
  scoped_ptr<StrictMock<MockDirectoryChangeObserver> > mock_directory_observer_;

  int num_callback_invocations_;
  DriveFileError expected_error_;
  int expected_cache_state_;
  DriveCache::CacheSubDirectoryType expected_sub_dir_type_;
  bool expected_success_;
  bool expect_outgoing_symlink_;
  std::string expected_file_extension_;
  int root_feed_changestamp_;
};

void AsyncInitializationCallback(
    int* counter,
    int expected_counter,
    const FilePath& expected_file_path,
    MessageLoop* message_loop,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  ASSERT_EQ(DRIVE_FILE_OK, error);
  ASSERT_TRUE(entry_proto.get());
  ASSERT_TRUE(entry_proto->file_info().is_directory());
  EXPECT_EQ(expected_file_path.value(), entry_proto->base_name());

  (*counter)++;
  if (*counter >= expected_counter)
    message_loop->Quit();
}

TEST_F(DriveFileSystemTest, DuplicatedAsyncInitialization) {
  int counter = 0;
  GetEntryInfoCallback callback = base::Bind(
      &AsyncInitializationCallback,
      &counter,
      2,
      FilePath(FILE_PATH_LITERAL("drive")),
      &message_loop_);

  EXPECT_CALL(*mock_drive_service_, GetAccountMetadata(_)).Times(1);
  EXPECT_CALL(*mock_drive_service_,
              GetDocuments(Eq(GURL()), _, _, _, _)).Times(1);

  EXPECT_CALL(*mock_webapps_registry_, UpdateFromFeed(_)).Times(1);

  file_system_->GetEntryInfoByPath(
      FilePath(FILE_PATH_LITERAL("drive")), callback);
  file_system_->GetEntryInfoByPath(
      FilePath(FILE_PATH_LITERAL("drive")), callback);
  message_loop_.Run();  // Wait to get our result
  EXPECT_EQ(2, counter);
}

TEST_F(DriveFileSystemTest, SearchRootDirectory) {
  LoadRootFeedDocument("gdata/root_feed.json");

  const FilePath kFilePath = FilePath(FILE_PATH_LITERAL("drive"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(
      FilePath(FILE_PATH_LITERAL(kFilePath)));
  ASSERT_TRUE(entry.get());
  EXPECT_EQ(kDriveRootDirectoryResourceId, entry->resource_id());
}

TEST_F(DriveFileSystemTest, SearchExistingFile) {
  LoadRootFeedDocument("gdata/root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file:2_file_resource_id", entry->resource_id());
}

TEST_F(DriveFileSystemTest, SearchExistingDocument) {
  LoadRootFeedDocument("gdata/root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/Document 1.gdoc"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("document:5_document_resource_id", entry->resource_id());
}

TEST_F(DriveFileSystemTest, SearchNonExistingFile) {
  LoadRootFeedDocument("gdata/root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/nonexisting.file"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_FALSE(entry.get());
}

TEST_F(DriveFileSystemTest, SearchEncodedFileNames) {
  LoadRootFeedDocument("gdata/root_feed.json");

  const FilePath kFilePath1 = FilePath(
      FILE_PATH_LITERAL("drive/Slash / in file 1.txt"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath1);
  ASSERT_FALSE(entry.get());

  const FilePath kFilePath2 = FilePath::FromUTF8Unsafe(
      "drive/Slash \xE2\x88\x95 in file 1.txt");
  entry = GetEntryInfoByPathSync(kFilePath2);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file:slash_file_resource_id", entry->resource_id());

  const FilePath kFilePath3 = FilePath::FromUTF8Unsafe(
      "drive/Slash \xE2\x88\x95 in directory/Slash SubDir File.txt");
  entry = GetEntryInfoByPathSync(kFilePath3);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file:slash_subdir_file", entry->resource_id());
}

TEST_F(DriveFileSystemTest, SearchEncodedFileNamesLoadingRoot) {
  LoadRootFeedDocument("gdata/root_feed.json");

  const FilePath kFilePath1 = FilePath(
      FILE_PATH_LITERAL("drive/Slash / in file 1.txt"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath1);
  ASSERT_FALSE(entry.get());

  const FilePath kFilePath2 = FilePath::FromUTF8Unsafe(
      "drive/Slash \xE2\x88\x95 in file 1.txt");
  entry = GetEntryInfoByPathSync(kFilePath2);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file:slash_file_resource_id", entry->resource_id());

  const FilePath kFilePath3 = FilePath::FromUTF8Unsafe(
      "drive/Slash \xE2\x88\x95 in directory/Slash SubDir File.txt");
  entry = GetEntryInfoByPathSync(kFilePath3);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file:slash_subdir_file", entry->resource_id());
}

TEST_F(DriveFileSystemTest, SearchDuplicateNames) {
  LoadRootFeedDocument("gdata/root_feed.json");

  const FilePath kFilePath1 = FilePath(
      FILE_PATH_LITERAL("drive/Duplicate Name.txt"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath1);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file:3_file_resource_id", entry->resource_id());

  const FilePath kFilePath2 = FilePath(
      FILE_PATH_LITERAL("drive/Duplicate Name (2).txt"));
  entry = GetEntryInfoByPathSync(kFilePath2);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file:4_file_resource_id", entry->resource_id());
}

TEST_F(DriveFileSystemTest, SearchExistingDirectory) {
  LoadRootFeedDocument("gdata/root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/Directory 1"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  ASSERT_EQ("folder:1_folder_resource_id", entry->resource_id());
}

TEST_F(DriveFileSystemTest, SearchInSubdir) {
  LoadRootFeedDocument("gdata/root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  ASSERT_EQ("file:subdirectory_file_1_id", entry->resource_id());
}

// Check the reconstruction of the directory structure from only the root feed.
TEST_F(DriveFileSystemTest, SearchInSubSubdir) {
  LoadRootFeedDocument("gdata/root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/Directory 1/Sub Directory Folder/"
                        "Sub Sub Directory Folder"));
  scoped_ptr<DriveEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  ASSERT_EQ("folder:sub_sub_directory_folder_id", entry->resource_id());
}

TEST_F(DriveFileSystemTest, FilePathTests) {
  LoadRootFeedDocument("gdata/root_feed.json");

  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL("drive/File 1.txt"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))));
  EXPECT_TRUE(EntryExists(
      FilePath(
          FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"))));
}

TEST_F(DriveFileSystemTest, ChangeFeed_AddAndDeleteFileInRoot) {
  int latest_changelog = 0;
  LoadRootFeedDocument("gdata/root_feed.json");

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(2);

  LoadChangeFeed("gdata/delta_file_added_in_root.json", ++latest_changelog);
  EXPECT_TRUE(
      EntryExists(FilePath(FILE_PATH_LITERAL("drive/Added file.gdoc"))));

  LoadChangeFeed("gdata/delta_file_deleted_in_root.json", ++latest_changelog);
  EXPECT_FALSE(
      EntryExists(FilePath(FILE_PATH_LITERAL("drive/Added file.gdoc"))));
}


TEST_F(DriveFileSystemTest, ChangeFeed_AddAndDeleteFileFromExistingDirectory) {
  int latest_changelog = 0;
  LoadRootFeedDocument("gdata/root_feed.json");

  EXPECT_TRUE(EntryExists(FilePath(
      FILE_PATH_LITERAL("drive/Directory 1"))));

  // Add file to an existing directory.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  LoadChangeFeed("gdata/delta_file_added_in_directory.json",
                 ++latest_changelog);
  EXPECT_TRUE(EntryExists(FilePath(
      FILE_PATH_LITERAL("drive/Directory 1/Added file.gdoc"))));

  // Remove that file from the directory.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  LoadChangeFeed("gdata/delta_file_deleted_in_directory.json",
                 ++latest_changelog);
  EXPECT_TRUE(EntryExists(FilePath(
      FILE_PATH_LITERAL("drive/Directory 1"))));
  EXPECT_FALSE(EntryExists(FilePath(
      FILE_PATH_LITERAL("drive/Directory 1/Added file.gdoc"))));
}

TEST_F(DriveFileSystemTest, ChangeFeed_AddFileToNewDirectory) {
  int latest_changelog = 0;
  LoadRootFeedDocument("gdata/root_feed.json");
  // Add file to a new directory.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/New Directory"))))).Times(1);

  LoadChangeFeed("gdata/delta_file_added_in_new_directory.json",
                 ++latest_changelog);

  EXPECT_TRUE(EntryExists(FilePath(
      FILE_PATH_LITERAL("drive/New Directory"))));
  EXPECT_TRUE(EntryExists(FilePath(
      FILE_PATH_LITERAL("drive/New Directory/File in new dir.gdoc"))));
}

TEST_F(DriveFileSystemTest, ChangeFeed_AddFileToNewButDeletedDirectory) {
  int latest_changelog = 0;
  LoadRootFeedDocument("gdata/root_feed.json");

  // This feed contains thw following updates:
  // 1) A new PDF file is added to a new directory
  // 2) but the new directory is marked "deleted" (i.e. moved to Trash)
  // Hence, the PDF file should be just ignored.
  LoadChangeFeed("gdata/delta_file_added_in_new_but_deleted_directory.json",
                 ++latest_changelog);
}

TEST_F(DriveFileSystemTest, ChangeFeed_DirectoryMovedFromRootToDirectory) {
  int latest_changelog = 0;
  LoadRootFeedDocument("gdata/root_feed.json");

  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder/Sub Sub Directory Folder"))));

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 2"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 2/Directory 1")))))
      .Times(1);
  LoadChangeFeed("gdata/delta_dir_moved_from_root_to_directory.json",
                 ++latest_changelog);

  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2"))));
  EXPECT_FALSE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2/Directory 1"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2/Directory 1/SubDirectory File 1.txt"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2/Directory 1/Sub Directory Folder"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2/Directory 1/Sub Directory Folder/"
      "Sub Sub Directory Folder"))));
}

TEST_F(DriveFileSystemTest, ChangeFeed_FileMovedFromDirectoryToRoot) {
  int latest_changelog = 0;
  LoadRootFeedDocument("gdata/root_feed.json");

  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder/Sub Sub Directory Folder"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  LoadChangeFeed("gdata/delta_file_moved_from_directory_to_root.json",
                 ++latest_changelog);

  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder/Sub Sub Directory Folder"))));
  EXPECT_FALSE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/SubDirectory File 1.txt"))));
}

TEST_F(DriveFileSystemTest, ChangeFeed_FileRenamedInDirectory) {
  int latest_changelog = 0;
  LoadRootFeedDocument("gdata/root_feed.json");

  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  LoadChangeFeed("gdata/delta_file_renamed_in_directory.json",
                 ++latest_changelog);

  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_FALSE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/New SubDirectory File 1.txt"))));
}

TEST_F(DriveFileSystemTest, CachedFeedLoading) {
  SaveTestFileSystem();
  TestLoadMetadataFromCache();

  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL("drive/File1"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL("drive/Dir1"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL("drive/Dir1/File2"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL("drive/Dir1/SubDir2"))));
  EXPECT_TRUE(EntryExists(
      FilePath(FILE_PATH_LITERAL("drive/Dir1/SubDir2/File3"))));
}

TEST_F(DriveFileSystemTest, CachedFeadLoadingThenServerFeedLoading) {
  SaveTestFileSystem();

  // SaveTestFileSystem and "account_metadata.json" have the same changestamp,
  // so no request for new feeds (i.e., call to GetDocuments) should happen.
  mock_drive_service_->set_account_metadata(
      test_util::LoadJSONFile("gdata/account_metadata.json").release());
  EXPECT_CALL(*mock_drive_service_, GetAccountMetadata(_)).Times(1);
  EXPECT_CALL(*mock_webapps_registry_, UpdateFromFeed(_)).Times(1);
  EXPECT_CALL(*mock_drive_service_, GetDocuments(_, _, _, _, _)).Times(0);

  // Kicks loading of cached file system and query for server update.
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL("drive/File1"))));

  // Since the file system has verified that it holds the latest snapshot,
  // it should change its state to FROM_SERVER, which admits periodic refresh.
  // To test it, call CheckForUpdates and verify it does try to check updates.
  mock_drive_service_->set_account_metadata(
      test_util::LoadJSONFile("gdata/account_metadata.json").release());
  EXPECT_CALL(*mock_drive_service_, GetAccountMetadata(_)).Times(1);
  EXPECT_CALL(*mock_webapps_registry_, UpdateFromFeed(_)).Times(1);

  file_system_->CheckForUpdates();
  test_util::RunBlockingPoolTask();
}

TEST_F(DriveFileSystemTest, TransferFileFromLocalToRemote_RegularFile) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  LoadRootFeedDocument("gdata/root_feed.json");

  // We'll add a file to the Drive root directory.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  // Prepare a local file.
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const FilePath local_src_file_path = temp_dir.path().Append("local.txt");
  const std::string kContent = "hello";
  file_util::WriteFile(local_src_file_path, kContent.data(), kContent.size());

  // Confirm that the remote file does not exist.
  const FilePath remote_dest_file_path(FILE_PATH_LITERAL("drive/remote.txt"));
  EXPECT_FALSE(EntryExists(remote_dest_file_path));

  scoped_ptr<base::Value> value =
      test_util::LoadJSONFile("gdata/document_to_download.json");
  scoped_ptr<DocumentEntry> document_entry(
      DocumentEntry::ExtractAndParse(*value));

  EXPECT_CALL(*mock_uploader_, UploadNewFile(_, _, _, _, _, _, _, _))
      .WillOnce(MockUploadNewFile());

  // Transfer the local file to Drive.
  file_system_->TransferFileFromLocalToRemote(
      local_src_file_path, remote_dest_file_path, callback);
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, callback_helper_->last_error_);

  // Now the remote file should exist.
  EXPECT_TRUE(EntryExists(remote_dest_file_path));
}

TEST_F(DriveFileSystemTest, TransferFileFromLocalToRemote_HostedDocument) {
  LoadRootFeedDocument("gdata/root_feed.json");

  // Prepare a local file, which is a json file of a hosted document, which
  // matches "Document 1" in root_feed.json.
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const FilePath local_src_file_path = temp_dir.path().Append("local.gdoc");
  const std::string kEditUrl =
      "https://3_document_self_link/document:5_document_resource_id";
  const std::string kResourceId = "document:5_document_resource_id";
  const std::string kContent =
      base::StringPrintf("{\"url\": \"%s\", \"resource_id\": \"%s\"}",
                         kEditUrl.c_str(), kResourceId.c_str());
  file_util::WriteFile(local_src_file_path, kContent.data(), kContent.size());

  // Confirm that the remote file does not exist.
  const FilePath remote_dest_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/Document 1.gdoc"));
  EXPECT_FALSE(EntryExists(remote_dest_file_path));

  // We'll add a file to "Directory 1" directory on Drive.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);

  // We'll copy a hosted document using CopyDocument.
  // ".gdoc" suffix should be stripped when copying.
  scoped_ptr<base::Value> document =
      test_util::LoadJSONFile("gdata/uploaded_document.json");
  EXPECT_CALL(*mock_drive_service_,
              CopyDocument(kResourceId,
                           FILE_PATH_LITERAL("Document 1"),
                           _))
      .WillOnce(MockCopyDocument(gdata::HTTP_SUCCESS, &document));
  // We'll then add the hosted document to the destination directory.
  EXPECT_CALL(*mock_drive_service_,
              AddResourceToDirectory(_, _, _)).Times(1);

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  // Transfer the local file to Drive.
  file_system_->TransferFileFromLocalToRemote(
      local_src_file_path, remote_dest_file_path, callback);
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, callback_helper_->last_error_);

  // Now the remote file should exist.
  EXPECT_TRUE(EntryExists(remote_dest_file_path));
}

TEST_F(DriveFileSystemTest, TransferFileFromRemoteToLocal_RegularFile) {
  LoadRootFeedDocument("gdata/root_feed.json");

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath local_dest_file_path = temp_dir.path().Append("local_copy.txt");

  FilePath remote_src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> file = GetEntryInfoByPathSync(
      remote_src_file_path);
  FilePath cache_file = GetCachePathForFile(
      file->resource_id(),
      file->file_specific_info().file_md5());
  const int64 file_size = file->file_info().size();

  // Pretend we have enough space.
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(2).WillRepeatedly(Return(file_size + kMinFreeSpace));

  const std::string remote_src_file_data = "Test file data";
  mock_drive_service_->set_file_data(new std::string(remote_src_file_data));

  // Before Download starts metadata from server will be fetched.
  // We will read content url from the result.
  scoped_ptr<base::Value> document =
      test_util::LoadJSONFile("gdata/document_to_download.json");
  SetExpectationsForGetDocumentEntry(&document, "file:2_file_resource_id");

  // The file is obtained with the mock DriveService.
  EXPECT_CALL(*mock_drive_service_,
              DownloadFile(remote_src_file_path,
                           cache_file,
                           GURL("https://file_content_url_changed/"),
                           _, _))
      .Times(1);

  file_system_->TransferFileFromRemoteToLocal(
      remote_src_file_path, local_dest_file_path, callback);
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, callback_helper_->last_error_);

  std::string cache_file_data;
  EXPECT_TRUE(file_util::ReadFileToString(cache_file, &cache_file_data));
  EXPECT_EQ(remote_src_file_data, cache_file_data);

  std::string local_dest_file_data;
  EXPECT_TRUE(file_util::ReadFileToString(local_dest_file_path,
                                          &local_dest_file_data));
  EXPECT_EQ(remote_src_file_data, local_dest_file_data);
}

TEST_F(DriveFileSystemTest, TransferFileFromRemoteToLocal_HostedDocument) {
  LoadRootFeedDocument("gdata/root_feed.json");

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath local_dest_file_path = temp_dir.path().Append("local_copy.txt");
  FilePath remote_src_file_path(FILE_PATH_LITERAL("drive/Document 1.gdoc"));
  file_system_->TransferFileFromRemoteToLocal(
      remote_src_file_path, local_dest_file_path, callback);
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, callback_helper_->last_error_);

  scoped_ptr<DriveEntryProto> entry_proto = GetEntryInfoByPathSync(
      remote_src_file_path);
  ASSERT_TRUE(entry_proto.get());
  VerifyHostedDocumentJSONFile(*entry_proto, local_dest_file_path);
}

TEST_F(DriveFileSystemTest, CopyNotExistingFile) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/Dummy file.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Test.log"));

  LoadRootFeedDocument("gdata/root_feed.json");

  EXPECT_FALSE(EntryExists(src_file_path));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Copy(src_file_path, dest_file_path, callback);
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_FALSE(EntryExists(dest_file_path));
}

TEST_F(DriveFileSystemTest, CopyFileToNonExistingDirectory) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Dummy"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Dummy/Test.log"));

  LoadRootFeedDocument("gdata/root_feed.json");

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<DriveEntryProto> src_entry_proto = GetEntryInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_entry_proto.get());
  std::string src_file_path_resource_id =
      src_entry_proto->resource_id();
  EXPECT_FALSE(src_entry_proto->edit_url().empty());

  EXPECT_FALSE(EntryExists(dest_parent_path));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_TRUE(EntryExists(src_file_path));
  EXPECT_FALSE(EntryExists(dest_parent_path));
  EXPECT_FALSE(EntryExists(dest_file_path));
}

// Test the case where the parent of |dest_file_path| is an existing file,
// not a directory.
TEST_F(DriveFileSystemTest, CopyFileToInvalidPath) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/Document 1.gdoc"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Duplicate Name.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL(
      "drive/Duplicate Name.txt/Document 1.gdoc"));

  LoadRootFeedDocument("gdata/root_feed.json");

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

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Copy(src_file_path, dest_file_path, callback);
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_A_DIRECTORY,
            callback_helper_->last_error_);

  EXPECT_TRUE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(dest_parent_path));

  EXPECT_FALSE(EntryExists(dest_file_path));
}

TEST_F(DriveFileSystemTest, RenameFile) {
  const FilePath src_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
  const FilePath src_parent_path(FILE_PATH_LITERAL("drive/Directory 1"));
  const FilePath dest_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/Test.log"));

  LoadRootFeedDocument("gdata/root_feed.json");

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<DriveEntryProto> src_entry_proto = GetEntryInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_entry_proto.get());
  std::string src_file_resource_id =
      src_entry_proto->resource_id();

  EXPECT_CALL(*mock_drive_service_,
              RenameResource(GURL(src_entry_proto->edit_url()),
                             FILE_PATH_LITERAL("Test.log"), _));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);

  file_system_->Move(src_file_path, dest_file_path, callback);
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, callback_helper_->last_error_);

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(dest_file_path));
  EXPECT_EQ(src_file_resource_id, GetResourceIdByPath(dest_file_path));
}

TEST_F(DriveFileSystemTest, MoveFileFromRootToSubDirectory) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Directory 1"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Directory 1/Test.log"));

  LoadRootFeedDocument("gdata/root_feed.json");

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
  EXPECT_FALSE(dest_parent_proto->content_url().empty());

  EXPECT_CALL(*mock_drive_service_,
              RenameResource(GURL(src_entry_proto->edit_url()),
                             FILE_PATH_LITERAL("Test.log"), _));
  EXPECT_CALL(*mock_drive_service_,
              AddResourceToDirectory(
                  GURL(dest_parent_proto->content_url()),
                  GURL(src_entry_proto->edit_url()), _));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  // Expect notification for both source and destination directories.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);

  file_system_->Move(src_file_path, dest_file_path, callback);
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, callback_helper_->last_error_);

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(dest_file_path));
  EXPECT_EQ(src_file_resource_id, GetResourceIdByPath(dest_file_path));
}

TEST_F(DriveFileSystemTest, MoveFileFromSubDirectoryToRoot) {
  FilePath src_parent_path(FILE_PATH_LITERAL("drive/Directory 1"));
  FilePath src_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Test.log"));

  LoadRootFeedDocument("gdata/root_feed.json");

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
  EXPECT_FALSE(src_parent_proto->content_url().empty());

  EXPECT_CALL(*mock_drive_service_,
              RenameResource(GURL(src_entry_proto->edit_url()),
                             FILE_PATH_LITERAL("Test.log"), _));
  EXPECT_CALL(*mock_drive_service_,
              RemoveResourceFromDirectory(
                  GURL(src_parent_proto->content_url()),
                  GURL(src_entry_proto->edit_url()),
                  src_file_resource_id, _));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  // Expect notification for both source and destination directories.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);

  file_system_->Move(src_file_path, dest_file_path, callback);
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, callback_helper_->last_error_);

  EXPECT_FALSE(EntryExists(src_file_path));
  ASSERT_TRUE(EntryExists(dest_file_path));
  EXPECT_EQ(src_file_resource_id, GetResourceIdByPath(dest_file_path));
}

TEST_F(DriveFileSystemTest, MoveFileBetweenSubDirectories) {
  FilePath src_parent_path(FILE_PATH_LITERAL("drive/Directory 1"));
  FilePath src_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/New Folder 1"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/New Folder 1/Test.log"));
  FilePath interim_file_path(FILE_PATH_LITERAL("drive/Test.log"));

  LoadRootFeedDocument("gdata/root_feed.json");

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  AddDirectoryFromFile(dest_parent_path, "gdata/directory_entry_atom.json");

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
  EXPECT_FALSE(src_parent_proto->content_url().empty());

  ASSERT_TRUE(EntryExists(dest_parent_path));
  scoped_ptr<DriveEntryProto> dest_parent_proto = GetEntryInfoByPathSync(
      dest_parent_path);
  ASSERT_TRUE(dest_parent_proto.get());
  ASSERT_TRUE(dest_parent_proto->file_info().is_directory());
  EXPECT_FALSE(dest_parent_proto->content_url().empty());

  EXPECT_FALSE(EntryExists(interim_file_path));

  EXPECT_CALL(*mock_drive_service_,
              RenameResource(GURL(src_entry_proto->edit_url()),
                             FILE_PATH_LITERAL("Test.log"), _));
  EXPECT_CALL(*mock_drive_service_,
              RemoveResourceFromDirectory(
                  GURL(src_parent_proto->content_url()),
                  GURL(src_entry_proto->edit_url()),
                  src_file_resource_id, _));
  EXPECT_CALL(*mock_drive_service_,
              AddResourceToDirectory(
                  GURL(dest_parent_proto->content_url()),
                  GURL(src_entry_proto->edit_url()),
                  _));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  // Expect notification for both source and destination directories plus
  // interim file path.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/New Folder 1"))))).Times(1);

  file_system_->Move(src_file_path, dest_file_path, callback);
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, callback_helper_->last_error_);

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_FALSE(EntryExists(interim_file_path));

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(dest_file_path));
  EXPECT_EQ(src_file_resource_id, GetResourceIdByPath(dest_file_path));
}

TEST_F(DriveFileSystemTest, MoveNotExistingFile) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/Dummy file.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Test.log"));

  LoadRootFeedDocument("gdata/root_feed.json");

  EXPECT_FALSE(EntryExists(src_file_path));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_FALSE(EntryExists(dest_file_path));
}

TEST_F(DriveFileSystemTest, MoveFileToNonExistingDirectory) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Dummy"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Dummy/Test.log"));

  LoadRootFeedDocument("gdata/root_feed.json");

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<DriveEntryProto> src_entry_proto = GetEntryInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_entry_proto.get());
  std::string src_file_resource_id =
      src_entry_proto->resource_id();
  EXPECT_FALSE(src_entry_proto->edit_url().empty());

  EXPECT_FALSE(EntryExists(dest_parent_path));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);


  EXPECT_FALSE(EntryExists(dest_parent_path));
  EXPECT_FALSE(EntryExists(dest_file_path));
}

// Test the case where the parent of |dest_file_path| is a existing file,
// not a directory.
TEST_F(DriveFileSystemTest, MoveFileToInvalidPath) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Duplicate Name.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL(
      "drive/Duplicate Name.txt/Test.log"));

  LoadRootFeedDocument("gdata/root_feed.json");

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

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_A_DIRECTORY,
            callback_helper_->last_error_);

  EXPECT_TRUE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(dest_parent_path));
  EXPECT_FALSE(EntryExists(dest_file_path));
}

TEST_F(DriveFileSystemTest, RemoveEntries) {
  LoadRootFeedDocument("gdata/root_feed.json");

  FilePath nonexisting_file(FILE_PATH_LITERAL("drive/Dummy file.txt"));
  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dir_in_root(FILE_PATH_LITERAL("drive/Directory 1"));
  FilePath file_in_subdir(
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
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(2);

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
  EXPECT_FALSE(RemoveEntry(FilePath(FILE_PATH_LITERAL("drive"))));

  // Need this to ensure OnDirectoryChanged() is run.
  test_util::RunBlockingPoolTask();
}

TEST_F(DriveFileSystemTest, CreateDirectory) {
  LoadRootFeedDocument("gdata/root_feed.json");

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  // Create directory in root.
  FilePath dir_path(FILE_PATH_LITERAL("drive/New Folder 1"));
  EXPECT_FALSE(EntryExists(dir_path));
  AddDirectoryFromFile(dir_path, "gdata/directory_entry_atom.json");
  EXPECT_TRUE(EntryExists(dir_path));

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/New Folder 1"))))).Times(1);

  // Create directory in a sub directory.
  FilePath subdir_path(FILE_PATH_LITERAL("drive/New Folder 1/New Folder 2"));
  EXPECT_FALSE(EntryExists(subdir_path));
  AddDirectoryFromFile(subdir_path, "gdata/directory_entry_atom2.json");
  EXPECT_TRUE(EntryExists(subdir_path));
}

TEST_F(DriveFileSystemTest, FindFirstMissingParentDirectory) {
  LoadRootFeedDocument("gdata/root_feed.json");

  DriveFileSystem::FindFirstMissingParentDirectoryResult result;

  // Create directory in root.
  FilePath dir_path(FILE_PATH_LITERAL("drive/New Folder 1"));
  file_system_->FindFirstMissingParentDirectory(
      dir_path,
      base::Bind(&CopyResultFromFindFirstMissingParentDirectory,
                 &result));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DriveFileSystem::FIND_FIRST_FOUND_MISSING, result.error);
  EXPECT_EQ(FilePath(FILE_PATH_LITERAL("drive/New Folder 1")),
            result.first_missing_parent_path);
  EXPECT_TRUE(result.last_dir_content_url.is_empty());  // root directory.

  // Missing folders in subdir of an existing folder.
  FilePath dir_path2(FILE_PATH_LITERAL("drive/Directory 1/New Folder 2"));
  file_system_->FindFirstMissingParentDirectory(
      dir_path2,
      base::Bind(&CopyResultFromFindFirstMissingParentDirectory,
                 &result));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DriveFileSystem::FIND_FIRST_FOUND_MISSING, result.error);
  EXPECT_EQ(FilePath(FILE_PATH_LITERAL("drive/Directory 1/New Folder 2")),
            result.first_missing_parent_path);
  EXPECT_FALSE(result.last_dir_content_url.is_empty());  // non-root dir.

  // Missing two folders on the path.
  FilePath dir_path3 = dir_path2.Append(FILE_PATH_LITERAL("Another Folder"));
  file_system_->FindFirstMissingParentDirectory(
      dir_path3,
      base::Bind(&CopyResultFromFindFirstMissingParentDirectory,
                 &result));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DriveFileSystem::FIND_FIRST_FOUND_MISSING, result.error);
  EXPECT_EQ(FilePath(FILE_PATH_LITERAL("drive/Directory 1/New Folder 2")),
            result.first_missing_parent_path);
  EXPECT_FALSE(result.last_dir_content_url.is_empty());  // non-root dir.

  // Folders on top of an existing file.
  file_system_->FindFirstMissingParentDirectory(
      FilePath(FILE_PATH_LITERAL("drive/File 1.txt/BadDir")),
      base::Bind(&CopyResultFromFindFirstMissingParentDirectory,
                 &result));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DriveFileSystem::FIND_FIRST_FOUND_INVALID, result.error);

  // Existing folder.
  file_system_->FindFirstMissingParentDirectory(
      FilePath(FILE_PATH_LITERAL("drive/Directory 1")),
      base::Bind(&CopyResultFromFindFirstMissingParentDirectory,
                 &result));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DriveFileSystem::FIND_FIRST_DIRECTORY_ALREADY_PRESENT,
            result.error);
}

// Create a directory through the document service
TEST_F(DriveFileSystemTest, CreateDirectoryWithService) {
  LoadRootFeedDocument("gdata/root_feed.json");
  EXPECT_CALL(*mock_drive_service_,
              CreateDirectory(_, "Sample Directory Title", _)).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  // Set last error so it's not a valid error code.
  callback_helper_->last_error_ = static_cast<DriveFileError>(1);
  file_system_->CreateDirectory(
      FilePath(FILE_PATH_LITERAL("drive/Sample Directory Title")),
      false,  // is_exclusive
      true,  // is_recursive
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get()));
  test_util::RunBlockingPoolTask();
  // TODO(gspencer): Uncomment this when we get a blob that
  // works that can be returned from the mock.
  // EXPECT_EQ(DRIVE_FILE_OK, callback_helper_->last_error_);
}

TEST_F(DriveFileSystemTest, GetFileByPath_FromGData_EnoughSpace) {
  LoadRootFeedDocument("gdata/root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(file_in_root));
  FilePath downloaded_file = GetCachePathForFile(
      entry_proto->resource_id(),
      entry_proto->file_specific_info().file_md5());
  const int64 file_size = entry_proto->file_info().size();

  // Pretend we have enough space.
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(2).WillRepeatedly(Return(file_size + kMinFreeSpace));

  // Before Download starts metadata from server will be fetched.
  // We will read content url from the result.
  scoped_ptr<base::Value> document =
      test_util::LoadJSONFile("gdata/document_to_download.json");
  SetExpectationsForGetDocumentEntry(&document, "file:2_file_resource_id");

  // The file is obtained with the mock DriveService.
  EXPECT_CALL(*mock_drive_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url_changed/"),
                           _, _))
      .Times(1);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetContentCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, callback_helper_->last_error_);
  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_EQ(downloaded_file.value(),
            callback_helper_->download_path_.value());
}

TEST_F(DriveFileSystemTest, GetFileByPath_FromGData_NoSpaceAtAll) {
  LoadRootFeedDocument("gdata/root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(file_in_root));
  FilePath downloaded_file = GetCachePathForFile(
      entry_proto->resource_id(),
      entry_proto->file_specific_info().file_md5());

  // Pretend we have no space at all.
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(2).WillRepeatedly(Return(0));

  // Before Download starts metadata from server will be fetched.
  // We will read content url from the result.
  scoped_ptr<base::Value> document =
      test_util::LoadJSONFile("gdata/document_to_download.json");
  SetExpectationsForGetDocumentEntry(&document, "file:2_file_resource_id");

  // The file is not obtained with the mock DriveService, because of no space.
  EXPECT_CALL(*mock_drive_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url_changed/"),
                           _, _))
      .Times(0);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetContentCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_ERROR_NO_SPACE,
            callback_helper_->last_error_);
}

TEST_F(DriveFileSystemTest, GetFileByPath_FromGData_NoEnoughSpaceButCanFreeUp) {
  LoadRootFeedDocument("gdata/root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(file_in_root));
  FilePath downloaded_file = GetCachePathForFile(
      entry_proto->resource_id(),
      entry_proto->file_specific_info().file_md5());
  const int64 file_size = entry_proto->file_info().size();

  // Pretend we have no space first (checked before downloading a file),
  // but then start reporting we have space. This is to emulate that
  // the disk space was freed up by removing temporary files.
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .WillOnce(Return(file_size + kMinFreeSpace))
      .WillOnce(Return(0))
      .WillOnce(Return(file_size + kMinFreeSpace))
      .WillOnce(Return(file_size + kMinFreeSpace));

  // Store something in the temporary cache directory.
  TestStoreToCache("<resource_id>",
                   "<md5>",
                   test_util::GetTestFilePath("gdata/root_feed.json"),
                   DRIVE_FILE_OK,
                   test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);
  ASSERT_TRUE(CacheEntryExists("<resource_id>", "<md5>"));
  ASSERT_TRUE(CacheFileExists("<resource_id>", "<md5>"));

  // Before Download starts metadata from server will be fetched.
  // We will read content url from the result.
  scoped_ptr<base::Value> document =
      test_util::LoadJSONFile("gdata/document_to_download.json");
  SetExpectationsForGetDocumentEntry(&document, "file:2_file_resource_id");

  // The file is obtained with the mock DriveService, because of we freed up the
  // space.
  EXPECT_CALL(*mock_drive_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url_changed/"),
                           _, _))
      .Times(1);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetContentCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, callback_helper_->last_error_);
  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_EQ(downloaded_file.value(),
            callback_helper_->download_path_.value());

  // The file should be removed in order to free up space, and the cache
  // entry should also be removed.
  ASSERT_FALSE(CacheEntryExists("<resource_id>", "<md5>"));
  ASSERT_FALSE(CacheFileExists("<resource_id>", "<md5>"));
}

TEST_F(DriveFileSystemTest, GetFileByPath_FromGData_EnoughSpaceButBecomeFull) {
  LoadRootFeedDocument("gdata/root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(file_in_root));
  FilePath downloaded_file = GetCachePathForFile(
      entry_proto->resource_id(),
      entry_proto->file_specific_info().file_md5());
  const int64 file_size = entry_proto->file_info().size();

  // Pretend we have enough space first (checked before downloading a file),
  // but then start reporting we have not enough space. This is to emulate that
  // the disk space becomes full after the file is downloaded for some reason
  // (ex. the actual file was larger than the expected size).
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .WillOnce(Return(file_size + kMinFreeSpace))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillOnce(Return(kMinFreeSpace - 1));

  // Before Download starts metadata from server will be fetched.
  // We will read content url from the result.
  scoped_ptr<base::Value> document =
      test_util::LoadJSONFile("gdata/document_to_download.json");
  SetExpectationsForGetDocumentEntry(&document, "file:2_file_resource_id");

  // The file is obtained with the mock DriveService.
  EXPECT_CALL(*mock_drive_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url_changed/"),
                           _, _))
      .Times(1);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetContentCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_ERROR_NO_SPACE,
            callback_helper_->last_error_);
}

TEST_F(DriveFileSystemTest, GetFileByPath_FromCache) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  LoadRootFeedDocument("gdata/root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(file_in_root));
  FilePath downloaded_file = GetCachePathForFile(
      entry_proto->resource_id(),
      entry_proto->file_specific_info().file_md5());

  // Store something as cached version of this file.
  TestStoreToCache(entry_proto->resource_id(),
                   entry_proto->file_specific_info().file_md5(),
                   test_util::GetTestFilePath("gdata/root_feed.json"),
                   DRIVE_FILE_OK,
                   test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);

  // Make sure we don't fetch metadata for downloading file.
  EXPECT_CALL(*mock_drive_service_, GetDocumentEntry(_, _)).Times(0);

  // Make sure we don't call downloads at all.
  EXPECT_CALL(*mock_drive_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url_changed/"),
                           _, _))
      .Times(0);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetContentCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_EQ(downloaded_file.value(),
            callback_helper_->download_path_.value());
}

TEST_F(DriveFileSystemTest, GetFileByPath_HostedDocument) {
  LoadRootFeedDocument("gdata/root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/Document 1.gdoc"));
  scoped_ptr<DriveEntryProto> src_entry_proto =
      GetEntryInfoByPathSync(file_in_root);
  ASSERT_TRUE(src_entry_proto.get());

  file_system_->GetFileByPath(file_in_root, callback,
                              GetContentCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(HOSTED_DOCUMENT, callback_helper_->file_type_);
  EXPECT_FALSE(callback_helper_->download_path_.empty());

  ASSERT_TRUE(src_entry_proto.get());
  VerifyHostedDocumentJSONFile(*src_entry_proto,
                               callback_helper_->download_path_);
}

TEST_F(DriveFileSystemTest, GetFileByResourceId) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  LoadRootFeedDocument("gdata/root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(file_in_root));
  FilePath downloaded_file = GetCachePathForFile(
      entry_proto->resource_id(),
      entry_proto->file_specific_info().file_md5());

  // Before Download starts metadata from server will be fetched.
  // We will read content url from the result.
  scoped_ptr<base::Value> document =
      test_util::LoadJSONFile("gdata/document_to_download.json");
  SetExpectationsForGetDocumentEntry(&document, "file:2_file_resource_id");

  // The file is obtained with the mock DriveService, because it's not stored in
  // the cache.
  EXPECT_CALL(*mock_drive_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url_changed/"),
                           _, _))
      .Times(1);

  file_system_->GetFileByResourceId(entry_proto->resource_id(),
                                    callback,
                                    GetContentCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_EQ(downloaded_file.value(),
            callback_helper_->download_path_.value());
}

TEST_F(DriveFileSystemTest, GetFileByResourceId_FromCache) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  LoadRootFeedDocument("gdata/root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(file_in_root));
  FilePath downloaded_file = GetCachePathForFile(
      entry_proto->resource_id(),
      entry_proto->file_specific_info().file_md5());

  // Store something as cached version of this file.
  TestStoreToCache(entry_proto->resource_id(),
                   entry_proto->file_specific_info().file_md5(),
                   test_util::GetTestFilePath("gdata/root_feed.json"),
                   DRIVE_FILE_OK,
                   test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);

  // The file is obtained from the cache.
  // Make sure we don't call downloads at all.
  EXPECT_CALL(*mock_drive_service_, DownloadFile(_, _, _, _, _))
      .Times(0);

  file_system_->GetFileByResourceId(entry_proto->resource_id(),
                                    callback,
                                    GetContentCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_EQ(downloaded_file.value(),
            callback_helper_->download_path_.value());
}

TEST_F(DriveFileSystemTest, UpdateFileByResourceId_PersistentFile) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  LoadRootFeedDocument("gdata/root_feed.json");

  // This is a file defined in root_feed.json.
  const FilePath kFilePath(FILE_PATH_LITERAL("drive/File 1.txt"));
  const std::string kResourceId("file:2_file_resource_id");
  const std::string kMd5("3b4382ebefec6e743578c76bbd0575ce");

  // Pin the file so it'll be store in "persistent" directory.
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(kResourceId, kMd5)).Times(1);
  TestPin(kResourceId,
          kMd5,
          DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PINNED,
          DriveCache::CACHE_TYPE_TMP);

  // First store a file to cache. A cache file will be created at:
  // GCache/v1/persistent/<kResourceId>.<kMd5>
  const FilePath original_cache_file_path =
      DriveCache::GetCacheRootPath(profile_.get())
      .AppendASCII("persistent")
      .AppendASCII(kResourceId + "." + kMd5);
  TestStoreToCache(kResourceId,
                   kMd5,
                   // Anything works.
                   test_util::GetTestFilePath("gdata/root_feed.json"),
                   DRIVE_FILE_OK,
                   test_util::TEST_CACHE_STATE_PRESENT |
                   test_util::TEST_CACHE_STATE_PINNED |
                   test_util::TEST_CACHE_STATE_PERSISTENT,
                   DriveCache::CACHE_TYPE_PERSISTENT);
  ASSERT_TRUE(file_util::PathExists(original_cache_file_path));

  // Add the dirty bit. The cache file will be renamed to
  // GCache/v1/persistent/<kResourceId>.local
  TestMarkDirty(kResourceId,
                kMd5,
                DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_PINNED |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);
  const FilePath dirty_cache_file_path =
      DriveCache::GetCacheRootPath(profile_.get())
      .AppendASCII("persistent")
      .AppendASCII(kResourceId + ".local");
  ASSERT_FALSE(file_util::PathExists(original_cache_file_path));
  ASSERT_TRUE(file_util::PathExists(dirty_cache_file_path));

  // Modify the cached file.
  const std::string kDummyCacheContent("modification to the cache");
  ASSERT_TRUE(file_util::WriteFile(dirty_cache_file_path,
                                   kDummyCacheContent.c_str(),
                                   kDummyCacheContent.size()));

  // Commit the dirty bit. The cache file name remains the same
  // but a symlink will be created at:
  // GCache/v1/outgoing/<kResourceId>
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(kResourceId)).Times(1);
  TestCommitDirty(kResourceId,
                  kMd5,
                  DRIVE_FILE_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_PINNED |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  DriveCache::CACHE_TYPE_PERSISTENT);
  const FilePath outgoing_symlink_path =
      DriveCache::GetCacheRootPath(profile_.get())
      .AppendASCII("outgoing")
      .AppendASCII(kResourceId);
  ASSERT_TRUE(file_util::PathExists(dirty_cache_file_path));
  ASSERT_TRUE(file_util::PathExists(outgoing_symlink_path));

  // Create a DocumentEntry, which is needed to mock
  // DriveUploaderInterface::UploadExistingFile().
  // TODO(satorux): This should be cleaned up. crbug.com/134240.
  DocumentEntry* document_entry = NULL;
  scoped_ptr<base::Value> value =
      test_util::LoadJSONFile("gdata/root_feed.json");
  ASSERT_TRUE(value.get());
  base::DictionaryValue* as_dict = NULL;
  base::ListValue* entry_list = NULL;
  if (value->GetAsDictionary(&as_dict) &&
      as_dict->GetList("feed.entry", &entry_list)) {
    for (size_t i = 0; i < entry_list->GetSize(); ++i) {
      base::DictionaryValue* entry = NULL;
      std::string resource_id;
      if (entry_list->GetDictionary(i, &entry) &&
          entry->GetString("gd$resourceId.$t", &resource_id) &&
          resource_id == kResourceId) {
        // This will be deleted by UploadExistingFile().
        document_entry = DocumentEntry::CreateFrom(*entry);
      }
    }
  }
  ASSERT_TRUE(document_entry);

  // The mock uploader will be used to simulate a file upload.
  EXPECT_CALL(*mock_uploader_, UploadExistingFile(
      GURL("https://file_link_resumable_edit_media/"),
      kFilePath,
      dirty_cache_file_path,
      "audio/mpeg",
      kDummyCacheContent.size(),  // The size after modification must be used.
      _))  // callback
      .WillOnce(MockUploadExistingFile(
          DRIVE_FILE_OK,
          FilePath::FromUTF8Unsafe("drive/File1"),
          dirty_cache_file_path,
          document_entry));

  // We'll notify the directory change to the observer upon completion.
  EXPECT_CALL(*mock_directory_observer_,
              OnDirectoryChanged(Eq(FilePath(kDriveRootDirectory)))).Times(1);

  // The callback will be called upon completion of
  // UpdateFileByResourceId().
  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  // Check the number of files in the root directory. We'll compare the
  // number after updating a file.
  scoped_ptr<DriveEntryProtoVector> root_directory_entries(
      ReadDirectoryByPathSync(FilePath::FromUTF8Unsafe("drive")));
  ASSERT_TRUE(root_directory_entries.get());
  const int num_files_in_root = CountFiles(*root_directory_entries);

  file_system_->UpdateFileByResourceId(kResourceId, callback);
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_FILE_OK, callback_helper_->last_error_);
  // Make sure that the number of files did not change (i.e. we updated an
  // existing file, rather than adding a new file. The number of files
  // increases if we don't handle the file update right).
  EXPECT_EQ(num_files_in_root, CountFiles(*root_directory_entries));
  // After the file is updated, the dirty bit is cleared, hence the symlink
  // should be gone.
  ASSERT_FALSE(file_util::PathExists(outgoing_symlink_path));
}

TEST_F(DriveFileSystemTest, UpdateFileByResourceId_NonexistentFile) {
  LoadRootFeedDocument("gdata/root_feed.json");

  // This is nonexistent in root_feed.json.
  const FilePath kFilePath(FILE_PATH_LITERAL("drive/Nonexistent.txt"));
  const std::string kResourceId("file:nonexistent_resource_id");
  const std::string kMd5("nonexistent_md5");

  // The callback will be called upon completion of
  // UpdateFileByResourceId().
  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->UpdateFileByResourceId(kResourceId, callback);
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);
}

TEST_F(DriveFileSystemTest, ContentSearch) {
  LoadRootFeedDocument("gdata/root_feed.json");

  mock_drive_service_->set_search_result("gdata/search_result_feed.json");

  EXPECT_CALL(*mock_drive_service_, GetDocuments(Eq(GURL()), _, "foo", _, _))
      .Times(1);

  const SearchResultPair kExpectedResults[] = {
    { "drive/Directory 1/SubDirectory File 1.txt", false },
    { "drive/Directory 1", true }
  };

  SearchCallback callback = base::Bind(&DriveSearchCallback,
      &message_loop_, kExpectedResults, ARRAYSIZE_UNSAFE(kExpectedResults));

  file_system_->Search("foo", GURL(), callback);
  message_loop_.Run();  // Wait to get our result.
}

TEST_F(DriveFileSystemTest, ContentSearchWithNewEntry) {
  LoadRootFeedDocument("gdata/root_feed.json");

  // Search result returning two entries "Directory 1/" and
  // "Directory 1/SubDirectory Newly Added File.txt". The latter is not
  // contained in the root feed.
  mock_drive_service_->set_search_result(
      "gdata/search_result_with_new_entry_feed.json");

  EXPECT_CALL(*mock_drive_service_, GetDocuments(Eq(GURL()), _, "foo", _, _))
      .Times(1);

  // As the result of the first Search(), only entries in the current file
  // system snapshot are expected to be returned.
  const SearchResultPair kExpectedResults[] = {
    { "drive/Directory 1", true }
  };

  // At the same time, unknown entry should trigger delta feed request.
  // This will cause notification to observers (e.g., File Browser) so that
  // they can request search again.
  EXPECT_CALL(*mock_drive_service_, GetAccountMetadata(_)).Times(1);
  EXPECT_CALL(*mock_drive_service_, GetDocuments(Eq(GURL()), _, "", _, _))
      .Times(1);
  EXPECT_CALL(*mock_webapps_registry_, UpdateFromFeed(_)).Times(1);

  SearchCallback callback = base::Bind(&DriveSearchCallback,
      &message_loop_, kExpectedResults, ARRAYSIZE_UNSAFE(kExpectedResults));

  file_system_->Search("foo", GURL(), callback);
  message_loop_.Run();  // Wait to get our result.
}

TEST_F(DriveFileSystemTest, ContentSearchEmptyResult) {
  LoadRootFeedDocument("gdata/root_feed.json");

  mock_drive_service_->set_search_result("gdata/empty_feed.json");

  EXPECT_CALL(*mock_drive_service_, GetDocuments(Eq(GURL()), _, "foo", _, _))
      .Times(1);

  const SearchResultPair* expected_results = NULL;

  SearchCallback callback = base::Bind(&DriveSearchCallback,
      &message_loop_, expected_results, 0u);

  file_system_->Search("foo", GURL(), callback);
  message_loop_.Run();  // Wait to get our result.
}

TEST_F(DriveFileSystemTest, GetAvailableSpace) {
  GetAvailableSpaceCallback callback =
      base::Bind(&CallbackHelper::GetAvailableSpaceCallback,
                 callback_helper_.get());

  EXPECT_CALL(*mock_drive_service_, GetAccountMetadata(_));

  file_system_->GetAvailableSpace(callback);
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(GG_LONGLONG(6789012345), callback_helper_->quota_bytes_used_);
  EXPECT_EQ(GG_LONGLONG(9876543210), callback_helper_->quota_bytes_total_);
}

TEST_F(DriveFileSystemTest, RequestDirectoryRefresh) {
  LoadRootFeedDocument("gdata/root_feed.json");

  // We'll fetch documents in the root directory with its resource ID.
  EXPECT_CALL(*mock_drive_service_,
              GetDocuments(Eq(GURL()), _, _, kDriveRootDirectoryResourceId, _))
      .Times(1);
  // We'll notify the directory change to the observer.
  EXPECT_CALL(*mock_directory_observer_,
              OnDirectoryChanged(Eq(FilePath(kDriveRootDirectory)))).Times(1);

  file_system_->RequestDirectoryRefresh(FilePath(kDriveRootDirectory));
  test_util::RunBlockingPoolTask();
}

TEST_F(DriveFileSystemTest, OpenAndCloseFile) {
  LoadRootFeedDocument("gdata/root_feed.json");

  OpenFileCallback callback =
      base::Bind(&CallbackHelper::OpenFileCallback,
                 callback_helper_.get());
  FileOperationCallback close_file_callback =
      base::Bind(&CallbackHelper::CloseFileCallback,
                 callback_helper_.get());

  const FilePath kFileInRoot(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<DriveEntryProto> entry_proto(GetEntryInfoByPathSync(kFileInRoot));
  FilePath downloaded_file = GetCachePathForFile(
      entry_proto->resource_id(),
      entry_proto->file_specific_info().file_md5());
  const int64 file_size = entry_proto->file_info().size();
  const std::string& file_resource_id =
      entry_proto->resource_id();
  const std::string& file_md5 = entry_proto->file_specific_info().file_md5();

  // A dirty file is created on close.
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(file_resource_id))
      .Times(1);

  // Pretend we have enough space.
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(2).WillRepeatedly(Return(file_size + kMinFreeSpace));

  const std::string kExpectedFileData = "test file data";
  mock_drive_service_->set_file_data(new std::string(kExpectedFileData));

  // Before Download starts metadata from server will be fetched.
  // We will read content url from the result.
  scoped_ptr<base::Value> document =
      test_util::LoadJSONFile("gdata/document_to_download.json");
  SetExpectationsForGetDocumentEntry(&document, "file:2_file_resource_id");

  // The file is obtained with the mock DriveService.
  EXPECT_CALL(*mock_drive_service_,
              DownloadFile(kFileInRoot,
                           downloaded_file,
                           GURL("https://file_content_url_changed/"),
                           _, _))
      .Times(1);

  // Open kFileInRoot ("drive/File 1.txt").
  file_system_->OpenFile(kFileInRoot, callback);
  message_loop_.Run();
  const FilePath opened_file_path = callback_helper_->opened_file_path_;

  // Verify that the file was properly opened.
  EXPECT_EQ(DRIVE_FILE_OK, callback_helper_->last_error_);

  // Try to open the already opened file.
  file_system_->OpenFile(kFileInRoot, callback);
  message_loop_.Run();

  // It must fail.
  EXPECT_EQ(DRIVE_FILE_ERROR_IN_USE, callback_helper_->last_error_);

  // Verify that the file contents match the expected contents.
  std::string cache_file_data;
  EXPECT_TRUE(file_util::ReadFileToString(opened_file_path, &cache_file_data));
  EXPECT_EQ(kExpectedFileData, cache_file_data);

  // Verify that the cache state was changed as expected.
  VerifyCacheStateAfterOpenFile(DRIVE_FILE_OK,
                                file_resource_id,
                                file_md5,
                                opened_file_path);

  // Close kFileInRoot ("drive/File 1.txt").
  file_system_->CloseFile(kFileInRoot, close_file_callback);
  message_loop_.Run();

  // Verify that the file was properly closed.
  EXPECT_EQ(DRIVE_FILE_OK, callback_helper_->last_error_);

  // Verify that the cache state was changed as expected.
  VerifyCacheStateAfterCloseFile(DRIVE_FILE_OK,
                                 file_resource_id,
                                 file_md5);

  // Try to close the same file twice.
  file_system_->CloseFile(kFileInRoot, close_file_callback);
  message_loop_.Run();

  // It must fail.
  EXPECT_EQ(DRIVE_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);
}

}   // namespace gdata
