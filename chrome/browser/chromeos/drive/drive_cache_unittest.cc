// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_cache.h"

#include <algorithm>
#include <vector>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_system.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_test_util.h"
#include "chrome/browser/chromeos/drive/fake_free_disk_space_getter.h"
#include "chrome/browser/chromeos/drive/mock_drive_cache_observer.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtLeast;
using ::testing::Return;
using ::testing::StrictMock;

namespace drive {
namespace {

const char kSymLinkToDevNull[] = "/dev/null";

struct TestCacheResource {
  const char* source_file;
  const char* resource_id;
  const char* md5;
  bool is_pinned;
  bool is_dirty;
} const test_cache_resources[] = {
  // Cache resource in tmp dir, i.e. not pinned or dirty.
  { "gdata/root_feed.json", "tmp:resource_id", "md5_tmp_alphanumeric",
    false, false },
  // Cache resource in tmp dir, i.e. not pinned or dirty, with resource_id
  // containing non-alphanumeric characters.
  { "gdata/subdir_feed.json", "tmp:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?",
    "md5_tmp_non_alphanumeric", false, false },
  // Cache resource that is pinned and persistent.
  { "gdata/directory_entry_atom.json", "pinned:existing", "md5_pinned_existing",
    true, false },
  // Cache resource with a non-existent source file that is pinned.
  { "", "pinned:non-existent", "md5_pinned_non_existent", true, false },
  // Cache resource that is dirty.
  { "gdata/account_metadata.json", "dirty:existing", "md5_dirty_existing",
    false, true },
  // Cache resource that is pinned and dirty.
  { "gdata/basic_feed.json", "dirty_and_pinned:existing",
    "md5_dirty_and_pinned_existing", true, true },
};

const int64 kLotsOfSpace = kMinFreeSpace * 10;

struct PathToVerify {
  PathToVerify(const base::FilePath& in_path_to_scan,
               const base::FilePath& in_expected_existing_path) :
      path_to_scan(in_path_to_scan),
      expected_existing_path(in_expected_existing_path) {
  }

  base::FilePath path_to_scan;
  base::FilePath expected_existing_path;
};

// Copies results from Iterate().
void OnIterate(std::vector<std::string>* out_resource_ids,
               std::vector<DriveCacheEntry>* out_cache_entries,
               const std::string& resource_id,
               const DriveCacheEntry& cache_entry) {
  out_resource_ids->push_back(resource_id);
  out_cache_entries->push_back(cache_entry);
}

// Called upon completion of Iterate().
void OnIterateCompleted(bool* out_is_called) {
  *out_is_called = true;
}

}  // namespace

class DriveCacheTest : public testing::Test {
 protected:
  DriveCacheTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        cache_(NULL),
        expected_error_(DRIVE_FILE_OK),
        expected_cache_state_(0),
        expected_sub_dir_type_(DriveCache::CACHE_TYPE_META),
        expected_success_(true),
        expect_outgoing_symlink_(false),
        root_feed_changestamp_(0) {
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);

    fake_free_disk_space_getter_.reset(new FakeFreeDiskSpaceGetter);

    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());
    cache_.reset(new DriveCache(DriveCache::GetCacheRootPath(profile_.get()),
                                blocking_task_runner_,
                                fake_free_disk_space_getter_.get()));

    mock_cache_observer_.reset(new StrictMock<MockDriveCacheObserver>);
    cache_->AddObserver(mock_cache_observer_.get());

