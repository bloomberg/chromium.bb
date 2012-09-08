// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_cache.h"
#include "chrome/browser/chromeos/gdata/drive_file_system.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/chromeos/gdata/drive_test_util.h"
#include "chrome/browser/chromeos/gdata/mock_drive_cache_observer.h"
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
  DriveCache::CacheSubDirectoryType expected_sub_dir_type;
} const initial_cache_resources[] = {
  // Cache resource in tmp dir, i.e. not pinned or dirty.
  { "root_feed.json", "tmp:resource_id", "md5_tmp_alphanumeric",
    test_util::TEST_CACHE_STATE_PRESENT,
    "md5_tmp_alphanumeric", DriveCache::CACHE_TYPE_TMP },
  // Cache resource in tmp dir, i.e. not pinned or dirty, with resource_id
  // containing non-alphanumeric characters, to test resource_id is escaped and
  // unescaped correctly.
  { "subdir_feed.json", "tmp:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?",
    "md5_tmp_non_alphanumeric",
    test_util::TEST_CACHE_STATE_PRESENT,
    "md5_tmp_non_alphanumeric", DriveCache::CACHE_TYPE_TMP },
  // Cache resource that is pinned, to test a pinned file is in persistent dir
  // with a symlink in pinned dir referencing it.
  { "directory_entry_atom.json", "pinned:existing", "md5_pinned_existing",
    test_util::TEST_CACHE_STATE_PRESENT |
    test_util::TEST_CACHE_STATE_PINNED |
    test_util::TEST_CACHE_STATE_PERSISTENT,
    "md5_pinned_existing", DriveCache::CACHE_TYPE_PERSISTENT },
  // Cache resource with a non-existent source file that is pinned, to test that
  // a pinned file can reference a non-existent file.
  { "", "pinned:non-existent", "md5_pinned_non_existent",
    test_util::TEST_CACHE_STATE_PINNED,
    "md5_pinned_non_existent", DriveCache::CACHE_TYPE_TMP },
  // Cache resource that is dirty, to test a dirty file is in persistent dir
  // with a symlink in outgoing dir referencing it.
  { "account_metadata.json", "dirty:existing", "md5_dirty_existing",
    test_util::TEST_CACHE_STATE_PRESENT |
    test_util::TEST_CACHE_STATE_DIRTY |
    test_util::TEST_CACHE_STATE_PERSISTENT,
    "local", DriveCache::CACHE_TYPE_PERSISTENT },
  // Cache resource that is pinned and dirty, to test a dirty pinned file is in
  // persistent dir with symlink in pinned and outgoing dirs referencing it.
  { "basic_feed.json", "dirty_and_pinned:existing",
    "md5_dirty_and_pinned_existing",
    test_util::TEST_CACHE_STATE_PRESENT |
    test_util::TEST_CACHE_STATE_PINNED |
    test_util::TEST_CACHE_STATE_DIRTY |
    test_util::TEST_CACHE_STATE_PERSISTENT,
    "local", DriveCache::CACHE_TYPE_PERSISTENT },
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

class MockFreeDiskSpaceGetter : public FreeDiskSpaceGetterInterface {
 public:
  virtual ~MockFreeDiskSpaceGetter() {}
  MOCK_CONST_METHOD0(AmountOfFreeDiskSpace, int64());
};

// Copies results from GetResourceIdsOfBacklogCallback.
void OnGetResourceIdsOfBacklog(std::vector<std::string>* out_to_fetch,
                               std::vector<std::string>* out_to_upload,
                               const std::vector<std::string>& to_fetch,
                               const std::vector<std::string>& to_upload) {
  *out_to_fetch = to_fetch;
  *out_to_upload = to_upload;
}

// Copies results from GetResourceIdsCallback.
void OnGetResourceIds(std::vector<std::string>* out_resource_ids,
                      const std::vector<std::string>& resource_ids) {
  *out_resource_ids = resource_ids;
}

// Copies results from ClearAllOnUIThread.
void OnClearAll(DriveFileError* out_error,
                FilePath* out_file_path,
                DriveFileError error,
                const FilePath& file_path) {
  *out_file_path = file_path;
  *out_error = error;
}

}  // namespace

