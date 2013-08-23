// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_cache.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_temp_dir.h"
#include "base/md5.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/fake_free_disk_space_getter.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/resource_metadata_storage.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {
namespace {

// Bitmask of cache states in FileCacheEntry.
enum TestFileCacheState {
  TEST_CACHE_STATE_NONE       = 0,
  TEST_CACHE_STATE_PINNED     = 1 << 0,
  TEST_CACHE_STATE_PRESENT    = 1 << 1,
  TEST_CACHE_STATE_DIRTY      = 1 << 2,
};

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

// Tests FileCache methods from UI thread. It internally uses a real blocking
// pool and tests the interaction among threads.
// TODO(hashimoto): remove this class. crbug.com/231221.
class FileCacheTestOnUIThread : public testing::Test {
 protected:
  FileCacheTestOnUIThread() : expected_error_(FILE_ERROR_OK),
                              expected_cache_state_(0) {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(file_util::CreateDirectory(
        temp_dir_.path().Append(util::kMetadataDirectory)));
    ASSERT_TRUE(file_util::CreateDirectory(
        temp_dir_.path().Append(util::kCacheFileDirectory)));

    ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                    &dummy_file_path_));
    fake_free_disk_space_getter_.reset(new FakeFreeDiskSpaceGetter);

    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());

    metadata_storage_.reset(new ResourceMetadataStorage(
        temp_dir_.path(), blocking_task_runner_.get()));

    bool success = false;
    base::PostTaskAndReplyWithResult(
        blocking_task_runner_.get(),
        FROM_HERE,
        base::Bind(&ResourceMetadataStorage::Initialize,
                   base::Unretained(metadata_storage_.get())),
        google_apis::test_util::CreateCopyResultCallback(&success));
    test_util::RunBlockingPoolTask();
    ASSERT_TRUE(success);

    cache_.reset(new FileCache(
        metadata_storage_.get(),
        temp_dir_.path().Append(util::kCacheFileDirectory),
        blocking_task_runner_.get(),
        fake_free_disk_space_getter_.get()));

    success = false;
    base::PostTaskAndReplyWithResult(
        blocking_task_runner_.get(),
        FROM_HERE,
        base::Bind(&FileCache::Initialize, base::Unretained(cache_.get())),
        google_apis::test_util::CreateCopyResultCallback(&success));
    test_util::RunBlockingPoolTask();
    ASSERT_TRUE(success);
  }

  void TestGetFile(const std::string& resource_id,
                   FileError expected_error) {
    FileError error = FILE_ERROR_OK;
    base::FilePath cache_file_path;
    cache_->GetFileOnUIThread(resource_id,
                              google_apis::test_util::CreateCopyResultCallback(
                                  &error, &cache_file_path));
    test_util::RunBlockingPoolTask();

    EXPECT_EQ(expected_error, error);
    if (error == FILE_ERROR_OK) {
      // Verify filename of |cache_file_path|.
      EXPECT_EQ(util::EscapeCacheFileName(resource_id),
                cache_file_path.BaseName().AsUTF8Unsafe());
    } else {
      EXPECT_TRUE(cache_file_path.empty());
    }
  }

  void TestStoreToCache(const std::string& resource_id,
                        const std::string& md5,
                        const base::FilePath& source_path,
                        FileError expected_error,
                        int expected_cache_state) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;

    FileError error = FILE_ERROR_OK;
    cache_->StoreOnUIThread(
        resource_id, md5, source_path,
        FileCache::FILE_OPERATION_COPY,
        google_apis::test_util::CreateCopyResultCallback(&error));
    test_util::RunBlockingPoolTask();

    if (error == FILE_ERROR_OK) {
      FileCacheEntry cache_entry;
      EXPECT_TRUE(GetCacheEntryFromOriginThread(resource_id, &cache_entry));
      EXPECT_EQ(md5, cache_entry.md5());
    }

    VerifyCacheFileState(error, resource_id);
  }

  void TestRemoveFromCache(const std::string& resource_id,
                           FileError expected_error) {
    expected_error_ = expected_error;

    FileError error = FILE_ERROR_OK;
    cache_->RemoveOnUIThread(
        resource_id,
        google_apis::test_util::CreateCopyResultCallback(&error));
    test_util::RunBlockingPoolTask();
    VerifyRemoveFromCache(error, resource_id);
  }

  void VerifyRemoveFromCache(FileError error, const std::string& resource_id) {
    EXPECT_EQ(expected_error_, error);

    FileCacheEntry cache_entry;
    if (!GetCacheEntryFromOriginThread(resource_id, &cache_entry)) {
      EXPECT_EQ(FILE_ERROR_OK, error);

      const base::FilePath path = cache_->GetCacheFilePath(resource_id);
      EXPECT_FALSE(base::PathExists(path));
    }
  }

  void TestPin(const std::string& resource_id,
               FileError expected_error,
               int expected_cache_state) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;

    FileError error = FILE_ERROR_OK;
    cache_->PinOnUIThread(
        resource_id,
        google_apis::test_util::CreateCopyResultCallback(&error));
    test_util::RunBlockingPoolTask();
    VerifyCacheFileState(error, resource_id);
  }

  void TestUnpin(const std::string& resource_id,
                 FileError expected_error,
                 int expected_cache_state) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;

    FileError error = FILE_ERROR_OK;
    cache_->UnpinOnUIThread(
        resource_id,
        google_apis::test_util::CreateCopyResultCallback(&error));
    test_util::RunBlockingPoolTask();
    VerifyCacheFileState(error, resource_id);
  }

  void TestMarkDirty(const std::string& resource_id,
                     FileError expected_error,
                     int expected_cache_state) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;

    FileError error = FILE_ERROR_OK;
    cache_->MarkDirtyOnUIThread(
        resource_id,
        google_apis::test_util::CreateCopyResultCallback(&error));
    test_util::RunBlockingPoolTask();

    VerifyCacheFileState(error, resource_id);

    // Verify filename.
    if (error == FILE_ERROR_OK) {
      base::FilePath cache_file_path;
      cache_->GetFileOnUIThread(
          resource_id,
          google_apis::test_util::CreateCopyResultCallback(
              &error, &cache_file_path));
      test_util::RunBlockingPoolTask();

      EXPECT_EQ(FILE_ERROR_OK, error);
      EXPECT_EQ(util::EscapeCacheFileName(resource_id),
                cache_file_path.BaseName().AsUTF8Unsafe());
    }
  }

  void TestClearDirty(const std::string& resource_id,
                      const std::string& md5,
                      FileError expected_error,
                      int expected_cache_state) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;

    FileError error = FILE_ERROR_OK;
    PostTaskAndReplyWithResult(
        blocking_task_runner_.get(),
        FROM_HERE,
        base::Bind(&FileCache::ClearDirty,
                   base::Unretained(cache_.get()),
                   resource_id,
                   md5),
        google_apis::test_util::CreateCopyResultCallback(&error));
    test_util::RunBlockingPoolTask();

    if (error == FILE_ERROR_OK) {
      FileCacheEntry cache_entry;
      EXPECT_TRUE(GetCacheEntryFromOriginThread(resource_id, &cache_entry));
      EXPECT_EQ(md5, cache_entry.md5());
    }

    VerifyCacheFileState(error, resource_id);
  }

  void TestMarkAsMounted(const std::string& resource_id,
                         FileError expected_error,
                         int expected_cache_state) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;

    FileCacheEntry entry;
    EXPECT_TRUE(GetCacheEntryFromOriginThread(resource_id, &entry));

    FileError error = FILE_ERROR_OK;
    base::FilePath cache_file_path;
    cache_->MarkAsMountedOnUIThread(
        resource_id,
        google_apis::test_util::CreateCopyResultCallback(
            &error, &cache_file_path));
    test_util::RunBlockingPoolTask();

    EXPECT_TRUE(base::PathExists(cache_file_path));
    EXPECT_EQ(cache_file_path, cache_->GetCacheFilePath(resource_id));
  }

  void TestMarkAsUnmounted(const std::string& resource_id,
                           const base::FilePath& file_path,
                           FileError expected_error,
                           int expected_cache_state) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;

    FileError error = FILE_ERROR_OK;
    cache_->MarkAsUnmountedOnUIThread(
        file_path,
        google_apis::test_util::CreateCopyResultCallback(&error));
    test_util::RunBlockingPoolTask();

    base::FilePath cache_file_path;
    cache_->GetFileOnUIThread(
        resource_id,
        google_apis::test_util::CreateCopyResultCallback(
            &error, &cache_file_path));
    test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);

    EXPECT_TRUE(base::PathExists(cache_file_path));
    EXPECT_EQ(cache_file_path, cache_->GetCacheFilePath(resource_id));
  }

  void VerifyCacheFileState(FileError error, const std::string& resource_id) {
    EXPECT_EQ(expected_error_, error);

    // Verify cache map.
    FileCacheEntry cache_entry;
    const bool cache_entry_found =
        GetCacheEntryFromOriginThread(resource_id, &cache_entry);
    if ((expected_cache_state_ & TEST_CACHE_STATE_PRESENT) ||
        (expected_cache_state_ & TEST_CACHE_STATE_PINNED)) {
      ASSERT_TRUE(cache_entry_found);
      EXPECT_EQ((expected_cache_state_ & TEST_CACHE_STATE_PINNED) != 0,
                cache_entry.is_pinned());
      EXPECT_EQ((expected_cache_state_ & TEST_CACHE_STATE_PRESENT) != 0,
                cache_entry.is_present());
      EXPECT_EQ((expected_cache_state_ & TEST_CACHE_STATE_DIRTY) != 0,
                cache_entry.is_dirty());
    } else {
      EXPECT_FALSE(cache_entry_found);
    }

    // Verify actual cache file.
    base::FilePath dest_path = cache_->GetCacheFilePath(resource_id);
    EXPECT_EQ((expected_cache_state_ & TEST_CACHE_STATE_PRESENT) != 0,
              base::PathExists(dest_path));
  }

  // Helper function to call GetCacheEntry from origin thread.
  bool GetCacheEntryFromOriginThread(const std::string& resource_id,
                                     FileCacheEntry* cache_entry) {
    bool result = false;
    cache_->GetCacheEntryOnUIThread(
        resource_id,
        google_apis::test_util::CreateCopyResultCallback(&result, cache_entry));
    test_util::RunBlockingPoolTask();
    return result;
  }

  // Returns true if the cache entry exists for the given resource ID.
  bool CacheEntryExists(const std::string& resource_id) {
    FileCacheEntry cache_entry;
    return GetCacheEntryFromOriginThread(resource_id, &cache_entry);
  }

  // Returns the number of the cache files with name <resource_id>, and Confirm
  // that they have the <md5>. This should return 1 or 0.
  size_t CountCacheFiles(const std::string& resource_id,
                         const std::string& md5) {
    base::FilePath path = cache_->GetCacheFilePath(resource_id);
    base::FileEnumerator enumerator(path.DirName(),
                                    false,  // recursive
                                    base::FileEnumerator::FILES,
                                    path.BaseName().value());
    size_t num_files_found = 0;
    for (base::FilePath current = enumerator.Next(); !current.empty();
         current = enumerator.Next()) {
      ++num_files_found;
      EXPECT_EQ(util::EscapeCacheFileName(resource_id),
                current.BaseName().AsUTF8Unsafe());
    }
    return num_files_found;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  base::ScopedTempDir temp_dir_;
  base::FilePath dummy_file_path_;

  scoped_ptr<ResourceMetadataStorage, test_util::DestroyHelperForTests>
      metadata_storage_;
  scoped_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
  scoped_ptr<FakeFreeDiskSpaceGetter> fake_free_disk_space_getter_;

  FileError expected_error_;
  int expected_cache_state_;
  std::string expected_file_extension_;
};