    bool initialization_success = false;
    cache_->RequestInitialize(
        base::Bind(&test_util::CopyResultFromInitializeCacheCallback,
                   &initialization_success));
    google_apis::test_util::RunBlockingPoolTask();
    ASSERT_TRUE(initialization_success);
  }

  virtual void TearDown() OVERRIDE {
    cache_.reset();
    profile_.reset(NULL);
  }

  void PrepareTestCacheResources() {
    fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cache_resources); ++i) {
      const struct TestCacheResource& resource = test_cache_resources[i];
      // Copy file from data dir to cache.
      if (!std::string(resource.source_file).empty()) {
        base::FilePath source_path =
            google_apis::test_util::GetTestFilePath(
                std::string("chromeos/") + resource.source_file);

        DriveFileError error = DRIVE_FILE_OK;
        cache_->Store(
            resource.resource_id,
            resource.md5,
            source_path,
            DriveCache::FILE_OPERATION_COPY,
            google_apis::test_util::CreateCopyResultCallback(&error));
        google_apis::test_util::RunBlockingPoolTask();
        EXPECT_EQ(DRIVE_FILE_OK, error);
      }
      // Pin.
      if (resource.is_pinned) {
        DriveFileError error = DRIVE_FILE_OK;
        EXPECT_CALL(*mock_cache_observer_,
                    OnCachePinned(resource.resource_id, resource.md5)).Times(1);
        cache_->Pin(
            resource.resource_id,
            resource.md5,
            google_apis::test_util::CreateCopyResultCallback(&error));
        google_apis::test_util::RunBlockingPoolTask();
        EXPECT_EQ(DRIVE_FILE_OK, error);
      }
      // Mark dirty.
      if (resource.is_dirty) {
        DriveFileError error = DRIVE_FILE_OK;
        cache_->MarkDirty(
            resource.resource_id,
            resource.md5,
            google_apis::test_util::CreateCopyResultCallback(&error));
        google_apis::test_util::RunBlockingPoolTask();
        EXPECT_EQ(DRIVE_FILE_OK, error);

        EXPECT_CALL(*mock_cache_observer_,
                    OnCacheCommitted(resource.resource_id)).Times(1);
        cache_->CommitDirty(
            resource.resource_id,
            resource.md5,
            google_apis::test_util::CreateCopyResultCallback(&error));
        google_apis::test_util::RunBlockingPoolTask();
        EXPECT_EQ(DRIVE_FILE_OK, error);
      }
    }
  }

  void TestGetFileFromCacheByResourceIdAndMd5(
      const std::string& resource_id,
      const std::string& md5,
      DriveFileError expected_error,
      const std::string& expected_file_extension) {
    DriveFileError error = DRIVE_FILE_OK;
    base::FilePath cache_file_path;
    cache_->GetFile(
        resource_id, md5,
        base::Bind(&test_util::CopyResultsFromGetFileFromCacheCallback,
                   &error, &cache_file_path));
    google_apis::test_util::RunBlockingPoolTask();

    EXPECT_EQ(expected_error, error);
    if (error == DRIVE_FILE_OK) {
      // Verify filename of |cache_file_path|.
      base::FilePath base_name = cache_file_path.BaseName();
      EXPECT_EQ(util::EscapeCacheFileName(resource_id) +
                base::FilePath::kExtensionSeparator +
                util::EscapeCacheFileName(
                    expected_file_extension.empty() ?
                    md5 : expected_file_extension),
                base_name.value());
    } else {
      EXPECT_TRUE(cache_file_path.empty());
    }
  }

  void TestStoreToCache(
      const std::string& resource_id,
      const std::string& md5,
      const base::FilePath& source_path,
      DriveFileError expected_error,
      int expected_cache_state,
      DriveCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    DriveFileError error = DRIVE_FILE_OK;
    cache_->Store(resource_id, md5, source_path,
                  DriveCache::FILE_OPERATION_COPY,
                  google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    VerifyCacheFileState(error, resource_id, md5);
  }

  void TestRemoveFromCache(const std::string& resource_id,
                           DriveFileError expected_error) {
    expected_error_ = expected_error;

    DriveFileError error = DRIVE_FILE_OK;
    cache_->Remove(
        resource_id,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    VerifyRemoveFromCache(error, resource_id, "");
  }

  void VerifyRemoveFromCache(DriveFileError error,
                             const std::string& resource_id,
                             const std::string& md5) {
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
                     DriveCache::CACHED_FILE_FROM_SERVER), base::FilePath()));
    paths_to_verify.push_back(  // Index 1: CACHE_TYPE_PERSISTENT.
        PathToVerify(cache_->GetCacheFilePath(resource_id, "*",
                     DriveCache::CACHE_TYPE_PERSISTENT,
                     DriveCache::CACHED_FILE_FROM_SERVER), base::FilePath()));
    paths_to_verify.push_back(  // Index 2: CACHE_TYPE_OUTGOING.
        PathToVerify(cache_->GetCacheFilePath(resource_id, "",
                     DriveCache::CACHE_TYPE_OUTGOING,
                     DriveCache::CACHED_FILE_FROM_SERVER), base::FilePath()));
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

      // Change expected_existing_path of CACHE_TYPE_OUTGOING (index 2).
      paths_to_verify[2].expected_existing_path =
          GetCacheFilePath(resource_id,
                           std::string(),
                           DriveCache::CACHE_TYPE_OUTGOING,
                           DriveCache::CACHED_FILE_FROM_SERVER);

      for (size_t i = 0; i < paths_to_verify.size(); ++i) {
        const struct PathToVerify& verify = paths_to_verify[i];
        file_util::FileEnumerator enumerator(
            verify.path_to_scan.DirName(), false /* not recursive */,
            file_util::FileEnumerator::FILES |
            file_util::FileEnumerator::SHOW_SYM_LINKS,
            verify.path_to_scan.BaseName().value());
        size_t num_files_found = 0;
        for (base::FilePath current = enumerator.Next(); !current.empty();
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

    DriveFileError error = DRIVE_FILE_OK;
    cache_->Pin(resource_id, md5,
                google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    VerifyCacheFileState(error, resource_id, md5);
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

    DriveFileError error = DRIVE_FILE_OK;
    cache_->Unpin(resource_id, md5,
                  google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    VerifyCacheFileState(error, resource_id, md5);
  }

  void TestMarkDirty(const std::string& resource_id,
                     const std::string& md5,
                     DriveFileError expected_error,
                     int expected_cache_state,
                     DriveCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = false;

    DriveFileError error = DRIVE_FILE_OK;
    cache_->MarkDirty(
        resource_id, md5,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();

    VerifyCacheFileState(error, resource_id, md5);

    // Verify filename.
    if (error == DRIVE_FILE_OK) {
      base::FilePath cache_file_path;
      cache_->GetFile(
          resource_id, md5,
          base::Bind(&test_util::CopyResultsFromGetFileFromCacheCallback,
                     &error, &cache_file_path));
      google_apis::test_util::RunBlockingPoolTask();

      EXPECT_EQ(DRIVE_FILE_OK, error);
      base::FilePath base_name = cache_file_path.BaseName();
      EXPECT_EQ(util::EscapeCacheFileName(resource_id) +
                base::FilePath::kExtensionSeparator +
                "local",
                base_name.value());
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

    DriveFileError error = DRIVE_FILE_OK;
    cache_->CommitDirty(
        resource_id, md5,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    VerifyCacheFileState(error, resource_id, md5);
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

    DriveFileError error = DRIVE_FILE_OK;
    cache_->ClearDirty(
        resource_id, md5,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    VerifyCacheFileState(error, resource_id, md5);
  }

  void TestMarkAsMounted(
      const std::string& resource_id,
      const std::string& md5,
      DriveFileError expected_error,
      int expected_cache_state,
      DriveCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = false;

    DriveFileError error = DRIVE_FILE_OK;
    base::FilePath cache_file_path;
    cache_->MarkAsMounted(
        resource_id,
        md5,
        base::Bind(&test_util::CopyResultsFromGetFileFromCacheCallback,
                   &error, &cache_file_path));
    google_apis::test_util::RunBlockingPoolTask();

    EXPECT_TRUE(file_util::PathExists(cache_file_path));
    EXPECT_EQ(cache_file_path,
              cache_->GetCacheFilePath(resource_id,
                                       md5,
                                       expected_sub_dir_type_,
                                       DriveCache::CACHED_FILE_MOUNTED));
  }

  void TestMarkAsUnmounted(
      const std::string& resource_id,
      const std::string& md5,
      const base::FilePath& file_path,
      DriveFileError expected_error,
      int expected_cache_state,
      DriveCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;
    expect_outgoing_symlink_ = false;

    DriveFileError error = DRIVE_FILE_OK;
    cache_->MarkAsUnmounted(
        file_path,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();

    base::FilePath cache_file_path;
    cache_->GetFile(
        resource_id, md5,
        base::Bind(&test_util::CopyResultsFromGetFileFromCacheCallback,
                   &error, &cache_file_path));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(DRIVE_FILE_OK, error);

    EXPECT_TRUE(file_util::PathExists(cache_file_path));
    EXPECT_EQ(cache_file_path,
              cache_->GetCacheFilePath(resource_id,
                                       md5,
                                       expected_sub_dir_type_,
                                       DriveCache::CACHED_FILE_FROM_SERVER));
  }

  void VerifyCacheFileState(DriveFileError error,
                            const std::string& resource_id,
                            const std::string& md5) {
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
    base::FilePath dest_path = cache_->GetCacheFilePath(
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

    // Verify symlink in outgoing dir.
    base::FilePath symlink_path = cache_->GetCacheFilePath(
        resource_id,
        std::string(),
        DriveCache::CACHE_TYPE_OUTGOING,
        DriveCache::CACHED_FILE_FROM_SERVER);
    // Check that outgoing symlink exists, without dereferencing to target path.
    exists = file_util::IsLink(symlink_path);
    if (expect_outgoing_symlink_ &&
        test_util::ToCacheEntry(expected_cache_state_).is_dirty()) {
      EXPECT_TRUE(exists);
      base::FilePath target_path;
      EXPECT_TRUE(file_util::ReadSymbolicLink(symlink_path, &target_path));
      EXPECT_TRUE(target_path.value() != kSymLinkToDevNull);
      if (test_util::ToCacheEntry(expected_cache_state_).is_present())
        EXPECT_EQ(dest_path, target_path);
    } else {
      EXPECT_FALSE(exists);
    }
  }

  base::FilePath GetCacheFilePath(const std::string& resource_id,
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
    cache_->GetCacheEntry(
        resource_id, md5,
        base::Bind(&test_util::CopyResultsFromGetCacheEntryCallback,
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

  void TestGetCacheFilePath(const std::string& resource_id,
                            const std::string& md5,
                            const std::string& expected_filename) {
    base::FilePath actual_path = cache_->GetCacheFilePath(
        resource_id,
        md5,
        DriveCache::CACHE_TYPE_TMP,
        DriveCache::CACHED_FILE_FROM_SERVER);
    base::FilePath expected_path =
        cache_->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_TMP);
    expected_path = expected_path.Append(expected_filename);
    EXPECT_EQ(expected_path, actual_path);

    base::FilePath base_name = actual_path.BaseName();

    // base::FilePath::Extension returns ".", so strip it.
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
    base::FilePath path = GetCacheFilePath(
        resource_id, "*",
        (test_util::ToCacheEntry(expected_cache_state_).is_pinned() ?
         DriveCache::CACHE_TYPE_PERSISTENT :
         DriveCache::CACHE_TYPE_TMP),
        DriveCache::CACHED_FILE_FROM_SERVER);
    file_util::FileEnumerator enumerator(path.DirName(), false,
                                         file_util::FileEnumerator::FILES,
                                         path.BaseName().value());
    size_t num_files_found = 0;
    for (base::FilePath current = enumerator.Next(); !current.empty();
         current = enumerator.Next()) {
      ++num_files_found;
      EXPECT_EQ(util::EscapeCacheFileName(resource_id) +
                base::FilePath::kExtensionSeparator +
                util::EscapeCacheFileName(md5),
                current.BaseName().value());
    }
    return num_files_found;
  }

  MessageLoopForUI message_loop_;
  // The order of the test threads is important, do not change the order.
  // See also content/browser/browser_thread_imple.cc.
  content::TestBrowserThread ui_thread_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<DriveCache, test_util::DestroyHelperForTests> cache_;
  scoped_ptr<FakeFreeDiskSpaceGetter> fake_free_disk_space_getter_;
  scoped_ptr<StrictMock<MockDriveCacheObserver> > mock_cache_observer_;

  DriveFileError expected_error_;
  int expected_cache_state_;
  DriveCache::CacheSubDirectoryType expected_sub_dir_type_;
  bool expected_success_;
  bool expect_outgoing_symlink_;
  std::string expected_file_extension_;
  int root_feed_changestamp_;
};

TEST_F(DriveCacheTest, GetCacheFilePath) {
  // Use alphanumeric characters for resource id.
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  TestGetCacheFilePath(resource_id, md5,
                       resource_id + base::FilePath::kExtensionSeparator + md5);

  // Use non-alphanumeric characters for resource id, including '.' which is an
  // extension separator, to test that the characters are escaped and unescaped
  // correctly, and '.' doesn't mess up the filename format and operations.
  resource_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";
  std::string escaped_resource_id = util::EscapeCacheFileName(resource_id);
  std::string escaped_md5 = util::EscapeCacheFileName(md5);
  TestGetCacheFilePath(
      resource_id, md5, escaped_resource_id +
      base::FilePath::kExtensionSeparator + escaped_md5);
}

TEST_F(DriveCacheTest, StoreToCacheSimple) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Store an existing file.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
      DriveCache::CACHE_TYPE_TMP);

  // Store a non-existent file to the same |resource_id| and |md5|.
  TestStoreToCache(resource_id, md5, base::FilePath("./non_existent.json"),
                   DRIVE_FILE_ERROR_FAILED,
                   test_util::TEST_CACHE_STATE_PRESENT,
                   DriveCache::CACHE_TYPE_TMP);

  // Store a different existing file to the same |resource_id| but different
  // |md5|.
  md5 = "new_md5";
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath(
          "chromeos/gdata/subdir_feed.json"),
      DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
      DriveCache::CACHE_TYPE_TMP);

  // Verify that there's only one file with name <resource_id>, i.e. previously
  // cached file with the different md5 should be deleted.
  EXPECT_EQ(1U, CountCacheFiles(resource_id, md5));
}

TEST_F(DriveCacheTest, GetFromCacheSimple) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  // First store a file to cache.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
      DriveCache::CACHE_TYPE_TMP);

  // Then try to get the existing file from cache.
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, DRIVE_FILE_OK, md5);

  // Get file from cache with same resource id as existing file but different
  // md5.
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, "9999", DRIVE_FILE_ERROR_NOT_FOUND, md5);

  // Get file from cache with different resource id from existing file but same
  // md5.
  resource_id = "document:1a2b";
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, DRIVE_FILE_ERROR_NOT_FOUND, md5);
}

TEST_F(DriveCacheTest, RemoveFromCacheSimple) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  // Use alphanumeric characters for resource id.
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  // First store a file to cache.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
      DriveCache::CACHE_TYPE_TMP);

  // Then try to remove existing file from cache.
  TestRemoveFromCache(resource_id, DRIVE_FILE_OK);

  // Repeat using non-alphanumeric characters for resource id, including '.'
  // which is an extension separator.
  resource_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
      DriveCache::CACHE_TYPE_TMP);

  TestRemoveFromCache(resource_id, DRIVE_FILE_OK);
}

