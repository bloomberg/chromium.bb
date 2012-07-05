// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"
#include "chrome/browser/chromeos/gdata/gdata_cache.h"
#include "chrome/browser/chromeos/gdata/gdata_test_util.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/gdata/mock_gdata_documents_service.h"
#include "chrome/browser/chromeos/gdata/mock_gdata_sync_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtLeast;
using ::testing::Return;
using ::testing::StrictMock;

namespace gdata {
namespace {

const char kSymLinkToDevNull[] = "/dev/null";

struct InitialCacheResource {
  const char* source_file;              // Source file to be used for cache.
  const char* resource_id;              // Resource id of cache file.
  const char* md5;                      // MD5 of cache file.
  int cache_state;                      // Cache state of cache file.
  const char* expected_file_extension;  // Expected extension of cached file.
  // Expected CacheSubDirectoryType of cached file.
  GDataCache::CacheSubDirectoryType expected_sub_dir_type;
} const initial_cache_resources[] = {
  // Cache resource in tmp dir, i.e. not pinned or dirty.
  { "root_feed.json", "tmp:resource_id", "md5_tmp_alphanumeric",
    GDataCache::CACHE_STATE_PRESENT,
    "md5_tmp_alphanumeric", GDataCache::CACHE_TYPE_TMP },
  // Cache resource in tmp dir, i.e. not pinned or dirty, with resource_id
  // containing non-alphanumeric characters, to test resource_id is escaped and
  // unescaped correctly.
  { "subdir_feed.json", "tmp:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?",
    "md5_tmp_non_alphanumeric",
    GDataCache::CACHE_STATE_PRESENT,
    "md5_tmp_non_alphanumeric", GDataCache::CACHE_TYPE_TMP },
  // Cache resource that is pinned, to test a pinned file is in persistent dir
  // with a symlink in pinned dir referencing it.
  { "directory_entry_atom.json", "pinned:existing", "md5_pinned_existing",
    GDataCache::CACHE_STATE_PRESENT |
    GDataCache::CACHE_STATE_PINNED,
    "md5_pinned_existing", GDataCache::CACHE_TYPE_PERSISTENT },
  // Cache resource with a non-existent source file that is pinned, to test that
  // a pinned file can reference a non-existent file.
  { "", "pinned:non-existent", "md5_pinned_non_existent",
    GDataCache::CACHE_STATE_PINNED,
    "md5_pinned_non_existent", GDataCache::CACHE_TYPE_PINNED },
  // Cache resource that is dirty, to test a dirty file is in persistent dir
  // with a symlink in outgoing dir referencing it.
  { "account_metadata.json", "dirty:existing", "md5_dirty_existing",
    GDataCache::CACHE_STATE_PRESENT |
    GDataCache::CACHE_STATE_DIRTY,
     "local", GDataCache::CACHE_TYPE_PERSISTENT },
  // Cache resource that is pinned and dirty, to test a dirty pinned file is in
  // persistent dir with symlink in pinned and outgoing dirs referencing it.
  { "basic_feed.json", "dirty_and_pinned:existing",
    "md5_dirty_and_pinned_existing",
    GDataCache::CACHE_STATE_PRESENT |
    GDataCache::CACHE_STATE_PINNED |
    GDataCache::CACHE_STATE_DIRTY,
     "local", GDataCache::CACHE_TYPE_PERSISTENT },
};

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
}  // anonymous namespace

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
    return -1;
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

class GDataCacheTest : public testing::Test {
 protected:
  GDataCacheTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO),
        sequence_token_(
            content::BrowserThread::GetBlockingPool()->GetSequenceToken()),
        cache_(NULL),
        file_system_(NULL),
        mock_doc_service_(NULL),
        num_callback_invocations_(0),
        expected_error_(base::PLATFORM_FILE_OK),
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

    // Allocate and keep a pointer to the mock, and inject it into the
    // GDataFileSystem object, which will own the mock object.
    mock_doc_service_ = new MockDocumentsService;

    EXPECT_CALL(*mock_doc_service_, Initialize(profile_.get())).Times(1);

    // Likewise, this will be owned by GDataFileSystem.
    mock_free_disk_space_checker_ = new MockFreeDiskSpaceGetter;
    SetFreeDiskSpaceGetterForTesting(mock_free_disk_space_checker_);

    cache_ = GDataCache::CreateGDataCacheOnUIThread(
        GDataCache::GetCacheRootPath(profile_.get()),
        content::BrowserThread::GetBlockingPool(),
        sequence_token_);

    mock_uploader_.reset(new StrictMock<MockGDataUploader>);
    mock_webapps_registry_.reset(new StrictMock<MockDriveWebAppsRegistry>);

    ASSERT_FALSE(file_system_);
    file_system_ = new GDataFileSystem(profile_.get(),
                                       cache_,
                                       mock_doc_service_,
                                       mock_uploader_.get(),
                                       mock_webapps_registry_.get(),
                                       sequence_token_);

    mock_sync_client_.reset(new StrictMock<MockGDataSyncClient>);
    cache_->AddObserver(mock_sync_client_.get());

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

  // Updates the content of directory under |directory_path| with parsed feed
  // |value|.
  bool UpdateContent(const std::vector<DocumentFeed*>& list,
                     int largest_changestamp) {
    GURL unused;
    return file_system_->UpdateFromFeed(
        list,
        FROM_SERVER,
        largest_changestamp,
        root_feed_changestamp_++) == base::PLATFORM_FILE_OK;
  }

  GDataEntry* FindEntry(const FilePath& file_path) {
    return file_system_->GetGDataEntryByPath(file_path);
  }

