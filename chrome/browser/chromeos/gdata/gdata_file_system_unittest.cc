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
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/gdata/mock_directory_change_observer.h"
#include "chrome/browser/chromeos/gdata/mock_gdata_documents_service.h"
#include "chrome/browser/chromeos/gdata/mock_gdata_sync_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "content/public/browser/browser_thread.h"
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
using ::testing::_;

using base::Value;
using base::DictionaryValue;
using base::ListValue;
using content::BrowserThread;

namespace {

const char kSymLinkToDevNull[] = "/dev/null";

struct InitialCacheResource {
  const char* source_file;              // Source file to be used for cache.
  const char* resource_id;              // Resource id of cache file.
  const char* md5;                      // MD5 of cache file.
  int cache_state;                      // Cache state of cache file.
  const char* expected_file_extension;  // Expected extension of cached file.
  // Expected CacheSubDirectoryType of cached file.
  gdata::GDataRootDirectory::CacheSubDirectoryType expected_sub_dir_type;
} const initial_cache_resources[] = {
  // Cache resource in tmp dir, i.e. not pinned or dirty.
  { "root_feed.json", "tmp:resource_id", "md5_tmp_alphanumeric",
    gdata::GDataFile::CACHE_STATE_PRESENT,
    "md5_tmp_alphanumeric", gdata::GDataRootDirectory::CACHE_TYPE_TMP },
  // Cache resource in tmp dir, i.e. not pinned or dirty, with resource_id
  // containing non-alphanumeric characters, to test resource_id is escaped and
  // unescaped correctly.
  { "subdir_feed.json", "tmp:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?",
    "md5_tmp_non_alphanumeric",
    gdata::GDataFile::CACHE_STATE_PRESENT,
    "md5_tmp_non_alphanumeric", gdata::GDataRootDirectory::CACHE_TYPE_TMP },
  // Cache resource that is pinned, to test a pinned file is in persistent dir
  // with a symlink in pinned dir referencing it.
  { "directory_entry_atom.json", "pinned:existing", "md5_pinned_existing",
    gdata::GDataFile::CACHE_STATE_PRESENT |
    gdata::GDataFile::CACHE_STATE_PINNED,
    "md5_pinned_existing", gdata::GDataRootDirectory::CACHE_TYPE_PERSISTENT },
  // Cache resource with a non-existent source file that is pinned, to test that
  // a pinned file can reference a non-existent file.
  { "", "pinned:non-existent", "md5_pinned_non_existent",
    gdata::GDataFile::CACHE_STATE_PINNED,
    "md5_pinned_non_existent", gdata::GDataRootDirectory::CACHE_TYPE_PINNED },
  // Cache resource that is dirty, to test a dirty file is in persistent dir
  // with a symlink in outgoing dir referencing it.
  { "account_metadata.json", "dirty:existing", "md5_dirty_existing",
    gdata::GDataFile::CACHE_STATE_PRESENT |
    gdata::GDataFile::CACHE_STATE_DIRTY,
     "local", gdata::GDataRootDirectory::CACHE_TYPE_PERSISTENT },
  // Cache resource that is pinned and dirty, to test a dirty pinned file is in
  // persistent dir with symlink in pinned and outgoing dirs referencing it.
  { "basic_feed.json", "dirty_and_pinned:existing",
    "md5_dirty_and_pinned_existing",
    gdata::GDataFile::CACHE_STATE_PRESENT |
    gdata::GDataFile::CACHE_STATE_PINNED |
    gdata::GDataFile::CACHE_STATE_DIRTY,
     "local", gdata::GDataRootDirectory::CACHE_TYPE_PERSISTENT },
};

const int64 kLotsOfSpace = gdata::kMinFreeSpace * 10;

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

// Callback to GDataFileSystem::SearchAsync used in ContentSearch test.
// Verifies that returned proto buffer contains entries specified in search
// feed, and that treing file names are formatted like
// "<resource_id>.<file_name>".
void ContentSearchCallback(MessageLoop* message_loop,
                           base::PlatformFileError error,
                           scoped_ptr<gdata::GDataDirectoryProto> dir_proto) {
  ASSERT_EQ(base::PLATFORM_FILE_OK, error);
  ASSERT_TRUE(dir_proto.get());

  // Search feed contains 2 entries. One file (SubDirectory File 1.txt) and one
  // directory (Directory 1). Entries generated from the fedd should have names
  // in format resource_id.actual_file_name.
  ASSERT_EQ(1, dir_proto->child_files_size());
  EXPECT_EQ("file:2_file_resouce_id.SubDirectory File 1.txt",
            dir_proto->child_files(0).gdata_entry().file_name());

  ASSERT_EQ(1, dir_proto->child_directories_size());
  EXPECT_EQ("folder:1_folder_resource_id.Directory 1",
            dir_proto->child_directories(0).gdata_entry().file_name());

  message_loop->Quit();
}

}  // anonymous namespace

namespace gdata {

class MockFreeDiskSpaceGetter : public FreeDiskSpaceGetterInterface {
 public:
  virtual ~MockFreeDiskSpaceGetter() {}
  MOCK_CONST_METHOD0(AmountOfFreeDiskSpace, int64());
};

class GDataFileSystemTest : public testing::Test {
 protected:
  GDataFileSystemTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO),
        file_system_(NULL),
        mock_doc_service_(NULL),
        num_callback_invocations_(0),
        expected_error_(base::PLATFORM_FILE_OK),
        expected_cache_state_(0),
        expected_sub_dir_type_(GDataRootDirectory::CACHE_TYPE_META),
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
    mock_doc_service_ = new MockDocumentsService;

    EXPECT_CALL(*mock_doc_service_, Initialize(profile_.get())).Times(1);

    // Likewise, this will be owned by GDataFileSystem.
    mock_free_disk_space_checker_ = new MockFreeDiskSpaceGetter;
    SetFreeDiskSpaceGetterForTesting(mock_free_disk_space_checker_);

    ASSERT_FALSE(file_system_);
    file_system_ = new GDataFileSystem(profile_.get(),
                                       mock_doc_service_);
    file_system_->Initialize();

    mock_sync_client_.reset(new MockGDataSyncClient);
    file_system_->AddObserver(mock_sync_client_.get());