TEST_F(DriveCacheTest, PinAndUnpin) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(2);
  EXPECT_CALL(*mock_cache_observer_, OnCacheUnpinned(resource_id, md5))
      .Times(1);

  // First store a file to cache.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
      DriveCache::CACHE_TYPE_TMP);

  // Pin the existing file in cache.
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          DriveCache::CACHE_TYPE_PERSISTENT);

  // Unpin the existing file in cache.
  TestUnpin(resource_id, md5, DRIVE_FILE_OK,
            test_util::TEST_CACHE_STATE_PRESENT,
            DriveCache::CACHE_TYPE_TMP);

  // Pin back the same existing file in cache.
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          DriveCache::CACHE_TYPE_PERSISTENT);

  // Pin a non-existent file in cache.
  resource_id = "document:1a2b";
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_cache_observer_, OnCacheUnpinned(resource_id, md5))
      .Times(1);

  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PINNED,
          DriveCache::CACHE_TYPE_TMP);

  // Unpin the previously pinned non-existent file in cache.
  TestUnpin(resource_id, md5, DRIVE_FILE_OK,
            test_util::TEST_CACHE_STATE_NONE,
            DriveCache::CACHE_TYPE_TMP);

  // Unpin a file that doesn't exist in cache and is not pinned, i.e. cache
  // has zero knowledge of the file.
  resource_id = "not-in-cache:1a2b";
  // Because unpinning will fail, OnCacheUnpinned() won't be run.
  EXPECT_CALL(*mock_cache_observer_, OnCacheUnpinned(resource_id, md5))
      .Times(0);

  TestUnpin(resource_id, md5, DRIVE_FILE_ERROR_NOT_FOUND,
            test_util::TEST_CACHE_STATE_NONE,
            DriveCache::CACHE_TYPE_TMP /* non-applicable */);
}