class DriveCacheTest : public testing::Test {
 protected:
  DriveCacheTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO),
        cache_(NULL),
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

    mock_free_disk_space_checker_ = new MockFreeDiskSpaceGetter;
    SetFreeDiskSpaceGetterForTesting(mock_free_disk_space_checker_);

    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());
    cache_ = DriveCache::CreateDriveCacheOnUIThread(
        DriveCache::GetCacheRootPath(profile_.get()), blocking_task_runner_);

    mock_cache_observer_.reset(new StrictMock<MockDriveCacheObserver>);
    cache_->AddObserver(mock_cache_observer_.get());

    cache_->RequestInitializeOnUIThread();
    test_util::RunBlockingPoolTask();
  }

  virtual void TearDown() OVERRIDE {
    SetFreeDiskSpaceGetterForTesting(NULL);
    cache_->DestroyOnUIThread();
    // The cache destruction requires to post a task to the blocking pool.
    test_util::RunBlockingPoolTask();

    profile_.reset(NULL);
  }

  void PrepareForInitCacheTest() {
    DVLOG(1) << "PrepareForInitCacheTest start";
    // Create drive cache sub directories.
    ASSERT_TRUE(file_util::CreateDirectory(
        cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_PERSISTENT)));
    ASSERT_TRUE(file_util::CreateDirectory(
        cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_TMP)));
    ASSERT_TRUE(file_util::CreateDirectory(
        cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_PINNED)));
    ASSERT_TRUE(file_util::CreateDirectory(
        cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_OUTGOING)));

    // Dump some files into cache dirs so that
    // DriveFileSystem::InitializeCacheOnBlockingPool would scan through them
    // and populate cache map accordingly.

    // Copy files from data dir to cache dir to act as cached files.
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(initial_cache_resources); ++i) {
      const struct InitialCacheResource& resource = initial_cache_resources[i];
      // Determine drive cache file absolute path according to cache state.
      FilePath dest_path = cache_->GetCacheFilePath(
          resource.resource_id,
          resource.md5,
          test_util::ToCacheEntry(resource.cache_state).is_pinned() ||
          test_util::ToCacheEntry(resource.cache_state).is_dirty() ?
                  DriveCache::CACHE_TYPE_PERSISTENT :
                  DriveCache::CACHE_TYPE_TMP,
          test_util::ToCacheEntry(resource.cache_state).is_dirty() ?
              DriveCache::CACHED_FILE_LOCALLY_MODIFIED :
              DriveCache::CACHED_FILE_FROM_SERVER);

      // Copy file from data dir to cache subdir, naming it per cache files
      // convention.
      if (test_util::ToCacheEntry(resource.cache_state).is_present()) {
        FilePath source_path = GetTestFilePath(resource.source_file);
        ASSERT_TRUE(file_util::CopyFile(source_path, dest_path));
      } else {
        dest_path = FilePath(FILE_PATH_LITERAL(kSymLinkToDevNull));
      }

      // Create symbolic link in pinned dir, naming it per cache files
      // convention.
      if (test_util::ToCacheEntry(resource.cache_state).is_pinned()) {
        FilePath link_path = cache_->GetCacheFilePath(
            resource.resource_id,
            "",
            DriveCache::CACHE_TYPE_PINNED,
            DriveCache::CACHED_FILE_FROM_SERVER);
        ASSERT_TRUE(file_util::CreateSymbolicLink(dest_path, link_path));
      }

      // Create symbolic link in outgoing dir, naming it per cache files
      // convention.
      if (test_util::ToCacheEntry(resource.cache_state).is_dirty()) {
        FilePath link_path = cache_->GetCacheFilePath(
            resource.resource_id,
            "",
            DriveCache::CACHE_TYPE_OUTGOING,
            DriveCache::CACHED_FILE_FROM_SERVER);
        ASSERT_TRUE(file_util::CreateSymbolicLink(dest_path, link_path));
      }
    }

    DVLOG(1) << "PrepareForInitCacheTest finished";
    cache_->ForceRescanOnUIThreadForTesting();
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
          test_util::ToCacheEntry(resource.cache_state).is_present() ?
          DRIVE_FILE_OK :
          DRIVE_FILE_ERROR_NOT_FOUND,
          resource.expected_file_extension);
      EXPECT_EQ(1, num_callback_invocations_);

      // Verify cache state.
      std::string md5;
      if (test_util::ToCacheEntry(resource.cache_state).is_present())
         md5 = resource.md5;
      DriveCacheEntry cache_entry;
      ASSERT_TRUE(GetCacheEntryFromOriginThread(
          resource.resource_id, md5, &cache_entry));
      EXPECT_TRUE(test_util::CacheStatesEqual(
          test_util::ToCacheEntry(resource.cache_state),
          cache_entry));
      EXPECT_EQ(resource.expected_sub_dir_type,
                DriveCache::GetSubDirectoryType(cache_entry));
    }
  }

  void TestGetFileFromCacheByResourceIdAndMd5(
      const std::string& resource_id,
      const std::string& md5,
      DriveFileError expected_error,
      const std::string& expected_file_extension) {
    expected_error_ = expected_error;
    expected_file_extension_ = expected_file_extension;

    cache_->GetFileOnUIThread(
        resource_id,
        md5,
        base::Bind(&DriveCacheTest::VerifyGetFromCache,
                   base::Unretained(this),
                   resource_id,
                   md5));

    test_util::RunBlockingPoolTask();
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
        base::Bind(&DriveCacheTest::VerifyCacheFileState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void VerifyGetFromCache(const std::string& resource_id,
                          const std::string& md5,
                          DriveFileError error,
                          const FilePath& cache_file_path) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

    if (error == DRIVE_FILE_OK) {
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
                           DriveFileError expected_error) {
    expected_error_ = expected_error;

    cache_->RemoveOnUIThread(
        resource_id,
        base::Bind(&DriveCacheTest::VerifyRemoveFromCache,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void VerifyRemoveFromCache(DriveFileError error,
                             const std::string& resource_id,
                             const std::string& md5) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

    // Verify cache map.
    DriveCacheEntry cache_entry;
    const bool cache_entry_found =
        GetCacheEntryFromOriginThread(resource_id, md5, &cache_entry);
    if (cache_entry_found)
      EXPECT_TRUE(cache_entry.is_dirty());

    // If entry doesn't exist, verify that:
    // - no files with "<resource_id>.* exists in persistent and tmp dirs
    // - no "<resource_id>" symlink exists in pinned and outgoing dirs.
    std::vector<PathToVerify> paths_to_verify;
    paths_to_verify.push_back(  // Index 0: CACHE_TYPE_TMP.
        PathToVerify(cache_->GetCacheFilePath(resource_id, "*",
                     DriveCache::CACHE_TYPE_TMP,
                     DriveCache::CACHED_FILE_FROM_SERVER), FilePath()));
    paths_to_verify.push_back(  // Index 1: CACHE_TYPE_PERSISTENT.
        PathToVerify(cache_->GetCacheFilePath(resource_id, "*",
                     DriveCache::CACHE_TYPE_PERSISTENT,
                     DriveCache::CACHED_FILE_FROM_SERVER), FilePath()));
    paths_to_verify.push_back(  // Index 2: CACHE_TYPE_TMP, but STATE_PINNED.
        PathToVerify(cache_->GetCacheFilePath(resource_id, "",
                     DriveCache::CACHE_TYPE_PINNED,
                     DriveCache::CACHED_FILE_FROM_SERVER), FilePath()));
    paths_to_verify.push_back(  // Index 3: CACHE_TYPE_OUTGOING.
        PathToVerify(cache_->GetCacheFilePath(resource_id, "",
                     DriveCache::CACHE_TYPE_OUTGOING,
                     DriveCache::CACHED_FILE_FROM_SERVER), FilePath()));
    if (!cache_entry_found) {
      for (size_t i = 0; i < paths_to_verify.size(); ++i) {
        file_util::FileEnumerator enumerator(
            paths_to_verify[i].path_to_scan.DirName(), false /* not recursive*/,
            file_util::FileEnumerator::FILES |
            file_util::FileEnumerator::SHOW_SYM_LINKS,
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
                           DriveCache::CACHE_TYPE_PERSISTENT,
                           DriveCache::CACHED_FILE_LOCALLY_MODIFIED);

      // Change expected_existing_path of CACHE_TYPE_OUTGOING (index 3).
      paths_to_verify[3].expected_existing_path =
          GetCacheFilePath(resource_id,
                           std::string(),
                           DriveCache::CACHE_TYPE_OUTGOING,
                           DriveCache::CACHED_FILE_FROM_SERVER);

      if (cache_entry.is_pinned()) {
         // Change expected_existing_path of CACHE_TYPE_TMP but STATE_PINNED
         // (index 2).
         paths_to_verify[2].expected_existing_path =
             GetCacheFilePath(resource_id,
                              std::string(),
                              DriveCache::CACHE_TYPE_PINNED,
                              DriveCache::CACHED_FILE_FROM_SERVER);
      }

      for (size_t i = 0; i < paths_to_verify.size(); ++i) {
        const struct PathToVerify& verify = paths_to_verify[i];
        file_util::FileEnumerator enumerator(
            verify.path_to_scan.DirName(), false /* not recursive */,
            file_util::FileEnumerator::FILES |
            file_util::FileEnumerator::SHOW_SYM_LINKS,
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
      DriveFileError expected_error,
      int expected_cache_state,
      DriveCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    cache_->PinOnUIThread(
        resource_id, md5,
        base::Bind(&DriveCacheTest::VerifyCacheFileState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void TestUnpin(
      const std::string& resource_id,
      const std::string& md5,
      DriveFileError expected_error,
      int expected_cache_state,
      DriveCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    cache_->UnpinOnUIThread(
        resource_id, md5,
        base::Bind(&DriveCacheTest::VerifyCacheFileState,
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
        base::Bind(&DriveCacheTest::VerifyMarkDirty,
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
        base::Bind(&DriveCacheTest::VerifyCacheFileState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void TestClearDirty(
      const std::string& resource_id,
      const std::string& md5,
      DriveFileError expected_error,
      int expected_cache_state,
      DriveCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = false;

    cache_->ClearDirtyOnUIThread(resource_id, md5,
        base::Bind(&DriveCacheTest::VerifyCacheFileState,
                   base::Unretained(this)));

    test_util::RunBlockingPoolTask();
  }

  void TestSetMountedState(
      const std::string& resource_id,
      const std::string& md5,
      const FilePath& file_path,
      bool to_mount,
      DriveFileError expected_error,
      int expected_cache_state,
      DriveCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = false;

    cache_->SetMountedStateOnUIThread(file_path, to_mount,
        base::Bind(&DriveCacheTest::VerifySetMountedState,
                   base::Unretained(this), resource_id, md5, to_mount));

    test_util::RunBlockingPoolTask();
  }

  void VerifySetMountedState(const std::string& resource_id,
                             const std::string& md5,
                             bool to_mount,
                             DriveFileError error,
                             const FilePath& file_path) {
    ++num_callback_invocations_;
    EXPECT_TRUE(file_util::PathExists(file_path));
    EXPECT_TRUE(file_path == cache_->GetCacheFilePath(
        resource_id,
        md5,
        expected_sub_dir_type_,
        to_mount ?
            DriveCache::CACHED_FILE_MOUNTED :
            DriveCache::CACHED_FILE_FROM_SERVER));
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
        DriveCache::CACHE_TYPE_OUTGOING,
        DriveCache::CACHED_FILE_FROM_SERVER);
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

  FilePath GetCacheFilePath(const std::string& resource_id,
                            const std::string& md5,
                            DriveCache::CacheSubDirectoryType sub_dir_type,
                            DriveCache::CachedFileOrigin file_origin) {
    return cache_->GetCacheFilePath(resource_id, md5, sub_dir_type,
                                    file_origin);
  }

  // Helper function to call GetCacheEntry from origin thread.
  bool GetCacheEntryFromOriginThread(const std::string& resource_id,
                                     const std::string& md5,
                                     DriveCacheEntry* cache_entry) {
    bool result = false;
    blocking_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DriveCacheTest::GetCacheEntryFromOriginThreadInternal,
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

  void TestGetCacheFilePath(const std::string& resource_id,
                            const std::string& md5,
                            const std::string& expected_filename) {
    FilePath actual_path = cache_->GetCacheFilePath(
        resource_id,
        md5,
        DriveCache::CACHE_TYPE_TMP,
        DriveCache::CACHED_FILE_FROM_SERVER);
    FilePath expected_path =
        cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_TMP);
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

  // Returns the number of the cache files with name <resource_id>, and Confirm
  // that they have the <md5>. This should return 1 or 0.
  size_t CountCacheFiles(const std::string& resource_id,
                         const std::string& md5) {
    FilePath path = GetCacheFilePath(
        resource_id, "*",
        (test_util::ToCacheEntry(expected_cache_state_).is_pinned() ?
         DriveCache::CACHE_TYPE_PERSISTENT :
         DriveCache::CACHE_TYPE_TMP),
        DriveCache::CACHED_FILE_FROM_SERVER);
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
    return num_files_found;
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
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<TestingProfile> profile_;
  DriveCache* cache_;
  MockFreeDiskSpaceGetter* mock_free_disk_space_checker_;
  scoped_ptr<StrictMock<MockDriveCacheObserver> > mock_cache_observer_;

  int num_callback_invocations_;
  DriveFileError expected_error_;
  int expected_cache_state_;
  DriveCache::CacheSubDirectoryType expected_sub_dir_type_;
  bool expected_success_;
  bool expect_outgoing_symlink_;
  std::string expected_file_extension_;
  int root_feed_changestamp_;
};

TEST_F(DriveCacheTest, InitializeCache) {
  PrepareForInitCacheTest();
  TestInitializeCache();
}

TEST_F(DriveCacheTest, GetCacheFilePath) {
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

TEST_F(DriveCacheTest, StoreToCacheSimple) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Store an existing file.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store a non-existent file to the same |resource_id| and |md5|.
  num_callback_invocations_ = 0;
  TestStoreToCache(resource_id, md5, FilePath("./non_existent.json"),
                   DRIVE_FILE_ERROR_FAILED,
                   test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store a different existing file to the same |resource_id| but different
  // |md5|.
  md5 = "new_md5";
  num_callback_invocations_ = 0;
  TestStoreToCache(resource_id, md5, GetTestFilePath("subdir_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Verify that there's only one file with name <resource_id>, i.e. previously
  // cached file with the different md5 should be deleted.
  EXPECT_EQ(1U, CountCacheFiles(resource_id, md5));
}

TEST_F(DriveCacheTest, GetFromCacheSimple) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  // First store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);

  // Then try to get the existing file from cache.
  num_callback_invocations_ = 0;
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, DRIVE_FILE_OK, md5);
  EXPECT_EQ(1, num_callback_invocations_);

  // Get file from cache with same resource id as existing file but different
  // md5.
  num_callback_invocations_ = 0;
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, "9999", DRIVE_FILE_ERROR_NOT_FOUND, md5);
  EXPECT_EQ(1, num_callback_invocations_);

  // Get file from cache with different resource id from existing file but same
  // md5.
  num_callback_invocations_ = 0;
  resource_id = "document:1a2b";
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, DRIVE_FILE_ERROR_NOT_FOUND, md5);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(DriveCacheTest, RemoveFromCacheSimple) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  // Use alphanumeric characters for resource id.
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  // First store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);

  // Then try to remove existing file from cache.
  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, DRIVE_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // Repeat using non-alphanumeric characters for resource id, including '.'
  // which is an extension separator.
  resource_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);

  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, DRIVE_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(DriveCacheTest, PinAndUnpin) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(2);
  EXPECT_CALL(*mock_cache_observer_, OnCacheUnpinned(resource_id, md5))
      .Times(1);

  // First store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);

  // Pin the existing file in cache.
  num_callback_invocations_ = 0;
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Unpin the existing file in cache.
  num_callback_invocations_ = 0;
  TestUnpin(resource_id, md5, DRIVE_FILE_OK,
            test_util::TEST_CACHE_STATE_PRESENT,
            DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Pin back the same existing file in cache.
  num_callback_invocations_ = 0;
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Pin a non-existent file in cache.
  resource_id = "document:1a2b";
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_cache_observer_, OnCacheUnpinned(resource_id, md5))
      .Times(1);

  num_callback_invocations_ = 0;
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PINNED,
          DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Unpin the previously pinned non-existent file in cache.
  num_callback_invocations_ = 0;
  TestUnpin(resource_id, md5, DRIVE_FILE_OK,
            test_util::TEST_CACHE_STATE_NONE,
            DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Unpin a file that doesn't exist in cache and is not pinned, i.e. cache
  // has zero knowledge of the file.
  resource_id = "not-in-cache:1a2b";
  // Because unpinning will fail, OnCacheUnpinned() won't be run.
  EXPECT_CALL(*mock_cache_observer_, OnCacheUnpinned(resource_id, md5))
      .Times(0);

  num_callback_invocations_ = 0;
  TestUnpin(resource_id, md5, DRIVE_FILE_ERROR_NOT_FOUND,
            test_util::TEST_CACHE_STATE_NONE,
            DriveCache::CACHE_TYPE_TMP /* non-applicable */);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(DriveCacheTest, StoreToCachePinned) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);

  // Pin a non-existent file.
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PINNED,
          DriveCache::CACHE_TYPE_TMP);

  // Store an existing file to a previously pinned file.
  num_callback_invocations_ = 0;
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK,
                   test_util::TEST_CACHE_STATE_PRESENT |
                   test_util::TEST_CACHE_STATE_PINNED |
                   test_util::TEST_CACHE_STATE_PERSISTENT,
                   DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store a non-existent file to a previously pinned and stored file.
  num_callback_invocations_ = 0;
  TestStoreToCache(resource_id, md5, FilePath("./non_existent.json"),
                   DRIVE_FILE_ERROR_FAILED,
                   test_util::TEST_CACHE_STATE_PRESENT |
                   test_util::TEST_CACHE_STATE_PINNED |
                   test_util::TEST_CACHE_STATE_PERSISTENT,
                   DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(DriveCacheTest, GetFromCachePinned) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);

  // Pin a non-existent file.
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PINNED,
          DriveCache::CACHE_TYPE_TMP);

  // Get the non-existent pinned file from cache.
  num_callback_invocations_ = 0;
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, DRIVE_FILE_ERROR_NOT_FOUND, md5);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store an existing file to the previously pinned non-existent file.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK,
                   test_util::TEST_CACHE_STATE_PRESENT |
                   test_util::TEST_CACHE_STATE_PINNED |
                   test_util::TEST_CACHE_STATE_PERSISTENT,
                   DriveCache::CACHE_TYPE_PERSISTENT);

  // Get the previously pinned and stored file from cache.
  num_callback_invocations_ = 0;
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, DRIVE_FILE_OK, md5);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(DriveCacheTest, RemoveFromCachePinned) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  // Use alphanumeric characters for resource_id.
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);

  // Store a file to cache, and pin it.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          DriveCache::CACHE_TYPE_PERSISTENT);

  // Remove |resource_id| from cache.
  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, DRIVE_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // Repeat using non-alphanumeric characters for resource id, including '.'
  // which is an extension separator.
  resource_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);

  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          DriveCache::CACHE_TYPE_PERSISTENT);

  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, DRIVE_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(DriveCacheTest, DirtyCacheSimple) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(resource_id)).Times(1);

  // First store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);

  // Mark the file dirty.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Commit the file dirty.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, DRIVE_FILE_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Clear dirty state of the file.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, DRIVE_FILE_OK,
                 test_util::TEST_CACHE_STATE_PRESENT,
                 DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(DriveCacheTest, DirtyCachePinned) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(resource_id)).Times(1);

  // First store a file to cache and pin it.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          DriveCache::CACHE_TYPE_PERSISTENT);

  // Mark the file dirty.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PINNED |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Commit the file dirty.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, DRIVE_FILE_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PINNED |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Clear dirty state of the file.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, DRIVE_FILE_OK,
                 test_util::TEST_CACHE_STATE_PRESENT |
                 test_util::TEST_CACHE_STATE_PINNED |
                 test_util::TEST_CACHE_STATE_PERSISTENT,
                 DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);
}

// Test is disabled because it is flaky (http://crbug.com/134146)
TEST_F(DriveCacheTest, PinAndUnpinDirtyCache) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_cache_observer_, OnCacheUnpinned(resource_id, md5))
      .Times(1);

  // First store a file to cache and mark it as dirty.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);
  TestMarkDirty(resource_id, md5, DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);

  // Verifies dirty file exists.
  FilePath dirty_path = GetCacheFilePath(
      resource_id,
      md5,
      DriveCache::CACHE_TYPE_PERSISTENT,
      DriveCache::CACHED_FILE_LOCALLY_MODIFIED);
  EXPECT_TRUE(file_util::PathExists(dirty_path));

  // Pin the dirty file.
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_DIRTY |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          DriveCache::CACHE_TYPE_PERSISTENT);

  // Verify dirty file still exist at the same pathname.
  EXPECT_TRUE(file_util::PathExists(dirty_path));

  // Unpin the dirty file.
  TestUnpin(resource_id, md5, DRIVE_FILE_OK,
            test_util::TEST_CACHE_STATE_PRESENT |
            test_util::TEST_CACHE_STATE_DIRTY |
            test_util::TEST_CACHE_STATE_PERSISTENT,
            DriveCache::CACHE_TYPE_PERSISTENT);

  // Verify dirty file still exist at the same pathname.
  EXPECT_TRUE(file_util::PathExists(dirty_path));
}

TEST_F(DriveCacheTest, DirtyCacheRepetitive) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(resource_id)).Times(3);

  // First store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);

  // Mark the file dirty.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Again, mark the file dirty.  Nothing should change.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Commit the file dirty.  Outgoing symlink should be created.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, DRIVE_FILE_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Again, commit the file dirty.  Nothing should change.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, DRIVE_FILE_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Mark the file dirty agian after it's being committed.  Outgoing symlink
  // should be deleted.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Commit the file dirty.  Outgoing symlink should be created again.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, DRIVE_FILE_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);

  // Clear dirty state of the file.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, DRIVE_FILE_OK,
                 test_util::TEST_CACHE_STATE_PRESENT,
                 DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Again, clear dirty state of the file, which is no longer dirty.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, DRIVE_FILE_ERROR_INVALID_OPERATION,
                 test_util::TEST_CACHE_STATE_PRESENT,
                 DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(DriveCacheTest, DirtyCacheInvalid) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Mark a non-existent file dirty.
  num_callback_invocations_ = 0;
  TestMarkDirty(resource_id, md5, DRIVE_FILE_ERROR_NOT_FOUND,
                test_util::TEST_CACHE_STATE_NONE,
                DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Commit a non-existent file dirty.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, DRIVE_FILE_ERROR_NOT_FOUND,
                  test_util::TEST_CACHE_STATE_NONE,
                  DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Clear dirty state of a non-existent file.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, DRIVE_FILE_ERROR_NOT_FOUND,
                 test_util::TEST_CACHE_STATE_NONE,
                 DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store a file to cache.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);

  // Commit a non-dirty existing file dirty.
  num_callback_invocations_ = 0;
  TestCommitDirty(resource_id, md5, DRIVE_FILE_ERROR_INVALID_OPERATION,
                 test_util::TEST_CACHE_STATE_PRESENT,
                 DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Clear dirty state of a non-dirty existing file.
  num_callback_invocations_ = 0;
  TestClearDirty(resource_id, md5, DRIVE_FILE_ERROR_INVALID_OPERATION,
                 test_util::TEST_CACHE_STATE_PRESENT,
                 DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Mark an existing file dirty, then store a new file to the same resource id
  // but different md5, which should fail.
  TestMarkDirty(resource_id, md5, DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);
  num_callback_invocations_ = 0;
  md5 = "new_md5";
  TestStoreToCache(resource_id, md5, GetTestFilePath("subdir_feed.json"),
                   DRIVE_FILE_ERROR_IN_USE,
                   test_util::TEST_CACHE_STATE_PRESENT |
                   test_util::TEST_CACHE_STATE_DIRTY |
                   test_util::TEST_CACHE_STATE_PERSISTENT,
                   DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(DriveCacheTest, RemoveFromDirtyCache) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(resource_id)).Times(1);

  // Store a file to cache, pin it, mark it dirty and commit it.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          DriveCache::CACHE_TYPE_PERSISTENT);
  TestMarkDirty(resource_id, md5, DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_PINNED |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);
  TestCommitDirty(resource_id, md5, DRIVE_FILE_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_PINNED |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  DriveCache::CACHE_TYPE_PERSISTENT);

  // Try to remove the file.  Since file is dirty, it and the corresponding
  // pinned and outgoing symlinks should not be removed.
  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, DRIVE_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(DriveCacheTest, MountUnmount) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  FilePath file_path;
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // First store a file to cache in the tmp subdir.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);

  // Mark the file mounted.
  num_callback_invocations_ = 0;
  file_path = cache_->GetCacheFilePath(resource_id, md5,
                                       DriveCache::CACHE_TYPE_TMP,
                                       DriveCache::CACHED_FILE_FROM_SERVER);
  TestSetMountedState(resource_id, md5, file_path, true,
                      DRIVE_FILE_OK,
                      test_util::TEST_CACHE_STATE_PRESENT |
                      test_util::TEST_CACHE_STATE_MOUNTED |
                      test_util::TEST_CACHE_STATE_PERSISTENT,
                      DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_EQ(1, num_callback_invocations_);
  EXPECT_TRUE(CacheEntryExists(resource_id, md5));

  // Clear mounted state of the file.
  num_callback_invocations_ = 0;
  file_path = cache_->GetCacheFilePath(resource_id,
                                       md5,
                                       DriveCache::CACHE_TYPE_PERSISTENT,
                                       DriveCache::CACHED_FILE_MOUNTED);
  TestSetMountedState(resource_id, md5, file_path, false,
                      DRIVE_FILE_OK,
                      test_util::TEST_CACHE_STATE_PRESENT,
                      DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);
  EXPECT_TRUE(CacheEntryExists(resource_id, md5));

  // Try to remove the file.
  num_callback_invocations_ = 0;
  TestRemoveFromCache(resource_id, DRIVE_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(DriveCacheTest, GetResourceIdsOfBacklogOnUIThread) {
  PrepareForInitCacheTest();

  std::vector<std::string> to_fetch;
  std::vector<std::string> to_upload;
  cache_->GetResourceIdsOfBacklogOnUIThread(
      base::Bind(&OnGetResourceIdsOfBacklog, &to_fetch, &to_upload));
  test_util::RunBlockingPoolTask();

  sort(to_fetch.begin(), to_fetch.end());
  ASSERT_EQ(1U, to_fetch.size());
  EXPECT_EQ("pinned:non-existent", to_fetch[0]);

  sort(to_upload.begin(), to_upload.end());
  ASSERT_EQ(2U, to_upload.size());
  EXPECT_EQ("dirty:existing", to_upload[0]);
  EXPECT_EQ("dirty_and_pinned:existing", to_upload[1]);
}

TEST_F(DriveCacheTest, GetResourceIdsOfExistingPinnedFilesOnUIThread) {
  PrepareForInitCacheTest();

  std::vector<std::string> resource_ids;
  cache_->GetResourceIdsOfExistingPinnedFilesOnUIThread(
      base::Bind(&OnGetResourceIds, &resource_ids));
  test_util::RunBlockingPoolTask();

  sort(resource_ids.begin(), resource_ids.end());
  ASSERT_EQ(2U, resource_ids.size());
  EXPECT_EQ("dirty_and_pinned:existing", resource_ids[0]);
  EXPECT_EQ("pinned:existing", resource_ids[1]);
}

TEST_F(DriveCacheTest, GetResourceIdsOfAllFilesOnUIThread) {
  PrepareForInitCacheTest();

  std::vector<std::string> resource_ids;
  cache_->GetResourceIdsOfAllFilesOnUIThread(
      base::Bind(&OnGetResourceIds, &resource_ids));
  test_util::RunBlockingPoolTask();

  sort(resource_ids.begin(), resource_ids.end());
  ASSERT_EQ(6U, resource_ids.size());
  EXPECT_EQ("dirty:existing", resource_ids[0]);
  EXPECT_EQ("dirty_and_pinned:existing", resource_ids[1]);
  EXPECT_EQ("pinned:existing", resource_ids[2]);
  EXPECT_EQ("pinned:non-existent", resource_ids[3]);
  EXPECT_EQ("tmp:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?", resource_ids[4]);
  EXPECT_EQ("tmp:resource_id", resource_ids[5]);
}


TEST_F(DriveCacheTest, ClearAllOnUIThread) {
  PrepareForInitCacheTest();

  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(kLotsOfSpace));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Store an existing file.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Verify that there's only one cached file.
  EXPECT_EQ(1U, CountCacheFiles(resource_id, md5));

  // Clear cache.
  DriveFileError error = DRIVE_FILE_OK;
  FilePath file_path;
  cache_->ClearAllOnUIThread(base::Bind(&OnClearAll,
                                        &error,
                                        &file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(DRIVE_FILE_OK, error);

  // Verify that all the cache is removed.
  VerifyRemoveFromCache(error, resource_id, md5);
  EXPECT_EQ(0U, CountCacheFiles(resource_id, md5));
}

TEST_F(DriveCacheTest, StoreToCacheNoSpace) {
  EXPECT_CALL(*mock_free_disk_space_checker_, AmountOfFreeDiskSpace())
      .Times(AtLeast(1)).WillRepeatedly(Return(0));

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Try to store an existing file.
  TestStoreToCache(resource_id, md5, GetTestFilePath("root_feed.json"),
                   DRIVE_FILE_ERROR_NO_SPACE,
                   test_util::TEST_CACHE_STATE_NONE,
                   DriveCache::CACHE_TYPE_TMP);
  EXPECT_EQ(1, num_callback_invocations_);

  // Verify that there's no files added.
  EXPECT_EQ(0U, CountCacheFiles(resource_id, md5));
}


}   // namespace gdata