    mock_directory_observer_.reset(new MockDirectoryChangeObserver);
    file_system_->AddObserver(mock_directory_observer_.get());
  }

  virtual void TearDown() OVERRIDE {
    ASSERT_TRUE(file_system_);
    EXPECT_CALL(*mock_doc_service_, CancelAll()).Times(1);
    delete file_system_;
    file_system_ = NULL;
    SetFreeDiskSpaceGetterForTesting(NULL);

    // Run the remaining tasks on the main thread, so that reply tasks (2nd
    // callback of PostTaskAndReply) are run. Otherwise, there will be a leak
    // from PostTaskAndReply() as it deletes an internal object when the
    // reply task is run. Note that actual reply tasks (functions passed to
    // the 2nd callback of PostTaskAndReply) will be canceled, as these are
    // bound to weak pointers, which are invalidated in ShutdownOnUIThread().
    message_loop_.RunAllPending();

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
              base::PLATFORM_FILE_OK)
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
        root_feed_changestamp_++) == base::PLATFORM_FILE_OK;
  }

  bool RemoveEntry(const FilePath& file_path) {
    return file_system_->RemoveEntryFromFileSystem(file_path) ==
        base::PLATFORM_FILE_OK;
  }

  FilePath GetCachePathForFile(GDataFile* file) {
    return file_system_->GetCacheFilePath(
        file->resource_id(),
        file->file_md5(),
        GDataRootDirectory::CACHE_TYPE_TMP,
        GDataFileSystem::CACHED_FILE_FROM_SERVER);
  }

  GDataEntry* FindEntry(const FilePath& file_path) {
    GDataEntry* entry = NULL;
    file_system_->root_->FindEntryByPath(
        file_path, base::Bind(&ReadOnlyFindEntryCallback, &entry));
    return entry;
  }

  void FindAndTestFilePath(const FilePath& file_path) {
    GDataEntry* entry = FindEntry(file_path);
    ASSERT_TRUE(entry) << "Entry can't be found " << file_path.value();
    EXPECT_EQ(entry->GetFilePath(), file_path);
  }

  GDataEntry* FindEntryByResourceId(const std::string& resource_id) {
    GDataEntry* entry = NULL;
    file_system_->FindEntryByResourceIdSync(
        resource_id, base::Bind(&ReadOnlyFindEntryCallback, &entry));
    return entry;
  }

  // Gets the entry info for |file_path| and compares the contents against
  // |entry|. Returns true if the entry info matches |entry|.
  bool GetEntryInfoAndCompare(const FilePath& file_path,
                              GDataEntry* entry) {
    file_system_->GetEntryInfoByPathAsync(
        file_path,
        base::Bind(&CallbackHelper::GetEntryInfoCallback,
                   callback_helper_.get()));
    message_loop_.RunAllPending();

    if (entry == NULL) {
      // Entry info is expected not to be found.
      return (callback_helper_->last_error_ ==
              base::PLATFORM_FILE_ERROR_NOT_FOUND &&
              callback_helper_->entry_proto_ == NULL);
    }

    if (callback_helper_->last_error_ != base::PLATFORM_FILE_OK)
      return false;

    scoped_ptr<GDataEntryProto> entry_proto =
        callback_helper_->entry_proto_.Pass();
    return (entry->resource_id() == entry_proto->resource_id());
  }

  // Gets the file info for |file_path| and compares the contents against
  // |file|. Returns true if the file info matches |file|.
  bool GetFileInfoAndCompare(const FilePath& file_path,
                             GDataFile* file) {
    file_system_->GetFileInfoByPathAsync(
        file_path,
        base::Bind(&CallbackHelper::GetFileInfoCallback,
                   callback_helper_.get()));
    message_loop_.RunAllPending();

    if (file == NULL) {
      // File info is expected not to be found.
      return (callback_helper_->last_error_ ==
              base::PLATFORM_FILE_ERROR_NOT_FOUND &&
              callback_helper_->file_proto_ == NULL);
    }

    if (callback_helper_->last_error_ != base::PLATFORM_FILE_OK)
      return false;

    scoped_ptr<GDataFileProto> file_proto =
        callback_helper_->file_proto_.Pass();
    return (file->resource_id() == file_proto->gdata_entry().resource_id());
  }

  // Reads the directory at |file_path| and compares the contents against
  // |directory|. Returns true if the file info matches |directory|.
  bool ReadDirectoryAndCompare(const FilePath& file_path,
                               GDataDirectory* directory) {
    file_system_->ReadDirectoryByPathAsync(
        file_path,
        base::Bind(&CallbackHelper::ReadDirectoryCallback,
                   callback_helper_.get()));
    message_loop_.RunAllPending();

    if (directory == NULL) {
      // Directory is expected not to be found.
      return (callback_helper_->last_error_ ==
              base::PLATFORM_FILE_ERROR_NOT_FOUND &&
              callback_helper_->directory_proto_ == NULL);
    }

    if (callback_helper_->last_error_ != base::PLATFORM_FILE_OK)
      return false;

    scoped_ptr<GDataDirectoryProto> directory_proto =
        callback_helper_->directory_proto_.Pass();
    return (directory->resource_id() ==
            directory_proto->gdata_entry().resource_id());
  }

  FilePath GetCacheFilePath(
      const std::string& resource_id,
      const std::string& md5,
      GDataRootDirectory::CacheSubDirectoryType sub_dir_type,
      GDataFileSystem::CachedFileOrigin file_origin) {
    return file_system_->GetCacheFilePath(resource_id, md5, sub_dir_type,
                                          file_origin);
  }

  // Returns true if the cache entry exists for the given resource ID and MD5.
  bool CacheEntryExists(const std::string& resource_id,
                        const std::string& md5) {
    GDataRootDirectory::CacheEntry* entry =
        file_system_->root_->GetCacheEntry(resource_id, md5);
    return entry != NULL;
  }

  // Returns true if the cache file exists for the given resource ID and MD5.
  bool CacheFileExists(const std::string& resource_id,
                       const std::string& md5) {
    const FilePath file_path = file_system_->GetCacheFilePath(
        resource_id,
        md5,
        GDataRootDirectory::CACHE_TYPE_TMP,
        GDataFileSystem::CACHED_FILE_FROM_SERVER);
    return file_util::PathExists(file_path);
  }

  void TestGetCacheFilePath(const std::string& resource_id,
                            const std::string& md5,
                            const std::string& expected_filename) {
    FilePath actual_path = file_system_->GetCacheFilePath(
        resource_id,
        md5,
        GDataRootDirectory::CACHE_TYPE_TMP,
        GDataFileSystem::CACHED_FILE_FROM_SERVER);
    FilePath expected_path =
        file_system_->cache_paths_[GDataRootDirectory::CACHE_TYPE_TMP];
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

  void TestStoreToCache(
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& source_path,
      base::PlatformFileError expected_error,
      int expected_cache_state,
      GDataRootDirectory::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    file_system_->StoreToCache(resource_id, md5, source_path,
        GDataFileSystem::FILE_OPERATION_COPY,
        base::Bind(&GDataFileSystemTest::VerifyCacheFileState,
                   base::Unretained(this)));

    RunAllPendingForIO();
  }

  void TestGetFileFromCacheByResourceIdAndMd5(
      const std::string& resource_id,
      const std::string& md5,
      base::PlatformFileError expected_error,
      const std::string& expected_file_extension) {
    expected_error_ = expected_error;
    expected_file_extension_ = expected_file_extension;

    file_system_->GetFileFromCacheByResourceIdAndMd5(resource_id, md5,
        base::Bind(&GDataFileSystemTest::VerifyGetFromCache,
                   base::Unretained(this)));

    RunAllPendingForIO();
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

    file_system_->RemoveFromCache(resource_id,
        base::Bind(&GDataFileSystemTest::VerifyRemoveFromCache,
                   base::Unretained(this)));

    RunAllPendingForIO();
  }

  void VerifyRemoveFromCache(base::PlatformFileError error,
                             const std::string& resource_id,
                             const std::string& md5) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

    // Verify cache map.
    GDataRootDirectory::CacheEntry* entry =
        file_system_->root_->GetCacheEntry(resource_id, md5);
    if (entry)
      EXPECT_TRUE(entry->IsDirty());

    // If entry doesn't exist, verify that:
    // - no files with "<resource_id>.* exists in persistent and tmp dirs
    // - no "<resource_id>" symlink exists in pinned and outgoing dirs.
    std::vector<PathToVerify> paths_to_verify;
    paths_to_verify.push_back(  // Index 0: CACHE_TYPE_TMP.
        PathToVerify(file_system_->GetCacheFilePath(resource_id, "*",
                     GDataRootDirectory::CACHE_TYPE_TMP,
                     GDataFileSystem::CACHED_FILE_FROM_SERVER), FilePath()));
    paths_to_verify.push_back(  // Index 1: CACHE_TYPE_PERSISTENT.
        PathToVerify(file_system_->GetCacheFilePath(resource_id, "*",
                     GDataRootDirectory::CACHE_TYPE_PERSISTENT,
                     GDataFileSystem::CACHED_FILE_FROM_SERVER), FilePath()));
    paths_to_verify.push_back(  // Index 2: CACHE_TYPE_PINNED.
        PathToVerify(file_system_->GetCacheFilePath(resource_id, "",
                     GDataRootDirectory::CACHE_TYPE_PINNED,
                     GDataFileSystem::CACHED_FILE_FROM_SERVER), FilePath()));
    paths_to_verify.push_back(  // Index 3: CACHE_TYPE_OUTGOING.
        PathToVerify(file_system_->GetCacheFilePath(resource_id, "",
                     GDataRootDirectory::CACHE_TYPE_OUTGOING,
                     GDataFileSystem::CACHED_FILE_FROM_SERVER), FilePath()));
    if (!entry) {
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
                           GDataRootDirectory::CACHE_TYPE_PERSISTENT,
                           GDataFileSystem::CACHED_FILE_LOCALLY_MODIFIED);

      // Change expected_existing_path of CACHE_TYPE_OUTGOING (index 3).
      paths_to_verify[3].expected_existing_path =
          GetCacheFilePath(resource_id,
                           std::string(),
                           GDataRootDirectory::CACHE_TYPE_OUTGOING,
                           GDataFileSystem::CACHED_FILE_FROM_SERVER);

      if (entry->IsPinned()) {
         // Change expected_existing_path of CACHE_TYPE_PINNED (index 2).
         paths_to_verify[2].expected_existing_path =
             GetCacheFilePath(resource_id,
                              std::string(),
                              GDataRootDirectory::CACHE_TYPE_PINNED,
                              GDataFileSystem::CACHED_FILE_FROM_SERVER);
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
      GDataRootDirectory::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    file_system_->Pin(resource_id, md5,
        base::Bind(&GDataFileSystemTest::VerifyCacheFileState,
                   base::Unretained(this)));

    RunAllPendingForIO();
  }

  void TestUnpin(
      const std::string& resource_id,
      const std::string& md5,
      base::PlatformFileError expected_error,
      int expected_cache_state,
      GDataRootDirectory::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    file_system_->Unpin(resource_id, md5,
        base::Bind(&GDataFileSystemTest::VerifyCacheFileState,
                   base::Unretained(this)));

    RunAllPendingForIO();
  }

  void TestGetCacheState(const std::string& resource_id, const std::string& md5,
                         base::PlatformFileError expected_error,
                         int expected_cache_state, GDataFile* expected_file) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;

    file_system_->GetCacheState(resource_id, md5,
        base::Bind(&GDataFileSystemTest::VerifyGetCacheState,
                   base::Unretained(this)));

    RunAllPendingForIO();
  }

  void VerifyGetCacheState(base::PlatformFileError error, int cache_state) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

    if (error == base::PLATFORM_FILE_OK) {
      EXPECT_EQ(expected_cache_state_, cache_state);
    }
  }

  void TestMarkDirty(
      const std::string& resource_id,
      const std::string& md5,
      base::PlatformFileError expected_error,
      int expected_cache_state,
      GDataRootDirectory::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = false;

    file_system_->MarkDirtyInCache(resource_id, md5,
        base::Bind(&GDataFileSystemTest::VerifyMarkDirty,
                   base::Unretained(this)));

    RunAllPendingForIO();
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
      GDataRootDirectory::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = true;

    file_system_->CommitDirtyInCache(resource_id, md5,
        base::Bind(&GDataFileSystemTest::VerifyCacheFileState,
                   base::Unretained(this)));

    RunAllPendingForIO();
  }

  void TestClearDirty(
      const std::string& resource_id,
      const std::string& md5,
      base::PlatformFileError expected_error,
      int expected_cache_state,
      GDataRootDirectory::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = false;

    file_system_->ClearDirtyInCache(resource_id, md5,
        base::Bind(&GDataFileSystemTest::VerifyCacheFileState,
                   base::Unretained(this)));

    RunAllPendingForIO();
  }

  void VerifySetMountedState(const std::string& resource_id,
                             const std::string& md5,
                             bool to_mount,
                             base::PlatformFileError error,
                             const FilePath& file_path) {
    ++num_callback_invocations_;
    EXPECT_TRUE(file_util::PathExists(file_path));
    EXPECT_TRUE(file_path == file_system_->GetCacheFilePath(
        resource_id,
        md5,
        expected_sub_dir_type_,
        to_mount ?
            GDataFileSystemInterface::CACHED_FILE_MOUNTED :
            GDataFileSystemInterface::CACHED_FILE_FROM_SERVER));
  }

  void TestSetMountedState(
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& file_path,
      bool to_mount,
      base::PlatformFileError expected_error,
      int expected_cache_state,
      GDataRootDirectory::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = false;

    file_system_->SetMountedState(file_path, to_mount,
        base::Bind(&GDataFileSystemTest::VerifySetMountedState,
                   base::Unretained(this), resource_id, md5, to_mount));

    RunAllPendingForIO();
  }

  void PrepareForInitCacheTest() {
    // Create gdata cache sub directories.
    ASSERT_TRUE(file_util::CreateDirectory(
        file_system_->cache_paths_[GDataRootDirectory::CACHE_TYPE_PERSISTENT]));
    ASSERT_TRUE(file_util::CreateDirectory(
        file_system_->cache_paths_[GDataRootDirectory::CACHE_TYPE_TMP]));
    ASSERT_TRUE(file_util::CreateDirectory(
        file_system_->cache_paths_[GDataRootDirectory::CACHE_TYPE_PINNED]));
    ASSERT_TRUE(file_util::CreateDirectory(
        file_system_->cache_paths_[GDataRootDirectory::CACHE_TYPE_OUTGOING]));

    // Dump some files into cache dirs so that
    // GDataFileSystem::InitializeCacheIOThreadPool would scan through them and
    // populate cache map accordingly.

    // Copy files from data dir to cache dir to act as cached files.
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(initial_cache_resources); ++i) {
      const struct InitialCacheResource& resource = initial_cache_resources[i];
      // Determine gdata cache file absolute path according to cache state.
      FilePath dest_path = file_system_->GetCacheFilePath(
          resource.resource_id,
          resource.md5,
          GDataFile::IsCachePinned(resource.cache_state) ||
              GDataFile::IsCacheDirty(resource.cache_state) ?
                  GDataRootDirectory::CACHE_TYPE_PERSISTENT :
                  GDataRootDirectory::CACHE_TYPE_TMP,
          GDataFile::IsCacheDirty(resource.cache_state) ?
              GDataFileSystem::CACHED_FILE_LOCALLY_MODIFIED :
              GDataFileSystem::CACHED_FILE_FROM_SERVER);

      // Copy file from data dir to cache subdir, naming it per cache files
      // convention.
      if (GDataFile::IsCachePresent(resource.cache_state)) {
        FilePath source_path = GetTestFilePath(resource.source_file);
        ASSERT_TRUE(file_util::CopyFile(source_path, dest_path));
      } else {
        dest_path = FilePath(FILE_PATH_LITERAL(kSymLinkToDevNull));
      }

      // Create symbolic link in pinned dir, naming it per cache files
      // convention.
      if (GDataFile::IsCachePinned(resource.cache_state)) {
        FilePath link_path = file_system_->GetCacheFilePath(
            resource.resource_id,
            "",
            GDataRootDirectory::CACHE_TYPE_PINNED,
            GDataFileSystem::CACHED_FILE_FROM_SERVER);
        ASSERT_TRUE(file_util::CreateSymbolicLink(dest_path, link_path));
      }

      // Create symbolic link in outgoing dir, naming it per cache files
      // convention.
      if (GDataFile::IsCacheDirty(resource.cache_state)) {
        FilePath link_path = file_system_->GetCacheFilePath(
            resource.resource_id,
            "",
            GDataRootDirectory::CACHE_TYPE_OUTGOING,
            GDataFileSystem::CACHED_FILE_FROM_SERVER);
        ASSERT_TRUE(file_util::CreateSymbolicLink(dest_path, link_path));
      }
    }
  }

  void TestInitializeCache() {
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(initial_cache_resources); ++i) {
      const struct InitialCacheResource& resource = initial_cache_resources[i];
      // Check cache file.
      num_callback_invocations_ = 0;
      TestGetFileFromCacheByResourceIdAndMd5(
          resource.resource_id,
          resource.md5,
          GDataFile::IsCachePresent(resource.cache_state) ?
          base::PLATFORM_FILE_OK :
          base::PLATFORM_FILE_ERROR_NOT_FOUND,
          resource.expected_file_extension);
      EXPECT_EQ(1, num_callback_invocations_);

      // Verify cache state.
      std::string md5;
      if (GDataFile::IsCachePresent(resource.cache_state))
         md5 = resource.md5;
      GDataRootDirectory::CacheEntry* entry =
          file_system_->root_->GetCacheEntry(resource.resource_id, md5);
      ASSERT_TRUE(entry != NULL);
      EXPECT_EQ(resource.cache_state, entry->cache_state);
      EXPECT_EQ(resource.expected_sub_dir_type, entry->sub_dir_type);
    }
  }

  void VerifyCacheFileState(base::PlatformFileError error,
                            const std::string& resource_id,
                            const std::string& md5) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

    // Verify cache map.
    GDataRootDirectory::CacheEntry* entry =
        file_system_->root_->GetCacheEntry(resource_id, md5);
    if (GDataFile::IsCachePresent(expected_cache_state_) ||
        GDataFile::IsCachePinned(expected_cache_state_)) {
      ASSERT_TRUE(entry != NULL);
      EXPECT_EQ(expected_cache_state_, entry->cache_state);
      EXPECT_EQ(expected_sub_dir_type_, entry->sub_dir_type);
    } else {
      EXPECT_TRUE(entry == NULL);
    }

    // Verify actual cache file.
    FilePath dest_path = file_system_->GetCacheFilePath(
        resource_id,
        md5,
        GDataFile::IsCachePinned(expected_cache_state_) ||
            GDataFile::IsCacheDirty(expected_cache_state_) ?
                GDataRootDirectory::CACHE_TYPE_PERSISTENT :
                GDataRootDirectory::CACHE_TYPE_TMP,
        GDataFile::IsCacheDirty(expected_cache_state_) ?
            GDataFileSystem::CACHED_FILE_LOCALLY_MODIFIED :
            GDataFileSystem::CACHED_FILE_FROM_SERVER);
    bool exists = file_util::PathExists(dest_path);
    if (GDataFile::IsCachePresent(expected_cache_state_))
      EXPECT_TRUE(exists);
    else
      EXPECT_FALSE(exists);

    // Verify symlink in pinned dir.
    FilePath symlink_path = file_system_->GetCacheFilePath(
        resource_id,
        std::string(),
        GDataRootDirectory::CACHE_TYPE_PINNED,
        GDataFileSystem::CACHED_FILE_FROM_SERVER);
    // Check that pin symlink exists, without deferencing to target path.
    exists = file_util::IsLink(symlink_path);
    if (GDataFile::IsCachePinned(expected_cache_state_)) {
      EXPECT_TRUE(exists);
      FilePath target_path;
      EXPECT_TRUE(file_util::ReadSymbolicLink(symlink_path, &target_path));
      if (GDataFile::IsCachePresent(expected_cache_state_))
        EXPECT_EQ(dest_path, target_path);
      else
        EXPECT_EQ(kSymLinkToDevNull, target_path.value());
    } else {
      EXPECT_FALSE(exists);
    }

    // Verify symlink in outgoing dir.
    symlink_path = file_system_->GetCacheFilePath(
        resource_id,
        std::string(),
        GDataRootDirectory::CACHE_TYPE_OUTGOING,
        GDataFileSystem::CACHED_FILE_FROM_SERVER);
    // Check that outgoing symlink exists, without deferencing to target path.
    exists = file_util::IsLink(symlink_path);
    if (expect_outgoing_symlink_ &&
        GDataFile::IsCacheDirty(expected_cache_state_)) {
      EXPECT_TRUE(exists);
      FilePath target_path;
      EXPECT_TRUE(file_util::ReadSymbolicLink(symlink_path, &target_path));
      EXPECT_TRUE(target_path.value() != kSymLinkToDevNull);
      if (GDataFile::IsCachePresent(expected_cache_state_))
        EXPECT_EQ(dest_path, target_path);
    } else {
      EXPECT_FALSE(exists);
    }
  }

  // Used to wait for the result from an operation that involves file IO,
  // that runs on the blocking pool thread.
  void RunAllPendingForIO() {
    // We should first flush tasks on UI thread, as it can require some
    // tasks to be run before IO tasks start.
    message_loop_.RunAllPending();
    BrowserThread::GetBlockingPool()->FlushForTesting();
    // Once IO tasks are done, flush UI thread again so the results from IO
    // tasks are processed.
    message_loop_.RunAllPending();
  }

  // Loads serialized proto file from GCache, and makes sure the root
  // filesystem has a root at 'drive'
  void TestLoadMetadataFromCache() {
    file_system_->LoadRootFeedFromCache(
        false,     // load_from_server
        FilePath(FILE_PATH_LITERAL("drive")),
        base::Bind(&GDataFileSystemTest::OnExpectToFindEntry,
                   FilePath(FILE_PATH_LITERAL("drive"))));
    BrowserThread::GetBlockingPool()->FlushForTesting();
    message_loop_.RunAllPending();
  }

  static void OnExpectToFindEntry(const FilePath& search_file_path,
                                  base::PlatformFileError error,
                                  GDataEntry* entry) {
    ASSERT_TRUE(entry);
    ASSERT_EQ(search_file_path, entry->GetFilePath());
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
    platform_info->set_is_directory(true);

    // drive/File1
    GDataFileProto* file = root_dir->add_child_files();
    file_base = file->mutable_gdata_entry();
    platform_info = file_base->mutable_file_info();
    file_base->set_title("File1");
    platform_info->set_is_directory(false);
    platform_info->set_size(1048576);

    // drive/Dir1
    GDataDirectoryProto* dir1 = root_dir->add_child_directories();
    file_base = dir1->mutable_gdata_entry();
    platform_info = file_base->mutable_file_info();
    file_base->set_title("Dir1");
    platform_info->set_is_directory(true);

    // drive/Dir1/File2
    file = dir1->add_child_files();
    file_base = file->mutable_gdata_entry();
    platform_info = file_base->mutable_file_info();
    file_base->set_title("File2");
    platform_info->set_is_directory(false);
    platform_info->set_size(555);

    // drive/Dir1/SubDir2
    GDataDirectoryProto* dir2 = dir1->add_child_directories();
    file_base = dir2->mutable_gdata_entry();
    platform_info = file_base->mutable_file_info();
    file_base->set_title("SubDir2");
    platform_info->set_is_directory(true);

    // drive/Dir1/SubDir2/File3
    file = dir2->add_child_files();
    file_base = file->mutable_gdata_entry();
    platform_info = file_base->mutable_file_info();
    file_base->set_title("File3");
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
  void VerifyHostedDocumentJSONFile(const GDataFile* gdata_file,
                                    const FilePath& file_path) {
    ASSERT_TRUE(gdata_file != NULL);

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

    EXPECT_EQ(gdata_file->alternate_url().spec(), edit_url);
    EXPECT_EQ(gdata_file->resource_id(), resource_id);
  }

  // This is used as a helper for registering callbacks that need to be
  // RefCountedThreadSafe, and a place where we can fetch results from various
  // operations.
  class CallbackHelper
    : public base::RefCountedThreadSafe<CallbackHelper> {
   public:
    CallbackHelper()
        : last_error_(base::PLATFORM_FILE_OK),
          quota_bytes_total_(0),
          quota_bytes_used_(0),
          file_proto_(NULL) {}
    virtual ~CallbackHelper() {}

    virtual void GetFileCallback(base::PlatformFileError error,
                                 const FilePath& file_path,
                                 const std::string& mime_type,
                                 GDataFileType file_type) {
      last_error_ = error;
      download_path_ = file_path;
      mime_type_ = mime_type;
      file_type_ = file_type;
    }

    virtual void FileOperationCallback(base::PlatformFileError error) {
      DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

      last_error_ = error;
    }

    virtual void GetAvailableSpaceCallback(base::PlatformFileError error,
                                           int64 bytes_total,
                                           int64 bytes_used) {
      last_error_ = error;
      quota_bytes_total_ = bytes_total;
      quota_bytes_used_ = bytes_used;
    }

    virtual void GetEntryInfoCallback(
        base::PlatformFileError error,
        const FilePath& entry_path,
        scoped_ptr<GDataEntryProto> entry_proto) {
      last_error_ = error;
      entry_proto_ = entry_proto.Pass();
    }

    virtual void GetFileInfoCallback(
        base::PlatformFileError error,
        scoped_ptr<GDataFileProto> file_proto) {
      last_error_ = error;
      file_proto_ = file_proto.Pass();
    }

    virtual void ReadDirectoryCallback(
        base::PlatformFileError error,
        scoped_ptr<GDataDirectoryProto> directory_proto) {
      last_error_ = error;
      directory_proto_ = directory_proto.Pass();
    }

    base::PlatformFileError last_error_;
    FilePath download_path_;
    std::string mime_type_;
    GDataFileType file_type_;
    int64 quota_bytes_total_;
    int64 quota_bytes_used_;
    scoped_ptr<GDataEntryProto> entry_proto_;
    scoped_ptr<GDataFileProto> file_proto_;
    scoped_ptr<GDataDirectoryProto> directory_proto_;
  };

  MessageLoopForUI message_loop_;
  // The order of the test threads is important, do not change the order.
  // See also content/browser/browser_thread_imple.cc.
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<CallbackHelper> callback_helper_;
  GDataFileSystem* file_system_;
  MockDocumentsService* mock_doc_service_;
  MockFreeDiskSpaceGetter* mock_free_disk_space_checker_;
  scoped_ptr<MockGDataSyncClient> mock_sync_client_;
  scoped_ptr<MockDirectoryChangeObserver> mock_directory_observer_;

  int num_callback_invocations_;
  base::PlatformFileError expected_error_;
  int expected_cache_state_;
  GDataRootDirectory::CacheSubDirectoryType expected_sub_dir_type_;
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
    base::PlatformFileError error,
    scoped_ptr<GDataDirectoryProto> directory_proto) {
  ASSERT_EQ(base::PLATFORM_FILE_OK, error);
  ASSERT_TRUE(directory_proto.get());
  EXPECT_EQ(expected_file_path.value(),
            directory_proto->gdata_entry().file_name());

  (*counter)++;
  if (*counter >= expected_counter)
    message_loop->Quit();
}