TEST_F(DriveCacheTest, StoreToCachePinned) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);

  // Pin a non-existent file.
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PINNED,
          DriveCache::CACHE_TYPE_TMP);

  // Store an existing file to a previously pinned file.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK,
      test_util::TEST_CACHE_STATE_PRESENT |
      test_util::TEST_CACHE_STATE_PINNED |
      test_util::TEST_CACHE_STATE_PERSISTENT,
      DriveCache::CACHE_TYPE_PERSISTENT);

  // Store a non-existent file to a previously pinned and stored file.
  TestStoreToCache(resource_id, md5, base::FilePath("./non_existent.json"),
                   DRIVE_FILE_ERROR_FAILED,
                   test_util::TEST_CACHE_STATE_PRESENT |
                   test_util::TEST_CACHE_STATE_PINNED |
                   test_util::TEST_CACHE_STATE_PERSISTENT,
                   DriveCache::CACHE_TYPE_PERSISTENT);
}

TEST_F(DriveCacheTest, GetFromCachePinned) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);

  // Pin a non-existent file.
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PINNED,
          DriveCache::CACHE_TYPE_TMP);

  // Get the non-existent pinned file from cache.
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, DRIVE_FILE_ERROR_NOT_FOUND, md5);

  // Store an existing file to the previously pinned non-existent file.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK,
      test_util::TEST_CACHE_STATE_PRESENT |
      test_util::TEST_CACHE_STATE_PINNED |
      test_util::TEST_CACHE_STATE_PERSISTENT,
      DriveCache::CACHE_TYPE_PERSISTENT);

  // Get the previously pinned and stored file from cache.
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, DRIVE_FILE_OK, md5);
}

