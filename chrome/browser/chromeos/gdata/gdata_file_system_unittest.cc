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
#include "base/string16.h"
#include "base/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/browser/chromeos/gdata/mock_gdata_documents_service.h"
#include "chrome/browser/chromeos/gdata/mock_gdata_sync_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
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

namespace {

const char kSlash[] = "/";
const char kEscapedSlash[] = "\xE2\x88\x95";
const char kSymLinkToDevNull[] = "/dev/null";

struct InitialCacheResource {
  const char* source_file;  // Source file to be used for cache.
  const char* resource_id;  // Resource id of cache file.
  const char* md5;          // MD5 of cache file.
  int cache_state;          // Cache state of cache file.
} const initial_cache_resources[] = {
  // Cache resource in tmp dir, i.e. not pinned.
  { "root_feed.json", "file_1_resource", "file_1_md5",
    gdata::GDataFile::CACHE_STATE_PRESENT },
  // Cache resource in tmp dir, i.e. not pinned.
  { "subdir_feed.json", "file_2_resource", "file_2_md5",
    gdata::GDataFile::CACHE_STATE_PRESENT },
  // Cache resource with resource_id containing non-alphanumeric characters that
  // is pinned, to test resource_id is escaped and unescaped correctly, and
  // a pinned file is in persistent dir with a symlink in pinned dir
  // referencing it.
  { "directory_entry_atom.json", "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?",
    "abcdef0123456789",
    gdata::GDataFile::CACHE_STATE_PRESENT |
    gdata::GDataFile::CACHE_STATE_PINNED },
  // Cache resource with a non-existent source file that is pinned, to test that
  // a pinned file can reference a non-existent file.
  { "", "file:non-existent", "beef",
    gdata::GDataFile::CACHE_STATE_PINNED },
};

}  // anonymous namespace

namespace gdata {

class GDataFileSystemTest : public testing::Test {
 protected:
  GDataFileSystemTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO, &message_loop_),
        file_system_(NULL),
        mock_doc_service_(NULL),
        mock_sync_client_(NULL),
        num_callback_invocations_(0),
        expected_error_(base::PLATFORM_FILE_OK),
        expected_cache_state_(0),
        expected_sub_dir_type_(GDataRootDirectory::CACHE_TYPE_META),
        expected_file_(NULL),
        expect_outgoing_symlink_(false) {
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);

    callback_helper_ = new CallbackHelper;

    // Allocate and keep a pointer to the mock, and inject it into the
    // GDataFileSystem object, which will own the mock object.
    mock_doc_service_ = new MockDocumentsService;

    EXPECT_CALL(*mock_doc_service_, Initialize(profile_.get())).Times(1);

    ASSERT_FALSE(file_system_);
    file_system_ = new GDataFileSystem(profile_.get(),
                                       mock_doc_service_);
    file_system_->Initialize();

    mock_sync_client_.reset(new MockGDataSyncClient);
    file_system_->AddObserver(mock_sync_client_.get());

