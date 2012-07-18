// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_test_util.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
#include "chrome/browser/chromeos/gdata/mock_directory_change_observer.h"
#include "chrome/browser/chromeos/gdata/mock_gdata_documents_service.h"
#include "chrome/browser/chromeos/gdata/mock_gdata_sync_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Ne;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::StrictMock;
using ::testing::_;

using base::Value;
using base::DictionaryValue;
using base::ListValue;
using content::BrowserThread;

namespace gdata {
namespace {

const char kSymLinkToDevNull[] = "/dev/null";

const int64 kLotsOfSpace = kMinFreeSpace * 10;

struct PathToVerify {
  PathToVerify(const FilePath& in_path_to_scan,
               const FilePath& in_expected_existing_path) :
      path_to_scan(in_path_to_scan),
      expected_existing_path(in_expected_existing_path) {
  }

  FilePath path_to_scan;
  FilePath expected_existing_path;
};

struct SearchResultPair {
  const char* search_path;
  const char* real_path;
};

// Callback to GDataFileSystem::Search used in ContentSearch test.
// Verifies returned vector of results.
void DriveSearchCallback(
    MessageLoop* message_loop,
    GDataFileError error,
    scoped_ptr<std::vector<SearchResultInfo> > results) {
  // Search feed contains 2 entries. One file (SubDirectory File 1.txt) and one
  // directory (Directory 1). Entries generated from the feed should have names
  // in format resource_id.actual_file_name.
  ASSERT_TRUE(results.get());
  ASSERT_EQ(2u, results->size());

  EXPECT_EQ(FilePath("drive/Directory 1/SubDirectory File 1.txt"),
            results->at(0).path);
  EXPECT_FALSE(results->at(0).is_directory);

  EXPECT_EQ(FilePath("drive/Directory 1"), results->at(1).path);
  EXPECT_TRUE(results->at(1).is_directory);

  message_loop->Quit();
}

// Action used to set mock expectations for
// DocumentsService::GetDocumentEntry().
ACTION_P2(MockGetDocumentEntry, status, value) {
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(arg1, status, base::Passed(value)));
}

// Action used to set mock expectations for
// GDataUploaderInterface::UploadExistingFile().
ACTION_P4(MockUploadExistingFile,
          error, gdata_path, local_file_path, document_entry) {
  scoped_ptr<UploadFileInfo> upload_file_info(new UploadFileInfo);
  upload_file_info->gdata_path = gdata_path;
  upload_file_info->file_path = local_file_path;
  upload_file_info->entry.reset(document_entry);
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(arg5, error, base::Passed(&upload_file_info)));

  const int kUploadId = 123;
  return kUploadId;
}

// Action used to set mock expectations for
// GDataFileSystem::CopyDocument().
ACTION_P2(MockCopyDocument, status, value) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg2, status, base::Passed(value)));
}

// Returns the absolute path for a test file stored under
// chrome/test/data/chromeos/gdata.
FilePath GetTestFilePath(const FilePath::StringType& base_name) {
  FilePath path;
  std::string error;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("chromeos")
      .AppendASCII("gdata")
      .AppendASCII(base_name.c_str());
  EXPECT_TRUE(file_util::PathExists(path)) <<
      "Couldn't find " << path.value();
  return path;
}

// Loads a test JSON file as a base::Value.
base::Value* LoadJSONFile(const std::string& base_name) {
  FilePath path = GetTestFilePath(base_name);

  std::string error;
  JSONFileValueSerializer serializer(path);
  base::Value* value = serializer.Deserialize(NULL, &error);
  EXPECT_TRUE(value) <<
      "Parse error " << path.value() << ": " << error;
  return value;
}

}  // namespace

class MockFreeDiskSpaceGetter : public FreeDiskSpaceGetterInterface {
 public:
  virtual ~MockFreeDiskSpaceGetter() {}
  MOCK_CONST_METHOD0(AmountOfFreeDiskSpace, int64());
};

class MockGDataUploader : public GDataUploaderInterface {
 public:
  virtual ~MockGDataUploader() {}
  // This function is not mockable by gmock.
  virtual int UploadNewFile(
      scoped_ptr<UploadFileInfo> upload_file_info) OVERRIDE {
    // Set a document entry for an uploaded file.
    // Used for TransferFileFromLocalToRemote_RegularFile test.
    scoped_ptr<base::Value> value(LoadJSONFile("uploaded_file.json"));
    scoped_ptr<DocumentEntry> document_entry(
        DocumentEntry::ExtractAndParse(*value));
    upload_file_info->entry = document_entry.Pass();

    // Run the complection callback.
    const UploadFileInfo::UploadCompletionCallback callback =
        upload_file_info->completion_callback;
    if (!callback.is_null())
      callback.Run(GDATA_FILE_OK, upload_file_info.Pass());

    const int kUploadId = 123;
    return kUploadId;
  }

  MOCK_METHOD6(UploadExistingFile,
               int(const GURL& upload_location,
               const FilePath& gdata_file_path,
               const FilePath& local_file_path,
               int64 file_size,
               const std::string& content_type,
               const UploadFileInfo::UploadCompletionCallback& callback));

  MOCK_METHOD2(UpdateUpload, void(int upload_id,
                                  content::DownloadItem* download));
  MOCK_CONST_METHOD1(GetUploadedBytes, int64(int upload_id));
};

class MockDriveWebAppsRegistry : public DriveWebAppsRegistryInterface {
 public:
  virtual ~MockDriveWebAppsRegistry() {}

  MOCK_METHOD3(GetWebAppsForFile, void(const FilePath& file,
                                       const std::string& mime_type,
                                       ScopedVector<DriveWebAppInfo>* apps));
  MOCK_METHOD1(GetExtensionsForWebStoreApp,
               std::set<std::string>(const std::string& web_store_id));
  MOCK_METHOD1(UpdateFromFeed, void(AccountMetadataFeed* metadata));
};