TEST_F(DriveCacheTest, RemoveFromCachePinned) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  // Use alphanumeric characters for resource_id.
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);

  // Store a file to cache, and pin it.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
      DriveCache::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          DriveCache::CACHE_TYPE_PERSISTENT);

  // Remove |resource_id| from cache.
  TestRemoveFromCache(resource_id, DRIVE_FILE_OK);

  // Repeat using non-alphanumeric characters for resource id, including '.'
  // which is an extension separator.
  resource_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);

  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
      DriveCache::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          DriveCache::CACHE_TYPE_PERSISTENT);

  TestRemoveFromCache(resource_id, DRIVE_FILE_OK);
}

TEST_F(DriveCacheTest, DirtyCacheSimple) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(resource_id)).Times(1);

  // First store a file to cache.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
      DriveCache::CACHE_TYPE_TMP);

  // Mark the file dirty.
  TestMarkDirty(resource_id, md5, DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);

  // Commit the file dirty.
  TestCommitDirty(resource_id, md5, DRIVE_FILE_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  DriveCache::CACHE_TYPE_PERSISTENT);

  // Clear dirty state of the file.
  TestClearDirty(resource_id, md5, DRIVE_FILE_OK,
                 test_util::TEST_CACHE_STATE_PRESENT,
                 DriveCache::CACHE_TYPE_TMP);
}