TEST_F(FileCacheTestOnUIThread, StoreToCacheSimple) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Store an existing file.
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);

  // Store a non-existent file to the same |resource_id| and |md5|.
  TestStoreToCache(resource_id, md5,
                   base::FilePath::FromUTF8Unsafe("non_existent_file"),
                   FILE_ERROR_FAILED,
                   TEST_CACHE_STATE_PRESENT);

  // Store a different existing file to the same |resource_id| but different
  // |md5|.
  md5 = "new_md5";
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);

  // Verify that there's only one file with name <resource_id>, i.e. previously
  // cached file with the different md5 should be deleted.
  EXPECT_EQ(1U, CountCacheFiles(resource_id, md5));
}


TEST_F(FileCacheTestOnUIThread, GetFromCacheSimple) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  // First store a file to cache.
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);

  // Then try to get the existing file from cache.
  TestGetFile(resource_id, FILE_ERROR_OK);

  // Get file from cache with different resource id.
  resource_id = "document:1a2b";
  TestGetFile(resource_id, FILE_ERROR_NOT_FOUND);
}

TEST_F(FileCacheTestOnUIThread, RemoveFromCacheSimple) {
  // Use alphanumeric characters for resource id.
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  // First store a file to cache.
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);

  // Then try to remove existing file from cache.
  TestRemoveFromCache(resource_id, FILE_ERROR_OK);

  // Repeat using non-alphanumeric characters for resource id, including '.'
  // which is an extension separator.
  resource_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);

  TestRemoveFromCache(resource_id, FILE_ERROR_OK);
}