class GDataFileSystemTest : public testing::Test {
 protected:
  GDataFileSystemTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO),
        cache_(NULL),
        file_system_(NULL),
        mock_doc_service_(NULL),
        mock_webapps_registry_(NULL),
        num_callback_invocations_(0),
        expected_error_(GDATA_FILE_OK),
        expected_cache_state_(0),
        expected_sub_dir_type_(GDataCache::CACHE_TYPE_META),
        expected_success_(true),
        expect_outgoing_symlink_(false),
        root_feed_changestamp_(0) {
  }

  virtual void SetUp() OVERRIDE {
    chromeos::CrosLibrary::Initialize(true /* use_stub */);
    io_thread_.StartIOThread();

    profile_.reset(new TestingProfile);

    callback_helper_ = new CallbackHelper;

    // Allocate and keep a pointer to the mock, and inject it into the
    // GDataFileSystem object, which will own the mock object.
    mock_doc_service_ = new StrictMock<MockDocumentsService>;

    EXPECT_CALL(*mock_doc_service_, Initialize(profile_.get())).Times(1);

    // Likewise, this will be owned by GDataFileSystem.
    mock_free_disk_space_checker_ = new StrictMock<MockFreeDiskSpaceGetter>;
    SetFreeDiskSpaceGetterForTesting(mock_free_disk_space_checker_);

    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());

    cache_ = GDataCache::CreateGDataCacheOnUIThread(
        GDataCache::GetCacheRootPath(profile_.get()), blocking_task_runner_);

    mock_uploader_.reset(new StrictMock<MockGDataUploader>);
    mock_webapps_registry_.reset(new StrictMock<MockDriveWebAppsRegistry>);

    ASSERT_FALSE(file_system_);
    file_system_ = new GDataFileSystem(profile_.get(),
                                       cache_,
                                       mock_doc_service_,
                                       mock_uploader_.get(),
                                       mock_webapps_registry_.get(),
                                       blocking_task_runner_);

    mock_sync_client_.reset(new StrictMock<MockGDataSyncClient>);
    cache_->AddObserver(mock_sync_client_.get());

    mock_directory_observer_.reset(new StrictMock<MockDirectoryChangeObserver>);
    file_system_->AddObserver(mock_directory_observer_.get());

    file_system_->Initialize();
    cache_->RequestInitializeOnUIThread();
    test_util::RunBlockingPoolTask();
  }

  virtual void TearDown() OVERRIDE {
    ASSERT_TRUE(file_system_);
    EXPECT_CALL(*mock_doc_service_, CancelAll()).Times(1);
    delete file_system_;
    file_system_ = NULL;
    delete mock_doc_service_;
    mock_doc_service_ = NULL;
    SetFreeDiskSpaceGetterForTesting(NULL);
    cache_->DestroyOnUIThread();
    // The cache destruction requires to post a task to the blocking pool.
    test_util::RunBlockingPoolTask();

    profile_.reset(NULL);
    chromeos::CrosLibrary::Shutdown();
  }

  // Loads test json file as root ("/drive") element.
  void LoadRootFeedDocument(const std::string& filename) {
    LoadChangeFeed(filename, 0);
  }

  void LoadChangeFeed(const std::string& filename,
                      int largest_changestamp) {
    std::string error;
    scoped_ptr<Value> document(LoadJSONFile(filename));
    ASSERT_TRUE(document.get());
    ASSERT_TRUE(document->GetType() == Value::TYPE_DICTIONARY);
    scoped_ptr<DocumentFeed> document_feed(
        DocumentFeed::ExtractAndParse(*document));
    ASSERT_TRUE(document_feed.get());
    std::vector<DocumentFeed*> feed_list;
    feed_list.push_back(document_feed.get());
    ASSERT_TRUE(UpdateContent(feed_list, largest_changestamp));
  }

  void AddDirectoryFromFile(const FilePath& directory_path,
                            const std::string& filename) {
    std::string error;
    scoped_ptr<Value> atom(LoadJSONFile(filename));
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

    ASSERT_EQ(file_system_->AddNewDirectory(directory_path.DirName(),
                                            entry_value),
              GDATA_FILE_OK)
        << "Failed adding "
        << directory_path.DirName().value();
  }

  // Updates the content of directory under |directory_path| with parsed feed
  // |value|.
  bool UpdateContent(const std::vector<DocumentFeed*>& list,
                     int largest_changestamp) {
    GURL unused;
    return file_system_->UpdateFromFeed(
        list,
        FROM_SERVER,
        largest_changestamp,
        root_feed_changestamp_++) == GDATA_FILE_OK;
  }

  bool RemoveEntry(const FilePath& file_path) {
    return file_system_->RemoveEntryFromFileSystem(file_path) ==
        GDATA_FILE_OK;
  }

  FilePath GetCachePathForFile(const std::string& resource_id,
                               const std::string& md5) {
    return cache_->GetCacheFilePath(resource_id,
                                    md5,
                                    GDataCache::CACHE_TYPE_TMP,
                                    GDataCache::CACHED_FILE_FROM_SERVER);
  }

  // Gets entry info by path synchronously.
  scoped_ptr<GDataEntryProto> GetEntryInfoByPathSync(
      const FilePath& file_path) {
    file_system_->GetEntryInfoByPath(
        file_path,
        base::Bind(&CallbackHelper::GetEntryInfoCallback,
                   callback_helper_.get()));
    message_loop_.RunAllPending();

    return callback_helper_->entry_proto_.Pass();
  }

  // Gets file info by path synchronously.
  scoped_ptr<GDataFileProto> GetFileInfoByPathSync(
      const FilePath& file_path) {
    file_system_->GetFileInfoByPath(
        file_path,
        base::Bind(&CallbackHelper::GetFileInfoCallback,
                   callback_helper_.get()));
    message_loop_.RunAllPending();

    return callback_helper_->file_proto_.Pass();
  }

  // Gets directory info by path synchronously.
  scoped_ptr<GDataDirectoryProto> ReadDirectoryByPathSync(
      const FilePath& file_path) {
    file_system_->ReadDirectoryByPath(
        file_path,
        base::Bind(&CallbackHelper::ReadDirectoryCallback,
                   callback_helper_.get()));
    message_loop_.RunAllPending();

    return callback_helper_->directory_proto_.Pass();
  }

  // Returns true if an entry exists at |file_path|.
  bool EntryExists(const FilePath& file_path) {
    return GetEntryInfoByPathSync(file_path).get();
  }


  // Gets the resource ID of |file_path|. Returns an empty string if not found.
  std::string GetResourceIdByPath(const FilePath& file_path) {
    scoped_ptr<GDataEntryProto> entry_proto =
        GetEntryInfoByPathSync(file_path);
    if (entry_proto.get())
      return entry_proto->resource_id();
    else
      return "";
  }

  // Helper function to call GetCacheEntry from origin thread.
  bool GetCacheEntryFromOriginThread(const std::string& resource_id,
                                     const std::string& md5,
                                     GDataCacheEntry* cache_entry) {
    bool result = false;
    blocking_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&GDataFileSystemTest::GetCacheEntryFromOriginThreadInternal,
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
      GDataCacheEntry* cache_entry,
      bool* result) {
    *result = cache_->GetCacheEntry(resource_id, md5, cache_entry);
  }

  // Returns true if the cache entry exists for the given resource ID and MD5.
  bool CacheEntryExists(const std::string& resource_id,
                        const std::string& md5) {
    GDataCacheEntry cache_entry;
    return GetCacheEntryFromOriginThread(resource_id, md5, &cache_entry);
  }

  // Returns true if the cache file exists for the given resource ID and MD5.
  bool CacheFileExists(const std::string& resource_id,
                       const std::string& md5) {
    const FilePath file_path = cache_->GetCacheFilePath(
        resource_id,
        md5,
        GDataCache::CACHE_TYPE_TMP,
        GDataCache::CACHED_FILE_FROM_SERVER);
    return file_util::PathExists(file_path);
  }

  void TestStoreToCache(
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& source_path,
      GDataFileError expected_error,
      int expected_cache_state,
      GDataCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    cache_->StoreOnUIThread(
        resource_id, md5, source_path,
        GDataCache::FILE_OPERATION_COPY,
        base::Bind(&GDataFileSystemTest::VerifyCacheFileState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void TestPin(
      const std::string& resource_id,
      const std::string& md5,
      GDataFileError expected_error,
      int expected_cache_state,
      GDataCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    cache_->PinOnUIThread(
        resource_id, md5,
        base::Bind(&GDataFileSystemTest::VerifyCacheFileState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void TestMarkDirty(
      const std::string& resource_id,
      const std::string& md5,
      GDataFileError expected_error,
      int expected_cache_state,
      GDataCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = false;

    cache_->MarkDirtyOnUIThread(
        resource_id, md5,
        base::Bind(&GDataFileSystemTest::VerifyMarkDirty,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void VerifyMarkDirty(GDataFileError error,
                       const std::string& resource_id,
                       const std::string& md5,
                       const FilePath& cache_file_path) {
    VerifyCacheFileState(error, resource_id, md5);

    // Verify filename of |cache_file_path|.
    if (error == GDATA_FILE_OK) {
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
      GDataFileError expected_error,
      int expected_cache_state,
      GDataCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = true;

    cache_->CommitDirtyOnUIThread(
        resource_id, md5,
        base::Bind(&GDataFileSystemTest::VerifyCacheFileState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  // Verify the file identified by |resource_id| and |md5| is in the expected
  // cache state after |OpenFile|, that is, marked dirty and has no outgoing
  // symlink, etc.
  void VerifyCacheStateAfterOpenFile(GDataFileError error,
                                     const std::string& resource_id,
                                     const std::string& md5,
                                     const FilePath& cache_file_path) {
    expected_error_ = GDATA_FILE_OK;
    expected_cache_state_ = (test_util::TEST_CACHE_STATE_PRESENT |
                             test_util::TEST_CACHE_STATE_DIRTY |
                             test_util::TEST_CACHE_STATE_PERSISTENT);
    expected_sub_dir_type_ = GDataCache::CACHE_TYPE_PERSISTENT;
    expect_outgoing_symlink_ = false;
    VerifyMarkDirty(error, resource_id, md5, cache_file_path);
  }

  // Verify the file identified by |resource_id| and |md5| is in the expected
  // cache state after |CloseFile|, that is, marked dirty and has an outgoing
  // symlink, etc.
  void VerifyCacheStateAfterCloseFile(GDataFileError error,
                                      const std::string& resource_id,
                                      const std::string& md5) {
    expected_error_ = GDATA_FILE_OK;
    expected_cache_state_ = (test_util::TEST_CACHE_STATE_PRESENT |
                             test_util::TEST_CACHE_STATE_DIRTY |
                             test_util::TEST_CACHE_STATE_PERSISTENT);
    expected_sub_dir_type_ = GDataCache::CACHE_TYPE_PERSISTENT;
    expect_outgoing_symlink_ = true;
    VerifyCacheFileState(error, resource_id, md5);
  }

  void VerifyCacheFileState(GDataFileError error,
                            const std::string& resource_id,
                            const std::string& md5) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

    // Verify cache map.
    GDataCacheEntry cache_entry;
    const bool cache_entry_found =
        GetCacheEntryFromOriginThread(resource_id, md5, &cache_entry);
    if (test_util::ToCacheEntry(expected_cache_state_).is_present() ||
        test_util::ToCacheEntry(expected_cache_state_).is_pinned()) {
      ASSERT_TRUE(cache_entry_found);
      EXPECT_TRUE(test_util::CacheStatesEqual(
          test_util::ToCacheEntry(expected_cache_state_),
          cache_entry));
      EXPECT_EQ(expected_sub_dir_type_,
                GDataCache::GetSubDirectoryType(cache_entry));
    } else {
      EXPECT_FALSE(cache_entry_found);
    }

    // Verify actual cache file.
    FilePath dest_path = cache_->GetCacheFilePath(
        resource_id,
        md5,
        test_util::ToCacheEntry(expected_cache_state_).is_pinned() ||
        test_util::ToCacheEntry(expected_cache_state_).is_dirty() ?
                GDataCache::CACHE_TYPE_PERSISTENT :
                GDataCache::CACHE_TYPE_TMP,
        test_util::ToCacheEntry(expected_cache_state_).is_dirty() ?
            GDataCache::CACHED_FILE_LOCALLY_MODIFIED :
            GDataCache::CACHED_FILE_FROM_SERVER);
    bool exists = file_util::PathExists(dest_path);
    if (test_util::ToCacheEntry(expected_cache_state_).is_present())
      EXPECT_TRUE(exists);
    else
      EXPECT_FALSE(exists);

    // Verify symlink in pinned dir.
    FilePath symlink_path = cache_->GetCacheFilePath(
        resource_id,
        std::string(),
        GDataCache::CACHE_TYPE_PINNED,
        GDataCache::CACHED_FILE_FROM_SERVER);
    // Check that pin symlink exists, without deferencing to target path.
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
        GDataCache::CACHE_TYPE_OUTGOING,
        GDataCache::CACHED_FILE_FROM_SERVER);
    // Check that outgoing symlink exists, without deferencing to target path.
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
    EXPECT_CALL(*mock_doc_service_, GetDocumentEntry(resource_id, _))
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
  // drive/File1, drive/Dir1/File2, drive/Dir1/SubDir2/File3
  void SaveTestFileSystem() {
    GDataRootDirectoryProto root;
    GDataDirectoryProto* root_dir = root.mutable_gdata_directory();
    GDataEntryProto* file_base = root_dir->mutable_gdata_entry();
    PlatformFileInfoProto* platform_info = file_base->mutable_file_info();
    file_base->set_title("drive");
    file_base->set_resource_id(kGDataRootDirectoryResourceId);
    file_base->set_upload_url("http://resumable-create-media/1");
    platform_info->set_is_directory(true);

    // drive/File1
    GDataFileProto* file = root_dir->add_child_files();
    file_base = file->mutable_gdata_entry();
    file_base->set_title("File1");
    file_base->set_upload_url("http://resumable-edit-media/1");
    platform_info = file_base->mutable_file_info();
    platform_info->set_is_directory(false);
    platform_info->set_size(1048576);

    // drive/Dir1
    GDataDirectoryProto* dir1 = root_dir->add_child_directories();
    file_base = dir1->mutable_gdata_entry();
    file_base->set_title("Dir1");
    file_base->set_upload_url("http://resumable-create-media/2");
    platform_info = file_base->mutable_file_info();
    platform_info->set_is_directory(true);

    // drive/Dir1/File2
    file = dir1->add_child_files();
    file_base = file->mutable_gdata_entry();
    file_base->set_title("File2");
    file_base->set_upload_url("http://resumable-edit-media/2");
    platform_info = file_base->mutable_file_info();
    platform_info->set_is_directory(false);
    platform_info->set_size(555);

    // drive/Dir1/SubDir2
    GDataDirectoryProto* dir2 = dir1->add_child_directories();
    file_base = dir2->mutable_gdata_entry();
    file_base->set_title("SubDir2");
    file_base->set_upload_url("http://resumable-create-media/3");
    platform_info = file_base->mutable_file_info();
    platform_info->set_is_directory(true);

    // drive/Dir1/SubDir2/File3
    file = dir2->add_child_files();
    file_base = file->mutable_gdata_entry();
    file_base->set_title("File3");
    file_base->set_upload_url("http://resumable-edit-media/3");
    platform_info = file_base->mutable_file_info();
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
  void VerifyHostedDocumentJSONFile(const GDataFileProto& file_proto,
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

    EXPECT_EQ(file_proto.alternate_url(), edit_url);
    EXPECT_EQ(file_proto.gdata_entry().resource_id(), resource_id);
  }

  // This is used as a helper for registering callbacks that need to be
  // RefCountedThreadSafe, and a place where we can fetch results from various
  // operations.
  class CallbackHelper
    : public base::RefCountedThreadSafe<CallbackHelper> {
   public:
    CallbackHelper()
        : last_error_(GDATA_FILE_OK),
          quota_bytes_total_(0),
          quota_bytes_used_(0),
          file_proto_(NULL) {}

    virtual void GetFileCallback(GDataFileError error,
                                 const FilePath& file_path,
                                 const std::string& mime_type,
                                 GDataFileType file_type) {
      last_error_ = error;
      download_path_ = file_path;
      mime_type_ = mime_type;
      file_type_ = file_type;
    }

    virtual void FileOperationCallback(GDataFileError error) {
      DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

      last_error_ = error;
    }

    virtual void GetAvailableSpaceCallback(GDataFileError error,
                                           int64 bytes_total,
                                           int64 bytes_used) {
      last_error_ = error;
      quota_bytes_total_ = bytes_total;
      quota_bytes_used_ = bytes_used;
    }

    virtual void OpenFileCallback(GDataFileError error,
                                  const FilePath& file_path) {
      last_error_ = error;
      opened_file_path_ = file_path;
      MessageLoop::current()->Quit();
    }

    virtual void CloseFileCallback(GDataFileError error) {
      last_error_ = error;
      MessageLoop::current()->Quit();
    }

    virtual void GetEntryInfoCallback(
        GDataFileError error,
        scoped_ptr<GDataEntryProto> entry_proto) {
      last_error_ = error;
      entry_proto_ = entry_proto.Pass();
    }

    virtual void GetFileInfoCallback(
        GDataFileError error,
        scoped_ptr<GDataFileProto> file_proto) {
      last_error_ = error;
      file_proto_ = file_proto.Pass();
    }

    virtual void ReadDirectoryCallback(
        GDataFileError error,
        bool /* hide_hosted_documents */,
        scoped_ptr<GDataDirectoryProto> directory_proto) {
      last_error_ = error;
      directory_proto_ = directory_proto.Pass();
    }

    GDataFileError last_error_;
    FilePath download_path_;
    FilePath opened_file_path_;
    std::string mime_type_;
    GDataFileType file_type_;
    int64 quota_bytes_total_;
    int64 quota_bytes_used_;
    scoped_ptr<GDataEntryProto> entry_proto_;
    scoped_ptr<GDataFileProto> file_proto_;
    scoped_ptr<GDataDirectoryProto> directory_proto_;

   protected:
    virtual ~CallbackHelper() {}

   private:
    friend class base::RefCountedThreadSafe<CallbackHelper>;
  };

  MessageLoopForUI message_loop_;
  // The order of the test threads is important, do not change the order.
  // See also content/browser/browser_thread_imple.cc.
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<CallbackHelper> callback_helper_;
  GDataCache* cache_;
  scoped_ptr<StrictMock<MockGDataUploader> > mock_uploader_;
  GDataFileSystem* file_system_;
  StrictMock<MockDocumentsService>* mock_doc_service_;
  scoped_ptr<StrictMock<MockDriveWebAppsRegistry> > mock_webapps_registry_;
  StrictMock<MockFreeDiskSpaceGetter>* mock_free_disk_space_checker_;
  scoped_ptr<StrictMock<MockGDataSyncClient> > mock_sync_client_;
  scoped_ptr<StrictMock<MockDirectoryChangeObserver> > mock_directory_observer_;

  int num_callback_invocations_;
  GDataFileError expected_error_;
  int expected_cache_state_;
  GDataCache::CacheSubDirectoryType expected_sub_dir_type_;
  bool expected_success_;
  bool expect_outgoing_symlink_;
  std::string expected_file_extension_;
  int root_feed_changestamp_;
  static bool cros_initialized_;
};

bool GDataFileSystemTest::cros_initialized_ = false;

void AsyncInitializationCallback(
    int* counter,
    int expected_counter,
    const FilePath& expected_file_path,
    MessageLoop* message_loop,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto) {
  ASSERT_EQ(GDATA_FILE_OK, error);
  ASSERT_TRUE(entry_proto.get());
  ASSERT_TRUE(entry_proto->file_info().is_directory());
  EXPECT_EQ(expected_file_path.value(), entry_proto->file_name());

  (*counter)++;
  if (*counter >= expected_counter)
    message_loop->Quit();
}

TEST_F(GDataFileSystemTest, DuplicatedAsyncInitialization) {
  int counter = 0;
  GetEntryInfoCallback callback = base::Bind(
      &AsyncInitializationCallback,
      &counter,
      2,
      FilePath(FILE_PATH_LITERAL("drive")),
      &message_loop_);

  EXPECT_CALL(*mock_doc_service_, GetAccountMetadata(_)).Times(1);
  EXPECT_CALL(*mock_doc_service_,
              GetDocuments(Eq(GURL()), _, _, _, _)).Times(1);

  EXPECT_CALL(*mock_webapps_registry_, UpdateFromFeed(NotNull())).Times(1);

  file_system_->GetEntryInfoByPath(
      FilePath(FILE_PATH_LITERAL("drive")), callback);
  file_system_->GetEntryInfoByPath(
      FilePath(FILE_PATH_LITERAL("drive")), callback);
  message_loop_.Run();  // Wait to get our result
  EXPECT_EQ(2, counter);
}

TEST_F(GDataFileSystemTest, SearchRootDirectory) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath = FilePath(FILE_PATH_LITERAL("drive"));
  scoped_ptr<GDataEntryProto> entry = GetEntryInfoByPathSync(
      FilePath(FILE_PATH_LITERAL(kFilePath)));
  ASSERT_TRUE(entry.get());
  EXPECT_EQ(kGDataRootDirectoryResourceId, entry->resource_id());
}

TEST_F(GDataFileSystemTest, SearchExistingFile) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<GDataEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file:2_file_resource_id", entry->resource_id());
}

TEST_F(GDataFileSystemTest, SearchExistingDocument) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/Document 1.gdoc"));
  scoped_ptr<GDataEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("document:5_document_resource_id", entry->resource_id());
}

TEST_F(GDataFileSystemTest, SearchNonExistingFile) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/nonexisting.file"));
  scoped_ptr<GDataEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_FALSE(entry.get());
}

TEST_F(GDataFileSystemTest, SearchEncodedFileNames) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath1 = FilePath(
      FILE_PATH_LITERAL("drive/Slash / in file 1.txt"));
  scoped_ptr<GDataEntryProto> entry = GetEntryInfoByPathSync(kFilePath1);
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

TEST_F(GDataFileSystemTest, SearchEncodedFileNamesLoadingRoot) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath1 = FilePath(
      FILE_PATH_LITERAL("drive/Slash / in file 1.txt"));
  scoped_ptr<GDataEntryProto> entry = GetEntryInfoByPathSync(kFilePath1);
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

TEST_F(GDataFileSystemTest, SearchDuplicateNames) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath1 = FilePath(
      FILE_PATH_LITERAL("drive/Duplicate Name.txt"));
  scoped_ptr<GDataEntryProto> entry = GetEntryInfoByPathSync(kFilePath1);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file:3_file_resource_id", entry->resource_id());

  const FilePath kFilePath2 = FilePath(
      FILE_PATH_LITERAL("drive/Duplicate Name (2).txt"));
  entry = GetEntryInfoByPathSync(kFilePath2);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ("file:4_file_resource_id", entry->resource_id());
}

TEST_F(GDataFileSystemTest, SearchExistingDirectory) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/Directory 1"));
  scoped_ptr<GDataEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  ASSERT_EQ("folder:1_folder_resource_id", entry->resource_id());
}

TEST_F(GDataFileSystemTest, SearchInSubdir) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
  scoped_ptr<GDataEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  ASSERT_EQ("file:subdirectory_file_1_id", entry->resource_id());
}

// Check the reconstruction of the directory structure from only the root feed.
TEST_F(GDataFileSystemTest, SearchInSubSubdir) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/Directory 1/Sub Directory Folder/"
                        "Sub Sub Directory Folder"));
  scoped_ptr<GDataEntryProto> entry = GetEntryInfoByPathSync(kFilePath);
  ASSERT_TRUE(entry.get());
  ASSERT_EQ("folder:sub_sub_directory_folder_id", entry->resource_id());
}

TEST_F(GDataFileSystemTest, FilePathTests) {
  LoadRootFeedDocument("root_feed.json");

  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL("drive/File 1.txt"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))));
  EXPECT_TRUE(EntryExists(
      FilePath(
          FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"))));
}

TEST_F(GDataFileSystemTest, ChangeFeed_AddAndDeleteFileInRoot) {
  int latest_changelog = 0;
  LoadRootFeedDocument("root_feed.json");

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(2);

  LoadChangeFeed("delta_file_added_in_root.json", ++latest_changelog);
  EXPECT_TRUE(
      EntryExists(FilePath(FILE_PATH_LITERAL("drive/Added file.gdoc"))));

  LoadChangeFeed("delta_file_deleted_in_root.json", ++latest_changelog);
  EXPECT_FALSE(
      EntryExists(FilePath(FILE_PATH_LITERAL("drive/Added file.gdoc"))));
}


TEST_F(GDataFileSystemTest, ChangeFeed_AddAndDeleteFileFromExistingDirectory) {
  int latest_changelog = 0;
  LoadRootFeedDocument("root_feed.json");

  EXPECT_TRUE(EntryExists(FilePath(
      FILE_PATH_LITERAL("drive/Directory 1"))));

  // Add file to an existing directory.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  LoadChangeFeed("delta_file_added_in_directory.json", ++latest_changelog);
  EXPECT_TRUE(EntryExists(FilePath(
      FILE_PATH_LITERAL("drive/Directory 1/Added file.gdoc"))));

  // Remove that file from the directory.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  LoadChangeFeed("delta_file_deleted_in_directory.json", ++latest_changelog);
  EXPECT_TRUE(EntryExists(FilePath(
      FILE_PATH_LITERAL("drive/Directory 1"))));
  EXPECT_FALSE(EntryExists(FilePath(
      FILE_PATH_LITERAL("drive/Directory 1/Added file.gdoc"))));
}

TEST_F(GDataFileSystemTest, ChangeFeed_AddFileToNewDirectory) {
  int latest_changelog = 0;
  LoadRootFeedDocument("root_feed.json");
  // Add file to a new directory.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/New Directory"))))).Times(1);

  LoadChangeFeed("delta_file_added_in_new_directory.json", ++latest_changelog);

  EXPECT_TRUE(EntryExists(FilePath(
      FILE_PATH_LITERAL("drive/New Directory"))));
  EXPECT_TRUE(EntryExists(FilePath(
      FILE_PATH_LITERAL("drive/New Directory/File in new dir.gdoc"))));
}

TEST_F(GDataFileSystemTest, ChangeFeed_AddFileToNewButDeletedDirectory) {
  int latest_changelog = 0;
  LoadRootFeedDocument("root_feed.json");

  // This feed contains thw following updates:
  // 1) A new PDF file is added to a new directory
  // 2) but the new directory is marked "deleted" (i.e. moved to Trash)
  // Hence, the PDF file should be just ignored.
  LoadChangeFeed("delta_file_added_in_new_but_deleted_directory.json",
                 ++latest_changelog);
}

TEST_F(GDataFileSystemTest, ChangeFeed_DirectoryMovedFromRootToDirectory) {
  int latest_changelog = 0;
  LoadRootFeedDocument("root_feed.json");

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
  LoadChangeFeed("delta_dir_moved_from_root_to_directory.json",
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

TEST_F(GDataFileSystemTest, ChangeFeed_FileMovedFromDirectoryToRoot) {
  int latest_changelog = 0;
  LoadRootFeedDocument("root_feed.json");

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
  LoadChangeFeed("delta_file_moved_from_directory_to_root.json",
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

TEST_F(GDataFileSystemTest, ChangeFeed_FileRenamedInDirectory) {
  int latest_changelog = 0;
  LoadRootFeedDocument("root_feed.json");

  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  LoadChangeFeed("delta_file_renamed_in_directory.json",
                 ++latest_changelog);

  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_FALSE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/New SubDirectory File 1.txt"))));
}

TEST_F(GDataFileSystemTest, CachedFeedLoading) {
  SaveTestFileSystem();
  TestLoadMetadataFromCache();

  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL("drive/File1"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL("drive/Dir1"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL("drive/Dir1/File2"))));
  EXPECT_TRUE(EntryExists(FilePath(FILE_PATH_LITERAL("drive/Dir1/SubDir2"))));
  EXPECT_TRUE(EntryExists(
      FilePath(FILE_PATH_LITERAL("drive/Dir1/SubDir2/File3"))));
}

TEST_F(GDataFileSystemTest, TransferFileFromLocalToRemote_RegularFile) {
  LoadRootFeedDocument("root_feed.json");

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

  // Transfer the local file to Drive.
  file_system_->TransferFileFromLocalToRemote(
      local_src_file_path, remote_dest_file_path, callback);
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(GDATA_FILE_OK, callback_helper_->last_error_);

  // Now the remote file should exist.
  EXPECT_TRUE(EntryExists(remote_dest_file_path));
}

TEST_F(GDataFileSystemTest, TransferFileFromLocalToRemote_HostedDocument) {
  LoadRootFeedDocument("root_feed.json");

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
  scoped_ptr<base::Value> document(LoadJSONFile("uploaded_document.json"));
  EXPECT_CALL(*mock_doc_service_,
              CopyDocument(kResourceId,
                           FILE_PATH_LITERAL("Document 1"),
                           _))
      .WillOnce(MockCopyDocument(gdata::HTTP_SUCCESS, &document));
  // We'll then add the hosted document to the destination directory.
  EXPECT_CALL(*mock_doc_service_,
              AddResourceToDirectory(_, _, _)).Times(1);

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  // Transfer the local file to Drive.
  file_system_->TransferFileFromLocalToRemote(
      local_src_file_path, remote_dest_file_path, callback);
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(GDATA_FILE_OK, callback_helper_->last_error_);

  // Now the remote file should exist.
  EXPECT_TRUE(EntryExists(remote_dest_file_path));
}

TEST_F(GDataFileSystemTest, TransferFileFromRemoteToLocal_RegularFile) {
  LoadRootFeedDocument("root_feed.json");

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath local_dest_file_path = temp_dir.path().Append("local_copy.txt");

  FilePath remote_src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<GDataFileProto> file = GetFileInfoByPathSync(
      remote_src_file_path);
  FilePath cache_file = GetCachePathForFile(file->gdata_entry().resource_id(),
                                            file->file_md5());
  const int64 file_size = file->gdata_entry().file_info().size();

  // Pretend we have enough space.
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(2).WillRepeatedly(Return(file_size + kMinFreeSpace));

  const std::string remote_src_file_data = "Test file data";
  mock_doc_service_->set_file_data(new std::string(remote_src_file_data));

  // Before Download starts metadata from server will be fetched.
  // We will read content url from the result.
  scoped_ptr<base::Value> document(LoadJSONFile("document_to_download.json"));
  SetExpectationsForGetDocumentEntry(&document, "file:2_file_resource_id");

  // The file is obtained with the mock DocumentsService.
  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(remote_src_file_path,
                           cache_file,
                           GURL("https://file_content_url_changed/"),
                           _, _))
      .Times(1);

  file_system_->TransferFileFromRemoteToLocal(
      remote_src_file_path, local_dest_file_path, callback);
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(GDATA_FILE_OK, callback_helper_->last_error_);

  std::string cache_file_data;
  EXPECT_TRUE(file_util::ReadFileToString(cache_file, &cache_file_data));
  EXPECT_EQ(remote_src_file_data, cache_file_data);

  std::string local_dest_file_data;
  EXPECT_TRUE(file_util::ReadFileToString(local_dest_file_path,
                                          &local_dest_file_data));
  EXPECT_EQ(remote_src_file_data, local_dest_file_data);
}

TEST_F(GDataFileSystemTest, TransferFileFromRemoteToLocal_HostedDocument) {
  LoadRootFeedDocument("root_feed.json");

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

  EXPECT_EQ(GDATA_FILE_OK, callback_helper_->last_error_);

  scoped_ptr<GDataFileProto> file_proto = GetFileInfoByPathSync(
      remote_src_file_path);
  ASSERT_TRUE(file_proto.get());
  VerifyHostedDocumentJSONFile(*file_proto, local_dest_file_path);
}

TEST_F(GDataFileSystemTest, CopyNotExistingFile) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/Dummy file.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  EXPECT_FALSE(EntryExists(src_file_path));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Copy(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result
  EXPECT_EQ(GDATA_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_FALSE(EntryExists(dest_file_path));
}

TEST_F(GDataFileSystemTest, CopyFileToNonExistingDirectory) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Dummy"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Dummy/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<GDataFileProto> src_file_proto = GetFileInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_file_proto.get());
  std::string src_file_path_resource_id =
      src_file_proto->gdata_entry().resource_id();
  EXPECT_FALSE(src_file_proto->gdata_entry().edit_url().empty());

  EXPECT_FALSE(EntryExists(dest_parent_path));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();
  EXPECT_EQ(GDATA_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_TRUE(EntryExists(src_file_path));
  EXPECT_FALSE(EntryExists(dest_parent_path));
  EXPECT_FALSE(EntryExists(dest_file_path));
}

// Test the case where the parent of |dest_file_path| is a existing file,
// not a directory.
TEST_F(GDataFileSystemTest, CopyFileToInvalidPath) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/Document 1.gdoc"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Duplicate Name.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL(
      "drive/Duplicate Name.txt/Document 1.gdoc"));

  LoadRootFeedDocument("root_feed.json");

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<GDataFileProto> src_file_proto = GetFileInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_file_proto.get());
  std::string src_file_resource_id =
      src_file_proto->gdata_entry().resource_id();
  EXPECT_FALSE(src_file_proto->gdata_entry().edit_url().empty());

  ASSERT_TRUE(EntryExists(dest_parent_path));
  scoped_ptr<GDataFileProto> dest_file_proto = GetFileInfoByPathSync(
      dest_parent_path);
  ASSERT_TRUE(dest_file_proto.get());

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Copy(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();
  EXPECT_EQ(GDATA_FILE_ERROR_NOT_A_DIRECTORY,
            callback_helper_->last_error_);

  EXPECT_TRUE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(dest_parent_path));

  EXPECT_FALSE(EntryExists(dest_file_path));
}

TEST_F(GDataFileSystemTest, RenameFile) {
  const FilePath src_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
  const FilePath src_parent_path(FILE_PATH_LITERAL("drive/Directory 1"));
  const FilePath dest_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<GDataFileProto> src_file_proto = GetFileInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_file_proto.get());
  std::string src_file_resource_id =
      src_file_proto->gdata_entry().resource_id();

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(GURL(src_file_proto->gdata_entry().edit_url()),
                             FILE_PATH_LITERAL("Test.log"), _));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();
  EXPECT_EQ(GDATA_FILE_OK, callback_helper_->last_error_);

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(dest_file_path));
  EXPECT_EQ(src_file_resource_id, GetResourceIdByPath(dest_file_path));
}

