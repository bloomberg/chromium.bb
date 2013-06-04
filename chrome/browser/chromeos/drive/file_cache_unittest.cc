// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_cache.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/fake_free_disk_space_getter.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/mock_file_cache_observer.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;

namespace drive {
namespace internal {
namespace {

// Copies results from Iterate().
void OnIterate(std::vector<std::string>* out_resource_ids,
               std::vector<FileCacheEntry>* out_cache_entries,
               const std::string& resource_id,
               const FileCacheEntry& cache_entry) {
  out_resource_ids->push_back(resource_id);
  out_cache_entries->push_back(cache_entry);
}

// Called upon completion of Iterate().
void OnIterateCompleted(bool* out_is_called) {
  *out_is_called = true;
}

}  // namespace

class FileCacheTest : public testing::Test {
 protected:
  FileCacheTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        expected_error_(FILE_ERROR_OK),
        expected_cache_state_(0),
        expected_sub_dir_type_(FileCache::CACHE_TYPE_META),
        expected_success_(true) {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    fake_free_disk_space_getter_.reset(new FakeFreeDiskSpaceGetter);

    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());
    cache_.reset(new FileCache(temp_dir_.path(),
                                blocking_task_runner_,
                                fake_free_disk_space_getter_.get()));

    mock_cache_observer_.reset(new StrictMock<MockCacheObserver>);
    cache_->AddObserver(mock_cache_observer_.get());

    bool success = false;
    cache_->RequestInitialize(
        google_apis::test_util::CreateCopyResultCallback(&success));
    google_apis::test_util::RunBlockingPoolTask();
    ASSERT_TRUE(success);
  }

  virtual void TearDown() OVERRIDE {
    cache_.reset();
  }