TEST_F(FileCacheTestOnUIThread, PinAndUnpin) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // First store a file to cache.
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);

  // Pin the existing file in cache.
  TestPin(resource_id, FILE_ERROR_OK,
          TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_PINNED);

  // Unpin the existing file in cache.
  TestUnpin(resource_id, FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);

  // Pin back the same existing file in cache.
  TestPin(resource_id, FILE_ERROR_OK,
          TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_PINNED);

  // Pin a non-existent file in cache.
  resource_id = "document:1a2b";

  TestPin(resource_id, FILE_ERROR_OK, TEST_CACHE_STATE_PINNED);

  // Unpin the previously pinned non-existent file in cache.
  TestUnpin(resource_id, FILE_ERROR_OK, TEST_CACHE_STATE_NONE);

  // Unpin a file that doesn't exist in cache and is not pinned, i.e. cache
  // has zero knowledge of the file.
  resource_id = "not-in-cache:1a2b";

  TestUnpin(resource_id, FILE_ERROR_NOT_FOUND, TEST_CACHE_STATE_NONE);
}

TEST_F(FileCacheTestOnUIThread, StoreToCachePinned) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Pin a non-existent file.
  TestPin(resource_id, FILE_ERROR_OK, TEST_CACHE_STATE_PINNED);

  // Store an existing file to a previously pinned file.
  TestStoreToCache(resource_id, md5, dummy_file_path_, FILE_ERROR_OK,
                   TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_PINNED);

  // Store a non-existent file to a previously pinned and stored file.
  TestStoreToCache(resource_id, md5,
                   base::FilePath::FromUTF8Unsafe("non_existent_file"),
                   FILE_ERROR_FAILED,
                   TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_PINNED);
}