TEST_F(GDataFileSystemTest, MoveFileFromRootToSubDirectory) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Directory 1"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Directory 1/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<GDataFileProto> src_file_proto = GetFileInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_file_proto.get());
  std::string src_file_resource_id =
      src_file_proto->gdata_entry().resource_id();
  EXPECT_FALSE(src_file_proto->gdata_entry().edit_url().empty());

  ASSERT_TRUE(EntryExists(dest_parent_path));
  scoped_ptr<GDataEntryProto> dest_parent_proto = GetEntryInfoByPathSync(
      dest_parent_path);
  ASSERT_TRUE(dest_parent_proto.get());
  ASSERT_TRUE(dest_parent_proto->file_info().is_directory());
  EXPECT_FALSE(dest_parent_proto->content_url().empty());

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(GURL(src_file_proto->gdata_entry().edit_url()),
                             FILE_PATH_LITERAL("Test.log"), _));
  EXPECT_CALL(*mock_doc_service_,
              AddResourceToDirectory(
                  GURL(dest_parent_proto->content_url()),
                  GURL(src_file_proto->gdata_entry().edit_url()), _));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  // Expect notification for both source and destination directories.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();
  EXPECT_EQ(GDATA_FILE_OK, callback_helper_->last_error_);

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(dest_file_path));
  EXPECT_EQ(src_file_resource_id, GetResourceIdByPath(dest_file_path));
}