  void TestGetFileFromCacheByResourceIdAndMd5(
      const std::string& resource_id,
      const std::string& md5,
      FileError expected_error,
      const std::string& expected_file_extension) {
    FileError error = FILE_ERROR_OK;
    base::FilePath cache_file_path;
    cache_->GetFileOnUIThread(resource_id, md5,
                              google_apis::test_util::CreateCopyResultCallback(
                                  &error, &cache_file_path));
    google_apis::test_util::RunBlockingPoolTask();

    EXPECT_EQ(expected_error, error);
    if (error == FILE_ERROR_OK) {
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
      FileError expected_error,
      int expected_cache_state,
      FileCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    FileError error = FILE_ERROR_OK;
    cache_->StoreOnUIThread(
        resource_id, md5, source_path,
        FileCache::FILE_OPERATION_COPY,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    VerifyCacheFileState(error, resource_id, md5);
  }

  void TestStoreLocallyModifiedToCache(
      const std::string& resource_id,
      const std::string& md5,
      const base::FilePath& source_path,
      FileError expected_error,
      int expected_cache_state,
      FileCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    FileError error = FILE_ERROR_OK;
    cache_->StoreLocallyModifiedOnUIThread(
        resource_id, md5, source_path,
        FileCache::FILE_OPERATION_COPY,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    VerifyCacheFileState(error, resource_id, md5);
  }

  void TestRemoveFromCache(const std::string& resource_id,
                           FileError expected_error) {
    expected_error_ = expected_error;

    FileError error = FILE_ERROR_OK;
    cache_->RemoveOnUIThread(
        resource_id,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    VerifyRemoveFromCache(error, resource_id, "");
  }

  // Returns number of files matching to |path_pattern|.
  int CountFilesWithPathPattern(const base::FilePath& path_pattern) {
    int result = 0;
    file_util::FileEnumerator enumerator(
        path_pattern.DirName(), false /* not recursive*/,
        file_util::FileEnumerator::FILES,
        path_pattern.BaseName().value());
    for (base::FilePath current = enumerator.Next(); !current.empty();
         current = enumerator.Next())
      ++result;
    return result;
  }

  void VerifyRemoveFromCache(FileError error,
                             const std::string& resource_id,
                             const std::string& md5) {
    EXPECT_EQ(expected_error_, error);

    // Verify cache map.
    FileCacheEntry cache_entry;
    const bool cache_entry_found =
        GetCacheEntryFromOriginThread(resource_id, md5, &cache_entry);
    if (cache_entry_found)
      EXPECT_TRUE(cache_entry.is_dirty());

    // If entry doesn't exist, verify that no files with "<resource_id>.*"
    // exist in persistent and tmp dirs.
    const base::FilePath path_pattern_tmp =
        cache_->GetCacheFilePath(resource_id, "*",
                                 FileCache::CACHE_TYPE_TMP,
                                 FileCache::CACHED_FILE_FROM_SERVER);
    const base::FilePath path_pattern_persistent =
        cache_->GetCacheFilePath(resource_id, "*",
                                 FileCache::CACHE_TYPE_PERSISTENT,
                                 FileCache::CACHED_FILE_FROM_SERVER);

    EXPECT_EQ(0, CountFilesWithPathPattern(path_pattern_tmp));
    if (!cache_entry_found) {
      EXPECT_EQ(0, CountFilesWithPathPattern(path_pattern_persistent));
    } else {
      // Entry is dirty, verify that only 1 "<resource_id>.local" exists in
      // persistent dir.
      EXPECT_EQ(1, CountFilesWithPathPattern(path_pattern_persistent));
      EXPECT_TRUE(file_util::PathExists(
          GetCacheFilePath(resource_id,
                           std::string(),
                           FileCache::CACHE_TYPE_PERSISTENT,
                           FileCache::CACHED_FILE_LOCALLY_MODIFIED)));
    }
  }

  void TestPin(
      const std::string& resource_id,
      const std::string& md5,
      FileError expected_error,
      int expected_cache_state,
      FileCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    FileError error = FILE_ERROR_OK;
    cache_->PinOnUIThread(
        resource_id, md5,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    VerifyCacheFileState(error, resource_id, md5);
  }

  void TestUnpin(
      const std::string& resource_id,
      const std::string& md5,
      FileError expected_error,
      int expected_cache_state,
      FileCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    FileError error = FILE_ERROR_OK;
    cache_->UnpinOnUIThread(
        resource_id, md5,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    VerifyCacheFileState(error, resource_id, md5);
  }

  void TestMarkDirty(const std::string& resource_id,
                     const std::string& md5,
                     FileError expected_error,
                     int expected_cache_state,
                     FileCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    FileError error = FILE_ERROR_OK;
    cache_->MarkDirtyOnUIThread(
        resource_id, md5,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();

    VerifyCacheFileState(error, resource_id, md5);

    // Verify filename.
    if (error == FILE_ERROR_OK) {
      base::FilePath cache_file_path;
      cache_->GetFileOnUIThread(
          resource_id, md5,
          google_apis::test_util::CreateCopyResultCallback(
              &error, &cache_file_path));
      google_apis::test_util::RunBlockingPoolTask();

      EXPECT_EQ(FILE_ERROR_OK, error);
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
      FileError expected_error,
      int expected_cache_state,
      FileCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    FileError error = FILE_ERROR_OK;
    cache_->CommitDirtyOnUIThread(
        resource_id, md5,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    VerifyCacheFileState(error, resource_id, md5);
  }

  void TestClearDirty(
      const std::string& resource_id,
      const std::string& md5,
      FileError expected_error,
      int expected_cache_state,
      FileCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    FileError error = FILE_ERROR_OK;
    PostTaskAndReplyWithResult(
        blocking_task_runner_,
        FROM_HERE,
        base::Bind(&FileCache::ClearDirty,
                   base::Unretained(cache_.get()),
                   resource_id, md5),
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    VerifyCacheFileState(error, resource_id, md5);
  }

  void TestMarkAsMounted(
      const std::string& resource_id,
      const std::string& md5,
      FileError expected_error,
      int expected_cache_state,
      FileCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    FileError error = FILE_ERROR_OK;
    base::FilePath cache_file_path;
    cache_->MarkAsMountedOnUIThread(
        resource_id, md5,
        google_apis::test_util::CreateCopyResultCallback(
            &error, &cache_file_path));
    google_apis::test_util::RunBlockingPoolTask();

    EXPECT_TRUE(file_util::PathExists(cache_file_path));
    EXPECT_EQ(cache_file_path,
              cache_->GetCacheFilePath(resource_id,
                                       md5,
                                       expected_sub_dir_type_,
                                       FileCache::CACHED_FILE_MOUNTED));
  }

  void TestMarkAsUnmounted(
      const std::string& resource_id,
      const std::string& md5,
      const base::FilePath& file_path,
      FileError expected_error,
      int expected_cache_state,
      FileCache::CacheSubDirectoryType expected_sub_dir_type) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_sub_dir_type_ = expected_sub_dir_type;

    FileError error = FILE_ERROR_OK;
    cache_->MarkAsUnmountedOnUIThread(
        file_path,
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();

    base::FilePath cache_file_path;
    cache_->GetFileOnUIThread(
        resource_id, md5,
        google_apis::test_util::CreateCopyResultCallback(
            &error, &cache_file_path));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);

    EXPECT_TRUE(file_util::PathExists(cache_file_path));
    EXPECT_EQ(cache_file_path,
              cache_->GetCacheFilePath(resource_id,
                                       md5,
                                       expected_sub_dir_type_,
                                       FileCache::CACHED_FILE_FROM_SERVER));
  }

  void VerifyCacheFileState(FileError error,
                            const std::string& resource_id,
                            const std::string& md5) {
    EXPECT_EQ(expected_error_, error);

    // Verify cache map.
    FileCacheEntry cache_entry;
    const bool cache_entry_found =
        GetCacheEntryFromOriginThread(resource_id, md5, &cache_entry);
    if (test_util::ToCacheEntry(expected_cache_state_).is_present() ||
        test_util::ToCacheEntry(expected_cache_state_).is_pinned()) {
      ASSERT_TRUE(cache_entry_found);
      EXPECT_TRUE(test_util::CacheStatesEqual(
          test_util::ToCacheEntry(expected_cache_state_),
          cache_entry));
      EXPECT_EQ(expected_sub_dir_type_,
                FileCache::GetSubDirectoryType(cache_entry));
    } else {
      EXPECT_FALSE(cache_entry_found);
    }

    // Verify actual cache file.
    base::FilePath dest_path = cache_->GetCacheFilePath(
        resource_id,
        md5,
        test_util::ToCacheEntry(expected_cache_state_).is_pinned() ||
        test_util::ToCacheEntry(expected_cache_state_).is_dirty() ?
                FileCache::CACHE_TYPE_PERSISTENT :
                FileCache::CACHE_TYPE_TMP,
        test_util::ToCacheEntry(expected_cache_state_).is_dirty() ?
            FileCache::CACHED_FILE_LOCALLY_MODIFIED :
            FileCache::CACHED_FILE_FROM_SERVER);
    bool exists = file_util::PathExists(dest_path);
    if (test_util::ToCacheEntry(expected_cache_state_).is_present())
      EXPECT_TRUE(exists);
    else
      EXPECT_FALSE(exists);
  }

  base::FilePath GetCacheFilePath(const std::string& resource_id,
                            const std::string& md5,
                            FileCache::CacheSubDirectoryType sub_dir_type,
                            FileCache::CachedFileOrigin file_origin) {
    return cache_->GetCacheFilePath(resource_id, md5, sub_dir_type,
                                    file_origin);
  }

  // Helper function to call GetCacheEntry from origin thread.
  bool GetCacheEntryFromOriginThread(const std::string& resource_id,
                                     const std::string& md5,
                                     FileCacheEntry* cache_entry) {
    bool result = false;
    cache_->GetCacheEntryOnUIThread(
        resource_id, md5,
        google_apis::test_util::CreateCopyResultCallback(&result, cache_entry));
    google_apis::test_util::RunBlockingPoolTask();
    return result;
  }

  // Returns true if the cache entry exists for the given resource ID and MD5.
  bool CacheEntryExists(const std::string& resource_id,
                        const std::string& md5) {
    FileCacheEntry cache_entry;
    return GetCacheEntryFromOriginThread(resource_id, md5, &cache_entry);
  }

  void TestGetCacheFilePath(const std::string& resource_id,
                            const std::string& md5,
                            const std::string& expected_filename) {
    base::FilePath actual_path = cache_->GetCacheFilePath(
        resource_id,
        md5,
        FileCache::CACHE_TYPE_TMP,
        FileCache::CACHED_FILE_FROM_SERVER);
    base::FilePath expected_path =
        cache_->GetCacheDirectoryPath(FileCache::CACHE_TYPE_TMP);
    expected_path = expected_path.Append(
        base::FilePath::FromUTF8Unsafe(expected_filename));
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
         FileCache::CACHE_TYPE_PERSISTENT :
         FileCache::CACHE_TYPE_TMP),
        FileCache::CACHED_FILE_FROM_SERVER);
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

  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  base::ScopedTempDir temp_dir_;

  scoped_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
  scoped_ptr<FakeFreeDiskSpaceGetter> fake_free_disk_space_getter_;
  scoped_ptr<StrictMock<MockCacheObserver> > mock_cache_observer_;

  FileError expected_error_;
  int expected_cache_state_;
  FileCache::CacheSubDirectoryType expected_sub_dir_type_;
  bool expected_success_;
  std::string expected_file_extension_;
};

TEST_F(FileCacheTest, GetCacheFilePath) {
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

TEST_F(FileCacheTest, StoreToCacheSimple) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Store an existing file.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);

  // Store a non-existent file to the same |resource_id| and |md5|.
  TestStoreToCache(resource_id, md5, base::FilePath("./non_existent.json"),
                   FILE_ERROR_FAILED,
                   test_util::TEST_CACHE_STATE_PRESENT,
                   FileCache::CACHE_TYPE_TMP);

  // Store a different existing file to the same |resource_id| but different
  // |md5|.
  md5 = "new_md5";
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/empty_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);

  // Verify that there's only one file with name <resource_id>, i.e. previously
  // cached file with the different md5 should be deleted.
  EXPECT_EQ(1U, CountCacheFiles(resource_id, md5));
}

TEST_F(FileCacheTest, LocallyModifiedSimple) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  const int kDirtyCacheState =
      test_util::TEST_CACHE_STATE_PRESENT |
      test_util::TEST_CACHE_STATE_DIRTY |
      test_util::TEST_CACHE_STATE_PERSISTENT;

  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(resource_id)).Times(1);
  TestStoreLocallyModifiedToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, kDirtyCacheState, FileCache::CACHE_TYPE_PERSISTENT);
}

TEST_F(FileCacheTest, GetFromCacheSimple) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  // First store a file to cache.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);

  // Then try to get the existing file from cache.
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, FILE_ERROR_OK, md5);

  // Get file from cache with same resource id as existing file but different
  // md5.
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, "9999", FILE_ERROR_NOT_FOUND, md5);