    RunAllPendingForCache();
  }

  virtual void TearDown() OVERRIDE {
    ASSERT_TRUE(file_system_);
    EXPECT_CALL(*mock_doc_service_, CancelAll()).Times(1);
    file_system_->ShutdownOnUIThread();
    delete file_system_;
    file_system_ = NULL;
  }

  // Loads test json file as root ("/gdata") element.
  void LoadRootFeedDocument(const std::string& filename) {
    std::string error;
    scoped_ptr<Value> document(LoadJSONFile(filename));
    ASSERT_TRUE(document.get());
    ASSERT_TRUE(document->GetType() == Value::TYPE_DICTIONARY);
    GURL unused;
    scoped_ptr<ListValue> feed_list(new ListValue());
    feed_list->Append(document.release());
    ASSERT_TRUE(UpdateContent(feed_list.get()));
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
              base::PLATFORM_FILE_OK);
  }

  // Updates the content of directory under |directory_path| with parsed feed
  // |value|.
  bool UpdateContent(ListValue* list) {
    GURL unused;
    return file_system_->UpdateDirectoryWithDocumentFeed(
        list, FROM_SERVER) == base::PLATFORM_FILE_OK;
  }

  bool RemoveFile(const FilePath& file_path) {
    return file_system_->RemoveFileFromFileSystem(file_path) ==
        base::PLATFORM_FILE_OK;
  }

  FilePath GetCachePathForFile(GDataFile* file) {
    return file_system_->GetCacheFilePath(
        file->resource_id(),
        file->file_md5(),
        GDataRootDirectory::CACHE_TYPE_TMP,
        GDataFileSystem::CACHED_FILE_FROM_SERVER);
  }

  GDataFileBase* FindFile(const FilePath& file_path) {
    ReadOnlyFindFileDelegate search_delegate;
    file_system_->FindFileByPathSync(file_path, &search_delegate);
    return search_delegate.file();
  }

  void FindAndTestFilePath(const FilePath& file_path) {
    GDataFileBase* file = FindFile(file_path);
    ASSERT_TRUE(file) << "File can't be found " << file_path.value();
    EXPECT_EQ(file->GetFilePath(), file_path);
  }

  GDataFileBase* FindFileByResource(const std::string& resource) {
    return file_system_->root_->GetFileByResource(resource);
  }

  FilePath GetCacheFilePath(
      const std::string& resource_id,
      const std::string& md5,
      GDataRootDirectory::CacheSubDirectoryType sub_dir_type,
      GDataFileSystem::CachedFileOrigin file_origin) {
    return file_system_->GetCacheFilePath(resource_id, md5, sub_dir_type,
                                          file_origin);
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
    std::string unescaped_md5 = GDataFileBase::UnescapeUtf8FileName(
        base_name.Extension().substr(1));
    EXPECT_EQ(md5, unescaped_md5);
    std::string unescaped_resource_id = GDataFileBase::UnescapeUtf8FileName(
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

    RunAllPendingForCache();
  }

  void TestGetFromCache(const std::string& resource_id,
                        const std::string& md5,
                        base::PlatformFileError expected_error) {
    expected_error_ = expected_error;

    file_system_->GetFromCache(resource_id, md5,
        base::Bind(&GDataFileSystemTest::VerifyGetFromCache,
                   base::Unretained(this)));

    RunAllPendingForCache();
  }

  void TestGetFromCacheForPath(const FilePath& gdata_file_path,
                               base::PlatformFileError expected_error) {
    expected_error_ = expected_error;

    file_system_->GetFromCacheForPath(gdata_file_path,
        base::Bind(&GDataFileSystemTest::VerifyGetFromCache,
                   base::Unretained(this)));

    RunAllPendingForCache();
  }

  void VerifyGetFromCache(base::PlatformFileError error,
                          const std::string& resource_id,
                          const std::string& md5,
                          const FilePath& gdata_file_path,
                          const FilePath& cache_file_path) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

    if (error == base::PLATFORM_FILE_OK) {
      // Verify filename of |cache_file_path|.
      FilePath base_name = cache_file_path.BaseName();
      EXPECT_EQ(GDataFileBase::EscapeUtf8FileName(resource_id) +
                FilePath::kExtensionSeparator +
                GDataFileBase::EscapeUtf8FileName(md5),
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

    RunAllPendingForCache();
  }

  void VerifyRemoveFromCache(base::PlatformFileError error,
                             const std::string& resource_id,
                             const std::string& md5) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

    // Verify cache map.
    EXPECT_TRUE(file_system_->root_->GetCacheEntry(resource_id, md5) == NULL);

    // Verify that no files with "<resource_id>.*" exists in persistent and tmp
    // dirs, and no "<resource_id>" exists in pinned dir.
    std::vector<FilePath> paths_to_check;
    paths_to_check.push_back(file_system_->GetCacheFilePath(resource_id, "*",
        GDataRootDirectory::CACHE_TYPE_PERSISTENT,
        GDataFileSystem::CACHED_FILE_FROM_SERVER));
    paths_to_check.push_back(file_system_->GetCacheFilePath(resource_id, "*",
        GDataRootDirectory::CACHE_TYPE_TMP,
        GDataFileSystem::CACHED_FILE_FROM_SERVER));
    paths_to_check.push_back(file_system_->GetCacheFilePath(resource_id, "",
        GDataRootDirectory::CACHE_TYPE_PINNED,
        GDataFileSystem::CACHED_FILE_FROM_SERVER));
    for (size_t i = 0; i < paths_to_check.size(); ++i) {
      file_util::FileEnumerator traversal(paths_to_check[i].DirName(), false,
                                          file_util::FileEnumerator::FILES,
                                          paths_to_check[i].BaseName().value());
      EXPECT_TRUE(traversal.Next().empty());
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

    RunAllPendingForCache();
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

    RunAllPendingForCache();
  }

  void TestGetCacheState(const std::string& resource_id, const std::string& md5,
                         base::PlatformFileError expected_error,
                         int expected_cache_state, GDataFile* expected_file) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_file_ = expected_file;

    {  // Lock to use GetCacheState, but release before flushing tasks because
       // OnGetCacheState callback will attempt to lock.
      base::AutoLock lock(file_system_->lock_);
      file_system_->GetCacheState(resource_id, md5,
          base::Bind(&GDataFileSystemTest::VerifyGetCacheState,
                     base::Unretained(this)));
    }

    RunAllPendingForCache();
  }

  void VerifyGetCacheState(base::PlatformFileError error, GDataFile* file,
                           int cache_state) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

    if (error == base::PLATFORM_FILE_OK) {
      EXPECT_EQ(expected_cache_state_, cache_state);
      EXPECT_EQ(expected_file_, file);
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

    RunAllPendingForCache();
  }

  void VerifyMarkDirty(base::PlatformFileError error,
                       const std::string& resource_id,
                       const std::string& md5,
                       const FilePath& gdata_file_path,
                       const FilePath& cache_file_path) {
    VerifyCacheFileState(error, resource_id, md5);

    // Verify filename of |cache_file_path|.
    if (error == base::PLATFORM_FILE_OK) {
      FilePath base_name = cache_file_path.BaseName();
      EXPECT_EQ(GDataFileBase::EscapeUtf8FileName(resource_id) +
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

    RunAllPendingForCache();
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

    RunAllPendingForCache();
  }

  void PrepareForInitCacheTest() {
    // Create gdata cache sub directories.
    ASSERT_TRUE(file_util::CreateDirectory(
        file_system_->cache_paths_[GDataRootDirectory::CACHE_TYPE_PERSISTENT]));
    ASSERT_TRUE(file_util::CreateDirectory(
        file_system_->cache_paths_[GDataRootDirectory::CACHE_TYPE_TMP]));
    ASSERT_TRUE(file_util::CreateDirectory(
        file_system_->cache_paths_[GDataRootDirectory::CACHE_TYPE_PINNED]));

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
          resource.cache_state & GDataFile::CACHE_STATE_PINNED ?
              GDataRootDirectory::CACHE_TYPE_PERSISTENT :
              GDataRootDirectory::CACHE_TYPE_TMP,
          GDataFileSystem::CACHED_FILE_FROM_SERVER);

      // Copy file from data dir to cache subdir, naming it per cache files
      // convention.
      if (resource.cache_state & GDataFile::CACHE_STATE_PRESENT) {
        FilePath source_path = GetTestFilePath(resource.source_file);
        ASSERT_TRUE(file_util::CopyFile(source_path, dest_path));
      }

      // Create symbolic link in pinned dir naming it per cache files
      // convention.
      if (resource.cache_state & GDataFile::CACHE_STATE_PINNED) {
        FilePath link_path = file_system_->GetCacheFilePath(
          resource.resource_id,
          "",
          GDataRootDirectory::CACHE_TYPE_PINNED,
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
      TestGetFromCache(resource.resource_id, resource.md5,
                       resource.cache_state & GDataFile::CACHE_STATE_PRESENT ?
                           base::PLATFORM_FILE_OK :
                           base::PLATFORM_FILE_ERROR_NOT_FOUND);
      EXPECT_EQ(1, num_callback_invocations_);

      // Verify cache state.
      std::string md5;
      if (resource.cache_state & GDataFile::CACHE_STATE_PRESENT)
         md5 = resource.md5;
      GDataRootDirectory::CacheEntry* entry =
          file_system_->root_->GetCacheEntry(resource.resource_id, md5);
      ASSERT_TRUE(entry != NULL);
      EXPECT_EQ(resource.cache_state, entry->cache_state);
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
    if (expected_cache_state_ & GDataFile::CACHE_STATE_PRESENT ||
        expected_cache_state_ & GDataFile::CACHE_STATE_PINNED) {
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
        expected_cache_state_ & GDataFile::CACHE_STATE_PINNED ||
            expected_cache_state_ & GDataFile::CACHE_STATE_DIRTY ?
            GDataRootDirectory::CACHE_TYPE_PERSISTENT :
            GDataRootDirectory::CACHE_TYPE_TMP,
        expected_cache_state_ & GDataFile::CACHE_STATE_DIRTY ?
            GDataFileSystem::CACHED_FILE_LOCALLY_MODIFIED :
            GDataFileSystem::CACHED_FILE_FROM_SERVER);
    bool exists = file_util::PathExists(dest_path);
    if (expected_cache_state_ & GDataFile::CACHE_STATE_PRESENT)
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
    if (expected_cache_state_ & GDataFile::CACHE_STATE_PINNED) {
      EXPECT_TRUE(exists);
      FilePath target_path;
      EXPECT_TRUE(file_util::ReadSymbolicLink(symlink_path, &target_path));
      if (expected_cache_state_ & GDataFile::CACHE_STATE_PRESENT)
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
    if (expected_cache_state_ & GDataFile::CACHE_STATE_DIRTY &&
        expect_outgoing_symlink_) {
      EXPECT_TRUE(exists);
      FilePath target_path;
      EXPECT_TRUE(file_util::ReadSymbolicLink(symlink_path, &target_path));
      EXPECT_TRUE(target_path.value() != kSymLinkToDevNull);
      if (expected_cache_state_ & GDataFile::CACHE_STATE_PRESENT)
        EXPECT_EQ(dest_path, target_path);
    } else {
      EXPECT_FALSE(exists);
    }
  }

  void RunAllPendingForCache() {
    // Let cache operations run on IO blocking pool.
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    // Let callbacks for cache operations run on UI thread.
    message_loop_.RunAllPending();
  }

  void TestLoadMetadataFromCache(const FilePath::StringType& feeds_path,
                                 const FilePath& meta_cache_path) {
    FilePath file_path = GetTestFilePath(feeds_path);
    // Move test file into the correct cache location first.
    FilePath cache_dir_path = profile_->GetPath();
    cache_dir_path = cache_dir_path.Append(meta_cache_path).DirName();
    ASSERT_TRUE(file_util::CreateDirectory(cache_dir_path));
    ASSERT_TRUE(file_util::CopyFile(file_path,
        cache_dir_path.Append(meta_cache_path.BaseName())));

    ReadOnlyFindFileDelegate delegate;
    file_system_->LoadRootFeedFromCache(
        FilePath(FILE_PATH_LITERAL("gdata")),
        false,     // load_from_server
        base::MessageLoopProxy::current(),
        base::Bind(&GDataFileSystemTest::OnExpectToFindFile,
                   FilePath(FILE_PATH_LITERAL("gdata"))));
    RunAllPendingForCache();
  }

  static void OnExpectToFindFile(const FilePath& search_file_path,
                                 base::PlatformFileError error,
                                 const FilePath& directory_path,
                                 GDataFileBase* file) {
    ASSERT_TRUE(file);
    if (file->file_info().is_directory)
      ASSERT_EQ(search_file_path, directory_path);
    else
      ASSERT_EQ(search_file_path, directory_path.Append(file->file_name()));
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

  // This is used as a helper for registering callbacks that need to be
  // RefCountedThreadSafe, and a place where we can fetch results from various
  // operations.
  class CallbackHelper
    : public base::RefCountedThreadSafe<CallbackHelper> {
   public:
    CallbackHelper()
        : last_error_(base::PLATFORM_FILE_OK),
          quota_bytes_total_(0),
          quota_bytes_used_(0) {}
    virtual ~CallbackHelper() {}
    virtual void GetFileCallback(base::PlatformFileError error,
                                 const FilePath& file_path,
                                 GDataFileType file_type) {
      last_error_ = error;
      download_path_ = file_path;
      file_type_ = file_type;
    }
    virtual void FileOperationCallback(base::PlatformFileError error) {
      DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

      last_error_ = error;
    }
    virtual void GetAvailableSpaceCallback(base::PlatformFileError error,
                                           int bytes_total,
                                           int bytes_used) {
      last_error_ = error;
      quota_bytes_total_ = bytes_total;
      quota_bytes_used_ = bytes_used;
    }

    base::PlatformFileError last_error_;
    FilePath download_path_;
    GDataFileType file_type_;
    int quota_bytes_total_;
    int quota_bytes_used_;
  };

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<CallbackHelper> callback_helper_;
  GDataFileSystem* file_system_;
  MockDocumentsService* mock_doc_service_;
  scoped_ptr<MockGDataSyncClient> mock_sync_client_;

  int num_callback_invocations_;
  base::PlatformFileError expected_error_;
  int expected_cache_state_;
  GDataRootDirectory::CacheSubDirectoryType expected_sub_dir_type_;
  GDataFile* expected_file_;
  bool expect_outgoing_symlink_;
};

// Delegate used to find a directory element for file system updates.
class MockFindFileDelegate : public gdata::FindFileDelegate {
 public:
  MockFindFileDelegate() {
  }

  virtual ~MockFindFileDelegate() {
  }

  // gdata::FindFileDelegate overrides.
  MOCK_METHOD3(OnDone, void(base::PlatformFileError,
                            const FilePath&,
                            GDataFileBase*));
};

TEST_F(GDataFileSystemTest, SearchRootDirectory) {
  MockFindFileDelegate mock_find_file_delegate;
  EXPECT_CALL(mock_find_file_delegate,
              OnDone(Eq(base::PLATFORM_FILE_OK),
                     Eq(FilePath(FILE_PATH_LITERAL("gdata"))),
                     NotNull()))
      .Times(1);

  file_system_->FindFileByPathSync(FilePath(FILE_PATH_LITERAL("gdata")),
                                   &mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchExistingFile) {
  LoadRootFeedDocument("root_feed.json");
  MockFindFileDelegate mock_find_file_delegate;
  EXPECT_CALL(mock_find_file_delegate,
              OnDone(Eq(base::PLATFORM_FILE_OK),
                     Eq(FilePath(FILE_PATH_LITERAL("gdata"))),
                     NotNull()))
      .Times(1);

  file_system_->FindFileByPathSync(
      FilePath(FILE_PATH_LITERAL("gdata/File 1.txt")),
      &mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchExistingDocument) {
  LoadRootFeedDocument("root_feed.json");
  MockFindFileDelegate mock_find_file_delegate;
  EXPECT_CALL(mock_find_file_delegate,
              OnDone(Eq(base::PLATFORM_FILE_OK),
                     Eq(FilePath(FILE_PATH_LITERAL("gdata"))),
                     NotNull()))
      .Times(1);

  file_system_->FindFileByPathSync(
      FilePath(FILE_PATH_LITERAL("gdata/Document 1.gdoc")),
      &mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchNonExistingFile) {
  LoadRootFeedDocument("root_feed.json");
  MockFindFileDelegate mock_find_file_delegate;
  EXPECT_CALL(mock_find_file_delegate,
              OnDone(Eq(base::PLATFORM_FILE_ERROR_NOT_FOUND),
                     Eq(FilePath()),
                     IsNull()))
      .Times(1);

  file_system_->FindFileByPathSync(
      FilePath(FILE_PATH_LITERAL("gdata/nonexisting.file")),
      &mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchEncodedFileNames) {
  LoadRootFeedDocument("root_feed.json");

  EXPECT_FALSE(FindFile(FilePath(FILE_PATH_LITERAL(
      "gdata/Slash / in file 1.txt"))));

  EXPECT_TRUE(FindFile(FilePath::FromUTF8Unsafe(
      "gdata/Slash \xE2\x88\x95 in file 1.txt")));

  EXPECT_TRUE(FindFile(FilePath::FromUTF8Unsafe(
      "gdata/Slash \xE2\x88\x95 in directory/SubDirectory File 1.txt")));
}

TEST_F(GDataFileSystemTest, SearchEncodedFileNamesLoadingRoot) {
  LoadRootFeedDocument("root_feed.json");

  EXPECT_FALSE(FindFile(FilePath(FILE_PATH_LITERAL(
      "gdata/Slash / in file 1.txt"))));

  EXPECT_TRUE(FindFile(FilePath::FromUTF8Unsafe(
      "gdata/Slash \xE2\x88\x95 in file 1.txt")));

  EXPECT_TRUE(FindFile(FilePath::FromUTF8Unsafe(
      "gdata/Slash \xE2\x88\x95 in directory/SubDirectory File 1.txt")));
}

TEST_F(GDataFileSystemTest, SearchDuplicateNames) {
  LoadRootFeedDocument("root_feed.json");
  MockFindFileDelegate mock_find_file_delegate;
  EXPECT_CALL(mock_find_file_delegate,
              OnDone(Eq(base::PLATFORM_FILE_OK),
                     Eq(FilePath(FILE_PATH_LITERAL("gdata"))),
                     NotNull()))
      .Times(1);

  file_system_->FindFileByPathSync(
      FilePath(FILE_PATH_LITERAL("gdata/Duplicate Name.txt")),
      &mock_find_file_delegate);

  MockFindFileDelegate mock_find_file_delegate2;
  EXPECT_CALL(mock_find_file_delegate2,
              OnDone(Eq(base::PLATFORM_FILE_OK),
                     Eq(FilePath(FILE_PATH_LITERAL("gdata"))),
                     NotNull()))
      .Times(1);

  file_system_->FindFileByPathSync(
      FilePath(FILE_PATH_LITERAL("gdata/Duplicate Name (2).txt")),
      &mock_find_file_delegate2);
}

TEST_F(GDataFileSystemTest, SearchExistingDirectory) {
  LoadRootFeedDocument("root_feed.json");
  MockFindFileDelegate mock_find_file_delegate;
  EXPECT_CALL(mock_find_file_delegate,
              OnDone(Eq(base::PLATFORM_FILE_OK),
                     Eq(FilePath(FILE_PATH_LITERAL("gdata/Directory 1"))),
                     NotNull()))
      .Times(1);

  file_system_->FindFileByPathSync(
      FilePath(FILE_PATH_LITERAL("gdata/Directory 1")),
      &mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchInSubdir) {
  LoadRootFeedDocument("root_feed.json");
  MockFindFileDelegate mock_find_file_delegate;
  EXPECT_CALL(mock_find_file_delegate,
              OnDone(Eq(base::PLATFORM_FILE_OK),
                     Eq(FilePath(FILE_PATH_LITERAL("gdata/Directory 1"))),
                     NotNull()))
      .Times(1);

  file_system_->FindFileByPathSync(
      FilePath(FILE_PATH_LITERAL("gdata/Directory 1/SubDirectory File 1.txt")),
      &mock_find_file_delegate);
}

// Check the reconstruction of the directory structure from only the root feed.
TEST_F(GDataFileSystemTest, SearchInSubSubdir) {
  LoadRootFeedDocument("root_feed.json");
  MockFindFileDelegate mock_find_file_delegate;
  EXPECT_CALL(mock_find_file_delegate,
        OnDone(Eq(base::PLATFORM_FILE_OK),
               Eq(FilePath(
                  FILE_PATH_LITERAL("gdata/Directory 1/Sub Directory Folder/"
                                    "Sub Sub Directory Folder"))),
               NotNull()))
      .Times(1);

  file_system_->FindFileByPathSync(
      FilePath(FILE_PATH_LITERAL("gdata/Directory 1/Sub Directory Folder/"
                                 "Sub Sub Directory Folder")),
      &mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, FilePathTests) {
  LoadRootFeedDocument("root_feed.json");

  FindAndTestFilePath(FilePath(FILE_PATH_LITERAL("gdata/File 1.txt")));
  FindAndTestFilePath(FilePath(FILE_PATH_LITERAL("gdata/Directory 1")));
  FindAndTestFilePath(
      FilePath(FILE_PATH_LITERAL("gdata/Directory 1/SubDirectory File 1.txt")));
}

TEST_F(GDataFileSystemTest, CachedFeedLoading) {
  TestLoadMetadataFromCache(FILE_PATH_LITERAL("cached_feeds.json"),
      FilePath(FILE_PATH_LITERAL("GCache/v1/meta/last_feed.json")));

  // Test first feed elements.
  FindAndTestFilePath(FilePath(FILE_PATH_LITERAL("gdata/Feed 1 File.txt")));
  FindAndTestFilePath(
      FilePath(FILE_PATH_LITERAL(
          "gdata/Directory 1/Feed 1 SubDirectory File.txt")));

  // Test second feed elements.
  FindAndTestFilePath(FilePath(FILE_PATH_LITERAL("gdata/Feed 2 File.txt")));
  FindAndTestFilePath(
      FilePath(FILE_PATH_LITERAL(
          "gdata/Directory 1/Sub Directory Folder/Feed 2 Directory")));
}

TEST_F(GDataFileSystemTest, CopyNotExistingFile) {
  FilePath src_file_path(FILE_PATH_LITERAL("gdata/Dummy file.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) == NULL);

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Copy(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_TRUE(FindFile(src_file_path) == NULL);
  EXPECT_TRUE(FindFile(dest_file_path) == NULL);
}

TEST_F(GDataFileSystemTest, CopyFileToNonExistingDirectory) {
  FilePath src_file_path(FILE_PATH_LITERAL("gdata/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("gdata/Dummy"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/Dummy/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_FALSE(src_file->self_url().is_empty());

  EXPECT_TRUE(FindFile(dest_parent_path) == NULL);

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_EQ(src_file, FindFile(src_file_path));
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));

  EXPECT_TRUE(FindFile(dest_parent_path) == NULL);
  EXPECT_TRUE(FindFile(dest_file_path) == NULL);
}

// Test the case where the parent of |dest_file_path| is a existing file,
// not a directory.
TEST_F(GDataFileSystemTest, CopyFileToInvalidPath) {
  FilePath src_file_path(FILE_PATH_LITERAL("gdata/Document 1.gdoc"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("gdata/Duplicate Name.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL(
      "gdata/Duplicate Name.txt/Document 1.gdoc"));

  LoadRootFeedDocument("root_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_FALSE(src_file->self_url().is_empty());

  GDataFileBase* dest_parent = NULL;
  EXPECT_TRUE((dest_parent = FindFile(dest_parent_path)) != NULL);
  EXPECT_TRUE(dest_parent->AsGDataFile() != NULL);

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Copy(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY,
            callback_helper_->last_error_);

  EXPECT_EQ(src_file, FindFile(src_file_path));
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_EQ(dest_parent, FindFile(dest_parent_path));

  EXPECT_TRUE(FindFile(dest_file_path) == NULL);
}

TEST_F(GDataFileSystemTest, RenameFile) {
  FilePath src_file_path(
      FILE_PATH_LITERAL("gdata/Directory 1/SubDirectory File 1.txt"));
  FilePath src_parent_path(FILE_PATH_LITERAL("gdata/Directory 1"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/Directory 1/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_resource));

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(src_file->self_url(),
                             FILE_PATH_LITERAL("Test.log"), _));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  EXPECT_CALL(*mock_sync_client_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("gdata/Directory 1"))))).Times(1);

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_TRUE(FindFile(src_file_path) == NULL);

  GDataFileBase* dest_file = NULL;
  EXPECT_TRUE((dest_file = FindFile(dest_file_path)) != NULL);
  EXPECT_EQ(dest_file, FindFileByResource(src_file_resource));
  EXPECT_EQ(src_file, dest_file);
}

TEST_F(GDataFileSystemTest, MoveFileFromRootToSubDirectory) {
  FilePath src_file_path(FILE_PATH_LITERAL("gdata/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("gdata/Directory 1"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/Directory 1/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_FALSE(src_file->self_url().is_empty());

  GDataFileBase* dest_parent = NULL;
  EXPECT_TRUE((dest_parent = FindFile(dest_parent_path)) != NULL);
  EXPECT_TRUE(dest_parent->AsGDataDirectory() != NULL);
  EXPECT_FALSE(dest_parent->content_url().is_empty());

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(src_file->self_url(),
                             FILE_PATH_LITERAL("Test.log"), _));
  EXPECT_CALL(*mock_doc_service_,
              AddResourceToDirectory(dest_parent->content_url(),
                                     src_file->self_url(), _));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  // Expect notification for both source and destination directories.
  EXPECT_CALL(*mock_sync_client_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("gdata"))))).Times(1);
  EXPECT_CALL(*mock_sync_client_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("gdata/Directory 1"))))).Times(1);

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_TRUE(FindFile(src_file_path) == NULL);

  GDataFileBase* dest_file = NULL;
  EXPECT_TRUE((dest_file = FindFile(dest_file_path)) != NULL);
  EXPECT_EQ(dest_file, FindFileByResource(src_file_path_resource));
  EXPECT_EQ(src_file, dest_file);
}

TEST_F(GDataFileSystemTest, MoveFileFromSubDirectoryToRoot) {
  FilePath src_parent_path(FILE_PATH_LITERAL("gdata/Directory 1"));
  FilePath src_file_path(
      FILE_PATH_LITERAL("gdata/Directory 1/SubDirectory File 1.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_FALSE(src_file->self_url().is_empty());

  GDataFileBase* src_parent = NULL;
  EXPECT_TRUE((src_parent = FindFile(src_parent_path)) != NULL);
  EXPECT_TRUE(src_parent->AsGDataDirectory() != NULL);
  EXPECT_FALSE(src_parent->content_url().is_empty());

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(src_file->self_url(),
                             FILE_PATH_LITERAL("Test.log"), _));
  EXPECT_CALL(*mock_doc_service_,
              RemoveResourceFromDirectory(src_parent->content_url(),
                                          src_file->self_url(),
                                          src_file_path_resource, _));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  // Expect notification for both source and destination directories.
  EXPECT_CALL(*mock_sync_client_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("gdata"))))).Times(1);
  EXPECT_CALL(*mock_sync_client_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("gdata/Directory 1"))))).Times(1);

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_TRUE(FindFile(src_file_path) == NULL);

  GDataFileBase* dest_file = NULL;
  EXPECT_TRUE((dest_file = FindFile(dest_file_path)) != NULL);
  EXPECT_EQ(dest_file, FindFileByResource(src_file_path_resource));
  EXPECT_EQ(src_file, dest_file);
}

TEST_F(GDataFileSystemTest, MoveFileBetweenSubDirectories) {
  FilePath src_parent_path(FILE_PATH_LITERAL("gdata/Directory 1"));
  FilePath src_file_path(
      FILE_PATH_LITERAL("gdata/Directory 1/SubDirectory File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("gdata/New Folder 1"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/New Folder 1/Test.log"));
  FilePath interim_file_path(FILE_PATH_LITERAL("gdata/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  EXPECT_CALL(*mock_sync_client_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("gdata"))))).Times(1);

  AddDirectoryFromFile(dest_parent_path, "directory_entry_atom.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_FALSE(src_file->self_url().is_empty());

  GDataFileBase* src_parent = NULL;
  EXPECT_TRUE((src_parent = FindFile(src_parent_path)) != NULL);
  EXPECT_TRUE(src_parent->AsGDataDirectory() != NULL);
  EXPECT_FALSE(src_parent->content_url().is_empty());

  GDataFileBase* dest_parent = NULL;
  ASSERT_TRUE((dest_parent = FindFile(dest_parent_path)) != NULL);
  EXPECT_TRUE(dest_parent->AsGDataDirectory() != NULL);
  EXPECT_FALSE(dest_parent->content_url().is_empty());

  EXPECT_TRUE(FindFile(interim_file_path) == NULL);

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(src_file->self_url(),
                             FILE_PATH_LITERAL("Test.log"), _));
  EXPECT_CALL(*mock_doc_service_,
              RemoveResourceFromDirectory(src_parent->content_url(),
                                          src_file->self_url(),
                                          src_file_path_resource, _));
  EXPECT_CALL(*mock_doc_service_,
              AddResourceToDirectory(dest_parent->content_url(),
                                     src_file->self_url(), _));

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  // Expect notification for both source and destination directories plus
  // interim file path.
  EXPECT_CALL(*mock_sync_client_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("gdata/Directory 1"))))).Times(1);
  EXPECT_CALL(*mock_sync_client_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("gdata"))))).Times(1);
  EXPECT_CALL(*mock_sync_client_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("gdata/New Folder 1"))))).Times(1);

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_TRUE(FindFile(src_file_path) == NULL);
  EXPECT_TRUE(FindFile(interim_file_path) == NULL);

  GDataFileBase* dest_file = NULL;
  EXPECT_TRUE((dest_file = FindFile(dest_file_path)) != NULL);
  EXPECT_EQ(dest_file, FindFileByResource(src_file_path_resource));
  EXPECT_EQ(src_file, dest_file);
}