TEST_F(GDataFileSystemTest, MoveFileFromSubDirectoryToRoot) {
  FilePath src_parent_path(FILE_PATH_LITERAL("drive/Directory 1"));
  FilePath src_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<GDataFileProto> src_file_proto = GetFileInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_file_proto.get());
  std::string src_file_resource_id =
      src_file_proto->gdata_entry().resource_id();
  EXPECT_FALSE(src_file_proto->gdata_entry().edit_url().empty());

  ASSERT_TRUE(EntryExists(src_parent_path));
  scoped_ptr<GDataEntryProto> src_parent_proto = GetEntryInfoByPathSync(
      src_parent_path);
  ASSERT_TRUE(src_parent_proto.get());
  ASSERT_TRUE(src_parent_proto->file_info().is_directory());
  EXPECT_FALSE(src_parent_proto->content_url().empty());

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(GURL(src_file_proto->gdata_entry().edit_url()),
                             FILE_PATH_LITERAL("Test.log"), _));
  EXPECT_CALL(*mock_doc_service_,
              RemoveResourceFromDirectory(
                  GURL(src_parent_proto->content_url()),
                  GURL(src_file_proto->gdata_entry().edit_url()),
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
  message_loop_.RunAllPending();
  EXPECT_EQ(GDATA_FILE_OK, callback_helper_->last_error_);

  EXPECT_FALSE(EntryExists(src_file_path));
  ASSERT_TRUE(EntryExists(dest_file_path));
  EXPECT_EQ(src_file_resource_id, GetResourceIdByPath(dest_file_path));
}

TEST_F(GDataFileSystemTest, MoveFileBetweenSubDirectories) {
  FilePath src_parent_path(FILE_PATH_LITERAL("drive/Directory 1"));
  FilePath src_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/New Folder 1"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/New Folder 1/Test.log"));
  FilePath interim_file_path(FILE_PATH_LITERAL("drive/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  AddDirectoryFromFile(dest_parent_path, "directory_entry_atom.json");

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<GDataFileProto> src_file_proto = GetFileInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_file_proto.get());
  std::string src_file_resource_id =
      src_file_proto->gdata_entry().resource_id();
  EXPECT_FALSE(src_file_proto->gdata_entry().edit_url().empty());

  ASSERT_TRUE(EntryExists(src_parent_path));
  scoped_ptr<GDataEntryProto> src_parent_proto = GetEntryInfoByPathSync(
      src_parent_path);
  ASSERT_TRUE(src_parent_proto.get());
  ASSERT_TRUE(src_parent_proto->file_info().is_directory());
  EXPECT_FALSE(src_parent_proto->content_url().empty());

  ASSERT_TRUE(EntryExists(dest_parent_path));
  scoped_ptr<GDataEntryProto> dest_parent_proto = GetEntryInfoByPathSync(
      dest_parent_path);
  ASSERT_TRUE(dest_parent_proto.get());
  ASSERT_TRUE(dest_parent_proto->file_info().is_directory());
  EXPECT_FALSE(dest_parent_proto->content_url().empty());

  EXPECT_FALSE(EntryExists(interim_file_path));

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(GURL(src_file_proto->gdata_entry().edit_url()),
                             FILE_PATH_LITERAL("Test.log"), _));
  EXPECT_CALL(*mock_doc_service_,
              RemoveResourceFromDirectory(
                  GURL(src_parent_proto->content_url()),
                  GURL(src_file_proto->gdata_entry().edit_url()),
                  src_file_resource_id, _));
  EXPECT_CALL(*mock_doc_service_,
              AddResourceToDirectory(
                  GURL(dest_parent_proto->content_url()),
                  GURL(src_file_proto->gdata_entry().edit_url()),
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
  message_loop_.RunAllPending();
  EXPECT_EQ(GDATA_FILE_OK, callback_helper_->last_error_);

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_FALSE(EntryExists(interim_file_path));

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(dest_file_path));
  EXPECT_EQ(src_file_resource_id, GetResourceIdByPath(dest_file_path));
}