TEST_F(GDataFileSystemTest, DuplicatedAsyncInitialization) {
  int counter = 0;
  ReadDirectoryCallback callback = base::Bind(
      &AsyncInitializationCallback,
      &counter,
      2,
      FilePath(FILE_PATH_LITERAL("drive")),
      &message_loop_);

  EXPECT_CALL(*mock_doc_service_, GetAccountMetadata(_)).Times(1);
  EXPECT_CALL(*mock_doc_service_, GetDocuments(Eq(GURL()), _, _, _)).Times(1);

  file_system_->ReadDirectoryByPathAsync(
      FilePath(FILE_PATH_LITERAL("drive")), callback);
  file_system_->ReadDirectoryByPathAsync(
      FilePath(FILE_PATH_LITERAL("drive")), callback);
  message_loop_.Run();  // Wait to get our result
  EXPECT_EQ(2, counter);
}

TEST_F(GDataFileSystemTest, SearchRootDirectory) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath = FilePath(FILE_PATH_LITERAL("drive"));
  GDataEntry* entry = FindEntry(FilePath(FILE_PATH_LITERAL(kFilePath)));
  ASSERT_TRUE(entry);
  EXPECT_EQ(kFilePath, entry->GetFilePath());
  EXPECT_TRUE(GetEntryInfoAndCompare(kFilePath, entry));
  EXPECT_TRUE(ReadDirectoryAndCompare(kFilePath, entry->AsGDataDirectory()));
}

TEST_F(GDataFileSystemTest, SearchExistingFile) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/File 1.txt"));
  GDataEntry* entry = FindEntry(kFilePath);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kFilePath, entry->GetFilePath());
  EXPECT_TRUE(GetEntryInfoAndCompare(kFilePath, entry));
  EXPECT_TRUE(GetFileInfoAndCompare(kFilePath, entry->AsGDataFile()));
}

TEST_F(GDataFileSystemTest, SearchExistingDocument) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/Document 1.gdoc"));
  GDataEntry* entry = FindEntry(kFilePath);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kFilePath, entry->GetFilePath());
  EXPECT_TRUE(GetEntryInfoAndCompare(kFilePath, entry));
  EXPECT_TRUE(GetFileInfoAndCompare(kFilePath, entry->AsGDataFile()));
}

TEST_F(GDataFileSystemTest, SearchNonExistingFile) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/nonexisting.file"));
  GDataEntry* entry = FindEntry(kFilePath);
  ASSERT_FALSE(entry);
  EXPECT_TRUE(GetEntryInfoAndCompare(kFilePath, entry));
  EXPECT_TRUE(GetFileInfoAndCompare(kFilePath, NULL));
}

TEST_F(GDataFileSystemTest, SearchEncodedFileNames) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath1 = FilePath(
      FILE_PATH_LITERAL("drive/Slash / in file 1.txt"));
  GDataEntry* entry = FindEntry(kFilePath1);
  ASSERT_FALSE(entry);
  EXPECT_TRUE(GetEntryInfoAndCompare(kFilePath1, NULL));
  EXPECT_TRUE(GetFileInfoAndCompare(kFilePath1, NULL));

  const FilePath kFilePath2 = FilePath::FromUTF8Unsafe(
      "drive/Slash \xE2\x88\x95 in file 1.txt");
  entry = FindEntry(kFilePath2);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kFilePath2, entry->GetFilePath());
  EXPECT_TRUE(GetEntryInfoAndCompare(kFilePath2, entry));
  EXPECT_TRUE(GetFileInfoAndCompare(kFilePath2, entry->AsGDataFile()));

  const FilePath kFilePath3 = FilePath::FromUTF8Unsafe(
      "drive/Slash \xE2\x88\x95 in directory/Slash SubDir File.txt");
  entry = FindEntry(kFilePath3);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kFilePath3, entry->GetFilePath());
  EXPECT_TRUE(GetEntryInfoAndCompare(kFilePath3, entry));
  EXPECT_TRUE(GetFileInfoAndCompare(kFilePath3, entry->AsGDataFile()));
}