TEST_F(FileCacheTestOnUIThread, GetFromCachePinned) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Pin a non-existent file.
  TestPin(resource_id, FILE_ERROR_OK, TEST_CACHE_STATE_PINNED);

  // Get the non-existent pinned file from cache.
  TestGetFile(resource_id, FILE_ERROR_NOT_FOUND);

  // Store an existing file to the previously pinned non-existent file.
  TestStoreToCache(resource_id, md5, dummy_file_path_, FILE_ERROR_OK,
                   TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_PINNED);

  // Get the previously pinned and stored file from cache.
  TestGetFile(resource_id, FILE_ERROR_OK);
}

TEST_F(FileCacheTestOnUIThread, RemoveFromCachePinned) {
  // Use alphanumeric characters for resource_id.
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Store a file to cache, and pin it.
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);
  TestPin(resource_id, FILE_ERROR_OK,
          TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_PINNED);

  // Remove |resource_id| from cache.
  TestRemoveFromCache(resource_id, FILE_ERROR_OK);

  // Repeat using non-alphanumeric characters for resource id, including '.'
  // which is an extension separator.
  resource_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";

  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);
  TestPin(resource_id, FILE_ERROR_OK,
          TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_PINNED);

  TestRemoveFromCache(resource_id, FILE_ERROR_OK);
}