  void PrepareForInitCacheTest() {
    DVLOG(1) << "PrepareForInitCacheTest start";
    // Create gdata cache sub directories.
    ASSERT_TRUE(file_util::CreateDirectory(
        file_system_->cache_->GetCacheDirectoryPath(
            GDataCache::CACHE_TYPE_PERSISTENT)));
    ASSERT_TRUE(file_util::CreateDirectory(
        file_system_->cache_->GetCacheDirectoryPath(
            GDataCache::CACHE_TYPE_TMP)));
    ASSERT_TRUE(file_util::CreateDirectory(
        file_system_->cache_->GetCacheDirectoryPath(
            GDataCache::CACHE_TYPE_PINNED)));
    ASSERT_TRUE(file_util::CreateDirectory(
        file_system_->cache_->GetCacheDirectoryPath(
            GDataCache::CACHE_TYPE_OUTGOING)));

    // Dump some files into cache dirs so that
    // GDataFileSystem::InitializeCacheOnBlockingPool would scan through them
    // and populate cache map accordingly.

    // Copy files from data dir to cache dir to act as cached files.
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(initial_cache_resources); ++i) {
      const struct InitialCacheResource& resource = initial_cache_resources[i];
      // Determine gdata cache file absolute path according to cache state.
      FilePath dest_path = cache_->GetCacheFilePath(
          resource.resource_id,
          resource.md5,
          GDataCache::IsCachePinned(resource.cache_state) ||
              GDataCache::IsCacheDirty(resource.cache_state) ?
                  GDataCache::CACHE_TYPE_PERSISTENT :
                  GDataCache::CACHE_TYPE_TMP,
          GDataCache::IsCacheDirty(resource.cache_state) ?
              GDataCache::CACHED_FILE_LOCALLY_MODIFIED :
              GDataCache::CACHED_FILE_FROM_SERVER);

      // Copy file from data dir to cache subdir, naming it per cache files
      // convention.
      if (GDataCache::IsCachePresent(resource.cache_state)) {
        FilePath source_path = GetTestFilePath(resource.source_file);
        ASSERT_TRUE(file_util::CopyFile(source_path, dest_path));
      } else {
        dest_path = FilePath(FILE_PATH_LITERAL(kSymLinkToDevNull));
      }

      // Create symbolic link in pinned dir, naming it per cache files
      // convention.
      if (GDataCache::IsCachePinned(resource.cache_state)) {
        FilePath link_path = cache_->GetCacheFilePath(
            resource.resource_id,
            "",
            GDataCache::CACHE_TYPE_PINNED,
            GDataCache::CACHED_FILE_FROM_SERVER);
        ASSERT_TRUE(file_util::CreateSymbolicLink(dest_path, link_path));
      }

      // Create symbolic link in outgoing dir, naming it per cache files
      // convention.
      if (GDataCache::IsCacheDirty(resource.cache_state)) {
        FilePath link_path = cache_->GetCacheFilePath(
            resource.resource_id,
            "",
            GDataCache::CACHE_TYPE_OUTGOING,
            GDataCache::CACHED_FILE_FROM_SERVER);
        ASSERT_TRUE(file_util::CreateSymbolicLink(dest_path, link_path));
      }
    }
    DVLOG(1) << "PrepareForInitCacheTest finished";
    cache_->RequestInitializeOnUIThread();  // Force a re-scan.
    test_util::RunBlockingPoolTask();
  }

  void TestInitializeCache() {
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(initial_cache_resources); ++i) {
      const struct InitialCacheResource& resource = initial_cache_resources[i];
      // Check cache file.
      num_callback_invocations_ = 0;
      TestGetFileFromCacheByResourceIdAndMd5(
          resource.resource_id,
          resource.md5,
          GDataCache::IsCachePresent(resource.cache_state) ?
          base::PLATFORM_FILE_OK :
          base::PLATFORM_FILE_ERROR_NOT_FOUND,
          resource.expected_file_extension);
      EXPECT_EQ(1, num_callback_invocations_);

      // Verify cache state.
      std::string md5;
      if (GDataCache::IsCachePresent(resource.cache_state))
         md5 = resource.md5;
      scoped_ptr<GDataCache::CacheEntry> cache_entry =
          GetCacheEntryFromOriginThread(resource.resource_id, md5);
      ASSERT_TRUE(cache_entry.get());
      EXPECT_EQ(resource.cache_state, cache_entry->cache_state);
      EXPECT_EQ(resource.expected_sub_dir_type, cache_entry->sub_dir_type);
    }
  }

  void TestGetFileFromCacheByResourceIdAndMd5(
      const std::string& resource_id,
      const std::string& md5,
      base::PlatformFileError expected_error,
      const std::string& expected_file_extension) {
    expected_error_ = expected_error;
    expected_file_extension_ = expected_file_extension;

    cache_->GetFileOnUIThread(
        resource_id, md5,
        base::Bind(&GDataCacheTest::VerifyGetFromCache,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void TestStoreToCache(
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& source_path,
      base::PlatformFileError expected_error,
      int expected_cache_state,
      GDataCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    cache_->StoreOnUIThread(
        resource_id, md5, source_path,
        GDataCache::FILE_OPERATION_COPY,
        base::Bind(&GDataCacheTest::VerifyCacheFileState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void VerifyGetFromCache(base::PlatformFileError error,
                          const std::string& resource_id,
                          const std::string& md5,
                          const FilePath& cache_file_path) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

    if (error == base::PLATFORM_FILE_OK) {
      // Verify filename of |cache_file_path|.
      FilePath base_name = cache_file_path.BaseName();
      EXPECT_EQ(util::EscapeCacheFileName(resource_id) +
                FilePath::kExtensionSeparator +
                util::EscapeCacheFileName(
                    expected_file_extension_.empty() ?
                    md5 : expected_file_extension_),
                base_name.value());
    } else {
      EXPECT_TRUE(cache_file_path.empty());
    }
  }

  void TestRemoveFromCache(const std::string& resource_id,
                           base::PlatformFileError expected_error) {
    expected_error_ = expected_error;

    cache_->RemoveOnUIThread(
        resource_id,
        base::Bind(&GDataCacheTest::VerifyRemoveFromCache,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void VerifyRemoveFromCache(base::PlatformFileError error,
                             const std::string& resource_id,
                             const std::string& md5) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

    // Verify cache map.
    scoped_ptr<GDataCache::CacheEntry> cache_entry =
        GetCacheEntryFromOriginThread(resource_id, md5);
    if (cache_entry.get())
      EXPECT_TRUE(cache_entry->IsDirty());

    // If entry doesn't exist, verify that:
    // - no files with "<resource_id>.* exists in persistent and tmp dirs
    // - no "<resource_id>" symlink exists in pinned and outgoing dirs.
    std::vector<PathToVerify> paths_to_verify;
    paths_to_verify.push_back(  // Index 0: CACHE_TYPE_TMP.
        PathToVerify(cache_->GetCacheFilePath(resource_id, "*",
                     GDataCache::CACHE_TYPE_TMP,
                     GDataCache::CACHED_FILE_FROM_SERVER), FilePath()));
    paths_to_verify.push_back(  // Index 1: CACHE_TYPE_PERSISTENT.
        PathToVerify(cache_->GetCacheFilePath(resource_id, "*",
                     GDataCache::CACHE_TYPE_PERSISTENT,
                     GDataCache::CACHED_FILE_FROM_SERVER), FilePath()));
    paths_to_verify.push_back(  // Index 2: CACHE_TYPE_PINNED.
        PathToVerify(cache_->GetCacheFilePath(resource_id, "",
                     GDataCache::CACHE_TYPE_PINNED,
                     GDataCache::CACHED_FILE_FROM_SERVER), FilePath()));
    paths_to_verify.push_back(  // Index 3: CACHE_TYPE_OUTGOING.
        PathToVerify(cache_->GetCacheFilePath(resource_id, "",
                     GDataCache::CACHE_TYPE_OUTGOING,
                     GDataCache::CACHED_FILE_FROM_SERVER), FilePath()));
    if (!cache_entry.get()) {
      for (size_t i = 0; i < paths_to_verify.size(); ++i) {
        file_util::FileEnumerator enumerator(
            paths_to_verify[i].path_to_scan.DirName(), false /* not recursive*/,
            static_cast<file_util::FileEnumerator::FileType>(
                file_util::FileEnumerator::FILES |
                file_util::FileEnumerator::SHOW_SYM_LINKS),
            paths_to_verify[i].path_to_scan.BaseName().value());
        EXPECT_TRUE(enumerator.Next().empty());
      }
    } else {
      // Entry is dirty, verify that:
      // - no files with "<resource_id>.*" exist in tmp dir
      // - only 1 "<resource_id>.local" exists in persistent dir
      // - only 1 <resource_id> exists in outgoing dir
      // - if entry is pinned, only 1 <resource_id> exists in pinned dir.

      // Change expected_existing_path of CACHE_TYPE_PERSISTENT (index 1).
      paths_to_verify[1].expected_existing_path =
          GetCacheFilePath(resource_id,
                           std::string(),
                           GDataCache::CACHE_TYPE_PERSISTENT,
                           GDataCache::CACHED_FILE_LOCALLY_MODIFIED);

      // Change expected_existing_path of CACHE_TYPE_OUTGOING (index 3).
      paths_to_verify[3].expected_existing_path =
          GetCacheFilePath(resource_id,
                           std::string(),
                           GDataCache::CACHE_TYPE_OUTGOING,
                           GDataCache::CACHED_FILE_FROM_SERVER);

      if (cache_entry->IsPinned()) {
         // Change expected_existing_path of CACHE_TYPE_PINNED (index 2).
         paths_to_verify[2].expected_existing_path =
             GetCacheFilePath(resource_id,
                              std::string(),
                              GDataCache::CACHE_TYPE_PINNED,
                              GDataCache::CACHED_FILE_FROM_SERVER);
      }

      for (size_t i = 0; i < paths_to_verify.size(); ++i) {
        const struct PathToVerify& verify = paths_to_verify[i];
        file_util::FileEnumerator enumerator(
            verify.path_to_scan.DirName(), false /* not recursive*/,
            static_cast<file_util::FileEnumerator::FileType>(
                file_util::FileEnumerator::FILES |
                file_util::FileEnumerator::SHOW_SYM_LINKS),
            verify.path_to_scan.BaseName().value());
        size_t num_files_found = 0;
        for (FilePath current = enumerator.Next(); !current.empty();
             current = enumerator.Next()) {
          ++num_files_found;
          EXPECT_EQ(verify.expected_existing_path, current);
        }
        if (verify.expected_existing_path.empty())
          EXPECT_EQ(0U, num_files_found);
        else
          EXPECT_EQ(1U, num_files_found);
      }
    }
  }

  void TestPin(
      const std::string& resource_id,
      const std::string& md5,
      base::PlatformFileError expected_error,
      int expected_cache_state,
      GDataCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    cache_->PinOnUIThread(
        resource_id, md5,
        base::Bind(&GDataCacheTest::VerifyCacheFileState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void TestUnpin(
      const std::string& resource_id,
      const std::string& md5,
      base::PlatformFileError expected_error,
      int expected_cache_state,
      GDataCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    cache_->UnpinOnUIThread(
        resource_id, md5,
        base::Bind(&GDataCacheTest::VerifyCacheFileState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void TestGetCacheState(const std::string& resource_id,
                         const std::string& md5,
                         bool expected_success,
                         int expected_cache_state,
                         GDataFile* expected_file) {
    expected_success_ = expected_success;
    expected_cache_state_ = expected_cache_state;

    cache_->GetCacheEntryOnUIThread(resource_id, md5,
        base::Bind(&GDataCacheTest::VerifyGetCacheState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void VerifyGetCacheState(bool success,
                           const GDataCache::CacheEntry& cache_entry) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_success_, success);

    if (success) {
      EXPECT_EQ(expected_cache_state_, cache_entry.cache_state);
    }
  }

  void TestMarkDirty(
      const std::string& resource_id,
      const std::string& md5,
      base::PlatformFileError expected_error,
      int expected_cache_state,
      GDataCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = false;

    cache_->MarkDirtyOnUIThread(
        resource_id, md5,
        base::Bind(&GDataCacheTest::VerifyMarkDirty,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void VerifyMarkDirty(base::PlatformFileError error,
                       const std::string& resource_id,
                       const std::string& md5,
                       const FilePath& cache_file_path) {
    VerifyCacheFileState(error, resource_id, md5);

    // Verify filename of |cache_file_path|.
    if (error == base::PLATFORM_FILE_OK) {
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
      base::PlatformFileError expected_error,
      int expected_cache_state,
      GDataCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = true;

    cache_->CommitDirtyOnUIThread(
        resource_id, md5,
        base::Bind(&GDataCacheTest::VerifyCacheFileState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void TestClearDirty(
      const std::string& resource_id,
      const std::string& md5,
      base::PlatformFileError expected_error,
      int expected_cache_state,
      GDataCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = false;

    cache_->ClearDirtyOnUIThread(resource_id, md5,
        base::Bind(&GDataCacheTest::VerifyCacheFileState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void TestSetMountedState(
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& file_path,
      bool to_mount,
      base::PlatformFileError expected_error,
      int expected_cache_state,
      GDataCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = false;

    cache_->SetMountedStateOnUIThread(file_path, to_mount,
        base::Bind(&GDataCacheTest::VerifySetMountedState,
                   base::Unretained(this), resource_id, md5, to_mount));

    test_util::RunBlockingPoolTask();
  }

  void VerifySetMountedState(const std::string& resource_id,
                             const std::string& md5,
                             bool to_mount,
                             base::PlatformFileError error,
                             const FilePath& file_path) {
    ++num_callback_invocations_;
    EXPECT_TRUE(file_util::PathExists(file_path));
    EXPECT_TRUE(file_path == cache_->GetCacheFilePath(
        resource_id,
        md5,
        expected_sub_dir_type_,
        to_mount ?
            GDataCache::CACHED_FILE_MOUNTED :
            GDataCache::CACHED_FILE_FROM_SERVER));
  }

  void VerifyCacheFileState(base::PlatformFileError error,
                            const std::string& resource_id,
                            const std::string& md5) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

    // Verify cache map.
    scoped_ptr<GDataCache::CacheEntry> cache_entry =
        GetCacheEntryFromOriginThread(resource_id, md5);
    if (GDataCache::IsCachePresent(expected_cache_state_) ||
        GDataCache::IsCachePinned(expected_cache_state_)) {
      ASSERT_TRUE(cache_entry.get());
      EXPECT_EQ(expected_cache_state_, cache_entry->cache_state);
      EXPECT_EQ(expected_sub_dir_type_, cache_entry->sub_dir_type);
    } else {
      EXPECT_FALSE(cache_entry.get());
    }

    // Verify actual cache file.
    FilePath dest_path = cache_->GetCacheFilePath(
        resource_id,
        md5,
        GDataCache::IsCachePinned(expected_cache_state_) ||
            GDataCache::IsCacheDirty(expected_cache_state_) ?
                GDataCache::CACHE_TYPE_PERSISTENT :
                GDataCache::CACHE_TYPE_TMP,
        GDataCache::IsCacheDirty(expected_cache_state_) ?
            GDataCache::CACHED_FILE_LOCALLY_MODIFIED :
            GDataCache::CACHED_FILE_FROM_SERVER);
    bool exists = file_util::PathExists(dest_path);
    if (GDataCache::IsCachePresent(expected_cache_state_))
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
    if (GDataCache::IsCachePinned(expected_cache_state_)) {
      EXPECT_TRUE(exists);
      FilePath target_path;
      EXPECT_TRUE(file_util::ReadSymbolicLink(symlink_path, &target_path));
      if (GDataCache::IsCachePresent(expected_cache_state_))
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
        GDataCache::IsCacheDirty(expected_cache_state_)) {
      EXPECT_TRUE(exists);
      FilePath target_path;
      EXPECT_TRUE(file_util::ReadSymbolicLink(symlink_path, &target_path));
      EXPECT_TRUE(target_path.value() != kSymLinkToDevNull);
      if (GDataCache::IsCachePresent(expected_cache_state_))
        EXPECT_EQ(dest_path, target_path);
    } else {
      EXPECT_FALSE(exists);
    }
  }

  FilePath GetCacheFilePath(const std::string& resource_id,
                            const std::string& md5,
                            GDataCache::CacheSubDirectoryType sub_dir_type,
                            GDataCache::CachedFileOrigin file_origin) {
    return cache_->GetCacheFilePath(resource_id, md5, sub_dir_type,
                                    file_origin);
  }

  // Helper function to call GetCacheEntry from origin thread.
  scoped_ptr<GDataCache::CacheEntry> GetCacheEntryFromOriginThread(
      const std::string& resource_id,
      const std::string& md5) {
    scoped_ptr<GDataCache::CacheEntry> cache_entry;
    content::BrowserThread::GetBlockingPool()
        ->GetSequencedTaskRunner(sequence_token_)->PostTask(
            FROM_HERE,
            base::Bind(
                &GDataCacheTest::GetCacheEntryFromOriginThreadInternal,
                base::Unretained(this),
                resource_id,
                md5,
                &cache_entry));
    test_util::RunBlockingPoolTask();
    return cache_entry.Pass();
  }

  // Used to implement GetCacheEntry.
  void GetCacheEntryFromOriginThreadInternal(
      const std::string& resource_id,
      const std::string& md5,
      scoped_ptr<GDataCache::CacheEntry>* cache_entry) {
    cache_entry->reset(cache_->GetCacheEntry(resource_id, md5).release());
  }

  // Returns true if the cache entry exists for the given resource ID and MD5.
  bool CacheEntryExists(const std::string& resource_id,
                        const std::string& md5) {
    return GetCacheEntryFromOriginThread(resource_id, md5).get();
  }

  void TestGetCacheFilePath(const std::string& resource_id,
                            const std::string& md5,
                            const std::string& expected_filename) {
    FilePath actual_path = cache_->GetCacheFilePath(
        resource_id,
        md5,
        GDataCache::CACHE_TYPE_TMP,
        GDataCache::CACHED_FILE_FROM_SERVER);
    FilePath expected_path =
        file_system_->cache_->GetCacheDirectoryPath(GDataCache::CACHE_TYPE_TMP);
    expected_path = expected_path.Append(expected_filename);
    EXPECT_EQ(expected_path, actual_path);

    FilePath base_name = actual_path.BaseName();

    // FilePath::Extension returns ".", so strip it.
    std::string unescaped_md5 = util::UnescapeCacheFileName(
        base_name.Extension().substr(1));
    EXPECT_EQ(md5, unescaped_md5);
    std::string unescaped_resource_id = util::UnescapeCacheFileName(
        base_name.RemoveExtension().value());
    EXPECT_EQ(resource_id, unescaped_resource_id);
  }

  static Value* LoadJSONFile(const std::string& filename) {
    FilePath path = GetTestFilePath(filename);

    std::string error;
    JSONFileValueSerializer serializer(path);
    Value* value = serializer.Deserialize(NULL, &error);
    EXPECT_TRUE(value) <<
        "Parse error " << path.value() << ": " << error;
    return value;
  }

  static FilePath GetTestFilePath(const FilePath::StringType& filename) {
    FilePath path;
    std::string error;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("chromeos")
        .AppendASCII("gdata")
        .AppendASCII(filename.c_str());
    EXPECT_TRUE(file_util::PathExists(path)) <<
        "Couldn't find " << path.value();
    return path;
  }

  MessageLoopForUI message_loop_;
  // The order of the test threads is important, do not change the order.
  // See also content/browser/browser_thread_imple.cc.
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  const base::SequencedWorkerPool::SequenceToken sequence_token_;
  scoped_ptr<TestingProfile> profile_;
  GDataCache* cache_;
  scoped_ptr<StrictMock<MockGDataUploader> > mock_uploader_;
  GDataFileSystem* file_system_;
  MockDocumentsService* mock_doc_service_;
  scoped_ptr<StrictMock<MockDriveWebAppsRegistry> > mock_webapps_registry_;
  MockFreeDiskSpaceGetter* mock_free_disk_space_checker_;
  scoped_ptr<StrictMock<MockGDataSyncClient> > mock_sync_client_;

  int num_callback_invocations_;
  base::PlatformFileError expected_error_;
  int expected_cache_state_;
  GDataCache::CacheSubDirectoryType expected_sub_dir_type_;
  bool expected_success_;
  bool expect_outgoing_symlink_;
  std::string expected_file_extension_;
  int root_feed_changestamp_;
};

TEST_F(GDataCacheTest, InitializeCache) {
  PrepareForInitCacheTest();
  TestInitializeCache();
}

TEST_F(GDataCacheTest, CacheStateBitmasks) {
  GDataCache::CacheEntry cache_entry("md5_cache_state_bitmasks",
                                     GDataCache::CACHE_TYPE_TMP,
                                     GDataCache::CACHE_STATE_NONE);
  EXPECT_FALSE(cache_entry.IsPresent());
  EXPECT_FALSE(cache_entry.IsPinned());
  EXPECT_FALSE(cache_entry.IsDirty());

  cache_entry.cache_state = GDataCache::CACHE_STATE_PRESENT;
  EXPECT_TRUE(cache_entry.IsPresent());
  EXPECT_FALSE(cache_entry.IsPinned());
  EXPECT_FALSE(cache_entry.IsDirty());

  cache_entry.cache_state = GDataCache::CACHE_STATE_PINNED;
  EXPECT_FALSE(cache_entry.IsPresent());
  EXPECT_TRUE(cache_entry.IsPinned());
  EXPECT_FALSE(cache_entry.IsDirty());

  cache_entry.cache_state = GDataCache::CACHE_STATE_PRESENT |
                            GDataCache::CACHE_STATE_PINNED;
  EXPECT_TRUE(cache_entry.IsPresent());
  EXPECT_TRUE(cache_entry.IsPinned());
  EXPECT_FALSE(cache_entry.IsDirty());

  cache_entry.cache_state = GDataCache::CACHE_STATE_PRESENT |
                            GDataCache::CACHE_STATE_DIRTY;
  EXPECT_TRUE(cache_entry.IsPresent());
  EXPECT_FALSE(cache_entry.IsPinned());
  EXPECT_TRUE(cache_entry.IsDirty());

  cache_entry.cache_state = GDataCache::CACHE_STATE_PRESENT |
                            GDataCache::CACHE_STATE_PINNED |
                            GDataCache::CACHE_STATE_DIRTY;
  EXPECT_TRUE(cache_entry.IsPresent());
  EXPECT_TRUE(cache_entry.IsPinned());
  EXPECT_TRUE(cache_entry.IsDirty());

  int cache_state = GDataCache::CACHE_STATE_NONE;
  EXPECT_EQ(GDataCache::CACHE_STATE_PRESENT,
            GDataCache::SetCachePresent(cache_state));
  EXPECT_EQ(GDataCache::CACHE_STATE_PINNED,
            GDataCache::SetCachePinned(cache_state));

  cache_state = GDataCache::CACHE_STATE_PRESENT;
  EXPECT_EQ(GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_PINNED,
            GDataCache::SetCachePinned(cache_state));
  EXPECT_EQ(GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_DIRTY,
            GDataCache::SetCacheDirty(cache_state));
  cache_state |= GDataCache::CACHE_STATE_PINNED;
  EXPECT_EQ(GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_PINNED |
            GDataCache::CACHE_STATE_DIRTY,
            GDataCache::SetCacheDirty(cache_state));

  cache_state = GDataCache::CACHE_STATE_PINNED;
  EXPECT_EQ(GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_PINNED,
            GDataCache::SetCachePresent(cache_state));

  cache_state = GDataCache::CACHE_STATE_PRESENT |
                GDataCache::CACHE_STATE_PINNED |
                GDataCache::CACHE_STATE_DIRTY;
  EXPECT_EQ(cache_state & ~GDataCache::CACHE_STATE_PRESENT,
            GDataCache::ClearCachePresent(cache_state));
  EXPECT_EQ(cache_state & ~GDataCache::CACHE_STATE_PINNED,
            GDataCache::ClearCachePinned(cache_state));
  EXPECT_EQ(cache_state & ~GDataCache::CACHE_STATE_DIRTY,
            GDataCache::ClearCacheDirty(cache_state));
}

TEST_F(GDataCacheTest, GetCacheFilePath) {
  // Use alphanumeric characters for resource id.
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  TestGetCacheFilePath(resource_id, md5,
                       resource_id + FilePath::kExtensionSeparator + md5);
  EXPECT_EQ(0, num_callback_invocations_);

  // Use non-alphanumeric characters for resource id, including '.' which is an
  // extension separator, to test that the characters are escaped and unescaped
  // correctly, and '.' doesn't mess up the filename format and operations.
  resource_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";
  std::string escaped_resource_id = util::EscapeCacheFileName(resource_id);
  std::string escaped_md5 = util::EscapeCacheFileName(md5);
  num_callback_invocations_ = 0;
  TestGetCacheFilePath(resource_id, md5,
                       escaped_resource_id + FilePath::kExtensionSeparator +
                       escaped_md5);
  EXPECT_EQ(0, num_callback_invocations_);
}

TEST_F(GDataCacheTest, StoreToCacheSimple) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Store an existing file.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store a non-existent file to the same |resource_id| and |md5|.
  num_callback_invocations_ = 0;
  TestStoreToCache(resource_id, md5, FilePath("./non_existent.json"),
                   base::PLATFORM_FILE_ERROR_NOT_FOUND,
                   GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store a different existing file to the same |resource_id| but different
  // |md5|.
  md5 = "new_md5";
  num_callback_invocations_ = 0;
  TestStoreToCache(resource_id, md5, GetTestFilePath("subdir_feed.json"),
                   base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Verify that there's only one file with name <resource_id>, i.e. previously
  // cached file with the different md5 should be deleted.
  FilePath path = GetCacheFilePath(resource_id, "*",
      (GDataCache::IsCachePinned(expected_cache_state_)) ?
          GDataCache::CACHE_TYPE_PERSISTENT :
          GDataCache::CACHE_TYPE_TMP,
      GDataCache::CACHED_FILE_FROM_SERVER);
  file_util::FileEnumerator enumerator(path.DirName(), false,
                                       file_util::FileEnumerator::FILES,
                                       path.BaseName().value());
  size_t num_files_found = 0;
  for (FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    ++num_files_found;
    EXPECT_EQ(util::EscapeCacheFileName(resource_id) +
              FilePath::kExtensionSeparator +
              util::EscapeCacheFileName(md5),
              current.BaseName().value());
  }
  EXPECT_EQ(1U, num_files_found);
}

TEST_F(GDataCacheTest, GetFromCacheSimple) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  // First store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);

  // Then try to get the existing file from cache.
  num_callback_invocations_ = 0;
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, base::PLATFORM_FILE_OK, md5);
  EXPECT_EQ(1, num_callback_invocations_);

  // Get file from cache with same resource id as existing file but different
  // md5.
  num_callback_invocations_ = 0;
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, "9999", base::PLATFORM_FILE_ERROR_NOT_FOUND, md5);
  EXPECT_EQ(1, num_callback_invocations_);

  // Get file from cache with different resource id from existing file but same
  // md5.
  num_callback_invocations_ = 0;
  resource_id = "document:1a2b";
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND, md5);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataCacheTest, RemoveFromCacheSimple) {
  // Use alphanumeric characters for resource id.
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  // First store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);

  // Then try to remove existing file from cache.
  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // Repeat using non-alphanumeric characters for resource id, including '.'
  // which is an extension separator.
  resource_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);

  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataCacheTest, PinAndUnpin) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnCachePinned(resource_id, md5)).Times(2);
  EXPECT_CALL(*mock_sync_client_, OnCacheUnpinned(resource_id, md5)).Times(1);

  // First store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);

  // Pin the existing file in cache.
  num_callback_invocations_ = 0;
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_PINNED,
          GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Unpin the existing file in cache.
  num_callback_invocations_ = 0;
  TestUnpin(resource_id, md5, base::PLATFORM_FILE_OK,
            GDataCache::CACHE_STATE_PRESENT,
            GDataCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Pin back the same existing file in cache.
  num_callback_invocations_ = 0;
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_PINNED,
          GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Pin a non-existent file in cache.
  resource_id = "document:1a2b";
  EXPECT_CALL(*mock_sync_client_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_sync_client_, OnCacheUnpinned(resource_id, md5)).Times(1);

  num_callback_invocations_ = 0;
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataCache::CACHE_STATE_PINNED,
          GDataCache::CACHE_TYPE_PINNED);
  EXPECT_EQ(1, num_callback_invocations_);

  // Unpin the previously pinned non-existent file in cache.
  num_callback_invocations_ = 0;
  TestUnpin(resource_id, md5, base::PLATFORM_FILE_OK,
            GDataCache::CACHE_STATE_NONE,
            GDataCache::CACHE_TYPE_PINNED);
  EXPECT_EQ(1, num_callback_invocations_);

  // Unpin a file that doesn't exist in cache and is not pinned, i.e. cache
  // has zero knowledge of the file.
  resource_id = "not-in-cache:1a2b";
  // Because unpinning will fail, OnCacheUnpinned() won't be run.
  EXPECT_CALL(*mock_sync_client_, OnCacheUnpinned(resource_id, md5)).Times(0);

  num_callback_invocations_ = 0;
  TestUnpin(resource_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND,
            GDataCache::CACHE_STATE_NONE,
            GDataCache::CACHE_TYPE_PINNED /* non-applicable */);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataCacheTest, StoreToCachePinned) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnCachePinned(resource_id, md5)).Times(1);

  // Pin a non-existent file.
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataCache::CACHE_STATE_PINNED,
          GDataCache::CACHE_TYPE_PINNED);

  // Store an existing file to a previously pinned file.
  num_callback_invocations_ = 0;
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK,
                   GDataCache::CACHE_STATE_PRESENT |
                   GDataCache::CACHE_STATE_PINNED,
                   GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store a non-existent file to a previously pinned and stored file.
  num_callback_invocations_ = 0;
  TestStoreToCache(resource_id, md5, FilePath("./non_existent.json"),
                   base::PLATFORM_FILE_ERROR_NOT_FOUND,
                   GDataCache::CACHE_STATE_PRESENT |
                   GDataCache::CACHE_STATE_PINNED,
                   GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataCacheTest, GetFromCachePinned) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnCachePinned(resource_id, md5)).Times(1);

  // Pin a non-existent file.
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataCache::CACHE_STATE_PINNED,
          GDataCache::CACHE_TYPE_PINNED);

  // Get the non-existent pinned file from cache.
  num_callback_invocations_ = 0;
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND, md5);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store an existing file to the previously pinned non-existent file.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK,
                   GDataCache::CACHE_STATE_PRESENT |
                   GDataCache::CACHE_STATE_PINNED,
                   GDataCache::CACHE_TYPE_PERSISTENT);

  // Get the previously pinned and stored file from cache.
  num_callback_invocations_ = 0;
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, base::PLATFORM_FILE_OK, md5);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataCacheTest, RemoveFromCachePinned) {
  // Use alphanumeric characters for resource_id.
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnCachePinned(resource_id, md5)).Times(1);

  // Store a file to cache, and pin it.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_PINNED,
          GDataCache::CACHE_TYPE_PERSISTENT);

  // Remove |resource_id| from cache.
  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // Repeat using non-alphanumeric characters for resource id, including '.'
  // which is an extension separator.
  resource_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";
  EXPECT_CALL(*mock_sync_client_, OnCachePinned(resource_id, md5)).Times(1);

  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_PINNED,
          GDataCache::CACHE_TYPE_PERSISTENT);

  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataCacheTest, DirtyCacheSimple) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnCacheCommitted(resource_id)).Times(1);

  // First store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);

  // Mark the file dirty.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_DIRTY,
                GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Commit the file dirty.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataCache::CACHE_STATE_PRESENT |
                 GDataCache::CACHE_STATE_DIRTY,
                GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Clear dirty state of the file.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataCache::CACHE_STATE_PRESENT,
                 GDataCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataCacheTest, DirtyCachePinned) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_sync_client_, OnCacheCommitted(resource_id)).Times(1);

  // First store a file to cache and pin it.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_PINNED,
          GDataCache::CACHE_TYPE_PERSISTENT);

  // Mark the file dirty.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataCache::CACHE_STATE_PRESENT |
                GDataCache::CACHE_STATE_DIRTY |
                GDataCache::CACHE_STATE_PINNED,
                GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Commit the file dirty.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataCache::CACHE_STATE_PRESENT |
                 GDataCache::CACHE_STATE_DIRTY |
                 GDataCache::CACHE_STATE_PINNED,
                 GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Clear dirty state of the file.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataCache::CACHE_STATE_PRESENT |
                 GDataCache::CACHE_STATE_PINNED,
                 GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);
}