  // Get file from cache with different resource id from existing file but same
  // md5.
  resource_id = "document:1a2b";
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, FILE_ERROR_NOT_FOUND, md5);
}

TEST_F(FileCacheTest, RemoveFromCacheSimple) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  // Use alphanumeric characters for resource id.
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  // First store a file to cache.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);

  // Then try to remove existing file from cache.
  TestRemoveFromCache(resource_id, FILE_ERROR_OK);

  // Repeat using non-alphanumeric characters for resource id, including '.'
  // which is an extension separator.
  resource_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);

  TestRemoveFromCache(resource_id, FILE_ERROR_OK);
}

TEST_F(FileCacheTest, PinAndUnpin) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(2);
  EXPECT_CALL(*mock_cache_observer_, OnCacheUnpinned(resource_id, md5))
      .Times(1);

  // First store a file to cache.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);

  // Pin the existing file in cache.
  TestPin(resource_id, md5, FILE_ERROR_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          FileCache::CACHE_TYPE_PERSISTENT);

  // Unpin the existing file in cache.
  TestUnpin(resource_id, md5, FILE_ERROR_OK,
            test_util::TEST_CACHE_STATE_PRESENT,
            FileCache::CACHE_TYPE_TMP);

  // Pin back the same existing file in cache.
  TestPin(resource_id, md5, FILE_ERROR_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          FileCache::CACHE_TYPE_PERSISTENT);

  // Pin a non-existent file in cache.
  resource_id = "document:1a2b";
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_cache_observer_, OnCacheUnpinned(resource_id, md5))
      .Times(1);

  TestPin(resource_id, md5, FILE_ERROR_OK,
          test_util::TEST_CACHE_STATE_PINNED,
          FileCache::CACHE_TYPE_TMP);

  // Unpin the previously pinned non-existent file in cache.
  TestUnpin(resource_id, md5, FILE_ERROR_OK,
            test_util::TEST_CACHE_STATE_NONE,
            FileCache::CACHE_TYPE_TMP);

  // Unpin a file that doesn't exist in cache and is not pinned, i.e. cache
  // has zero knowledge of the file.
  resource_id = "not-in-cache:1a2b";
  // Because unpinning will fail, OnCacheUnpinned() won't be run.
  EXPECT_CALL(*mock_cache_observer_, OnCacheUnpinned(resource_id, md5))
      .Times(0);

  TestUnpin(resource_id, md5, FILE_ERROR_NOT_FOUND,
            test_util::TEST_CACHE_STATE_NONE,
            FileCache::CACHE_TYPE_TMP /* non-applicable */);
}