TEST_F(GDataFileSystemTest, MoveNotExistingFile) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/Dummy file.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  EXPECT_FALSE(EntryExists(src_file_path));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result
  EXPECT_EQ(GDATA_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_FALSE(EntryExists(src_file_path));
  EXPECT_FALSE(EntryExists(dest_file_path));
}

TEST_F(GDataFileSystemTest, MoveFileToNonExistingDirectory) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Dummy"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Dummy/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<GDataFileProto> src_file_proto = GetFileInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_file_proto.get());
  std::string src_file_resource_id =
      src_file_proto->gdata_entry().resource_id();
  EXPECT_FALSE(src_file_proto->gdata_entry().edit_url().empty());

  EXPECT_FALSE(EntryExists(dest_parent_path));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();
  EXPECT_EQ(GDATA_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);


  EXPECT_FALSE(EntryExists(dest_parent_path));
  EXPECT_FALSE(EntryExists(dest_file_path));
}

// Test the case where the parent of |dest_file_path| is a existing file,
// not a directory.
TEST_F(GDataFileSystemTest, MoveFileToInvalidPath) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Duplicate Name.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL(
      "drive/Duplicate Name.txt/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  ASSERT_TRUE(EntryExists(src_file_path));
  scoped_ptr<GDataFileProto> src_file_proto = GetFileInfoByPathSync(
      src_file_path);
  ASSERT_TRUE(src_file_proto.get());
  std::string src_file_resource_id =
      src_file_proto->gdata_entry().resource_id();
  EXPECT_FALSE(src_file_proto->gdata_entry().edit_url().empty());

  ASSERT_TRUE(EntryExists(dest_parent_path));
  scoped_ptr<GDataFileProto> dest_parent_proto = GetFileInfoByPathSync(
      dest_parent_path);
  ASSERT_TRUE(dest_parent_proto.get());

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();
  EXPECT_EQ(GDATA_FILE_ERROR_NOT_A_DIRECTORY,
            callback_helper_->last_error_);

  EXPECT_TRUE(EntryExists(src_file_path));
  EXPECT_TRUE(EntryExists(dest_parent_path));
  EXPECT_FALSE(EntryExists(dest_file_path));
}