TEST_F(GDataFileSystemTest, MoveNotExistingFile) {
  FilePath src_file_path(FILE_PATH_LITERAL("gdata/Dummy file.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) == NULL);

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_TRUE(FindFile(src_file_path) == NULL);
  EXPECT_TRUE(FindFile(dest_file_path) == NULL);
}

TEST_F(GDataFileSystemTest, MoveFileToNonExistingDirectory) {
  FilePath src_file_path(FILE_PATH_LITERAL("gdata/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("gdata/Dummy"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/Dummy/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_FALSE(src_file->self_url().is_empty());

  EXPECT_TRUE(FindFile(dest_parent_path) == NULL);

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_EQ(src_file, FindFile(src_file_path));
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));

  EXPECT_TRUE(FindFile(dest_parent_path) == NULL);
  EXPECT_TRUE(FindFile(dest_file_path) == NULL);
}

// Test the case where the parent of |dest_file_path| is a existing file,
// not a directory.
TEST_F(GDataFileSystemTest, MoveFileToInvalidPath) {
  FilePath src_file_path(FILE_PATH_LITERAL("gdata/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("gdata/Duplicate Name.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL(
      "gdata/Duplicate Name.txt/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_FALSE(src_file->self_url().is_empty());

  GDataFileBase* dest_parent = NULL;
  EXPECT_TRUE((dest_parent = FindFile(dest_parent_path)) != NULL);
  EXPECT_TRUE(dest_parent->AsGDataFile() != NULL);

  FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY,
            callback_helper_->last_error_);

  EXPECT_EQ(src_file, FindFile(src_file_path));
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_EQ(dest_parent, FindFile(dest_parent_path));

  EXPECT_TRUE(FindFile(dest_file_path) == NULL);
}

TEST_F(GDataFileSystemTest, RemoveFiles) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  LoadRootFeedDocument("root_feed.json");

  FilePath nonexisting_file(FILE_PATH_LITERAL("gdata/Dummy file.txt"));
  FilePath file_in_root(FILE_PATH_LITERAL("gdata/File 1.txt"));
  FilePath dir_in_root(FILE_PATH_LITERAL("gdata/Directory 1"));
  FilePath file_in_subdir(
      FILE_PATH_LITERAL("gdata/Directory 1/SubDirectory File 1.txt"));

  GDataFileBase* file = NULL;
  EXPECT_TRUE((file = FindFile(file_in_root)) != NULL);
  EXPECT_TRUE(file->AsGDataFile() != NULL);
  std::string file_in_root_resource = file->AsGDataFile()->resource_id();
  EXPECT_EQ(file, FindFileByResource(file_in_root_resource));

  EXPECT_TRUE(FindFile(dir_in_root) != NULL);

  EXPECT_TRUE((file = FindFile(file_in_subdir)) != NULL);
  EXPECT_TRUE(file->AsGDataFile() != NULL);
  std::string file_in_subdir_resource = file->AsGDataFile()->resource_id();
  EXPECT_EQ(file, FindFileByResource(file_in_subdir_resource));

  // Once for file in root and once for file...
  EXPECT_CALL(*mock_sync_client_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("gdata"))))).Times(2);

  // Remove first file in root.
  EXPECT_TRUE(RemoveFile(file_in_root));
  EXPECT_TRUE(FindFile(file_in_root) == NULL);
  EXPECT_EQ(NULL, FindFileByResource(file_in_root_resource));
  EXPECT_TRUE(FindFile(dir_in_root) != NULL);
  EXPECT_TRUE((file = FindFile(file_in_subdir)) != NULL);
  EXPECT_EQ(file, FindFileByResource(file_in_subdir_resource));

  // Remove directory.
  EXPECT_TRUE(RemoveFile(dir_in_root));
  EXPECT_TRUE(FindFile(file_in_root) == NULL);
  EXPECT_EQ(NULL, FindFileByResource(file_in_root_resource));
  EXPECT_TRUE(FindFile(dir_in_root) == NULL);
  EXPECT_TRUE(FindFile(file_in_subdir) == NULL);
  EXPECT_EQ(NULL, FindFileByResource(file_in_subdir_resource));

  // Try removing file in already removed subdirectory.
  EXPECT_FALSE(RemoveFile(file_in_subdir));

  // Try removing non-existing file.
  EXPECT_FALSE(RemoveFile(nonexisting_file));

  // Try removing root file element.
  EXPECT_FALSE(RemoveFile(FilePath(FILE_PATH_LITERAL("gdata"))));

  // Need this to ensure OnDirectoryChanged() is run.
  RunAllPendingForCache();
}