TEST_F(FileCacheTest, StoreToCachePinned) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);

  // Pin a non-existent file.
  TestPin(resource_id, md5, FILE_ERROR_OK,
          test_util::TEST_CACHE_STATE_PINNED,
          FileCache::CACHE_TYPE_TMP);

  // Store an existing file to a previously pinned file.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK,
      test_util::TEST_CACHE_STATE_PRESENT |
      test_util::TEST_CACHE_STATE_PINNED |
      test_util::TEST_CACHE_STATE_PERSISTENT,
      FileCache::CACHE_TYPE_PERSISTENT);

  // Store a non-existent file to a previously pinned and stored file.
  TestStoreToCache(resource_id, md5, base::FilePath("./non_existent.json"),
                   FILE_ERROR_FAILED,
                   test_util::TEST_CACHE_STATE_PRESENT |
                   test_util::TEST_CACHE_STATE_PINNED |
                   test_util::TEST_CACHE_STATE_PERSISTENT,
                   FileCache::CACHE_TYPE_PERSISTENT);
}

TEST_F(FileCacheTest, GetFromCachePinned) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);

  // Pin a non-existent file.
  TestPin(resource_id, md5, FILE_ERROR_OK,
          test_util::TEST_CACHE_STATE_PINNED,
          FileCache::CACHE_TYPE_TMP);

  // Get the non-existent pinned file from cache.
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, FILE_ERROR_NOT_FOUND, md5);

  // Store an existing file to the previously pinned non-existent file.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK,
      test_util::TEST_CACHE_STATE_PRESENT |
      test_util::TEST_CACHE_STATE_PINNED |
      test_util::TEST_CACHE_STATE_PERSISTENT,
      FileCache::CACHE_TYPE_PERSISTENT);

  // Get the previously pinned and stored file from cache.
  TestGetFileFromCacheByResourceIdAndMd5(
      resource_id, md5, FILE_ERROR_OK, md5);
}