// Test is disabled because it is flaky (http://crbug.com/134146)
TEST_F(GDataCacheTest, PinAndUnpinDirtyCache) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_sync_client_, OnCacheUnpinned(resource_id, md5)).Times(1);

  // First store a file to cache and mark it as dirty.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_DIRTY,
                GDataCache::CACHE_TYPE_PERSISTENT);

  // Verifies dirty file exists.
  FilePath dirty_path = GetCacheFilePath(
      resource_id,
      md5,
      GDataCache::CACHE_TYPE_PERSISTENT,
      GDataCache::CACHED_FILE_LOCALLY_MODIFIED);
  EXPECT_TRUE(file_util::PathExists(dirty_path));

  // Pin the dirty file.
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_DIRTY |
          GDataCache::CACHE_STATE_PINNED,
          GDataCache::CACHE_TYPE_PERSISTENT);

  // Verify dirty file still exist at the same pathname.
  EXPECT_TRUE(file_util::PathExists(dirty_path));

  // Unpin the dirty file.
  TestUnpin(resource_id, md5, base::PLATFORM_FILE_OK,
            GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_DIRTY,
            GDataCache::CACHE_TYPE_PERSISTENT);

  // Verify dirty file still exist at the same pathname.
  EXPECT_TRUE(file_util::PathExists(dirty_path));
}