TEST_F(GDataFileSystemTest, RemoveEntries) {
  LoadRootFeedDocument("root_feed.json");

  FilePath nonexisting_file(FILE_PATH_LITERAL("drive/Dummy file.txt"));
  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dir_in_root(FILE_PATH_LITERAL("drive/Directory 1"));
  FilePath file_in_subdir(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));

  ASSERT_TRUE(EntryExists(file_in_root));
  scoped_ptr<GDataFileProto> file_in_root_proto = GetFileInfoByPathSync(
      file_in_root);
  ASSERT_TRUE(file_in_root_proto.get());
  std::string file_in_root_resource_id =
      file_in_root_proto->gdata_entry().resource_id();

  ASSERT_TRUE(EntryExists(dir_in_root));
  scoped_ptr<GDataEntryProto> dir_in_root_proto = GetEntryInfoByPathSync(
      dir_in_root);
  ASSERT_TRUE(dir_in_root_proto.get());
  ASSERT_TRUE(dir_in_root_proto->file_info().is_directory());

  ASSERT_TRUE(EntryExists(file_in_subdir));
  scoped_ptr<GDataFileProto> file_in_subdir_proto = GetFileInfoByPathSync(
      file_in_subdir);
  ASSERT_TRUE(file_in_subdir_proto.get());
  std::string file_in_subdir_resource_id =
      file_in_subdir_proto->gdata_entry().resource_id();

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

TEST_F(GDataFileSystemTest, CreateDirectory) {
  LoadRootFeedDocument("root_feed.json");

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  // Create directory in root.
  FilePath dir_path(FILE_PATH_LITERAL("drive/New Folder 1"));
  EXPECT_FALSE(EntryExists(dir_path));
  AddDirectoryFromFile(dir_path, "directory_entry_atom.json");
  EXPECT_TRUE(EntryExists(dir_path));

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/New Folder 1"))))).Times(1);

  // Create directory in a sub dirrectory.
  FilePath subdir_path(FILE_PATH_LITERAL("drive/New Folder 1/New Folder 2"));
  EXPECT_FALSE(EntryExists(subdir_path));
  AddDirectoryFromFile(subdir_path, "directory_entry_atom.json");
  EXPECT_TRUE(EntryExists(subdir_path));
}

TEST_F(GDataFileSystemTest, FindFirstMissingParentDirectory) {
  LoadRootFeedDocument("root_feed.json");

  GURL last_dir_content_url;
  FilePath first_missing_parent_path;

  // Create directory in root.
  FilePath dir_path(FILE_PATH_LITERAL("drive/New Folder 1"));
  EXPECT_EQ(
      GDataFileSystem::FOUND_MISSING,
      file_system_->FindFirstMissingParentDirectory(dir_path,
          &last_dir_content_url,
          &first_missing_parent_path));
  EXPECT_EQ(FilePath(FILE_PATH_LITERAL("drive/New Folder 1")),
            first_missing_parent_path);
  EXPECT_TRUE(last_dir_content_url.is_empty());    // root directory.

  // Missing folders in subdir of an existing folder.
  FilePath dir_path2(FILE_PATH_LITERAL("drive/Directory 1/New Folder 2"));
  EXPECT_EQ(
      GDataFileSystem::FOUND_MISSING,
      file_system_->FindFirstMissingParentDirectory(dir_path2,
          &last_dir_content_url,
          &first_missing_parent_path));
  EXPECT_EQ(FilePath(FILE_PATH_LITERAL("drive/Directory 1/New Folder 2")),
            first_missing_parent_path);
  EXPECT_FALSE(last_dir_content_url.is_empty());    // non-root directory.

  // Missing two folders on the path.
  FilePath dir_path3 = dir_path2.Append(FILE_PATH_LITERAL("Another Folder"));
  EXPECT_EQ(
      GDataFileSystem::FOUND_MISSING,
      file_system_->FindFirstMissingParentDirectory(dir_path3,
          &last_dir_content_url,
          &first_missing_parent_path));
  EXPECT_EQ(FilePath(FILE_PATH_LITERAL("drive/Directory 1/New Folder 2")),
            first_missing_parent_path);
  EXPECT_FALSE(last_dir_content_url.is_empty());    // non-root directory.

  // Folders on top of an existing file.
  EXPECT_EQ(
      GDataFileSystem::FOUND_INVALID,
      file_system_->FindFirstMissingParentDirectory(
          FilePath(FILE_PATH_LITERAL("drive/File 1.txt/BadDir")),
          &last_dir_content_url,
          &first_missing_parent_path));

  // Existing folder.
  EXPECT_EQ(
      GDataFileSystem::DIRECTORY_ALREADY_PRESENT,
      file_system_->FindFirstMissingParentDirectory(
          FilePath(FILE_PATH_LITERAL("drive/Directory 1")),
          &last_dir_content_url,
          &first_missing_parent_path));
}

// Create a directory through the document service
TEST_F(GDataFileSystemTest, CreateDirectoryWithService) {
  LoadRootFeedDocument("root_feed.json");
  EXPECT_CALL(*mock_doc_service_,
              CreateDirectory(_, "Sample Directory Title", _)).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  // Set last error so it's not a valid error code.
  callback_helper_->last_error_ = static_cast<GDataFileError>(1);
  file_system_->CreateDirectory(
      FilePath(FILE_PATH_LITERAL("drive/Sample Directory Title")),
      false,  // is_exclusive
      true,  // is_recursive
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get()));
  message_loop_.RunAllPending();
  // TODO(gspencer): Uncomment this when we get a blob that
  // works that can be returned from the mock.
  // EXPECT_EQ(GDATA_FILE_OK, callback_helper_->last_error_);
}