TEST_F(FileCacheTest, RemoveFromCachePinned) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  // Use alphanumeric characters for resource_id.
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);

  // Store a file to cache, and pin it.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, FILE_ERROR_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          FileCache::CACHE_TYPE_PERSISTENT);

  // Remove |resource_id| from cache.
  TestRemoveFromCache(resource_id, FILE_ERROR_OK);

  // Repeat using non-alphanumeric characters for resource id, including '.'
  // which is an extension separator.
  resource_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);

  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, FILE_ERROR_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          FileCache::CACHE_TYPE_PERSISTENT);

  TestRemoveFromCache(resource_id, FILE_ERROR_OK);
}

TEST_F(FileCacheTest, DirtyCacheSimple) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(resource_id)).Times(1);

  // First store a file to cache.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);

  // Mark the file dirty.
  TestMarkDirty(resource_id, md5, FILE_ERROR_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                FileCache::CACHE_TYPE_PERSISTENT);

  // Commit the file dirty.
  TestCommitDirty(resource_id, md5, FILE_ERROR_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  FileCache::CACHE_TYPE_PERSISTENT);

  // Clear dirty state of the file.
  TestClearDirty(resource_id, md5, FILE_ERROR_OK,
                 test_util::TEST_CACHE_STATE_PRESENT,
                 FileCache::CACHE_TYPE_TMP);
}