TEST_F(FileCacheTestOnUIThread, DirtyCacheSimple) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // First store a file to cache.
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);

  // Mark the file dirty.
  TestMarkDirty(resource_id, FILE_ERROR_OK,
                TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_DIRTY);

  // Clear dirty state of the file.
  TestClearDirty(resource_id, md5, FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);
}

TEST_F(FileCacheTestOnUIThread, DirtyCachePinned) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // First store a file to cache and pin it.
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);
  TestPin(resource_id, FILE_ERROR_OK,
          TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_PINNED);

  // Mark the file dirty.
  TestMarkDirty(resource_id, FILE_ERROR_OK,
                TEST_CACHE_STATE_PRESENT |
                TEST_CACHE_STATE_DIRTY |
                TEST_CACHE_STATE_PINNED);

  // Clear dirty state of the file.
  TestClearDirty(resource_id, md5, FILE_ERROR_OK,
                 TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_PINNED);
}

TEST_F(FileCacheTestOnUIThread, PinAndUnpinDirtyCache) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // First store a file to cache and mark it as dirty.
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);
  TestMarkDirty(resource_id, FILE_ERROR_OK,
                TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_DIRTY);

  // Verifies dirty file exists.
  base::FilePath dirty_path;
  FileError error = FILE_ERROR_FAILED;
  cache_->GetFileOnUIThread(
      resource_id,
      google_apis::test_util::CreateCopyResultCallback(&error, &dirty_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_TRUE(base::PathExists(dirty_path));

  // Pin the dirty file.
  TestPin(resource_id, FILE_ERROR_OK,
          TEST_CACHE_STATE_PRESENT |
          TEST_CACHE_STATE_DIRTY |
          TEST_CACHE_STATE_PINNED);

  // Verify dirty file still exist at the same pathname.
  EXPECT_TRUE(base::PathExists(dirty_path));

  // Unpin the dirty file.
  TestUnpin(resource_id, FILE_ERROR_OK,
            TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_DIRTY);

  // Verify dirty file still exist at the same pathname.
  EXPECT_TRUE(base::PathExists(dirty_path));
}

TEST_F(FileCacheTestOnUIThread, DirtyCacheRepetitive) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // First store a file to cache.
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);

  // Mark the file dirty.
  TestMarkDirty(resource_id, FILE_ERROR_OK,
                TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_DIRTY);

  // Again, mark the file dirty.  Nothing should change.
  TestMarkDirty(resource_id, FILE_ERROR_OK,
                TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_DIRTY);

  // Clear dirty state of the file.
  TestClearDirty(resource_id, md5, FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);

  // Again, clear dirty state of the file, which is no longer dirty.
  TestClearDirty(resource_id, md5, FILE_ERROR_INVALID_OPERATION,
                 TEST_CACHE_STATE_PRESENT);
}