TEST_F(GDataFileSystemTest, CreateDirectory) {
  LoadRootFeedDocument("root_feed.json");

  EXPECT_CALL(*mock_sync_client_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("gdata"))))).Times(1);

  // Create directory in root.
  FilePath dir_path(FILE_PATH_LITERAL("gdata/New Folder 1"));
  EXPECT_TRUE(FindFile(dir_path) == NULL);
  AddDirectoryFromFile(dir_path, "directory_entry_atom.json");
  EXPECT_TRUE(FindFile(dir_path) != NULL);

  EXPECT_CALL(*mock_sync_client_, OnDirectoryChanged(
      Eq(FilePath(FILE_PATH_LITERAL("gdata/New Folder 1"))))).Times(1);

  // Create directory in a sub dirrectory.
  FilePath subdir_path(FILE_PATH_LITERAL("gdata/New Folder 1/New Folder 2"));
  EXPECT_TRUE(FindFile(subdir_path) == NULL);
  AddDirectoryFromFile(subdir_path, "directory_entry_atom.json");
  EXPECT_TRUE(FindFile(subdir_path) != NULL);
}

TEST_F(GDataFileSystemTest, FindFirstMissingParentDirectory) {
  LoadRootFeedDocument("root_feed.json");

  GURL last_dir_content_url;
  FilePath first_missing_parent_path;

  // Create directory in root.
  FilePath dir_path(FILE_PATH_LITERAL("gdata/New Folder 1"));
  EXPECT_EQ(
      GDataFileSystem::FOUND_MISSING,
      file_system_->FindFirstMissingParentDirectory(dir_path,
          &last_dir_content_url,
          &first_missing_parent_path));
  EXPECT_EQ(FilePath(FILE_PATH_LITERAL("gdata/New Folder 1")),
            first_missing_parent_path);
  EXPECT_TRUE(last_dir_content_url.is_empty());    // root directory.

  // Missing folders in subdir of an existing folder.
  FilePath dir_path2(FILE_PATH_LITERAL("gdata/Directory 1/New Folder 2"));
  EXPECT_EQ(
      GDataFileSystem::FOUND_MISSING,
      file_system_->FindFirstMissingParentDirectory(dir_path2,
          &last_dir_content_url,
          &first_missing_parent_path));
  EXPECT_EQ(FilePath(FILE_PATH_LITERAL("gdata/Directory 1/New Folder 2")),
            first_missing_parent_path);
  EXPECT_FALSE(last_dir_content_url.is_empty());    // non-root directory.

  // Missing two folders on the path.
  FilePath dir_path3 = dir_path2.Append(FILE_PATH_LITERAL("Another Folder"));
  EXPECT_EQ(
      GDataFileSystem::FOUND_MISSING,
      file_system_->FindFirstMissingParentDirectory(dir_path3,
          &last_dir_content_url,
          &first_missing_parent_path));
  EXPECT_EQ(FilePath(FILE_PATH_LITERAL("gdata/Directory 1/New Folder 2")),
            first_missing_parent_path);
  EXPECT_FALSE(last_dir_content_url.is_empty());    // non-root directory.

  // Folders on top of an existing file.
  EXPECT_EQ(
      GDataFileSystem::FOUND_INVALID,
      file_system_->FindFirstMissingParentDirectory(
          FilePath(FILE_PATH_LITERAL("gdata/File 1.txt/BadDir")),
          &last_dir_content_url,
          &first_missing_parent_path));

  // Existing folder.
  EXPECT_EQ(
      GDataFileSystem::DIRECTORY_ALREADY_PRESENT,
      file_system_->FindFirstMissingParentDirectory(
          FilePath(FILE_PATH_LITERAL("gdata/Directory 1")),
          &last_dir_content_url,
          &first_missing_parent_path));
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
  std::string escaped_resource_id;
  ReplaceChars(resource_id, kSlash, std::string(kEscapedSlash),
               &escaped_resource_id);
  std::string escaped_md5;
  ReplaceChars(md5, kSlash, std::string(kEscapedSlash), &escaped_md5);
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

  // Store a non-existent file to the same |resource_id| and |md5}.
  num_callback_invocations_ = 0;
  TestStoreToCache(resource_id, md5, FilePath("./non_existent.json"),
                   base::PLATFORM_FILE_ERROR_NOT_FOUND,
                   GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);
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
  TestGetFromCache(resource_id, md5, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // Get file from cache with same resource id as existing file but different
  // md5.
  num_callback_invocations_ = 0;
  TestGetFromCache(resource_id, "9999", base::PLATFORM_FILE_ERROR_NOT_FOUND);
  EXPECT_EQ(1, num_callback_invocations_);

  // Get file from cache with different resource id from existing file but same
  // md5.
  num_callback_invocations_ = 0;
  resource_id = "document:1a2b";
  TestGetFromCache(resource_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND);
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
  EXPECT_CALL(*mock_sync_client_, OnFileUnpinned(resource_id, md5)).Times(1);

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
  TestGetFromCache(resource_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store an existing file to the previously pinned non-existent file.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK,
                   GDataFile::CACHE_STATE_PRESENT |
                   GDataFile::CACHE_STATE_PINNED,
                   GDataRootDirectory::CACHE_TYPE_PERSISTENT);

  // Get the previously pinned and stored file from cache.
  num_callback_invocations_ = 0;
  TestGetFromCache(resource_id, md5, base::PLATFORM_FILE_OK);
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
}

TEST_F(GDataFileSystemTest, GetCacheState) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  // Populate gdata file system.
  LoadRootFeedDocument("root_feed.json");

  {  // Test cache state of an existing normal file.
    // Retrieve resource id and md5 of a file from file system.
    FilePath file_path(FILE_PATH_LITERAL("gdata/File 1.txt"));
    GDataFileBase* file_base = FindFile(file_path);
    ASSERT_TRUE(file_base != NULL);
    GDataFile* file = file_base->AsGDataFile();
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
        FILE_PATH_LITERAL("gdata/Directory 1/SubDirectory File 1.txt"));
    GDataFileBase* file_base = FindFile(file_path);
    ASSERT_TRUE(file_base != NULL);
    GDataFile* file = file_base->AsGDataFile();
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

// TODO(satorux): Write a test for GetFile() once DocumentsService is
// mockable.

TEST_F(GDataFileSystemTest, GetGDataFileInfoFromPath) {
  LoadRootFeedDocument("root_feed.json");

  // Lock to call GetGDataFileInfoFromPath.
  base::AutoLock lock(file_system_->lock_);
  GDataFileBase* file_info = file_system_->GetGDataFileInfoFromPath(
      FilePath(FILE_PATH_LITERAL("gdata/File 1.txt")));
  ASSERT_TRUE(file_info != NULL);
  EXPECT_EQ("https://file1_link_self/", file_info->self_url().spec());
  EXPECT_EQ("https://file_content_url/", file_info->content_url().spec());

  GDataFileBase* non_existent = file_system_->GetGDataFileInfoFromPath(
      FilePath(FILE_PATH_LITERAL("gdata/Nonexistent.txt")));
  ASSERT_TRUE(non_existent == NULL);
}

TEST_F(GDataFileSystemTest, GetFromCacheForPath) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  LoadRootFeedDocument("root_feed.json");

  // First make sure the file exists in GData.
  FilePath gdata_file_path = FilePath(FILE_PATH_LITERAL("gdata/File 1.txt"));
  GDataFile* file = NULL;
  {  // Lock to call GetGDataFileInfoFromPath.
    base::AutoLock lock(file_system_->lock_);
    GDataFileBase* file_info =
        file_system_->GetGDataFileInfoFromPath(gdata_file_path);
    ASSERT_TRUE(file_info != NULL);
    file = file_info->AsGDataFile();
    ASSERT_TRUE(file != NULL);
  }

  // A file that exists in GData but not in cache.
  num_callback_invocations_ = 0;
  TestGetFromCacheForPath(gdata_file_path, base::PLATFORM_FILE_ERROR_NOT_FOUND);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store a file corresponding to resource and md5 of "gdata/File 1.txt" to
  // cache.
  num_callback_invocations_ = 0;
  TestStoreToCache(file->resource_id(), file->file_md5(),
                   GetTestFilePath("root_feed.json"), base::PLATFORM_FILE_OK,
                   GDataFile::CACHE_STATE_PRESENT,
                   GDataRootDirectory::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Now the file should exist in cache.
  num_callback_invocations_ = 0;
  TestGetFromCacheForPath(gdata_file_path, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // A file that doesn't exist in gdata.
  num_callback_invocations_ = 0;
  TestGetFromCacheForPath(FilePath(FILE_PATH_LITERAL("gdata/Nonexistent.txt")),
                          base::PLATFORM_FILE_ERROR_NOT_FOUND);
  EXPECT_EQ(1, num_callback_invocations_);
}

// Create a directory through the document service
TEST_F(GDataFileSystemTest, CreateDirectoryWithService) {
  LoadRootFeedDocument("root_feed.json");
  EXPECT_CALL(*mock_doc_service_,
              CreateDirectory(_, "Sample Directory Title", _)).Times(1);

  // Set last error so it's not a valid error code.
  callback_helper_->last_error_ = static_cast<base::PlatformFileError>(1);
  file_system_->CreateDirectory(
      FilePath(FILE_PATH_LITERAL("gdata/Sample Directory Title")),
      false,  // is_exclusive
      true,  // is_recursive
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get()));
  message_loop_.RunAllPending();  // Wait to get our result.
  // TODO(gspencer): Uncomment this when we get a blob that
  // works that can be returned from the mock.
  // EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);
}

TEST_F(GDataFileSystemTest, GetFileFromDownloads) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("gdata/File 1.txt"));
  GDataFileBase* file_base = FindFile(file_in_root);
  GDataFile* file = file_base->AsGDataFile();
  FilePath downloaded_file = GetCachePathForFile(file);

  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(file_in_root,
                           downloaded_file,
                           GURL("https://file_content_url/"),
                           _))
      .Times(1);

  file_system_->GetFile(file_in_root, callback);
  RunAllPendingForCache();

  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_STREQ(downloaded_file.value().c_str(),
               callback_helper_->download_path_.value().c_str());
}