TEST_F(GDataFileSystemTest, SearchEncodedFileNamesLoadingRoot) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath1 = FilePath(
      FILE_PATH_LITERAL("drive/Slash / in file 1.txt"));
  GDataEntry* entry = FindEntry(kFilePath1);
  ASSERT_FALSE(entry);
  EXPECT_TRUE(GetEntryInfoAndCompare(kFilePath1, NULL));
  EXPECT_TRUE(GetFileInfoAndCompare(kFilePath1, NULL));

  const FilePath kFilePath2 = FilePath::FromUTF8Unsafe(
      "drive/Slash \xE2\x88\x95 in file 1.txt");
  entry = FindEntry(kFilePath2);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kFilePath2, entry->GetFilePath());
  EXPECT_TRUE(GetEntryInfoAndCompare(kFilePath2, entry));
  EXPECT_TRUE(GetFileInfoAndCompare(kFilePath2, entry->AsGDataFile()));

  const FilePath kFilePath3 = FilePath::FromUTF8Unsafe(
      "drive/Slash \xE2\x88\x95 in directory/Slash SubDir File.txt");
  entry = FindEntry(kFilePath3);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kFilePath3, entry->GetFilePath());
  EXPECT_TRUE(GetEntryInfoAndCompare(kFilePath3, entry));
  EXPECT_TRUE(GetFileInfoAndCompare(kFilePath3, entry->AsGDataFile()));
}

TEST_F(GDataFileSystemTest, SearchDuplicateNames) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath1 = FilePath(
      FILE_PATH_LITERAL("drive/Duplicate Name.txt"));
  GDataEntry* entry = FindEntry(kFilePath1);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kFilePath1, entry->GetFilePath());
  EXPECT_TRUE(GetEntryInfoAndCompare(kFilePath1, entry));
  EXPECT_TRUE(GetFileInfoAndCompare(kFilePath1, entry->AsGDataFile()));

  const FilePath kFilePath2 = FilePath(
      FILE_PATH_LITERAL("drive/Duplicate Name (2).txt"));
  entry = FindEntry(kFilePath2);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kFilePath2, entry->GetFilePath());
  EXPECT_TRUE(GetEntryInfoAndCompare(kFilePath2, entry));
  EXPECT_TRUE(GetFileInfoAndCompare(kFilePath2, entry->AsGDataFile()));
}

TEST_F(GDataFileSystemTest, SearchExistingDirectory) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/Directory 1"));
  GDataEntry* entry = FindEntry(kFilePath);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kFilePath, entry->GetFilePath());
  EXPECT_TRUE(GetEntryInfoAndCompare(kFilePath, entry));
  EXPECT_TRUE(ReadDirectoryAndCompare(kFilePath, entry->AsGDataDirectory()));
}

TEST_F(GDataFileSystemTest, SearchInSubdir) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
  GDataEntry* entry = FindEntry(kFilePath);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kFilePath, entry->GetFilePath());
  EXPECT_TRUE(GetEntryInfoAndCompare(kFilePath, entry));
  EXPECT_TRUE(GetFileInfoAndCompare(kFilePath, entry->AsGDataFile()));
}

// Check the reconstruction of the directory structure from only the root feed.
TEST_F(GDataFileSystemTest, SearchInSubSubdir) {
  LoadRootFeedDocument("root_feed.json");

  const FilePath kFilePath = FilePath(
      FILE_PATH_LITERAL("drive/Directory 1/Sub Directory Folder/"
                        "Sub Sub Directory Folder"));
  GDataEntry* entry = FindEntry(kFilePath);
  ASSERT_TRUE(entry);
  EXPECT_EQ(kFilePath, entry->GetFilePath());
  EXPECT_TRUE(GetEntryInfoAndCompare(kFilePath, entry));
  EXPECT_TRUE(ReadDirectoryAndCompare(kFilePath, entry->AsGDataDirectory()));
}

TEST_F(GDataFileSystemTest, FilePathTests) {
  LoadRootFeedDocument("root_feed.json");

  FindAndTestFilePath(FilePath(FILE_PATH_LITERAL("drive/File 1.txt")));
  FindAndTestFilePath(FilePath(FILE_PATH_LITERAL("drive/Directory 1")));
  FindAndTestFilePath(
      FilePath(FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt")));
}

TEST_F(GDataFileSystemTest, ChangeFeed_AddAndDeleteFileInRoot) {
  int latest_changelog = 0;
  LoadRootFeedDocument("root_feed.json");

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(2);

  LoadChangeFeed("delta_file_added_in_root.json", ++latest_changelog);
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL("drive/Added file.gdoc"))));

  LoadChangeFeed("delta_file_deleted_in_root.json", ++latest_changelog);
  EXPECT_FALSE(FindEntry(FilePath(FILE_PATH_LITERAL("drive/Added file.gdoc"))));
}


TEST_F(GDataFileSystemTest, ChangeFeed_AddAndDeleteFileFromExistingDirectory) {
  int latest_changelog = 0;
  LoadRootFeedDocument("root_feed.json");

  EXPECT_TRUE(FindEntry(FilePath(
      FILE_PATH_LITERAL("drive/Directory 1"))));

  // Add file to an existing directory.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  LoadChangeFeed("delta_file_added_in_directory.json", ++latest_changelog);
  EXPECT_TRUE(FindEntry(FilePath(
      FILE_PATH_LITERAL("drive/Directory 1/Added file.gdoc"))));

  // Remove that file from the directory.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  LoadChangeFeed("delta_file_deleted_in_directory.json", ++latest_changelog);
  EXPECT_TRUE(FindEntry(FilePath(
      FILE_PATH_LITERAL("drive/Directory 1"))));
  EXPECT_FALSE(FindEntry(FilePath(
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

  EXPECT_TRUE(FindEntry(FilePath(
      FILE_PATH_LITERAL("drive/New Directory"))));
  EXPECT_TRUE(FindEntry(FilePath(
      FILE_PATH_LITERAL("drive/New Directory/File in new dir.gdoc"))));
}

TEST_F(GDataFileSystemTest, ChangeFeed_DirectoryMovedFromRootToDirectory) {
  int latest_changelog = 0;
  LoadRootFeedDocument("root_feed.json");

  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
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

  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2"))));
  EXPECT_FALSE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2/Directory 1"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2/Directory 1/SubDirectory File 1.txt"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2/Directory 1/Sub Directory Folder"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 2/Directory 1/Sub Directory Folder/"
      "Sub Sub Directory Folder"))));
}

TEST_F(GDataFileSystemTest, ChangeFeed_FileMovedFromDirectoryToRoot) {
  int latest_changelog = 0;
  LoadRootFeedDocument("root_feed.json");

  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder/Sub Sub Directory Folder"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  LoadChangeFeed("delta_file_moved_from_directory_to_root.json",
                 ++latest_changelog);

  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder/Sub Sub Directory Folder"))));
  EXPECT_FALSE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/SubDirectory File 1.txt"))));
}

TEST_F(GDataFileSystemTest, ChangeFeed_FileRenamedInDirectory) {
  int latest_changelog = 0;
  LoadRootFeedDocument("root_feed.json");

  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  LoadChangeFeed("delta_file_renamed_in_directory.json",
                 ++latest_changelog);

  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1"))));
  EXPECT_FALSE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"))));
  EXPECT_TRUE(FindEntry(FilePath(FILE_PATH_LITERAL(
      "drive/Directory 1/New SubDirectory File 1.txt"))));
}

TEST_F(GDataFileSystemTest, CachedFeedLoading) {
  SaveTestFileSystem();
  TestLoadMetadataFromCache();

  FindAndTestFilePath(FilePath(FILE_PATH_LITERAL("drive/File1")));
  FindAndTestFilePath(FilePath(FILE_PATH_LITERAL("drive/Dir1")));
  FindAndTestFilePath(FilePath(FILE_PATH_LITERAL("drive/Dir1/File2")));
  FindAndTestFilePath(FilePath(FILE_PATH_LITERAL("drive/Dir1/SubDir2")));
  FindAndTestFilePath(FilePath(FILE_PATH_LITERAL("drive/Dir1/SubDir2/File3")));
}

TEST_F(GDataFileSystemTest, TransferFileFromRemoteToLocal_RegularFile) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  LoadRootFeedDocument("root_feed.json");

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath local_dest_file_path = temp_dir.path().Append("local_copy.txt");

  FilePath remote_src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  GDataEntry* entry = FindEntry(remote_src_file_path);
  GDataFile* file = entry->AsGDataFile();
  FilePath cache_file = GetCachePathForFile(file);
  const int64 file_size = entry->file_info().size;

  // Pretend we have enough space.
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(2).WillRepeatedly(Return(file_size + kMinFreeSpace));

  const std::string remote_src_file_data = "Test file data";
  mock_doc_service_->set_file_data(new std::string(remote_src_file_data));
  // The file is obtained with the mock DocumentsService.
  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(remote_src_file_path,
                           cache_file,
                           GURL("https://file_content_url/"),
                           _, _))
      .Times(1);

  file_system_->TransferFileFromRemoteToLocal(
      remote_src_file_path, local_dest_file_path, callback);
  RunAllPendingForIO();  // Try to get from the cache.
  RunAllPendingForIO();  // Check if we have space before downloading.
  RunAllPendingForIO();  // Check if we have space after downloading.
  RunAllPendingForIO();  // Copy downloaded file from cache to destination.

  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  std::string cache_file_data;
  EXPECT_TRUE(file_util::ReadFileToString(cache_file, &cache_file_data));
  EXPECT_EQ(remote_src_file_data, cache_file_data);

  std::string local_dest_file_data;
  EXPECT_TRUE(file_util::ReadFileToString(local_dest_file_path,
                                          &local_dest_file_data));
  EXPECT_EQ(remote_src_file_data, local_dest_file_data);
}

TEST_F(GDataFileSystemTest, TransferFileFromRemoteToLocal_HostedDocument) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

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
  RunAllPendingForIO();  // Try to get from the cache.
  RunAllPendingForIO();  // Copy downloaded file from cache to destination.

  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  GDataEntry* entry = FindEntry(remote_src_file_path);
  VerifyHostedDocumentJSONFile(entry->AsGDataFile(), local_dest_file_path);
}

TEST_F(GDataFileSystemTest, CopyNotExistingFile) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/Dummy file.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataEntry* src_file = NULL;
  EXPECT_TRUE((src_file = FindEntry(src_file_path)) == NULL);

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Copy(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_TRUE(FindEntry(src_file_path) == NULL);
  EXPECT_TRUE(FindEntry(dest_file_path) == NULL);
}

TEST_F(GDataFileSystemTest, CopyFileToNonExistingDirectory) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Dummy"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Dummy/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataEntry* src_file = NULL;
  ASSERT_TRUE((src_file = FindEntry(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindEntryByResourceId(src_file_path_resource));
  EXPECT_FALSE(src_file->edit_url().is_empty());

  EXPECT_TRUE(FindEntry(dest_parent_path) == NULL);

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_EQ(src_file, FindEntry(src_file_path));
  EXPECT_EQ(src_file, FindEntryByResourceId(src_file_path_resource));

  EXPECT_TRUE(FindEntry(dest_parent_path) == NULL);
  EXPECT_TRUE(FindEntry(dest_file_path) == NULL);
}

// Test the case where the parent of |dest_file_path| is a existing file,
// not a directory.
TEST_F(GDataFileSystemTest, CopyFileToInvalidPath) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/Document 1.gdoc"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Duplicate Name.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL(
      "drive/Duplicate Name.txt/Document 1.gdoc"));

  LoadRootFeedDocument("root_feed.json");

  GDataEntry* src_file = NULL;
  EXPECT_TRUE((src_file = FindEntry(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindEntryByResourceId(src_file_path_resource));
  EXPECT_FALSE(src_file->edit_url().is_empty());

  GDataEntry* dest_parent = NULL;
  EXPECT_TRUE((dest_parent = FindEntry(dest_parent_path)) != NULL);
  EXPECT_TRUE(dest_parent->AsGDataFile() != NULL);

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Copy(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY,
            callback_helper_->last_error_);

  EXPECT_EQ(src_file, FindEntry(src_file_path));
  EXPECT_EQ(src_file, FindEntryByResourceId(src_file_path_resource));
  EXPECT_EQ(dest_parent, FindEntry(dest_parent_path));

  EXPECT_TRUE(FindEntry(dest_file_path) == NULL);
}

TEST_F(GDataFileSystemTest, RenameFile) {
  const FilePath src_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
  const FilePath src_parent_path(FILE_PATH_LITERAL("drive/Directory 1"));
  const FilePath dest_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataEntry* src_file = FindEntry(src_file_path);
  EXPECT_TRUE(src_file != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindEntryByResourceId(src_file_resource));

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(src_file->edit_url(),
                             FILE_PATH_LITERAL("Test.log"), _));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_TRUE(FindEntry(src_file_path) == NULL);

  GDataEntry* dest_file = FindEntry(dest_file_path);
  EXPECT_TRUE(dest_file != NULL);
  EXPECT_EQ(dest_file, FindEntryByResourceId(src_file_resource));
  EXPECT_EQ(src_file, dest_file);
}