TEST_F(FileCacheTestOnUIThread, DirtyCacheInvalid) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Mark a non-existent file dirty.
  TestMarkDirty(resource_id, FILE_ERROR_NOT_FOUND, TEST_CACHE_STATE_NONE);

  // Clear dirty state of a non-existent file.
  TestClearDirty(resource_id, md5, FILE_ERROR_NOT_FOUND, TEST_CACHE_STATE_NONE);

  // Store a file to cache.
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);

  // Clear dirty state of a non-dirty existing file.
  TestClearDirty(resource_id, md5, FILE_ERROR_INVALID_OPERATION,
                 TEST_CACHE_STATE_PRESENT);

  // Mark an existing file dirty, then store a new file to the same resource id
  // but different md5, which should fail.
  TestMarkDirty(resource_id, FILE_ERROR_OK,
                TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_DIRTY);
  md5 = "new_md5";
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_IN_USE,
                   TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_DIRTY);
}

TEST_F(FileCacheTestOnUIThread, RemoveFromDirtyCache) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Store a file to cache, pin it, mark it dirty and commit it.
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);
  TestPin(resource_id, FILE_ERROR_OK,
          TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_PINNED);
  TestMarkDirty(resource_id, FILE_ERROR_OK,
                TEST_CACHE_STATE_PRESENT |
                TEST_CACHE_STATE_PINNED |
                TEST_CACHE_STATE_DIRTY);

  // Try to remove the file.  Dirty caches can be removed at the level of
  // FileCache::Remove. Upper layer cache clearance functions like
  // FreeDiskSpaceIfNeededFor() and RemoveStaleCacheFiles() takes care of
  // securing dirty files.
  TestRemoveFromCache(resource_id, FILE_ERROR_OK);
}

TEST_F(FileCacheTestOnUIThread, MountUnmount) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // First store a file to cache.
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);

  // Mark the file mounted.
  TestMarkAsMounted(resource_id, FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);
  EXPECT_TRUE(CacheEntryExists(resource_id));

  // Try to remove the file.
  TestRemoveFromCache(resource_id, FILE_ERROR_IN_USE);

  // Clear mounted state of the file.
  base::FilePath file_path;
  FileError error = FILE_ERROR_FAILED;
  cache_->GetFileOnUIThread(
      resource_id,
      google_apis::test_util::CreateCopyResultCallback(&error, &file_path));
  test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);

  TestMarkAsUnmounted(resource_id, file_path, FILE_ERROR_OK,
                      TEST_CACHE_STATE_PRESENT);
  EXPECT_TRUE(CacheEntryExists(resource_id));

  // Try to remove the file.
  TestRemoveFromCache(resource_id, FILE_ERROR_OK);
}

TEST_F(FileCacheTestOnUIThread, Iterate) {
  const std::vector<test_util::TestCacheResource> cache_resources(
      test_util::GetDefaultTestCacheResources());
  ASSERT_TRUE(test_util::PrepareTestCacheResources(cache_.get(),
                                                   cache_resources));

  std::vector<std::string> resource_ids;
  std::vector<FileCacheEntry> cache_entries;
  bool completed = false;
  cache_->IterateOnUIThread(
      base::Bind(&OnIterate, &resource_ids, &cache_entries),
      base::Bind(&OnIterateCompleted, &completed));
  test_util::RunBlockingPoolTask();

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

TEST_F(FileCacheTestOnUIThread, ClearAll) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Store an existing file.
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_OK, TEST_CACHE_STATE_PRESENT);

  // Verify that there's only one cached file.
  EXPECT_EQ(1U, CountCacheFiles(resource_id, md5));

  // Clear cache.
  bool success = false;
  cache_->ClearAllOnUIThread(
      google_apis::test_util::CreateCopyResultCallback(&success));
  test_util::RunBlockingPoolTask();
  EXPECT_TRUE(success);

  // Verify that all the cache is removed.
  expected_error_ = FILE_ERROR_OK;
  VerifyRemoveFromCache(FILE_ERROR_OK, resource_id);
  EXPECT_EQ(0U, CountCacheFiles(resource_id, md5));
}