TEST_F(FileCacheTest, DirtyCachePinned) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(resource_id)).Times(1);

  // First store a file to cache and pin it.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, FILE_ERROR_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          FileCache::CACHE_TYPE_PERSISTENT);

  // Mark the file dirty.
  TestMarkDirty(resource_id, md5, FILE_ERROR_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PINNED |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                FileCache::CACHE_TYPE_PERSISTENT);

  // Commit the file dirty.
  TestCommitDirty(resource_id, md5, FILE_ERROR_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PINNED |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  FileCache::CACHE_TYPE_PERSISTENT);

  // Clear dirty state of the file.
  TestClearDirty(resource_id, md5, FILE_ERROR_OK,
                 test_util::TEST_CACHE_STATE_PRESENT |
                 test_util::TEST_CACHE_STATE_PINNED |
                 test_util::TEST_CACHE_STATE_PERSISTENT,
                 FileCache::CACHE_TYPE_PERSISTENT);
}

// Test is disabled because it is flaky (http://crbug.com/134146)
TEST_F(FileCacheTest, PinAndUnpinDirtyCache) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_cache_observer_, OnCacheUnpinned(resource_id, md5))
      .Times(1);

  // First store a file to cache and mark it as dirty.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);
  TestMarkDirty(resource_id, md5, FILE_ERROR_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                FileCache::CACHE_TYPE_PERSISTENT);

  // Verifies dirty file exists.
  base::FilePath dirty_path;
  FileError error = FILE_ERROR_FAILED;
  cache_->GetFileOnUIThread(
      resource_id, md5,
      google_apis::test_util::CreateCopyResultCallback(&error, &dirty_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_TRUE(file_util::PathExists(dirty_path));

  // Pin the dirty file.
  TestPin(resource_id, md5, FILE_ERROR_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_DIRTY |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          FileCache::CACHE_TYPE_PERSISTENT);

  // Verify dirty file still exist at the same pathname.
  EXPECT_TRUE(file_util::PathExists(dirty_path));

  // Unpin the dirty file.
  TestUnpin(resource_id, md5, FILE_ERROR_OK,
            test_util::TEST_CACHE_STATE_PRESENT |
            test_util::TEST_CACHE_STATE_DIRTY |
            test_util::TEST_CACHE_STATE_PERSISTENT,
            FileCache::CACHE_TYPE_PERSISTENT);

  // Verify dirty file still exist at the same pathname.
  EXPECT_TRUE(file_util::PathExists(dirty_path));
}

TEST_F(FileCacheTest, DirtyCacheRepetitive) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(resource_id)).Times(3);

  // First store a file to cache.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);

  // Mark the file dirty.
  TestMarkDirty(resource_id, md5, FILE_ERROR_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                FileCache::CACHE_TYPE_PERSISTENT);

  // Again, mark the file dirty.  Nothing should change.
  TestMarkDirty(resource_id, md5, FILE_ERROR_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                FileCache::CACHE_TYPE_PERSISTENT);

  // Commit the file dirty.
  TestCommitDirty(resource_id, md5, FILE_ERROR_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  FileCache::CACHE_TYPE_PERSISTENT);

  // Again, commit the file dirty.  Nothing should change.
  TestCommitDirty(resource_id, md5, FILE_ERROR_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  FileCache::CACHE_TYPE_PERSISTENT);

  // Mark the file dirty again after it's being committed.
  TestMarkDirty(resource_id, md5, FILE_ERROR_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                FileCache::CACHE_TYPE_PERSISTENT);

  // Commit the file dirty.
  TestCommitDirty(resource_id, md5, FILE_ERROR_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  FileCache::CACHE_TYPE_PERSISTENT);

  // Clear dirty state of the file.
  TestClearDirty(resource_id, md5, FILE_ERROR_OK,
                 test_util::TEST_CACHE_STATE_PRESENT,
                 FileCache::CACHE_TYPE_TMP);

  // Again, clear dirty state of the file, which is no longer dirty.
  TestClearDirty(resource_id, md5, FILE_ERROR_INVALID_OPERATION,
                 test_util::TEST_CACHE_STATE_PRESENT,
                 FileCache::CACHE_TYPE_TMP);
}