TEST_F(GDataFileSystemTest, MoveFileFromRootToSubDirectory) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Directory 1"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Directory 1/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataEntry* src_file = NULL;
  EXPECT_TRUE((src_file = FindEntry(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindEntryByResourceId(src_file_path_resource));
  EXPECT_FALSE(src_file->edit_url().is_empty());

  GDataEntry* dest_parent = NULL;
  EXPECT_TRUE((dest_parent = FindEntry(dest_parent_path)) != NULL);
  EXPECT_TRUE(dest_parent->AsGDataDirectory() != NULL);
  EXPECT_FALSE(dest_parent->content_url().is_empty());

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(src_file->edit_url(),
                             FILE_PATH_LITERAL("Test.log"), _));
  EXPECT_CALL(*mock_doc_service_,
              AddResourceToDirectory(dest_parent->content_url(),
                                     src_file->edit_url(), _));

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
  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_TRUE(FindEntry(src_file_path) == NULL);

  GDataEntry* dest_file = NULL;
  EXPECT_TRUE((dest_file = FindEntry(dest_file_path)) != NULL);
  EXPECT_EQ(dest_file, FindEntryByResourceId(src_file_path_resource));
  EXPECT_EQ(src_file, dest_file);
}

TEST_F(GDataFileSystemTest, MoveFileFromSubDirectoryToRoot) {
  FilePath src_parent_path(FILE_PATH_LITERAL("drive/Directory 1"));
  FilePath src_file_path(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataEntry* src_file = NULL;
  EXPECT_TRUE((src_file = FindEntry(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindEntryByResourceId(src_file_path_resource));
  EXPECT_FALSE(src_file->edit_url().is_empty());

  GDataEntry* src_parent = NULL;
  EXPECT_TRUE((src_parent = FindEntry(src_parent_path)) != NULL);
  EXPECT_TRUE(src_parent->AsGDataDirectory() != NULL);
  EXPECT_FALSE(src_parent->content_url().is_empty());

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(src_file->edit_url(),
                             FILE_PATH_LITERAL("Test.log"), _));
  EXPECT_CALL(*mock_doc_service_,
              RemoveResourceFromDirectory(src_parent->content_url(),
                                          src_file->edit_url(),
                                          src_file_path_resource, _));

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
  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_TRUE(FindEntry(src_file_path) == NULL);

  GDataEntry* dest_file = NULL;
  EXPECT_TRUE((dest_file = FindEntry(dest_file_path)) != NULL);
  EXPECT_EQ(dest_file, FindEntryByResourceId(src_file_path_resource));
  EXPECT_EQ(src_file, dest_file);
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

  GDataEntry* src_file = NULL;
  EXPECT_TRUE((src_file = FindEntry(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindEntryByResourceId(src_file_path_resource));
  EXPECT_FALSE(src_file->edit_url().is_empty());

  GDataEntry* src_parent = NULL;
  EXPECT_TRUE((src_parent = FindEntry(src_parent_path)) != NULL);
  EXPECT_TRUE(src_parent->AsGDataDirectory() != NULL);
  EXPECT_FALSE(src_parent->content_url().is_empty());

  GDataEntry* dest_parent = NULL;
  ASSERT_TRUE((dest_parent = FindEntry(dest_parent_path)) != NULL);
  EXPECT_TRUE(dest_parent->AsGDataDirectory() != NULL);
  EXPECT_FALSE(dest_parent->content_url().is_empty());

  EXPECT_TRUE(FindEntry(interim_file_path) == NULL);

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(src_file->edit_url(),
                             FILE_PATH_LITERAL("Test.log"), _));
  EXPECT_CALL(*mock_doc_service_,
              RemoveResourceFromDirectory(src_parent->content_url(),
                                          src_file->edit_url(),
                                          src_file_path_resource, _));
  EXPECT_CALL(*mock_doc_service_,
              AddResourceToDirectory(dest_parent->content_url(),
                                     src_file->edit_url(), _));

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
  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_TRUE(FindEntry(src_file_path) == NULL);
  EXPECT_TRUE(FindEntry(interim_file_path) == NULL);

  GDataEntry* dest_file = NULL;
  EXPECT_TRUE((dest_file = FindEntry(dest_file_path)) != NULL);
  EXPECT_EQ(dest_file, FindEntryByResourceId(src_file_path_resource));
  EXPECT_EQ(src_file, dest_file);
}

TEST_F(GDataFileSystemTest, MoveNotExistingFile) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/Dummy file.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataEntry* src_file = NULL;
  EXPECT_TRUE((src_file = FindEntry(src_file_path)) == NULL);

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_TRUE(FindEntry(src_file_path) == NULL);
  EXPECT_TRUE(FindEntry(dest_file_path) == NULL);
}

TEST_F(GDataFileSystemTest, MoveFileToNonExistingDirectory) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Dummy"));
  FilePath dest_file_path(FILE_PATH_LITERAL("drive/Dummy/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataEntry* src_file = NULL;
  EXPECT_TRUE((src_file = FindEntry(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindEntryByResourceId(src_file_path_resource));
  EXPECT_FALSE(src_file->edit_url().is_empty());

  EXPECT_TRUE(FindEntry(dest_parent_path) == NULL);

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_EQ(src_file, FindEntry(src_file_path));
  EXPECT_EQ(src_file, FindEntryByResourceId(src_file_path_resource));

  EXPECT_TRUE(FindEntry(dest_parent_path) == NULL);
  EXPECT_TRUE(FindEntry(dest_file_path) == NULL);
}

// Test the case where the parent of |dest_file_path| is a existing file,
// not a directory.
TEST_F(GDataFileSystemTest, MoveFileToInvalidPath) {
  FilePath src_file_path(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("drive/Duplicate Name.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL(
      "drive/Duplicate Name.txt/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataEntry* src_file = NULL;
  EXPECT_TRUE((src_file = FindEntry(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindEntryByResourceId(src_file_path_resource));
  EXPECT_FALSE(src_file->edit_url().is_empty());

  GDataEntry* dest_parent = NULL;
  EXPECT_TRUE((dest_parent = FindEntry(dest_parent_path)) != NULL);
  EXPECT_TRUE(dest_parent->AsGDataFile() != NULL);

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY,
            callback_helper_->last_error_);

  EXPECT_EQ(src_file, FindEntry(src_file_path));
  EXPECT_EQ(src_file, FindEntryByResourceId(src_file_path_resource));
  EXPECT_EQ(dest_parent, FindEntry(dest_parent_path));

  EXPECT_TRUE(FindEntry(dest_file_path) == NULL);
}

TEST_F(GDataFileSystemTest, RemoveEntries) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  LoadRootFeedDocument("root_feed.json");

  FilePath nonexisting_file(FILE_PATH_LITERAL("drive/Dummy file.txt"));
  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  FilePath dir_in_root(FILE_PATH_LITERAL("drive/Directory 1"));
  FilePath file_in_subdir(
      FILE_PATH_LITERAL("drive/Directory 1/SubDirectory File 1.txt"));

  GDataEntry* entry = NULL;
  EXPECT_TRUE((entry = FindEntry(file_in_root)) != NULL);
  EXPECT_TRUE(entry->AsGDataFile() != NULL);
  std::string file_in_root_resource = entry->AsGDataFile()->resource_id();
  EXPECT_EQ(entry, FindEntryByResourceId(file_in_root_resource));

  EXPECT_TRUE(FindEntry(dir_in_root) != NULL);

  EXPECT_TRUE((entry = FindEntry(file_in_subdir)) != NULL);
  EXPECT_TRUE(entry->AsGDataFile() != NULL);
  std::string file_in_subdir_resource = entry->AsGDataFile()->resource_id();
  EXPECT_EQ(entry, FindEntryByResourceId(file_in_subdir_resource));

  // Once for file in root and once for file...
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(2);

  // Remove first file in root.
  EXPECT_TRUE(RemoveEntry(file_in_root));
  EXPECT_TRUE(FindEntry(file_in_root) == NULL);
  EXPECT_EQ(NULL, FindEntryByResourceId(file_in_root_resource));
  EXPECT_TRUE(FindEntry(dir_in_root) != NULL);
  EXPECT_TRUE((entry = FindEntry(file_in_subdir)) != NULL);
  EXPECT_EQ(entry, FindEntryByResourceId(file_in_subdir_resource));

  // Remove directory.
  EXPECT_TRUE(RemoveEntry(dir_in_root));
  EXPECT_TRUE(FindEntry(file_in_root) == NULL);
  EXPECT_EQ(NULL, FindEntryByResourceId(file_in_root_resource));
  EXPECT_TRUE(FindEntry(dir_in_root) == NULL);
  EXPECT_TRUE(FindEntry(file_in_subdir) == NULL);
  EXPECT_EQ(NULL, FindEntryByResourceId(file_in_subdir_resource));

  // Try removing file in already removed subdirectory.
  EXPECT_FALSE(RemoveEntry(file_in_subdir));

  // Try removing non-existing file.
  EXPECT_FALSE(RemoveEntry(nonexisting_file));

  // Try removing root file element.
  EXPECT_FALSE(RemoveEntry(FilePath(FILE_PATH_LITERAL("drive"))));

  // Need this to ensure OnDirectoryChanged() is run.
  RunAllPendingForIO();
}

TEST_F(GDataFileSystemTest, CreateDirectory) {
  LoadRootFeedDocument("root_feed.json");

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  // Create directory in root.
  FilePath dir_path(FILE_PATH_LITERAL("drive/New Folder 1"));
  EXPECT_TRUE(FindEntry(dir_path) == NULL);
  AddDirectoryFromFile(dir_path, "directory_entry_atom.json");
  EXPECT_TRUE(FindEntry(dir_path) != NULL);

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/New Folder 1"))))).Times(1);

  // Create directory in a sub dirrectory.
  FilePath subdir_path(FILE_PATH_LITERAL("drive/New Folder 1/New Folder 2"));
  EXPECT_TRUE(FindEntry(subdir_path) == NULL);
  AddDirectoryFromFile(subdir_path, "directory_entry_atom.json");
  EXPECT_TRUE(FindEntry(subdir_path) != NULL);
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

TEST_F(GDataFileSystemTest, CacheStateBitmasks) {
  GDataRootDirectory::CacheEntry entry("md5_cache_state_bitmasks",
                                       GDataRootDirectory::CACHE_TYPE_TMP,
                                       GDataFile::CACHE_STATE_NONE);
  EXPECT_FALSE(entry.IsPresent());
  EXPECT_FALSE(entry.IsPinned());
  EXPECT_FALSE(entry.IsDirty());

  entry.cache_state = GDataFile::CACHE_STATE_PRESENT;
  EXPECT_TRUE(entry.IsPresent());
  EXPECT_FALSE(entry.IsPinned());
  EXPECT_FALSE(entry.IsDirty());

  entry.cache_state = GDataFile::CACHE_STATE_PINNED;
  EXPECT_FALSE(entry.IsPresent());
  EXPECT_TRUE(entry.IsPinned());
  EXPECT_FALSE(entry.IsDirty());

  entry.cache_state = GDataFile::CACHE_STATE_PRESENT |
                      GDataFile::CACHE_STATE_PINNED;
  EXPECT_TRUE(entry.IsPresent());
  EXPECT_TRUE(entry.IsPinned());
  EXPECT_FALSE(entry.IsDirty());

  entry.cache_state = GDataFile::CACHE_STATE_PRESENT |
                      GDataFile::CACHE_STATE_DIRTY;
  EXPECT_TRUE(entry.IsPresent());
  EXPECT_FALSE(entry.IsPinned());
  EXPECT_TRUE(entry.IsDirty());

  entry.cache_state = GDataFile::CACHE_STATE_PRESENT |
                      GDataFile::CACHE_STATE_PINNED |
                      GDataFile::CACHE_STATE_DIRTY;
  EXPECT_TRUE(entry.IsPresent());
  EXPECT_TRUE(entry.IsPinned());
  EXPECT_TRUE(entry.IsDirty());

  int cache_state = GDataFile::CACHE_STATE_NONE;
  EXPECT_EQ(GDataFile::CACHE_STATE_PRESENT,
            GDataFile::SetCachePresent(cache_state));
  EXPECT_EQ(GDataFile::CACHE_STATE_PINNED,
            GDataFile::SetCachePinned(cache_state));

  cache_state = GDataFile::CACHE_STATE_PRESENT;
  EXPECT_EQ(GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_PINNED,
            GDataFile::SetCachePinned(cache_state));
  EXPECT_EQ(GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_DIRTY,
            GDataFile::SetCacheDirty(cache_state));
  cache_state |= GDataFile::CACHE_STATE_PINNED;
  EXPECT_EQ(GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_PINNED |
            GDataFile::CACHE_STATE_DIRTY,
            GDataFile::SetCacheDirty(cache_state));

  cache_state = GDataFile::CACHE_STATE_PINNED;
  EXPECT_EQ(GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_PINNED,
            GDataFile::SetCachePresent(cache_state));

  cache_state = GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_PINNED |
                GDataFile::CACHE_STATE_DIRTY;
  EXPECT_EQ(cache_state & ~GDataFile::CACHE_STATE_PRESENT,
            GDataFile::ClearCachePresent(cache_state));
  EXPECT_EQ(cache_state & ~GDataFile::CACHE_STATE_PINNED,
            GDataFile::ClearCachePinned(cache_state));
  EXPECT_EQ(cache_state & ~GDataFile::CACHE_STATE_DIRTY,
            GDataFile::ClearCacheDirty(cache_state));
}

TEST_F(GDataFileSystemTest, GetCacheFilePath) {
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

TEST_F(GDataFileSystemTest, StoreToCacheSimple) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Store an existing file.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store a non-existent file to the same |resource_id| and |md5|.
  num_callback_invocations_ = 0;
  TestStoreToCache(resource_id, md5, FilePath("./non_existent.json"),
                   base::PLATFORM_FILE_ERROR_NOT_FOUND,
                   GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store a different existing file to the same |resource_id| but different
  // |md5|.
  md5 = "new_md5";
  num_callback_invocations_ = 0;
  TestStoreToCache(resource_id, md5, GetTestFilePath("subdir_feed.json"),
                   base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Verify that there's only one file with name <resource_id>, i.e. previously
  // cached file with the different md5 should be deleted.
  FilePath path = GetCacheFilePath(resource_id, "*",
      (GDataFile::IsCachePinned(expected_cache_state_)) ?
          GDataRootDirectory::CACHE_TYPE_PERSISTENT :
          GDataRootDirectory::CACHE_TYPE_TMP,
      GDataFileSystem::CACHED_FILE_FROM_SERVER);
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

TEST_F(GDataFileSystemTest, GetFromCacheSimple) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  // First store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);

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

TEST_F(GDataFileSystemTest, RemoveFromCacheSimple) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  // Use alphanumeric characters for resource id.
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  // First store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);

  // Then try to remove existing file from cache.
  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // Repeat using non-alphanumeric characters for resource id, including '.'
  // which is an extension separator.
  resource_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);

  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataFileSystemTest, PinAndUnpin) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnFilePinned(resource_id, md5)).Times(2);
  EXPECT_CALL(*mock_sync_client_, OnFileUnpinned(resource_id, md5)).Times(1);

  // First store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);

  // Pin the existing file in cache.
  num_callback_invocations_ = 0;
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_PINNED,
          GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Unpin the existing file in cache.
  num_callback_invocations_ = 0;
  TestUnpin(resource_id, md5, base::PLATFORM_FILE_OK,
            GDataFile::CACHE_STATE_PRESENT,
            GDataRootDirectory::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Pin back the same existing file in cache.
  num_callback_invocations_ = 0;
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_PINNED,
          GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Pin a non-existent file in cache.
  resource_id = "document:1a2b";
  EXPECT_CALL(*mock_sync_client_, OnFilePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_sync_client_, OnFileUnpinned(resource_id, md5)).Times(1);

  num_callback_invocations_ = 0;
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataFile::CACHE_STATE_PINNED,
          GDataRootDirectory::CACHE_TYPE_PINNED);
  EXPECT_EQ(1, num_callback_invocations_);

  // Unpin the previously pinned non-existent file in cache.
  num_callback_invocations_ = 0;
  TestUnpin(resource_id, md5, base::PLATFORM_FILE_OK,
            GDataFile::CACHE_STATE_NONE,
            GDataRootDirectory::CACHE_TYPE_PINNED);
  EXPECT_EQ(1, num_callback_invocations_);

  // Unpin a file that doesn't exist in cache and is not pinned, i.e. cache
  // has zero knowledge of the file.
  resource_id = "not-in-cache:1a2b";
  // Because unpinning will fail, OnFileUnpinned() won't be run.
  EXPECT_CALL(*mock_sync_client_, OnFileUnpinned(resource_id, md5)).Times(0);

  num_callback_invocations_ = 0;
  TestUnpin(resource_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND,
            GDataFile::CACHE_STATE_NONE,
            GDataRootDirectory::CACHE_TYPE_PINNED /* non-applicable */);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataFileSystemTest, StoreToCachePinned) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnFilePinned(resource_id, md5)).Times(1);

  // Pin a non-existent file.
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataFile::CACHE_STATE_PINNED,
          GDataRootDirectory::CACHE_TYPE_PINNED);

  // Store an existing file to a previously pinned file.
  num_callback_invocations_ = 0;
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK,
                   GDataFile::CACHE_STATE_PRESENT |
                   GDataFile::CACHE_STATE_PINNED,
                   GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store a non-existent file to a previously pinned and stored file.
  num_callback_invocations_ = 0;
  TestStoreToCache(resource_id, md5, FilePath("./non_existent.json"),
                   base::PLATFORM_FILE_ERROR_NOT_FOUND,
                   GDataFile::CACHE_STATE_PRESENT |
                   GDataFile::CACHE_STATE_PINNED,
                   GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataFileSystemTest, GetFromCachePinned) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnFilePinned(resource_id, md5)).Times(1);

  // Pin a non-existent file.
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataFile::CACHE_STATE_PINNED,
          GDataRootDirectory::CACHE_TYPE_PINNED);

  // Get the non-existent pinned file from cache.
  num_callback_invocations_ = 0;
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND, md5);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store an existing file to the previously pinned non-existent file.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK,
                   GDataFile::CACHE_STATE_PRESENT |
                   GDataFile::CACHE_STATE_PINNED,
                   GDataRootDirectory::CACHE_TYPE_PERSISTENT);

  // Get the previously pinned and stored file from cache.
  num_callback_invocations_ = 0;
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, base::PLATFORM_FILE_OK, md5);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataFileSystemTest, RemoveFromCachePinned) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  // Use alphanumeric characters for resource_id.
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnFilePinned(resource_id, md5)).Times(1);

  // Store a file to cache, and pin it.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_PINNED,
          GDataRootDirectory::CACHE_TYPE_PERSISTENT);

  // Remove |resource_id| from cache.
  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // Repeat using non-alphanumeric characters for resource id, including '.'
  // which is an extension separator.
  resource_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";
  EXPECT_CALL(*mock_sync_client_, OnFilePinned(resource_id, md5)).Times(1);

  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_PINNED,
          GDataRootDirectory::CACHE_TYPE_PERSISTENT);

  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataFileSystemTest, DirtyCacheSimple) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // First store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);

  // Mark the file dirty.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_DIRTY,
                GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Commit the file dirty.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_DIRTY,
                GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Clear dirty state of the file.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataFile::CACHE_STATE_PRESENT,
                 GDataRootDirectory::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataFileSystemTest, DirtyCachePinned) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnFilePinned(resource_id, md5)).Times(1);

  // First store a file to cache and pin it.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_PINNED,
          GDataRootDirectory::CACHE_TYPE_PERSISTENT);

  // Mark the file dirty.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_DIRTY |
                GDataFile::CACHE_STATE_PINNED,
                GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Commit the file dirty.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_DIRTY |
                 GDataFile::CACHE_STATE_PINNED,
                 GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Clear dirty state of the file.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataFile::CACHE_STATE_PRESENT |
                 GDataFile::CACHE_STATE_PINNED,
                 GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataFileSystemTest, PinAndUnpinDirtyCache) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnFilePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_sync_client_, OnFileUnpinned(resource_id, md5)).Times(1);

  // First store a file to cache and mark it as dirty.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_DIRTY,
                GDataRootDirectory::CACHE_TYPE_PERSISTENT);

  // Verifies dirty file exists.
  FilePath dirty_path = GetCacheFilePath(
      resource_id,
      md5,
      GDataRootDirectory::CACHE_TYPE_PERSISTENT,
      GDataFileSystem::CACHED_FILE_LOCALLY_MODIFIED);
  EXPECT_TRUE(file_util::PathExists(dirty_path));

  // Pin the dirty file.
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_DIRTY |
          GDataFile::CACHE_STATE_PINNED,
          GDataRootDirectory::CACHE_TYPE_PERSISTENT);

  // Verify dirty file still exist at the same pathname.
  EXPECT_TRUE(file_util::PathExists(dirty_path));

  // Unpin the dirty file.
  TestUnpin(resource_id, md5, base::PLATFORM_FILE_OK,
            GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_DIRTY,
            GDataRootDirectory::CACHE_TYPE_PERSISTENT);

  // Verify dirty file still exist at the same pathname.
  EXPECT_TRUE(file_util::PathExists(dirty_path));
}