TEST_F(FileCacheTestOnUIThread, StoreToCacheNoSpace) {
  fake_free_disk_space_getter_->set_default_value(0);

  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Try to store an existing file.
  TestStoreToCache(resource_id, md5, dummy_file_path_,
                   FILE_ERROR_NO_LOCAL_SPACE,
                   TEST_CACHE_STATE_NONE);

  // Verify that there's no files added.
  EXPECT_EQ(0U, CountCacheFiles(resource_id, md5));
}

TEST_F(FileCacheTestOnUIThread, UpdatePinnedCache) {
  std::string resource_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  std::string md5_modified("aaaaaa0000000000");

  // Store an existing file.
  TestStoreToCache(resource_id, md5, dummy_file_path_, FILE_ERROR_OK,
                   TEST_CACHE_STATE_PRESENT);

  // Pin the file.
  TestPin(resource_id, FILE_ERROR_OK,
          TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_PINNED);

  // Store the file with a modified content and md5. It should stay pinned.
  TestStoreToCache(resource_id, md5_modified, dummy_file_path_, FILE_ERROR_OK,
                   TEST_CACHE_STATE_PRESENT | TEST_CACHE_STATE_PINNED);
}

// Tests FileCache methods working with the blocking task runner.
class FileCacheTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(file_util::CreateDirectory(
        temp_dir_.path().Append(util::kMetadataDirectory)));
    ASSERT_TRUE(file_util::CreateDirectory(
        temp_dir_.path().Append(util::kCacheFileDirectory)));

    fake_free_disk_space_getter_.reset(new FakeFreeDiskSpaceGetter);

    metadata_storage_.reset(new ResourceMetadataStorage(
        temp_dir_.path().Append(util::kMetadataDirectory),
        base::MessageLoopProxy::current().get()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    cache_.reset(new FileCache(
        metadata_storage_.get(),
        temp_dir_.path().Append(util::kCacheFileDirectory),
        base::MessageLoopProxy::current().get(),
        fake_free_disk_space_getter_.get()));
    ASSERT_TRUE(cache_->Initialize());
  }

  static void RenameCacheFilesToNewFormat(FileCache* cache) {
    cache->RenameCacheFilesToNewFormat();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;

  scoped_ptr<ResourceMetadataStorage, test_util::DestroyHelperForTests>
      metadata_storage_;
  scoped_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
  scoped_ptr<FakeFreeDiskSpaceGetter> fake_free_disk_space_getter_;
};

TEST_F(FileCacheTest, ScanCacheFile) {
  // Set up files in the cache directory.
  const base::FilePath file_directory =
      temp_dir_.path().Append(util::kCacheFileDirectory);
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(
      file_directory.AppendASCII("id_foo"), "foo"));

  // Remove the existing DB.
  const base::FilePath metadata_directory =
      temp_dir_.path().Append(util::kMetadataDirectory);
  ASSERT_TRUE(base::DeleteFile(metadata_directory, true /* recursive */));

  // Create a new cache and initialize it.
  metadata_storage_.reset(new ResourceMetadataStorage(
      metadata_directory, base::MessageLoopProxy::current().get()));
  ASSERT_TRUE(metadata_storage_->Initialize());

  cache_.reset(new FileCache(metadata_storage_.get(),
                             temp_dir_.path().Append(util::kCacheFileDirectory),
                             base::MessageLoopProxy::current().get(),
                             fake_free_disk_space_getter_.get()));
  ASSERT_TRUE(cache_->Initialize());

  // Check contents of the cache.
  FileCacheEntry cache_entry;
  EXPECT_TRUE(cache_->GetCacheEntry("id_foo", &cache_entry));
  EXPECT_TRUE(cache_entry.is_present());
  EXPECT_EQ(base::MD5String("foo"), cache_entry.md5());
}