TEST_F(FileCacheTest, DirtyCacheInvalid) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Mark a non-existent file dirty.
  TestMarkDirty(resource_id, md5, FILE_ERROR_NOT_FOUND,
                test_util::TEST_CACHE_STATE_NONE,
                FileCache::CACHE_TYPE_TMP);

  // Clear dirty state of a non-existent file.
  TestClearDirty(resource_id, md5, FILE_ERROR_NOT_FOUND,
                 test_util::TEST_CACHE_STATE_NONE,
                 FileCache::CACHE_TYPE_TMP);

  // Store a file to cache.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);

  // Clear dirty state of a non-dirty existing file.
  TestClearDirty(resource_id, md5, FILE_ERROR_INVALID_OPERATION,
                 test_util::TEST_CACHE_STATE_PRESENT,
                 FileCache::CACHE_TYPE_TMP);

  // Mark an existing file dirty, then store a new file to the same resource id
  // but different md5, which should fail.
  TestMarkDirty(resource_id, md5, FILE_ERROR_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                FileCache::CACHE_TYPE_PERSISTENT);
  md5 = "new_md5";
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath(
          "chromeos/gdata/empty_feed.json"),
      FILE_ERROR_IN_USE,
      test_util::TEST_CACHE_STATE_PRESENT |
      test_util::TEST_CACHE_STATE_DIRTY |
      test_util::TEST_CACHE_STATE_PERSISTENT,
      FileCache::CACHE_TYPE_PERSISTENT);
}

TEST_F(FileCacheTest, RemoveFromDirtyCache) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);
  EXPECT_CALL(*mock_cache_observer_, OnCacheCommitted(resource_id)).Times(1);

  // Store a file to cache, pin it, mark it dirty and commit it.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);
  TestPin(resource_id, md5, FILE_ERROR_OK,
          test_util::TEST_CACHE_STATE_PRESENT |
          test_util::TEST_CACHE_STATE_PINNED |
          test_util::TEST_CACHE_STATE_PERSISTENT,
          FileCache::CACHE_TYPE_PERSISTENT);
  TestMarkDirty(resource_id, md5, FILE_ERROR_OK,
                test_util::TEST_CACHE_STATE_PRESENT |
                test_util::TEST_CACHE_STATE_PINNED |
                test_util::TEST_CACHE_STATE_DIRTY |
                test_util::TEST_CACHE_STATE_PERSISTENT,
                FileCache::CACHE_TYPE_PERSISTENT);
  TestCommitDirty(resource_id, md5, FILE_ERROR_OK,
                  test_util::TEST_CACHE_STATE_PRESENT |
                  test_util::TEST_CACHE_STATE_PINNED |
                  test_util::TEST_CACHE_STATE_DIRTY |
                  test_util::TEST_CACHE_STATE_PERSISTENT,
                  FileCache::CACHE_TYPE_PERSISTENT);

  // Try to remove the file.  Since file is dirty, it should not be removed.
  TestRemoveFromCache(resource_id, FILE_ERROR_OK);
}

TEST_F(FileCacheTest, MountUnmount) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // First store a file to cache in the tmp subdir.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);

  // Mark the file mounted.
  TestMarkAsMounted(resource_id,
                    md5,
                    FILE_ERROR_OK,
                    test_util::TEST_CACHE_STATE_PRESENT |
                    test_util::TEST_CACHE_STATE_MOUNTED |
                    test_util::TEST_CACHE_STATE_PERSISTENT,
                    FileCache::CACHE_TYPE_PERSISTENT);
  EXPECT_TRUE(CacheEntryExists(resource_id, md5));

  // Clear mounted state of the file.
  base::FilePath file_path;
  FileError error = FILE_ERROR_FAILED;
  cache_->GetFileOnUIThread(
      resource_id, md5,
      google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  TestMarkAsUnmounted(resource_id, md5, file_path,
                      FILE_ERROR_OK,
                      test_util::TEST_CACHE_STATE_PRESENT,
                      FileCache::CACHE_TYPE_TMP);
  EXPECT_TRUE(CacheEntryExists(resource_id, md5));

  // Try to remove the file.
  TestRemoveFromCache(resource_id, FILE_ERROR_OK);
}