TEST_F(GDataFileSystemTest, DirtyCacheRepetitive) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // First store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);

  // Mark the file dirty.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_DIRTY,
                GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Again, mark the file dirty.  Nothing should change.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_DIRTY,
                GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Commit the file dirty.  Outgoing symlink should be created.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_DIRTY,
                GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Again, commit the file dirty.  Nothing should change.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_DIRTY,
                GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Mark the file dirty agian after it's being committed.  Outgoing symlink
  // should be deleted.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_DIRTY,
                GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Commit the file dirty.  Outgoing symlink should be created again.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_DIRTY,
                GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Clear dirty state of the file.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataFile::CACHE_STATE_PRESENT,
                 GDataRootDirectory::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Again, clear dirty state of the file, which is no longer dirty.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, base::PLATFORM_FILE_ERROR_INVALID_OPERATION,
                 GDataFile::CACHE_STATE_PRESENT,
                 GDataRootDirectory::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataFileSystemTest, DirtyCacheInvalid) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Mark a non-existent file dirty.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND,
                GDataFile::CACHE_STATE_NONE,
                GDataRootDirectory::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Commit a non-existent file dirty.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND,
                  GDataFile::CACHE_STATE_NONE,
                  GDataRootDirectory::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Clear dirty state of a non-existent file.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND,
                 GDataFile::CACHE_STATE_NONE,
                 GDataRootDirectory::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);

  // Commit a non-dirty existing file dirty.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_ERROR_INVALID_OPERATION,
                 GDataFile::CACHE_STATE_PRESENT,
                 GDataRootDirectory::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Clear dirty state of a non-dirty existing file.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, base::PLATFORM_FILE_ERROR_INVALID_OPERATION,
                 GDataFile::CACHE_STATE_PRESENT,
                 GDataRootDirectory::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Mark an existing file dirty, then store a new file to the same resource id
  // but different md5, which should fail.
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_DIRTY,
                GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  num_callback_invocations_ = 0;
  md5 = "new_md5";
  TestStoreToCache(resource_id, md5, GetTestFilePath("subdir_feed.json"),
                   base::PLATFORM_FILE_ERROR_IN_USE,
                   GDataFile::CACHE_STATE_PRESENT |
                   GDataFile::CACHE_STATE_DIRTY,
                   GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataFileSystemTest, RemoveFromDirtyCache) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_sync_client_, OnFilePinned(resource_id, md5)).Times(1);

  // Store a file to cache, pin it, mark it dirty and commit it.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, base::PLATFORM_FILE_OK,
          GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_PINNED,
          GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  TestMarkDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                GDataFile::CACHE_STATE_PRESENT | GDataFile::CACHE_STATE_PINNED |
                GDataFile::CACHE_STATE_DIRTY,
                GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  TestCommitDirty(resource_id, md5, base::PLATFORM_FILE_OK,
                 GDataFile::CACHE_STATE_PRESENT |
                 GDataFile::CACHE_STATE_PINNED |
                 GDataFile::CACHE_STATE_DIRTY,
                 GDataRootDirectory::CACHE_TYPE_PERSISTENT);

  // Try to remove the file.  Since file is dirty, it and the corresponding
  // pinned and outgoing symlinks should not be removed.
  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataFileSystemTest, GetCacheState) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

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
                     base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                     GDataRootDirectory::CACHE_TYPE_TMP);

    // Get its cache state.
    num_callback_invocations_ = 0;
    TestGetCacheState(resource_id, md5, base::PLATFORM_FILE_OK,
                      GDataFile::CACHE_STATE_PRESENT, file);
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

    EXPECT_CALL(*mock_sync_client_, OnFilePinned(resource_id, md5)).Times(1);

    // Store a file corresponding to |resource_id| and |md5| to cache, and pin
    // it.
    int expected_cache_state = GDataFile::CACHE_STATE_PRESENT |
                               GDataFile::CACHE_STATE_PINNED;
    TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                     base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                     GDataRootDirectory::CACHE_TYPE_TMP);
    TestPin(resource_id, md5, base::PLATFORM_FILE_OK, expected_cache_state,
            GDataRootDirectory::CACHE_TYPE_PERSISTENT);

    // Get its cache state.
    num_callback_invocations_ = 0;
    TestGetCacheState(resource_id, md5, base::PLATFORM_FILE_OK,
                      expected_cache_state, file);
    EXPECT_EQ(1, num_callback_invocations_);
  }

  {  // Test cache state of a non-existent file.
    num_callback_invocations_ = 0;
    TestGetCacheState("pdf:12345", "abcd", base::PLATFORM_FILE_ERROR_NOT_FOUND,
                      0, NULL);
    EXPECT_EQ(1, num_callback_invocations_);
  }
}