TEST_F(GDataCacheTest, DirtyCacheRepetitive) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnCacheCommitted(resource_id)).Times(3);

  // First store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);

  // Mark the file dirty.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_DIRTY,
                GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Again, mark the file dirty.  Nothing should change.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_DIRTY,
                GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Commit the file dirty.  Outgoing symlink should be created.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataCache::CACHE_STATE_PRESENT |
                 GDataCache::CACHE_STATE_DIRTY,
                GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Again, commit the file dirty.  Nothing should change.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataCache::CACHE_STATE_PRESENT |
                 GDataCache::CACHE_STATE_DIRTY,
                GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Mark the file dirty agian after it's being committed.  Outgoing symlink
  // should be deleted.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataCache::CACHE_STATE_PRESENT |
                GDataCache::CACHE_STATE_DIRTY,
                GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Commit the file dirty.  Outgoing symlink should be created again.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataCache::CACHE_STATE_PRESENT |
                 GDataCache::CACHE_STATE_DIRTY,
                GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Clear dirty state of the file.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataCache::CACHE_STATE_PRESENT,
                 GDataCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Again, clear dirty state of the file, which is no longer dirty.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, base::PLATFORM_FILE_ERROR_INVALID_OPERATION,
                 GDataCache::CACHE_STATE_PRESENT,
                 GDataCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataCacheTest, DirtyCacheInvalid) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Mark a non-existent file dirty.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND,
                GDataCache::CACHE_STATE_NONE,
                GDataCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Commit a non-existent file dirty.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND,
                  GDataCache::CACHE_STATE_NONE,
                  GDataCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Clear dirty state of a non-existent file.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND,
                 GDataCache::CACHE_STATE_NONE,
                 GDataCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);

  // Commit a non-dirty existing file dirty.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_ERROR_INVALID_OPERATION,
                 GDataCache::CACHE_STATE_PRESENT,
                 GDataCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Clear dirty state of a non-dirty existing file.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, base::PLATFORM_FILE_ERROR_INVALID_OPERATION,
                 GDataCache::CACHE_STATE_PRESENT,
                 GDataCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Mark an existing file dirty, then store a new file to the same resource id
  // but different md5, which should fail.
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_DIRTY,
                GDataCache::CACHE_TYPE_PERSISTENT);
  num_callback_invocations_ = 0;
  md5 = "new_md5";
  TestStoreToCache(resource_id, md5, GetTestFilePath("subdir_feed.json"),
                   base::PLATFORM_FILE_ERROR_IN_USE,
                   GDataCache::CACHE_STATE_PRESENT |
                   GDataCache::CACHE_STATE_DIRTY,
                   GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataCacheTest, RemoveFromDirtyCache) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_sync_client_, OnCacheCommitted(resource_id)).Times(1);

  // Store a file to cache, pin it, mark it dirty and commit it.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_PINNED,
          GDataCache::CACHE_TYPE_PERSISTENT);
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataCache::CACHE_STATE_PRESENT |
                GDataCache::CACHE_STATE_PINNED |
                GDataCache::CACHE_STATE_DIRTY,
                GDataCache::CACHE_TYPE_PERSISTENT);
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataCache::CACHE_STATE_PRESENT |
                 GDataCache::CACHE_STATE_PINNED |
                 GDataCache::CACHE_STATE_DIRTY,
                 GDataCache::CACHE_TYPE_PERSISTENT);

  // Try to remove the file.  Since file is dirty, it and the corresponding
  // pinned and outgoing symlinks should not be removed.
  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataCacheTest, GetCacheState) {
  // Populate gdata file system.
  LoadRootFeedDocument("root_feed.json");

  {  // Test cache state of an existing normal file.
    // Retrieve resource id and md5 of a file from file system.
    FilePath file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
    GDataEntry* entry = FindEntry(file_path);
    ASSERT_TRUE(entry != NULL);
    GDataFile* file = entry->AsGDataFile();
    ASSERT_TRUE(file != NULL);
    std::string resource_id = file->resource_id();
    std::string md5 = file->file_md5();

    // Store a file corresponding to |resource_id| and |md5| to cache.
    TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                     base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                     GDataCache::CACHE_TYPE_TMP);

    // Get its cache state.
    num_callback_invocations_ = 0;
    TestGetCacheState(resource_id, md5, true,
                      GDataCache::CACHE_STATE_PRESENT, file);
    EXPECT_EQ(1, num_callback_invocations_);
  }

  {  // Test cache state of an existing pinned file.
    // Retrieve resource id and md5 of a file from file system.
    FilePath file_path(
        FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
    GDataEntry* entry = FindEntry(file_path);
    ASSERT_TRUE(entry != NULL);
    GDataFile* file = entry->AsGDataFile();
    ASSERT_TRUE(file != NULL);
    std::string resource_id = file->resource_id();
    std::string md5 = file->file_md5();

    EXPECT_CALL(*mock_sync_client_, OnCachePinned(resource_id, md5)).Times(1);

    // Store a file corresponding to |resource_id| and |md5| to cache, and pin
    // it.
    int expected_cache_state = GDataCache::CACHE_STATE_PRESENT |
                               GDataCache::CACHE_STATE_PINNED;
    TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                     base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                     GDataCache::CACHE_TYPE_TMP);
    TestPin(resource_id, md5, base::PLATFORM_FILE_OK, expected_cache_state,
            GDataCache::CACHE_TYPE_PERSISTENT);

    // Get its cache state.
    num_callback_invocations_ = 0;
    TestGetCacheState(resource_id, md5, true,
                      expected_cache_state, file);
    EXPECT_EQ(1, num_callback_invocations_);
  }

  {  // Test cache state of a non-existent file.
    num_callback_invocations_ = 0;
    TestGetCacheState("pdf:12345", "abcd", false,
                      0, NULL);
    EXPECT_EQ(1, num_callback_invocations_);
  }
}