TEST_F(DriveCacheTest, DirtyCachePinned) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(resource_id)).Times(1);

  // First store a file to cache and pin it.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
      DriveCache::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, DRIVE_FILE_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          DriveCache::CACHE_TYPE_PERSISTENT);

  // Mark the file dirty.
  TestMarkDirty(resource_id, md5, DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PINNED |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);

  // Commit the file dirty.
  TestCommitDirty(resource_id, md5, DRIVE_FILE_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PINNED |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  DriveCache::CACHE_TYPE_PERSISTENT);

  // Clear dirty state of the file.
  TestClearDirty(resource_id, md5, DRIVE_FILE_OK,
                 test_util::TEST_CACHE_STATE_PRESENT |
                 test_util::TEST_CACHE_STATE_PINNED |
                 test_util::TEST_CACHE_STATE_PERSISTENT,
                 DriveCache::CACHE_TYPE_PERSISTENT);
}

// Test is disabled because it is flaky (http://crbug.com/134146)
TEST_F(DriveCacheTest, PinAndUnpinDirtyCache) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_cache_observer_, OnCacheUnpinned(resource_id, md5))
      .Times(1);

  // First store a file to cache and mark it as dirty.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
      DriveCache::CACHE_TYPE_TMP);
  TestMarkDirty(resource_id, md5, DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);

  // Verifies dirty file exists.
  base::FilePath dirty_path = GetCacheFilePath(
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
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(resource_id)).Times(3);

  // First store a file to cache.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
      DriveCache::CACHE_TYPE_TMP);

  // Mark the file dirty.
  TestMarkDirty(resource_id, md5, DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);

  // Again, mark the file dirty.  Nothing should change.
  TestMarkDirty(resource_id, md5, DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);

  // Commit the file dirty.  Outgoing symlink should be created.
  TestCommitDirty(resource_id, md5, DRIVE_FILE_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  DriveCache::CACHE_TYPE_PERSISTENT);

  // Again, commit the file dirty.  Nothing should change.
  TestCommitDirty(resource_id, md5, DRIVE_FILE_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  DriveCache::CACHE_TYPE_PERSISTENT);

  // Mark the file dirty agian after it's being committed.  Outgoing symlink
  // should be deleted.
  TestMarkDirty(resource_id, md5, DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);

  // Commit the file dirty.  Outgoing symlink should be created again.
  TestCommitDirty(resource_id, md5, DRIVE_FILE_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  DriveCache::CACHE_TYPE_PERSISTENT);

  // Clear dirty state of the file.
  TestClearDirty(resource_id, md5, DRIVE_FILE_OK,
                 test_util::TEST_CACHE_STATE_PRESENT,
                 DriveCache::CACHE_TYPE_TMP);

  // Again, clear dirty state of the file, which is no longer dirty.
  TestClearDirty(resource_id, md5, DRIVE_FILE_ERROR_INVALID_OPERATION,
                 test_util::TEST_CACHE_STATE_PRESENT,
                 DriveCache::CACHE_TYPE_TMP);
}