TEST_F(GDataFileSystemTest, InitializeCache) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  PrepareForInitCacheTest();
  TestInitializeCache();
}

TEST_F(GDataFileSystemTest, GetGDataEntryByPath) {
  LoadRootFeedDocument("root_feed.json");

  // Lock to call GetGDataEntryByPath.
  base::AutoLock lock(file_system_->lock_);
  GDataEntry* entry = file_system_->GetGDataEntryByPath(
      FilePath(FILE_PATH_LITERAL("drive/File 1.txt")));
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ("https://file1_link_self/file:2_file_resource_id",
            entry->edit_url().spec());
  EXPECT_EQ("https://file_content_url/", entry->content_url().spec());

  GDataEntry* non_existent = file_system_->GetGDataEntryByPath(
      FilePath(FILE_PATH_LITERAL("drive/Nonexistent.txt")));
  ASSERT_TRUE(non_existent == NULL);
}

// Create a directory through the document service
TEST_F(GDataFileSystemTest, CreateDirectoryWithService) {
  LoadRootFeedDocument("root_feed.json");
  EXPECT_CALL(*mock_doc_service_,
              CreateDirectory(_, "Sample Directory Title", _)).Times(1);

  // Set last error so it's not a valid error code.
  callback_helper_->last_error_ = static_cast<base::PlatformFileError>(1);
  file_system_->CreateDirectory(
      FilePath(FILE_PATH_LITERAL("drive/Sample Directory Title")),
      false,  // is_exclusive
      true,  // is_recursive
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get()));
  message_loop_.RunAllPending();
  // TODO(gspencer): Uncomment this when we get a blob that
  // works that can be returned from the mock.
  // EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);
}

TEST_F(GDataFileSystemTest, GetFileByPath_FromGData_EnoughSpace) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  GDataEntry* entry = FindEntry(file_in_root);
  GDataFile* file = entry->AsGDataFile();
  FilePath downloaded_file = GetCachePathForFile(file);
  const int64 file_size = entry->file_info().size;

  // Pretend we have enough space.
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(2).WillRepeatedly(Return(file_size + kMinFreeSpace));

  // The file is obtained with the mock DocumentsService.
  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url/"),
                           _, _))
      .Times(1);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetDownloadDataCallback());
  RunAllPendingForIO();  // Try to get from the cache.
  RunAllPendingForIO();  // Check if we have space before downloading.
  RunAllPendingForIO();  // Check if we have space after downloading.

  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);
  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_EQ(downloaded_file.value(),
            callback_helper_->download_path_.value());
}

TEST_F(GDataFileSystemTest, GetFileByPath_FromGData_NoSpaceAtAll) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  GDataEntry* entry = FindEntry(file_in_root);
  GDataFile* file = entry->AsGDataFile();
  FilePath downloaded_file = GetCachePathForFile(file);

  // Pretend we have no space at all.
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(2).WillRepeatedly(Return(0));

  // The file is not obtained with the mock DocumentsService, because of no
  // space.
  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url/"),
                           _, _))
      .Times(0);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetDownloadDataCallback());
  RunAllPendingForIO();  // Try to get from the cache.
  RunAllPendingForIO();  // Check if we have space before downloading.

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            callback_helper_->last_error_);
}

TEST_F(GDataFileSystemTest, GetFileByPath_FromGData_NoEnoughSpaceButCanFreeUp) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  GDataEntry* entry = FindEntry(file_in_root);
  GDataFile* file = entry->AsGDataFile();
  FilePath downloaded_file = GetCachePathForFile(file);
  const int64 file_size = entry->file_info().size;

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
                   base::PLATFORM_FILE_OK,
                   GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);
  ASSERT_TRUE(CacheEntryExists("<resource_id>", "<md5>"));
  ASSERT_TRUE(CacheFileExists("<resource_id>", "<md5>"));

  // The file is obtained with the mock DocumentsService, because of we freed
  // up the space.
  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url/"),
                           _, _))
      .Times(1);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetDownloadDataCallback());
  RunAllPendingForIO();  // Try to get from the cache.
  RunAllPendingForIO();  // Check if we have space before downloading.
  RunAllPendingForIO();  // Check if we have space after downloading

  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);
  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_EQ(downloaded_file.value(),
            callback_helper_->download_path_.value());

  // The file should be removed in order to free up space, and the cache
  // entry should also be removed.
  ASSERT_FALSE(CacheEntryExists("<resource_id>", "<md5>"));
  ASSERT_FALSE(CacheFileExists("<resource_id>", "<md5>"));
}

TEST_F(GDataFileSystemTest, GetFileByPath_FromGData_EnoughSpaceButBecomeFull) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  GDataEntry* entry = FindEntry(file_in_root);
  GDataFile* file = entry->AsGDataFile();
  FilePath downloaded_file = GetCachePathForFile(file);
  const int64 file_size = entry->file_info().size;

  // Pretend we have enough space first (checked before downloading a file),
  // but then start reporting we have not enough space. This is to emulate that
  // the disk space becomes full after the file is downloaded for some reason
  // (ex. the actual file was larger than the expected size).
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .WillOnce(Return(file_size + kMinFreeSpace))
      .WillOnce(Return(kMinFreeSpace - 1))
      .WillOnce(Return(kMinFreeSpace - 1));

  // The file is obtained with the mock DocumentsService.
  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url/"),
                           _, _))
      .Times(1);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetDownloadDataCallback());
  RunAllPendingForIO();  // Try to get from the cache.
  RunAllPendingForIO();  // Check if we have space before downloading.
  RunAllPendingForIO();  // Check if we have space after downloading.

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE,
            callback_helper_->last_error_);
}

TEST_F(GDataFileSystemTest, GetFileByPath_FromCache) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  GDataEntry* entry = FindEntry(file_in_root);
  GDataFile* file = entry->AsGDataFile();
  FilePath downloaded_file = GetCachePathForFile(file);

  // Store something as cached version of this file.
  TestStoreToCache(file->resource_id(),
                   file->file_md5(),
                   GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK,
                   GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);

  // Make sure we don't call downloads at all.
  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url/"),
                           _, _))
      .Times(0);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetDownloadDataCallback());
  RunAllPendingForIO();

  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_EQ(downloaded_file.value(),
            callback_helper_->download_path_.value());
}

TEST_F(GDataFileSystemTest, GetFileByPath_HostedDocument) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/Document 1.gdoc"));
  GDataEntry* entry = NULL;
  EXPECT_TRUE((entry = FindEntry(file_in_root)) != NULL);

  file_system_->GetFileByPath(file_in_root, callback,
                              GetDownloadDataCallback());
  RunAllPendingForIO();

  EXPECT_EQ(HOSTED_DOCUMENT, callback_helper_->file_type_);
  EXPECT_FALSE(callback_helper_->download_path_.empty());

  VerifyHostedDocumentJSONFile(entry->AsGDataFile(),
                               callback_helper_->download_path_);
}

TEST_F(GDataFileSystemTest, GetFileByResourceId) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  GDataEntry* entry = FindEntry(file_in_root);
  GDataFile* file = entry->AsGDataFile();
  FilePath downloaded_file = GetCachePathForFile(file);

  // The file is obtained with the mock DocumentsService, because it's not
  // stored in the cache.
  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url/"),
                           _, _))
      .Times(1);

  file_system_->GetFileByResourceId(file->resource_id(), callback,
                                    GetDownloadDataCallback());
  RunAllPendingForIO();  // Try to get from the cache.
  RunAllPendingForIO();  // Check if we have space before downloading.
  RunAllPendingForIO();  // Check if we have space after downloading.

  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_EQ(downloaded_file.value(),
            callback_helper_->download_path_.value());
}

TEST_F(GDataFileSystemTest, GetFileByResourceId_FromCache) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("drive/File 1.txt"));
  GDataEntry* entry = FindEntry(file_in_root);
  GDataFile* file = entry->AsGDataFile();
  FilePath downloaded_file = GetCachePathForFile(file);

  // Store something as cached version of this file.
  TestStoreToCache(file->resource_id(),
                   file->file_md5(),
                   GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK,
                   GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);

  // The file is obtained from the cache.
  // Make sure we don't call downloads at all.
  EXPECT_CALL(*mock_doc_service_, DownloadFile(_, _, _, _, _))
      .Times(0);

  file_system_->GetFileByResourceId(file->resource_id(), callback,
                                    GetDownloadDataCallback());
  RunAllPendingForIO();

  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_EQ(downloaded_file.value(),
            callback_helper_->download_path_.value());
}

TEST_F(GDataFileSystemTest, ContentSearch) {
  LoadRootFeedDocument("root_feed.json");

  EXPECT_CALL(*mock_doc_service_, GetDocuments(Eq(GURL()), _, "foo", _))
      .Times(1);

  ReadDirectoryCallback callback = base::Bind(
      &ContentSearchCallback,
      &message_loop_);

  file_system_->SearchAsync("foo", callback);
  message_loop_.Run();  // Wait to get our result

  const SearchResultPair kSearchResultPairs[] = {
    { "drive/.search/foo/file:2_file_resouce_id.SubDirectory File 1.txt",
      "drive/Directory 1/SubDirectory File 1.txt" },
    { "drive/.search/foo/folder:1_folder_resource_id.Directory 1",
      "drive/Directory 1" },
    { "drive/.search/foo/folder:1_folder_resource_id.Directory 1/"
      "SubDirectory File 1.txt",
      "drive/Directory 1/SubDirectory File 1.txt" }
  };

  for (size_t i = 0; i < arraysize(kSearchResultPairs); ++i) {
    FilePath search_file_path(FILE_PATH_LITERAL(
        kSearchResultPairs[i].search_path));
    FilePath real_file_path(FILE_PATH_LITERAL(
        kSearchResultPairs[i].real_path));

    GDataEntry* search_file_entry = FindEntry(search_file_path);
    ASSERT_TRUE(search_file_entry)
        << "Can't find " << search_file_path.value();

    GDataEntry* real_file_entry = FindEntry(real_file_path);
    ASSERT_TRUE(real_file_entry)
        << "Can't find " << real_file_path.value();

    EXPECT_EQ(search_file_entry, real_file_entry);
  }
}

TEST_F(GDataFileSystemTest, ContentSearch_Delete) {
  LoadRootFeedDocument("root_feed.json");

  EXPECT_CALL(*mock_doc_service_, DeleteDocument(_, _)).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);

  FilePath search_file_path(FILE_PATH_LITERAL(
      "drive/.search/foo/file:2_file_resouce_id.SubDirectory File 1.txt"));
  FilePath real_file_path(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"));

  ASSERT_TRUE(FindEntry(real_file_path));
  ASSERT_TRUE(FindEntry(search_file_path));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Remove(search_file_path, false, callback);
  message_loop_.RunAllPending();  // Wait to get our result

  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_FALSE(FindEntry(real_file_path));
  EXPECT_FALSE(FindEntry(search_file_path));
}