TEST_F(FileCacheTest, FreeDiskSpaceIfNeededFor) {
  base::FilePath src_file;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(), &src_file));

  // Store a file as a 'temporary' file and remember the path.
  const std::string resource_id_tmp = "id_tmp", md5_tmp = "md5_tmp";
  ASSERT_EQ(FILE_ERROR_OK,
            cache_->Store(resource_id_tmp, md5_tmp, src_file,
                          FileCache::FILE_OPERATION_COPY));
  base::FilePath tmp_path;
  ASSERT_EQ(FILE_ERROR_OK, cache_->GetFile(resource_id_tmp, &tmp_path));

  // Store a file as a pinned file and remember the path.
  const std::string resource_id_pinned = "id_pinned", md5_pinned = "md5_pinned";
  ASSERT_EQ(FILE_ERROR_OK,
            cache_->Store(resource_id_pinned, md5_pinned, src_file,
                          FileCache::FILE_OPERATION_COPY));
  ASSERT_EQ(FILE_ERROR_OK, cache_->Pin(resource_id_pinned));
  base::FilePath pinned_path;
  ASSERT_EQ(FILE_ERROR_OK, cache_->GetFile(resource_id_pinned, &pinned_path));

  // Call FreeDiskSpaceIfNeededFor().
  fake_free_disk_space_getter_->set_default_value(test_util::kLotsOfSpace);
  fake_free_disk_space_getter_->PushFakeValue(0);
  const int64 kNeededBytes = 1;
  EXPECT_TRUE(cache_->FreeDiskSpaceIfNeededFor(kNeededBytes));

  // Only 'temporary' file gets removed.
  FileCacheEntry entry;
  EXPECT_FALSE(cache_->GetCacheEntry(resource_id_tmp, &entry));
  EXPECT_FALSE(base::PathExists(tmp_path));

  EXPECT_TRUE(cache_->GetCacheEntry(resource_id_pinned, &entry));
  EXPECT_TRUE(base::PathExists(pinned_path));

  // Returns false when disk space cannot be freed.
  fake_free_disk_space_getter_->set_default_value(0);
  EXPECT_FALSE(cache_->FreeDiskSpaceIfNeededFor(kNeededBytes));
}

TEST_F(FileCacheTest, RenameCacheFilesToNewFormat) {
  const base::FilePath file_directory =
      temp_dir_.path().Append(util::kCacheFileDirectory);

  // File with an old style "<resource ID>.<MD5>" name.
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(
      file_directory.AppendASCII("id_koo.md5"), "koo"));

  // File with multiple extensions should be removed.
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(
      file_directory.AppendASCII("id_kyu.md5.mounted"), "kyu (mounted)"));
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(
      file_directory.AppendASCII("id_kyu.md5"), "kyu"));

  // Rename and verify the result.
  RenameCacheFilesToNewFormat(cache_.get());
  std::string contents;
  EXPECT_TRUE(file_util::ReadFileToString(file_directory.AppendASCII("id_koo"),
                                          &contents));
  EXPECT_EQ("koo", contents);
  contents.clear();
  EXPECT_TRUE(file_util::ReadFileToString(file_directory.AppendASCII("id_kyu"),
                                          &contents));
  EXPECT_EQ("kyu", contents);

  // Rename again.
  RenameCacheFilesToNewFormat(cache_.get());

  // Files with new style names are not affected.
  contents.clear();
  EXPECT_TRUE(file_util::ReadFileToString(file_directory.AppendASCII("id_koo"),
                                          &contents));
  EXPECT_EQ("koo", contents);
  contents.clear();
  EXPECT_TRUE(file_util::ReadFileToString(file_directory.AppendASCII("id_kyu"),
                                          &contents));
  EXPECT_EQ("kyu", contents);
}

}  // namespace internal
}  // namespace drive