TEST_F(DriveCacheTest, DirtyCacheInvalid) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Mark a non-existent file dirty.
  TestMarkDirty(resource_id, md5, DRIVE_FILE_ERROR_NOT_FOUND,
                test_util::TEST_CACHE_STATE_NONE,
                DriveCache::CACHE_TYPE_TMP);

  // Commit a non-existent file dirty.
  TestCommitDirty(resource_id, md5, DRIVE_FILE_ERROR_NOT_FOUND,
                  test_util::TEST_CACHE_STATE_NONE,
                  DriveCache::CACHE_TYPE_TMP);

  // Clear dirty state of a non-existent file.
  TestClearDirty(resource_id, md5, DRIVE_FILE_ERROR_NOT_FOUND,
                 test_util::TEST_CACHE_STATE_NONE,
                 DriveCache::CACHE_TYPE_TMP);

  // Store a file to cache.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
      DriveCache::CACHE_TYPE_TMP);

  // Commit a non-dirty existing file dirty.
  TestCommitDirty(resource_id, md5, DRIVE_FILE_ERROR_INVALID_OPERATION,
                 test_util::TEST_CACHE_STATE_PRESENT,
                 DriveCache::CACHE_TYPE_TMP);

  // Clear dirty state of a non-dirty existing file.
  TestClearDirty(resource_id, md5, DRIVE_FILE_ERROR_INVALID_OPERATION,
                 test_util::TEST_CACHE_STATE_PRESENT,
                 DriveCache::CACHE_TYPE_TMP);

  // Mark an existing file dirty, then store a new file to the same resource id
  // but different md5, which should fail.
  TestMarkDirty(resource_id, md5, DRIVE_FILE_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                DriveCache::CACHE_TYPE_PERSISTENT);
  md5 = "new_md5";
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath(
          "chromeos/gdata/subdir_feed.json"),
      DRIVE_FILE_ERROR_IN_USE,
      test_util::TEST_CACHE_STATE_PRESENT |
      test_util::TEST_CACHE_STATE_DIRTY |
      test_util::TEST_CACHE_STATE_PERSISTENT,
      DriveCache::CACHE_TYPE_PERSISTENT);
}

TEST_F(DriveCacheTest, RemoveFromDirtyCache) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(resource_id)).Times(1);

  // Store a file to cache, pin it, mark it dirty and commit it.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
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
  TestRemoveFromCache(resource_id, DRIVE_FILE_OK);
}