TEST_F(GDataFileSystemTest, ContentSearch_RenameResult) {
  LoadRootFeedDocument("root_feed.json");

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);

  EXPECT_CALL(*mock_doc_service_, RenameResource(
      Eq(GURL("https://dir1_file_link_self/file:2_file_resouce_id")),
         "SubDirectory File 1.txt.renamed", _))
      .Times(1);

  FilePath source_path_search(FILE_PATH_LITERAL(
      "drive/.search/foo/file:2_file_resouce_id.SubDirectory File 1.txt"));
  FilePath source_path_real(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"));

  FilePath renamed_path_search(FILE_PATH_LITERAL(
      "drive/.search/foo/"
      "file:2_file_resouce_id.SubDirectory File 1.txt.renamed"));
  FilePath renamed_path_real(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt.renamed"));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(source_path_search, renamed_path_search, callback);
  message_loop_.RunAllPending();

  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_FALSE(FindEntry(source_path_real));
  EXPECT_FALSE(FindEntry(source_path_search));

  EXPECT_TRUE(FindEntry(renamed_path_real));
  EXPECT_TRUE(FindEntry(renamed_path_search));
}

TEST_F(GDataFileSystemTest, ContentSearch_Move) {
  LoadRootFeedDocument("root_feed.json");

  // Setup directory observer mocks.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL(
             "drive/Directory 1/Sub Directory Folder")))))
      .Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  // Setup documents service mocks.
  EXPECT_CALL(*mock_doc_service_, RenameResource(
      Eq(GURL("https://dir1_file_link_self/file:2_file_resouce_id")),
         "SubDirectory File 1.txt.dest", _))
      .Times(1);
  EXPECT_CALL(*mock_doc_service_, RemoveResourceFromDirectory(
      Eq(GURL("https://1_folder_content_url/")),
      Eq(GURL("https://dir1_file_link_self/file:2_file_resouce_id")),
      "file:2_file_resouce_id", _))
      .Times(1);
  EXPECT_CALL(*mock_doc_service_, AddResourceToDirectory(
      Eq(GURL("https://1_folder_content_url/")),
      Eq(GURL("https://dir1_file_link_self/file:2_file_resouce_id")), _))
      .Times(1);

  // Start the test.
  FilePath source_path_search(FILE_PATH_LITERAL(
      "drive/.search/foo/folder:1_folder_resource_id.Directory 1/"
      "SubDirectory File 1.txt"));
  FilePath source_path_real(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"));

  FilePath destination_path_search(FILE_PATH_LITERAL(
      "drive/.search/foo/folder:1_folder_resource_id.Directory 1/"
      "Sub Directory Folder/SubDirectory File 1.txt.dest"));
  FilePath destination_path_real(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder/SubDirectory File 1.txt.dest"));

  ASSERT_TRUE(FindEntry(source_path_search));
  ASSERT_TRUE(FindEntry(source_path_real));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(source_path_search, destination_path_search, callback);
  message_loop_.RunAllPending();

  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_FALSE(FindEntry(source_path_search));
  EXPECT_FALSE(FindEntry(source_path_real));

  EXPECT_TRUE(FindEntry(destination_path_search));
  EXPECT_TRUE(FindEntry(destination_path_real));
}

// Move file to gdata file system from search result directory.
TEST_F(GDataFileSystemTest, ContentSearch_MoveToGData) {
  LoadRootFeedDocument("root_feed.json");

  // Setup directory observer mocks.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL(
                      "drive/Directory 1/Sub Directory Folder")))))
      .Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  // Setup documents service mocks.
  EXPECT_CALL(*mock_doc_service_, RenameResource(
      Eq(GURL("https://dir1_file_link_self/file:2_file_resouce_id")),
         "SubDirectory File 1.txt.dest", _))
      .Times(1);
  EXPECT_CALL(*mock_doc_service_, RemoveResourceFromDirectory(
      Eq(GURL("https://1_folder_content_url/")),
      Eq(GURL("https://dir1_file_link_self/file:2_file_resouce_id")),
      "file:2_file_resouce_id", _))
      .Times(1);
  EXPECT_CALL(*mock_doc_service_, AddResourceToDirectory(
      Eq(GURL("https://1_folder_content_url/")),
      Eq(GURL("https://dir1_file_link_self/file:2_file_resouce_id")), _))
      .Times(1);

  // Statr the test.
  FilePath source_path_search(FILE_PATH_LITERAL(
      "drive/.search/foo/folder:1_folder_resource_id.Directory 1/"
      "SubDirectory File 1.txt"));
  FilePath source_path_real(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"));

  FilePath destination_path_search(FILE_PATH_LITERAL(
      "drive/.search/foo/folder:1_folder_resource_id.Directory 1/"
      "Sub Directory Folder/SubDirectory File 1.txt.dest"));
  FilePath destination_path_real(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder/SubDirectory File 1.txt.dest"));

  ASSERT_TRUE(FindEntry(source_path_search));
  ASSERT_TRUE(FindEntry(source_path_real));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(source_path_search, destination_path_real, callback);
  message_loop_.RunAllPending();

  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_FALSE(FindEntry(source_path_search));
  EXPECT_FALSE(FindEntry(source_path_real));

  EXPECT_TRUE(FindEntry(destination_path_search));
  EXPECT_TRUE(FindEntry(destination_path_real));
}

TEST_F(GDataFileSystemTest, ContentSearch_MoveToSearch) {
  LoadRootFeedDocument("root_feed.json");

  // Setup directory observer mocks.
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL(
             "drive/Directory 1/Sub Directory Folder")))))
      .Times(1);
  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive"))))).Times(1);

  // Setup documents service mocks.
  EXPECT_CALL(*mock_doc_service_, RenameResource(
      Eq(GURL("https://dir1_file_link_self/file:2_file_resouce_id")),
         "SubDirectory File 1.txt.dest", _))
      .Times(1);
  EXPECT_CALL(*mock_doc_service_, RemoveResourceFromDirectory(
      Eq(GURL("https://1_folder_content_url/")),
      Eq(GURL("https://dir1_file_link_self/file:2_file_resouce_id")),
      "file:2_file_resouce_id", _))
      .Times(1);
  EXPECT_CALL(*mock_doc_service_, AddResourceToDirectory(
      Eq(GURL("https://1_folder_content_url/")),
      Eq(GURL("https://dir1_file_link_self/file:2_file_resouce_id")), _))
     .Times(1);

  // Statr the test.
  FilePath source_path_search(FILE_PATH_LITERAL(
      "drive/.search/foo/folder:1_folder_resource_id.Directory 1/"
      "SubDirectory File 1.txt"));
  FilePath source_path_real(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"));

  FilePath destination_path_search(FILE_PATH_LITERAL(
      "drive/.search/foo/folder:1_folder_resource_id.Directory 1/"
      "Sub Directory Folder/SubDirectory File 1.txt.dest"));
  FilePath destination_path_real(FILE_PATH_LITERAL(
      "drive/Directory 1/Sub Directory Folder/SubDirectory File 1.txt.dest"));

  ASSERT_TRUE(FindEntry(source_path_search));
  ASSERT_TRUE(FindEntry(source_path_real));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(source_path_real, destination_path_search, callback);
  message_loop_.RunAllPending();

  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_FALSE(FindEntry(source_path_search));
  EXPECT_FALSE(FindEntry(source_path_real));

  EXPECT_TRUE(FindEntry(destination_path_search));
  EXPECT_TRUE(FindEntry(destination_path_real));
}

TEST_F(GDataFileSystemTest, ContentSearch_CopyToSearchDirectory) {
  LoadRootFeedDocument("root_feed.json");

  FilePath src_file_path(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL(
      "drive/.search/Directory 1"));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Copy(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();

  EXPECT_NE(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_TRUE(FindEntry(src_file_path));
}

TEST_F(GDataFileSystemTest, ContentSearch_CopyToSearchResultDirectory) {
  LoadRootFeedDocument("root_feed.json");

  FilePath src_file_path(FILE_PATH_LITERAL(
      "drive/Directory 1/SubDirectory File 1.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL(
      "drive/.search/foo/Directory 1"));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Copy(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION,
            callback_helper_->last_error_);

  EXPECT_TRUE(FindEntry(src_file_path));
}

TEST_F(GDataFileSystemTest, ContentSearch_CreateDirInSearchResultDirectory) {
  LoadRootFeedDocument("root_feed.json");

  FilePath dest_file_path(FILE_PATH_LITERAL(
      "drive/.search/foo/New Folder"));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->CreateDirectory(dest_file_path, false, false, callback);
  message_loop_.RunAllPending();

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION,
            callback_helper_->last_error_);
}

TEST_F(GDataFileSystemTest, ContentSearch_CreateDirInSearchDirectory) {
  LoadRootFeedDocument("root_feed.json");

  FilePath dest_file_path(FILE_PATH_LITERAL(
      "drive/.search/New Folder"));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->CreateDirectory(dest_file_path, false, false, callback);
  message_loop_.RunAllPending();

  EXPECT_EQ(base::PLATFORM_FILE_ERROR_INVALID_OPERATION,
            callback_helper_->last_error_);
}

TEST_F(GDataFileSystemTest, ContentSearch_CreateDirectory) {
  LoadRootFeedDocument("root_feed.json");

  EXPECT_CALL(*mock_directory_observer_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("drive/Directory 1"))))).Times(1);
  EXPECT_CALL(*mock_doc_service_, CreateDirectory(
      Eq(GURL("https://1_folder_content_url/")), "FolderNew", _)).Times(1);

  FilePath search_dir_path(FILE_PATH_LITERAL(
         "drive/.search/foo/folder:1_folder_resource_id.Directory 1/"
         "FolderNew"));
  FilePath real_dir_path(FILE_PATH_LITERAL("drive/Directory 1/FolderNew"));

  EXPECT_FALSE(FindEntry(search_dir_path));
  EXPECT_FALSE(FindEntry(real_dir_path));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->CreateDirectory(search_dir_path, false, false, callback);
  message_loop_.RunAllPending();

  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_TRUE(FindEntry(search_dir_path));
  EXPECT_TRUE(FindEntry(real_dir_path));
}

TEST_F(GDataFileSystemTest, GetAvailableSpace) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  GetAvailableSpaceCallback callback =
      base::Bind(&CallbackHelper::GetAvailableSpaceCallback,
                 callback_helper_.get());

  EXPECT_CALL(*mock_doc_service_, GetAccountMetadata(_));

  file_system_->GetAvailableSpace(callback);
  message_loop_.RunAllPending();
  EXPECT_EQ(GG_LONGLONG(6789012345), callback_helper_->quota_bytes_used_);
  EXPECT_EQ(GG_LONGLONG(9876543210), callback_helper_->quota_bytes_total_);

  // Verify account meta feed is saved to cache.
  RunAllPendingForIO();
  FilePath path = file_system_->cache_paths_[
      GDataRootDirectory::CACHE_TYPE_META].Append(
          FILE_PATH_LITERAL("account_metadata.json"));
  EXPECT_TRUE(file_util::PathExists(path));
}

TEST_F(GDataFileSystemTest, MountUnmount) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  FilePath file_path;
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // First store a file to cache in the tmp subdir.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK, GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);

  // Mark the file mounted.
  num_callback_invocations_ = 0;
  file_path = file_system_->GetCacheFilePath(
      resource_id, md5,
      GDataRootDirectory::CACHE_TYPE_TMP,
      GDataFileSystemInterface::CACHED_FILE_FROM_SERVER);
  TestSetMountedState(resource_id, md5, file_path, true,
                      base::PLATFORM_FILE_OK,
                      GDataFile::CACHE_STATE_PRESENT |
                      GDataFile::CACHE_STATE_MOUNTED,
                      GDataRootDirectory::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);
  EXPECT_TRUE(CacheEntryExists(resource_id, md5));

  // Clear mounted state of the file.
  num_callback_invocations_ = 0;
  file_path = file_system_->GetCacheFilePath(
      resource_id,
      md5,
      GDataRootDirectory::CACHE_TYPE_PERSISTENT,
      GDataFileSystemInterface::CACHED_FILE_MOUNTED);
  TestSetMountedState(resource_id, md5, file_path, false,
                      base::PLATFORM_FILE_OK,
                      GDataFile::CACHE_STATE_PRESENT,
                      GDataRootDirectory::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);
  EXPECT_TRUE(CacheEntryExists(resource_id, md5));

  // Try to remove the file.
  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);
}

}   // namespace gdata