TEST_F(FileCacheTest, Iterate) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);
  const std::vector<test_util::TestCacheResource> cache_resources(
      test_util::GetDefaultTestCacheResources());
  // Set mock expectations.
  for (size_t i = 0; i < cache_resources.size(); ++i) {
    if (cache_resources[i].is_pinned) {
      EXPECT_CALL(*mock_cache_observer_,
                  OnCachePinned(cache_resources[i].resource_id,
                                cache_resources[i].md5)).Times(1);
    }
    if (cache_resources[i].is_dirty) {
      EXPECT_CALL(*mock_cache_observer_,
                  OnCacheCommitted(cache_resources[i].resource_id)).Times(1);
    }
  }
  ASSERT_TRUE(test_util::PrepareTestCacheResources(
      cache_.get(),
      cache_resources));

  std::vector<std::string> resource_ids;
  std::vector<FileCacheEntry> cache_entries;
  bool completed = false;
  cache_->IterateOnUIThread(
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


TEST_F(FileCacheTest, ClearAll) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Store an existing file.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK, test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);

  // Verify that there's only one cached file.
  EXPECT_EQ(1U, CountCacheFiles(resource_id, md5));

  // Clear cache.
  bool success = false;
  cache_->ClearAllOnUIThread(
      google_apis::test_util::CreateCopyResultCallback(&success));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_TRUE(success);

  // Verify that all the cache is removed.
  expected_error_ = FILE_ERROR_OK;
  VerifyRemoveFromCache(FILE_ERROR_OK, resource_id, md5);
  EXPECT_EQ(0U, CountCacheFiles(resource_id, md5));
}

TEST_F(FileCacheTest, StoreToCacheNoSpace) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(0);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Try to store an existing file.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_NO_SPACE,
      test_util::TEST_CACHE_STATE_NONE,
      FileCache::CACHE_TYPE_TMP);

  // Verify that there's no files added.
  EXPECT_EQ(0U, CountCacheFiles(resource_id, md5));
}

// Don't use TEST_F, as we don't want SetUp() and TearDown() for this test.
TEST(FileCacheExtraTest, InitializationFailure) {
  base::MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       &message_loop);

  scoped_refptr<base::SequencedWorkerPool> pool =
      content::BrowserThread::GetBlockingPool();

  // Set the cache root to a non existent path, so the initialization fails.
  scoped_ptr<FileCache, test_util::DestroyHelperForTests> cache(new FileCache(
      base::FilePath::FromUTF8Unsafe("/somewhere/nonexistent/blah/blah"),
      pool->GetSequencedTaskRunner(pool->GetSequenceToken()),
      NULL /* free_disk_space_getter */));

  bool success = true;
  cache->RequestInitialize(
      google_apis::test_util::CreateCopyResultCallback(&success));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_FALSE(success);
}

TEST_F(FileCacheTest, UpdatePinnedCache) {
  fake_free_disk_space_getter_->set_fake_free_disk_space(
      test_util::kLotsOfSpace);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  std::string md5_modified("aaaaaa0000000000");

  // Store an existing file.
  TestStoreToCache(
      resource_id, md5,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/root_feed.json"),
      FILE_ERROR_OK,
      test_util::TEST_CACHE_STATE_PRESENT,
      FileCache::CACHE_TYPE_TMP);

  // Pin the file.
  EXPECT_CALL(*mock_cache_observer_, OnCachePinned(resource_id, md5)).Times(1);
  TestPin(
      resource_id, md5,
      FILE_ERROR_OK,
      test_util::TEST_CACHE_STATE_PRESENT |
      test_util::TEST_CACHE_STATE_PINNED |
      test_util::TEST_CACHE_STATE_PERSISTENT,
      FileCache::CACHE_TYPE_PERSISTENT);

  // Store the file with a modified content and md5. It should stay pinned.
  TestStoreToCache(
      resource_id, md5_modified,
      google_apis::test_util::GetTestFilePath("chromeos/gdata/empty_feed.json"),
      FILE_ERROR_OK,
      test_util::TEST_CACHE_STATE_PRESENT |
      test_util::TEST_CACHE_STATE_PINNED |
      test_util::TEST_CACHE_STATE_PERSISTENT,
      FileCache::CACHE_TYPE_PERSISTENT);
}

}  // namespace internal
}  // namespace drive