TEST_F(DriveCacheTest, MountUnmount) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  base::FilePath file_path;
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // First store a file to cache in the tmp subdir.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
      DriveCache::CACHE_TYPE_TMP);

  // Mark the file mounted.
  TestMarkAsMounted(resource_id,
                    md5,
                    DRIVE_FILE_OK,
                    test_util::TEST_CACHE_STATE_PRESENT |
                    test_util::TEST_CACHE_STATE_MOUNTED |
                    test_util::TEST_CACHE_STATE_PERSISTENT,
                    DriveCache::CACHE_TYPE_PERSISTENT);
  EXPECT_TRUE(CacheEntryExists(resource_id, md5));

  // Clear mounted state of the file.
  file_path = cache_->GetCacheFilePath(resource_id,
                                       md5,
                                       DriveCache::CACHE_TYPE_PERSISTENT,
                                       DriveCache::CACHED_FILE_MOUNTED);
  TestMarkAsUnmounted(resource_id, md5, file_path,
                      DRIVE_FILE_OK,
                      test_util::TEST_CACHE_STATE_PRESENT,
                      DriveCache::CACHE_TYPE_TMP);
  EXPECT_TRUE(CacheEntryExists(resource_id, md5));

  // Try to remove the file.
  TestRemoveFromCache(resource_id, DRIVE_FILE_OK);
}

TEST_F(DriveCacheTest, Iterate) {
  PrepareTestCacheResources();

  std::vector<std::string> resource_ids;
  std::vector<DriveCacheEntry> cache_entries;
  bool completed = false;
  cache_->Iterate(
      base::Bind(&OnIterate, &resource_ids, &cache_entries),
      base::Bind(&OnIterateCompleted, &completed));
  google_apis::test_util::RunBlockingPoolTask();

  ASSERT_TRUE(completed);

  sort(resource_ids.begin(), resource_ids.end());
  ASSERT_EQ(6U, resource_ids.size());
  EXPECT_EQ("dirty:existing", resource_ids[0]);
  EXPECT_EQ("dirty_and_pinned:existing", resource_ids[1]);
  EXPECT_EQ("pinned:existing", resource_ids[2]);
  EXPECT_EQ("pinned:non-existent", resource_ids[3]);
  EXPECT_EQ("tmp:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?", resource_ids[4]);
  EXPECT_EQ("tmp:resource_id", resource_ids[5]);

  ASSERT_EQ(6U, cache_entries.size());
}


TEST_F(DriveCacheTest, ClearAll) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Store an existing file.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_OK, test_util::TEST_CACHE_STATE_PRESENT,
      DriveCache::CACHE_TYPE_TMP);

  // Verify that there's only one cached file.
  EXPECT_EQ(1U, CountCacheFiles(resource_id, md5));

  // Clear cache.
  bool success = false;
  cache_->ClearAll(base::Bind(&test_util::CopyResultFromInitializeCacheCallback,
                              &success));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_TRUE(success);

  // Verify that all the cache is removed.
  expected_error_ = DRIVE_FILE_OK;
  VerifyRemoveFromCache(DRIVE_FILE_OK, resource_id, md5);
  EXPECT_EQ(0U, CountCacheFiles(resource_id, md5));
}

TEST_F(DriveCacheTest, StoreToCacheNoSpace) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(0);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Try to store an existing file.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      DRIVE_FILE_ERROR_NO_SPACE,
      test_util::TEST_CACHE_STATE_NONE,
      DriveCache::CACHE_TYPE_TMP);

  // Verify that there's no files added.
  EXPECT_EQ(0U, CountCacheFiles(resource_id, md5));
}

// Don't use TEST_F, as we don't want SetUp() and TearDown() for this test.
TEST(DriveCacheExtraTest, InitializationFailure) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       &message_loop);

  scoped_refptr<base::SequencedWorkerPool> pool =
      content::BrowserThread::GetBlockingPool();

  // Set the cache root to a non existent path, so the initialization fails.
  scoped_ptr<DriveCache, test_util::DestroyHelperForTests> cache(new DriveCache(
      base::FilePath::FromUTF8Unsafe("/somewhere/nonexistent/blah/blah"),
      pool->GetSequencedTaskRunner(pool->GetSequenceToken()),
      NULL /* free_disk_space_getter */));

  bool success = false;
  cache->RequestInitialize(
      base::Bind(&test_util::CopyResultFromInitializeCacheCallback, &success));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_FALSE(success);
}

}   // namespace drive