TEST_F(GDataFileSystemTest, GetFileFromCache) {
  EXPECT_CALL(*mock_sync_client_, OnCacheInitialized()).Times(1);

  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("gdata/File 1.txt"));
  GDataFileBase* file_base = FindFile(file_in_root);
  GDataFile* file = file_base->AsGDataFile();
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
                           _))
      .Times(0);

  file_system_->GetFile(file_in_root, callback);
  RunAllPendingForCache();

  EXPECT_EQ(REGULAR_FILE, callback_helper_->file_type_);
  EXPECT_STREQ(downloaded_file.value().c_str(),
               callback_helper_->download_path_.value().c_str());
}

TEST_F(GDataFileSystemTest, GetFileForHostedDocument) {
  LoadRootFeedDocument("root_feed.json");

  GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  FilePath file_in_root(FILE_PATH_LITERAL("gdata/Document 1.gdoc"));
  GDataFileBase* file = NULL;
  EXPECT_TRUE((file = FindFile(file_in_root)) != NULL);

  file_system_->GetFile(file_in_root, callback);
  RunAllPendingForCache();  // Wait to get our result.

  EXPECT_EQ(HOSTED_DOCUMENT, callback_helper_->file_type_);
  EXPECT_FALSE(callback_helper_->download_path_.empty());

  std::string error;
  JSONFileValueSerializer serializer(callback_helper_->download_path_);
  scoped_ptr<Value> value(serializer.Deserialize(NULL, &error));
  ASSERT_TRUE(value.get()) << "Parse error "
                           << callback_helper_->download_path_.value()
                           << ": " << error;
  DictionaryValue* dict_value = NULL;
  ASSERT_TRUE(value->GetAsDictionary(&dict_value));

  std::string edit_url, resource_id;
  EXPECT_TRUE(dict_value->GetString("url", &edit_url));
  EXPECT_TRUE(dict_value->GetString("resource_id", &resource_id));

  EXPECT_EQ(file->AsGDataFile()->edit_url().spec(), edit_url);
  EXPECT_EQ(file->resource_id(), resource_id);
}

TEST_F(GDataFileSystemTest, GetAvailableSpace) {
  GetAvailableSpaceCallback callback =
      base::Bind(&CallbackHelper::GetAvailableSpaceCallback,
                 callback_helper_.get());

  EXPECT_CALL(*mock_doc_service_, GetAccountMetadata(_));

  file_system_->GetAvailableSpace(callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(1234, callback_helper_->quota_bytes_used_);
  EXPECT_EQ(12345, callback_helper_->quota_bytes_total_);

  // Verify account meta feed is saved to cache.
  RunAllPendingForCache();  // Flush all cache operations.
  FilePath path = file_system_->cache_paths_[
      GDataRootDirectory::CACHE_TYPE_META].Append(
          FILE_PATH_LITERAL("account_metadata.json"));
  EXPECT_TRUE(file_util::PathExists(path));
}

}   // namespace gdata