TEST_F(GDataCacheTest, MountUnmount) {
  FilePath file_path;
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // First store a file to cache in the tmp subdir.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataCache::CACHE_STATE_PRESENT,
                   GDataCache::CACHE_TYPE_TMP);

  // Mark the file mounted.
  num_callback_invocations_ = 0;
  file_path = cache_->GetCacheFilePath(resource_id, md5,
                                       GDataCache::CACHE_TYPE_TMP,
                                       GDataCache::CACHED_FILE_FROM_SERVER);
  TestSetMountedState(resource_id, md5, file_path, true,
                      base::PLATFORM_FILE_OK,
                      GDataCache::CACHE_STATE_PRESENT |
                      GDataCache::CACHE_STATE_MOUNTED,
                      GDataCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);
  EXPECT_TRUE(CacheEntryExists(resource_id, md5));

  // Clear mounted state of the file.
  num_callback_invocations_ = 0;
  file_path = cache_->GetCacheFilePath(resource_id,
                                       md5,
                                       GDataCache::CACHE_TYPE_PERSISTENT,
                                       GDataCache::CACHED_FILE_MOUNTED);
  TestSetMountedState(resource_id, md5, file_path, false,
                      base::PLATFORM_FILE_OK,
                      GDataCache::CACHE_STATE_PRESENT,
                      GDataCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);
  EXPECT_TRUE(CacheEntryExists(resource_id, md5));

  // Try to remove the file.
  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);
}

}   // namespace gdata