TEST_F(GDataFileSystemTest, GetFileByPath_FromGData_EnoughSpace) {
  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<GDataFileProto> file_proto(GetFileInfoByPathSync(file_in_root));
  FilePath downloaded_file = GetCachePathForFile(
      file_proto->gdata_entry().resource_id(),
      file_proto->file_md5());
  const int64 file_size = file_proto->gdata_entry().file_info().size();

  // Pretend we have enough space.
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(2).WillRepeatedly(Return(file_size + kMinFreeSpace));

  // Before Download starts metadata from server will be fetched.
  // We will read content url from the result.
  scoped_ptr<base::Value> document(LoadJSONFile("document_to_download.json"));
  SetExpectationsForGetDocumentEntry(&document, "file:2_file_resource_id");

  // The file is obtained with the mock DocumentsService.
  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url_changed/"),
                           _, _))
      .Times(1);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetDownloadDataCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(GDATA_FILE_OK, callback_helper_->last_error_);
  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_EQ(downloaded_file.value(),
            callback_helper_->download_path_.value());
}

TEST_F(GDataFileSystemTest, GetFileByPath_FromGData_NoSpaceAtAll) {
  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<GDataFileProto> file_proto(GetFileInfoByPathSync(file_in_root));
  FilePath downloaded_file = GetCachePathForFile(
      file_proto->gdata_entry().resource_id(),
      file_proto->file_md5());

  // Pretend we have no space at all.
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(2).WillRepeatedly(Return(0));

  // Before Download starts metadata from server will be fetched.
  // We will read content url from the result.
  scoped_ptr<base::Value> document(LoadJSONFile("document_to_download.json"));
  SetExpectationsForGetDocumentEntry(&document, "file:2_file_resource_id");

  // The file is not obtained with the mock DocumentsService, because of no
  // space.
  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url_changed/"),
                           _, _))
      .Times(0);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetDownloadDataCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(GDATA_FILE_ERROR_NO_SPACE,
            callback_helper_->last_error_);
}

TEST_F(GDataFileSystemTest, GetFileByPath_FromGData_NoEnoughSpaceButCanFreeUp) {
  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<GDataFileProto> file_proto(GetFileInfoByPathSync(file_in_root));
  FilePath downloaded_file = GetCachePathForFile(
      file_proto->gdata_entry().resource_id(),
      file_proto->file_md5());
  const int64 file_size = file_proto->gdata_entry().file_info().size();

  // Pretend we have no space first (checked before downloading a file),
  // but then start reporting we have space. This is to emulate that
  // the disk space was freed up by removing temporary files.
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .WillOnce(Return(0))
      .WillOnce(Return(file_size + kMinFreeSpace))
      .WillOnce(Return(file_size + kMinFreeSpace));

  // Store something in the temporary cache directory.
  TestStoreToCache("<resource_id>",
                   "<md5>",
                   GetTestFilePath("root_feed.json"),
                   GDATA_FILE_OK,
                   test_util::TEST_CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);
  ASSERT_TRUE(CacheEntryExists("<resource_id>", "<md5>"));
  ASSERT_TRUE(CacheFileExists("<resource_id>", "<md5>"));

  // Before Download starts metadata from server will be fetched.
  // We will read content url from the result.
  scoped_ptr<base::Value> document(LoadJSONFile("document_to_download.json"));
  SetExpectationsForGetDocumentEntry(&document, "file:2_file_resource_id");

  // The file is obtained with the mock DocumentsService, because of we freed
  // up the space.
  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url_changed/"),
                           _, _))
      .Times(1);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetDownloadDataCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(GDATA_FILE_OK, callback_helper_->last_error_);
  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_EQ(downloaded_file.value(),
            callback_helper_->download_path_.value());

  // The file should be removed in order to free up space, and the cache
  // entry should also be removed.
  ASSERT_FALSE(CacheEntryExists("<resource_id>", "<md5>"));
  ASSERT_FALSE(CacheFileExists("<resource_id>", "<md5>"));
}

TEST_F(GDataFileSystemTest, GetFileByPath_FromGData_EnoughSpaceButBecomeFull) {
  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<GDataFileProto> file_proto(GetFileInfoByPathSync(file_in_root));
  FilePath downloaded_file = GetCachePathForFile(
      file_proto->gdata_entry().resource_id(),
      file_proto->file_md5());
  const int64 file_size = file_proto->gdata_entry().file_info().size();

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
  scoped_ptr<base::Value> document(LoadJSONFile("document_to_download.json"));
  SetExpectationsForGetDocumentEntry(&document, "file:2_file_resource_id");

  // The file is obtained with the mock DocumentsService.
  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url_changed/"),
                           _, _))
      .Times(1);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetDownloadDataCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(GDATA_FILE_ERROR_NO_SPACE,
            callback_helper_->last_error_);
}

TEST_F(GDataFileSystemTest, GetFileByPath_FromCache) {
  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<GDataFileProto> file_proto(GetFileInfoByPathSync(file_in_root));
  FilePath downloaded_file = GetCachePathForFile(
      file_proto->gdata_entry().resource_id(),
      file_proto->file_md5());

  // Store something as cached version of this file.
  TestStoreToCache(file_proto->gdata_entry().resource_id(),
                   file_proto->file_md5(),
                   GetTestFilePath("root_feed.json"),
                   GDATA_FILE_OK,
                   test_util::TEST_CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);

  // Make sure we don't fetch metadata for downloading file.
  EXPECT_CALL(*mock_doc_service_, GetDocumentEntry(_, _)).Times(0);

  // Make sure we don't call downloads at all.
  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url_changed/"),
                           _, _))
      .Times(0);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetDownloadDataCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_EQ(downloaded_file.value(),
            callback_helper_->download_path_.value());
}

TEST_F(GDataFileSystemTest, GetFileByPath_HostedDocument) {
  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/Document 1.gdoc"));
  scoped_ptr<GDataFileProto> src_file_proto =
      GetFileInfoByPathSync(file_in_root);
  ASSERT_TRUE(src_file_proto.get());

  file_system_->GetFileByPath(file_in_root, callback,
                              GetDownloadDataCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(HOSTED_DOCUMENT, callback_helper_->file_type_);
  EXPECT_FALSE(callback_helper_->download_path_.empty());

  ASSERT_TRUE(src_file_proto.get());
  VerifyHostedDocumentJSONFile(*src_file_proto,
                               callback_helper_->download_path_);
}

TEST_F(GDataFileSystemTest, GetFileByResourceId) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<GDataFileProto> file_proto(GetFileInfoByPathSync(file_in_root));
  FilePath downloaded_file = GetCachePathForFile(
      file_proto->gdata_entry().resource_id(),
      file_proto->file_md5());

  // Before Download starts metadata from server will be fetched.
  // We will read content url from the result.
  scoped_ptr<base::Value> document(LoadJSONFile("document_to_download.json"));
  SetExpectationsForGetDocumentEntry(&document, "file:2_file_resource_id");

  // The file is obtained with the mock DocumentsService, because it's not
  // stored in the cache.
  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url_changed/"),
                           _, _))
      .Times(1);

  file_system_->GetFileByResourceId(file_proto->gdata_entry().resource_id(),
                                    callback,
                                    GetDownloadDataCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_EQ(downloaded_file.value(),
            callback_helper_->download_path_.value());
}

TEST_F(GDataFileSystemTest, GetFileByResourceId_FromCache) {
  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<GDataFileProto> file_proto(GetFileInfoByPathSync(file_in_root));
  FilePath downloaded_file = GetCachePathForFile(
      file_proto->gdata_entry().resource_id(),
      file_proto->file_md5());

  // Store something as cached version of this file.
  TestStoreToCache(file_proto->gdata_entry().resource_id(),
                   file_proto->file_md5(),
                   GetTestFilePath("root_feed.json"),
                   GDATA_FILE_OK,
                   test_util::TEST_CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);

  // The file is obtained from the cache.
  // Make sure we don't call downloads at all.
  EXPECT_CALL(*mock_doc_service_, DownloadFile(_, _, _, _, _))
      .Times(0);

  file_system_->GetFileByResourceId(file_proto->gdata_entry().resource_id(),
                                    callback,
                                    GetDownloadDataCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_EQ(downloaded_file.value(),
            callback_helper_->download_path_.value());
}

TEST_F(GDataFileSystemTest, UpdateFileByResourceId_PersistentFile) {
  LoadRootFeedDocument("root_feed.json");

  // This is a file defined in root_feed.json.
  const FilePath kFilePath(FILE_PATH_LITERAL("drive/File 1.txt"));
  const std::string kResourceId("file:2_file_resource_id");
  const std::string kMd5("3b4382ebefec6e743578c76bbd0575ce");

  // Pin the file so it'll be store in "persistent" directory.
  EXPECT_CALL(*mock_sync_client_, OnCachePinned(kResourceId, kMd5)).Times(1);
  TestPin(kResourceId,
          kMd5,
          GDATA_FILE_OK,
          test_util::TEST_CACHE_STATE_PINNED,
          GDataCache::CACHE_TYPE_TMP);

  // First store a file to cache. A cache file will be created at:
  // GCache/v1/persistent/<kResourceId>.<kMd5>
  const FilePath original_cache_file_path =
      GDataCache::GetCacheRootPath(profile_.get())
      .AppendASCII("persistent")
      .AppendASCII(kResourceId + "." + kMd5);
  TestStoreToCache(kResourceId,
                   kMd5,
                   GetTestFilePath("root_feed.json"),  // Anything works.
                   GDATA_FILE_OK,
                   test_util::TEST_CACHE_STATE_PRESENT |
                   test_util::TEST_CACHE_STATE_PINNED |
                   test_util::TEST_CACHE_STATE_PERSISTENT,
                   GDataCache::CACHE_TYPE_PERSISTENT);
  ASSERT_TRUE(file_util::PathExists(original_cache_file_path));

  // Add the dirty bit. The cache file will be renamed to
  // GCache/v1/persistent/<kResourceId>.local
  TestMarkDirty(kResourceId,
                kMd5,
                GDATA_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_PINNED |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                GDataCache::CACHE_TYPE_PERSISTENT);
  const FilePath dirty_cache_file_path =
      GDataCache::GetCacheRootPath(profile_.get())
      .AppendASCII("persistent")
      .AppendASCII(kResourceId + ".local");
  ASSERT_FALSE(file_util::PathExists(original_cache_file_path));
  ASSERT_TRUE(file_util::PathExists(dirty_cache_file_path));

  // Commit the dirty bit. The cache file name remains the same
  // but a symlink will be created at:
  // GCache/v1/outgoing/<kResourceId>
  EXPECT_CALL(*mock_sync_client_, OnCacheCommitted(kResourceId)).Times(1);
  TestCommitDirty(kResourceId,
                  kMd5,
                  GDATA_FILE_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_PINNED |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  GDataCache::CACHE_TYPE_PERSISTENT);
  const FilePath outgoing_symlink_path =
      GDataCache::GetCacheRootPath(profile_.get())
      .AppendASCII("outgoing")
      .AppendASCII(kResourceId);
  ASSERT_TRUE(file_util::PathExists(dirty_cache_file_path));
  ASSERT_TRUE(file_util::PathExists(outgoing_symlink_path));

  // Create a DocumentEntry, which is needed to mock
  // GDataUploaderInterface::UploadExistingFile().
  // TODO(satorux): This should be cleaned up. crbug.com/134240.
  DocumentEntry* document_entry = NULL;
  scoped_ptr<base::Value> value(LoadJSONFile("root_feed.json"));
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
      892721,  // The size is written in the root_feed.json.
      "audio/mpeg",
      _))  // callback
      .WillOnce(MockUploadExistingFile(
          GDATA_FILE_OK,
          FilePath::FromUTF8Unsafe("drive/File1"),
          dirty_cache_file_path,
          document_entry));

  // We'll notify the directory change to the observer upon completion.
  EXPECT_CALL(*mock_directory_observer_,
              OnDirectoryChanged(Eq(FilePath(kGDataRootDirectory)))).Times(1);

  // The callback will be called upon completion of
  // UpdateFileByResourceId().
  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  // Check the number of files in the root directory. We'll compare the
  // number after updating a file.
  scoped_ptr<GDataDirectoryProto> root_directory_proto(
      ReadDirectoryByPathSync(FilePath::FromUTF8Unsafe("drive")));
  ASSERT_TRUE(root_directory_proto.get());
  const int num_files_in_root = root_directory_proto->child_files().size();

  file_system_->UpdateFileByResourceId(kResourceId, callback);
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(GDATA_FILE_OK, callback_helper_->last_error_);
  // Make sure that the number of files did not change (i.e. we updated an
  // existing file, rather than adding a new file. The number of files
  // increases if we don't handle the file update right).
  EXPECT_EQ(num_files_in_root, root_directory_proto->child_files().size());
  // After the file is updated, the dirty bit is cleared, hence the symlink
  // should be gone.
  ASSERT_FALSE(file_util::PathExists(outgoing_symlink_path));
}

TEST_F(GDataFileSystemTest, UpdateFileByResourceId_NonexistentFile) {
  LoadRootFeedDocument("root_feed.json");

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
  EXPECT_EQ(GDATA_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);
}

TEST_F(GDataFileSystemTest, ContentSearch) {
  LoadRootFeedDocument("root_feed.json");

  EXPECT_CALL(*mock_doc_service_, GetDocuments(Eq(GURL()), _, "foo", _, _))
      .Times(1);

  SearchCallback callback = base::Bind(&DriveSearchCallback, &message_loop_);

  file_system_->Search("foo", callback);
  message_loop_.Run();  // Wait to get our result
}

TEST_F(GDataFileSystemTest, GetAvailableSpace) {
  GetAvailableSpaceCallback callback =
      base::Bind(&CallbackHelper::GetAvailableSpaceCallback,
                 callback_helper_.get());

  EXPECT_CALL(*mock_doc_service_, GetAccountMetadata(_));

  file_system_->GetAvailableSpace(callback);
  message_loop_.RunAllPending();
  EXPECT_EQ(GG_LONGLONG(6789012345), callback_helper_->quota_bytes_used_);
  EXPECT_EQ(GG_LONGLONG(9876543210), callback_helper_->quota_bytes_total_);
}

TEST_F(GDataFileSystemTest, RequestDirectoryRefresh) {
  LoadRootFeedDocument("root_feed.json");

  // We'll fetch documents in the root directory with its resource ID.
  EXPECT_CALL(*mock_doc_service_,
              GetDocuments(Eq(GURL()), _, _, kGDataRootDirectoryResourceId, _))
      .Times(1);
  // We'll notify the directory change to the observer.
  EXPECT_CALL(*mock_directory_observer_,
              OnDirectoryChanged(Eq(FilePath(kGDataRootDirectory)))).Times(1);

  file_system_->RequestDirectoryRefresh(FilePath(kGDataRootDirectory));
  message_loop_.RunAllPending();
}

TEST_F(GDataFileSystemTest, OpenAndCloseFile) {
  LoadRootFeedDocument("root_feed.json");

  OpenFileCallback callback =
      base::Bind(&CallbackHelper::OpenFileCallback,
                 callback_helper_.get());
  FileOperationCallback close_file_callback =
      base::Bind(&CallbackHelper::CloseFileCallback,
                 callback_helper_.get());

  const FilePath kFileInRoot(FILE_PATH_LITERAL("drive/File 1.txt"));
  scoped_ptr<GDataFileProto> file_proto(GetFileInfoByPathSync(kFileInRoot));
  FilePath downloaded_file = GetCachePathForFile(
      file_proto->gdata_entry().resource_id(),
      file_proto->file_md5());
  const int64 file_size = file_proto->gdata_entry().file_info().size();
  const std::string& file_resource_id =
      file_proto->gdata_entry().resource_id();
  const std::string& file_md5 = file_proto->file_md5();

  // A dirty file is created on close.
  EXPECT_CALL(*mock_sync_client_, OnCacheCommitted(file_resource_id)).Times(1);

  // Pretend we have enough space.
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(2).WillRepeatedly(Return(file_size + kMinFreeSpace));

  const std::string kExpectedFileData = "test file data";
  mock_doc_service_->set_file_data(new std::string(kExpectedFileData));

  // Before Download starts metadata from server will be fetched.
  // We will read content url from the result.
  scoped_ptr<base::Value> document(LoadJSONFile("document_to_download.json"));
  SetExpectationsForGetDocumentEntry(&document, "file:2_file_resource_id");

  // The file is obtained with the mock DocumentsService.
  EXPECT_CALL(*mock_doc_service_,
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
  EXPECT_EQ(GDATA_FILE_OK, callback_helper_->last_error_);

  // Try to open the already opened file.
  file_system_->OpenFile(kFileInRoot, callback);
  message_loop_.Run();

  // It must fail.
  EXPECT_EQ(GDATA_FILE_ERROR_IN_USE, callback_helper_->last_error_);

  // Verify that the file contents match the expected contents.
  std::string cache_file_data;
  EXPECT_TRUE(file_util::ReadFileToString(opened_file_path, &cache_file_data));
  EXPECT_EQ(kExpectedFileData, cache_file_data);

  // Verify that the cache state was changed as expected.
  VerifyCacheStateAfterOpenFile(GDATA_FILE_OK,
                                file_resource_id,
                                file_md5,
                                opened_file_path);

  // Close kFileInRoot ("drive/File 1.txt").
  file_system_->CloseFile(kFileInRoot, close_file_callback);
  message_loop_.Run();

  // Verify that the file was properly closed.
  EXPECT_EQ(GDATA_FILE_OK, callback_helper_->last_error_);

  // Verify that the cache state was changed as expected.
  VerifyCacheStateAfterCloseFile(GDATA_FILE_OK,
                                 file_resource_id,
                                 file_md5);

  // Try to close the same file twice.
  file_system_->CloseFile(kFileInRoot, close_file_callback);
  message_loop_.Run();

  // It must fail.
  EXPECT_EQ(GDATA_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);
}

}   // namespace gdata
