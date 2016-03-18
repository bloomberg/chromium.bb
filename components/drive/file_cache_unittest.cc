// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/file_cache.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/drive/drive.pb.h"
#include "components/drive/drive_test_util.h"
#include "components/drive/fake_free_disk_space_getter.h"
#include "components/drive/file_system_core_util.h"
#include "components/drive/resource_metadata_storage.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {
namespace {

const base::FilePath::CharType kCacheFileDirectory[] =
    FILE_PATH_LITERAL("files");
const base::FilePath::CharType kNewCacheFileDirectory[] =
    FILE_PATH_LITERAL("blobs");

const int kTemporaryFileSizeInBytes = 10;

}  // namespace

// Tests FileCache methods working with the blocking task runner.
class FileCacheTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    const base::FilePath metadata_dir = temp_dir_.path().AppendASCII("meta");
    cache_files_dir_ = temp_dir_.path().Append(kCacheFileDirectory);

    ASSERT_TRUE(base::CreateDirectory(metadata_dir));
    ASSERT_TRUE(base::CreateDirectory(cache_files_dir_));

    fake_free_disk_space_getter_.reset(new FakeFreeDiskSpaceGetter);

    metadata_storage_.reset(new ResourceMetadataStorage(
        metadata_dir,
        base::ThreadTaskRunnerHandle::Get().get()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    cache_.reset(new FileCache(metadata_storage_.get(), cache_files_dir_,
                               base::ThreadTaskRunnerHandle::Get().get(),
                               fake_free_disk_space_getter_.get()));
    ASSERT_TRUE(cache_->Initialize());
  }

  static bool RenameCacheFilesToNewFormat(FileCache* cache) {
    return cache->RenameCacheFilesToNewFormat();
  }

  base::FilePath AddTestEntry(const std::string id,
                              const std::string md5,
                              const time_t last_accessed,
                              const base::FilePath& src_file) {
    ResourceEntry entry;
    entry.set_local_id(id);
    entry.mutable_file_info()->set_last_accessed(last_accessed);
    EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry));
    EXPECT_EQ(FILE_ERROR_OK,
              cache_->Store(id, md5, src_file, FileCache::FILE_OPERATION_COPY));

    base::FilePath path;
    EXPECT_EQ(FILE_ERROR_OK, cache_->GetFile(id, &path));

    // Update last modified and accessed time.
    base::Time time = base::Time::FromTimeT(last_accessed);
    EXPECT_TRUE(base::TouchFile(path, time, time));

    return path;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  base::FilePath cache_files_dir_;

  scoped_ptr<ResourceMetadataStorage, test_util::DestroyHelperForTests>
      metadata_storage_;
  scoped_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
  scoped_ptr<FakeFreeDiskSpaceGetter> fake_free_disk_space_getter_;
};

TEST_F(FileCacheTest, RecoverFilesFromCacheDirectory) {
  base::FilePath dir_source_root;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &dir_source_root));
  const base::FilePath src_path =
      dir_source_root.AppendASCII("chrome/test/data/chromeos/drive/image.png");

  // Store files. This file should not be moved.
  ResourceEntry entry;
  entry.set_local_id("id_foo");
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry));
  EXPECT_EQ(FILE_ERROR_OK, cache_->Store("id_foo", "md5", src_path,
                                         FileCache::FILE_OPERATION_COPY));

  // Set up files in the cache directory. These files should be moved.
  const base::FilePath file_directory =
      temp_dir_.path().Append(kCacheFileDirectory);
  ASSERT_TRUE(base::CopyFile(src_path, file_directory.AppendASCII("id_bar")));
  ASSERT_TRUE(base::CopyFile(src_path, file_directory.AppendASCII("id_baz")));

  // Insert a dirty entry with "id_baz" to |recovered_cache_info|.
  // This should not prevent the file from being recovered.
  ResourceMetadataStorage::RecoveredCacheInfoMap recovered_cache_info;
  recovered_cache_info["id_baz"].is_dirty = true;
  recovered_cache_info["id_baz"].title = "baz.png";

  // Recover files.
  const base::FilePath dest_directory = temp_dir_.path().AppendASCII("dest");
  EXPECT_TRUE(cache_->RecoverFilesFromCacheDirectory(dest_directory,
                                                     recovered_cache_info));

  // Only two files should be recovered.
  EXPECT_TRUE(base::PathExists(dest_directory));
  // base::FileEnumerator does not guarantee the order.
  if (base::PathExists(dest_directory.AppendASCII("baz00000001.png"))) {
    EXPECT_TRUE(base::ContentsEqual(
        src_path,
        dest_directory.AppendASCII("baz00000001.png")));
    EXPECT_TRUE(base::ContentsEqual(
        src_path,
        dest_directory.AppendASCII("image00000002.png")));
  } else {
    EXPECT_TRUE(base::ContentsEqual(
        src_path,
        dest_directory.AppendASCII("image00000001.png")));
    EXPECT_TRUE(base::ContentsEqual(
        src_path,
        dest_directory.AppendASCII("baz00000002.png")));
  }
  EXPECT_FALSE(base::PathExists(
      dest_directory.AppendASCII("image00000003.png")));
}

TEST_F(FileCacheTest, FreeDiskSpaceIfNeededFor) {
  base::FilePath src_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(), &src_file));

  // Store a file as a 'temporary' file and remember the path.
  const std::string id_tmp = "id_tmp", md5_tmp = "md5_tmp";
  const time_t last_accessed_tmp = 1;
  const base::FilePath& tmp_path =
      AddTestEntry(id_tmp, md5_tmp, last_accessed_tmp, src_file);

  // Store a file as a pinned file and remember the path.
  const std::string id_pinned = "id_pinned", md5_pinned = "md5_pinned";
  const time_t last_accessed_pinned = 1;
  const base::FilePath& pinned_path =
      AddTestEntry(id_pinned, md5_pinned, last_accessed_pinned, src_file);
  ASSERT_EQ(FILE_ERROR_OK, cache_->Pin(id_pinned));

  // Call FreeDiskSpaceIfNeededFor().
  fake_free_disk_space_getter_->set_default_value(test_util::kLotsOfSpace);
  fake_free_disk_space_getter_->PushFakeValue(0);
  fake_free_disk_space_getter_->PushFakeValue(0);
  const int64_t kNeededBytes = 1;
  EXPECT_TRUE(cache_->FreeDiskSpaceIfNeededFor(kNeededBytes));

  // Only 'temporary' file gets removed.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id_tmp, &entry));
  EXPECT_FALSE(entry.file_specific_info().cache_state().is_present());
  EXPECT_FALSE(base::PathExists(tmp_path));

  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id_pinned, &entry));
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_present());
  EXPECT_TRUE(base::PathExists(pinned_path));

  // Returns false when disk space cannot be freed.
  fake_free_disk_space_getter_->set_default_value(0);
  EXPECT_FALSE(cache_->FreeDiskSpaceIfNeededFor(kNeededBytes));
}

TEST_F(FileCacheTest, EvictDriveCacheInLRU) {
  // Create temporary file.
  base::FilePath src_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(), &src_file));
  ASSERT_EQ(kTemporaryFileSizeInBytes,
            base::WriteFile(src_file, "abcdefghij", kTemporaryFileSizeInBytes));

  // Add entries.
  const std::string id_a = "id_a", md5_a = "md5_a";
  const time_t last_accessed_a = 1;
  const base::FilePath& a_path =
      AddTestEntry(id_a, md5_a, last_accessed_a, src_file);

  const std::string id_pinned = "id_pinned", md5_pinned = "md5_pinned";
  const time_t last_accessed_pinned = 2;
  const base::FilePath& pinned_path =
      AddTestEntry(id_pinned, md5_pinned, last_accessed_pinned, src_file);
  ASSERT_EQ(FILE_ERROR_OK, cache_->Pin(id_pinned));

  const std::string id_b = "id_b", md5_b = "md5_b";
  const time_t last_accessed_b = 3;
  const base::FilePath& b_path =
      AddTestEntry(id_b, md5_b, last_accessed_b, src_file);

  const std::string id_c = "id_c", md5_c = "md5_c";
  const time_t last_accessed_c = 4;
  const base::FilePath& c_path =
      AddTestEntry(id_c, md5_c, last_accessed_c, src_file);

  // Call FreeDiskSpaceIfNeededFor.
  fake_free_disk_space_getter_->set_default_value(test_util::kLotsOfSpace);
  fake_free_disk_space_getter_->PushFakeValue(kMinFreeSpaceInBytes);
  fake_free_disk_space_getter_->PushFakeValue(kMinFreeSpaceInBytes);
  const int64_t kNeededBytes = kTemporaryFileSizeInBytes * 3 / 2;
  EXPECT_TRUE(cache_->FreeDiskSpaceIfNeededFor(kNeededBytes));

  // Entry A is evicted.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id_a, &entry));
  EXPECT_FALSE(entry.file_specific_info().cache_state().is_present());
  EXPECT_FALSE(base::PathExists(a_path));

  // Pinned entry should not be evicted.
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id_pinned, &entry));
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_present());
  EXPECT_TRUE(base::PathExists(pinned_path));

  // Entry B is evicted.
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id_b, &entry));
  EXPECT_FALSE(entry.file_specific_info().cache_state().is_present());
  EXPECT_FALSE(base::PathExists(b_path));

  // Entry C should not be evicted.
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id_c, &entry));
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_present());
  EXPECT_TRUE(base::PathExists(c_path));
}

// Test case for deleting invalid cache files which don't have corresponding
// metadata.
TEST_F(FileCacheTest, EvictInvalidCacheFile) {
  base::FilePath src_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(), &src_file));

  // Add entries.
  const std::string id_a = "id_a", md5_a = "md5_a";
  const time_t last_accessed_a = 1;
  const base::FilePath& a_path =
      AddTestEntry(id_a, md5_a, last_accessed_a, src_file);

  const std::string id_b = "id_b", md5_b = "md5_b";
  const time_t last_accessed_b = 2;
  const base::FilePath& b_path =
      AddTestEntry(id_b, md5_b, last_accessed_b, src_file);

  // Remove metadata of entry B.
  ASSERT_EQ(FILE_ERROR_OK, metadata_storage_->RemoveEntry(id_b));

  // Confirm cache file of entry B exists.
  ASSERT_TRUE(base::PathExists(b_path));

  // Run FreeDiskSpaceIfNeededFor.
  fake_free_disk_space_getter_->set_default_value(test_util::kLotsOfSpace);
  fake_free_disk_space_getter_->PushFakeValue(kMinFreeSpaceInBytes);
  const int64_t kNeededBytes = 1;
  EXPECT_TRUE(cache_->FreeDiskSpaceIfNeededFor(kNeededBytes));

  // Entry A is not evicted.
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id_a, &entry));
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_present());
  EXPECT_TRUE(base::PathExists(a_path));

  // Entry B is evicted.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, metadata_storage_->GetEntry(id_b, &entry));
  EXPECT_FALSE(base::PathExists(b_path));
}

TEST_F(FileCacheTest, TooManyCacheFiles) {
  const size_t kMaxNumOfEvictedCacheFiles = 50;
  cache_->SetMaxNumOfEvictedCacheFilesForTest(kMaxNumOfEvictedCacheFiles);

  // Create temporary file.
  base::FilePath src_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(), &src_file));
  ASSERT_EQ(kTemporaryFileSizeInBytes,
            base::WriteFile(src_file, "abcdefghij", kTemporaryFileSizeInBytes));

  // Add kNumOfTestFiles=kMaxNumOfEvictedCacheFiles*2 entries.
  std::vector<base::FilePath> paths;
  const int32_t kNumOfTestFiles = kMaxNumOfEvictedCacheFiles * 2;
  for (int i = 0; i < kNumOfTestFiles; ++i) {
    // Set last accessed in reverse order to the file name. i.e. If you sort
    // files in name-asc order, they will be last access desc order.
    paths.push_back(AddTestEntry(
        base::StringPrintf("id_%02d", i), base::StringPrintf("md5_%02d", i),
        kNumOfTestFiles - i /* last accessed */, src_file));
  }

  // Confirm cache files of kNumOfTestFiles actually exist.
  for (const auto& path : paths) {
    ASSERT_TRUE(base::PathExists(path)) << path.value();
  }

  // Try to free kMaxNumOfEvictedCacheFiles * 3 / 2.
  fake_free_disk_space_getter_->set_default_value(test_util::kLotsOfSpace);
  fake_free_disk_space_getter_->PushFakeValue(kMinFreeSpaceInBytes);
  fake_free_disk_space_getter_->PushFakeValue(kMinFreeSpaceInBytes);
  fake_free_disk_space_getter_->PushFakeValue(
      kMinFreeSpaceInBytes +
      (kMaxNumOfEvictedCacheFiles * kTemporaryFileSizeInBytes));
  const int64_t kNeededBytes =
      (kMaxNumOfEvictedCacheFiles * 3 / 2) * kTemporaryFileSizeInBytes;
  EXPECT_FALSE(cache_->FreeDiskSpaceIfNeededFor(kNeededBytes));

  for (uint32_t i = 0; i < kNumOfTestFiles; ++i) {
    // Assert that only first kMaxNumOfEvictedCacheFiles exist.
    ASSERT_EQ(i < kMaxNumOfEvictedCacheFiles, base::PathExists(paths[i]));
  }
}

TEST_F(FileCacheTest, GetFile) {
  const base::FilePath src_file_path = temp_dir_.path().Append("test.dat");
  const std::string src_contents = "test";
  EXPECT_TRUE(google_apis::test_util::WriteStringToFile(src_file_path,
                                                        src_contents));
  std::string id("id1");
  std::string md5(base::MD5String(src_contents));

  const base::FilePath cache_file_directory =
      temp_dir_.path().Append(kCacheFileDirectory);

  // Try to get an existing file from cache.
  ResourceEntry entry;
  entry.set_local_id(id);
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry));
  EXPECT_EQ(FILE_ERROR_OK, cache_->Store(id, md5, src_file_path,
                                         FileCache::FILE_OPERATION_COPY));
  base::FilePath cache_file_path;
  EXPECT_EQ(FILE_ERROR_OK, cache_->GetFile(id, &cache_file_path));
  EXPECT_EQ(
      cache_file_directory.AppendASCII(util::EscapeCacheFileName(id)).value(),
      cache_file_path.value());

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(cache_file_path, &contents));
  EXPECT_EQ(src_contents, contents);

  // Get file from cache with different id.
  id = "id2";
  entry.Clear();
  entry.set_local_id(id);
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry));
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, cache_->GetFile(id, &cache_file_path));

  // Pin a non-existent file.
  EXPECT_EQ(FILE_ERROR_OK, cache_->Pin(id));

  // Get the non-existent pinned file from cache.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, cache_->GetFile(id, &cache_file_path));

  // Get a previously pinned and stored file from cache.
  EXPECT_EQ(FILE_ERROR_OK, cache_->Store(id, md5, src_file_path,
                                         FileCache::FILE_OPERATION_COPY));

  EXPECT_EQ(FILE_ERROR_OK, cache_->GetFile(id, &cache_file_path));
  EXPECT_EQ(
      cache_file_directory.AppendASCII(util::EscapeCacheFileName(id)).value(),
      cache_file_path.value());

  contents.clear();
  EXPECT_TRUE(base::ReadFileToString(cache_file_path, &contents));
  EXPECT_EQ(src_contents, contents);
}

TEST_F(FileCacheTest, Store) {
  const base::FilePath src_file_path = temp_dir_.path().Append("test.dat");
  const std::string src_contents = "test";
  EXPECT_TRUE(google_apis::test_util::WriteStringToFile(src_file_path,
                                                        src_contents));
  std::string id("id");
  std::string md5(base::MD5String(src_contents));

  // Store a file.
  ResourceEntry entry;
  entry.set_local_id(id);
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry));
  EXPECT_EQ(FILE_ERROR_OK, cache_->Store(
      id, md5, src_file_path, FileCache::FILE_OPERATION_COPY));

  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id, &entry));
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_present());
  EXPECT_EQ(md5, entry.file_specific_info().cache_state().md5());

  base::FilePath cache_file_path;
  EXPECT_EQ(FILE_ERROR_OK, cache_->GetFile(id, &cache_file_path));
  EXPECT_TRUE(base::ContentsEqual(src_file_path, cache_file_path));

  // Store a non-existent file.
  EXPECT_EQ(FILE_ERROR_FAILED, cache_->Store(
      id, md5, base::FilePath::FromUTF8Unsafe("non_existent_file"),
      FileCache::FILE_OPERATION_COPY));

  // Passing empty MD5 marks the entry as dirty.
  EXPECT_EQ(FILE_ERROR_OK, cache_->Store(
      id, std::string(), src_file_path, FileCache::FILE_OPERATION_COPY));

  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id, &entry));
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_present());
  EXPECT_TRUE(entry.file_specific_info().cache_state().md5().empty());
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_dirty());

  // No free space available.
  fake_free_disk_space_getter_->set_default_value(0);

  EXPECT_EQ(FILE_ERROR_NO_LOCAL_SPACE, cache_->Store(
      id, md5, src_file_path, FileCache::FILE_OPERATION_COPY));
}

TEST_F(FileCacheTest, PinAndUnpin) {
  const base::FilePath src_file_path = temp_dir_.path().Append("test.dat");
  const std::string src_contents = "test";
  EXPECT_TRUE(google_apis::test_util::WriteStringToFile(src_file_path,
                                                        src_contents));
  std::string id("id_present");
  std::string md5(base::MD5String(src_contents));

  // Store a file.
  ResourceEntry entry;
  entry.set_local_id(id);
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry));
  EXPECT_EQ(FILE_ERROR_OK, cache_->Store(
      id, md5, src_file_path, FileCache::FILE_OPERATION_COPY));

  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id, &entry));
  EXPECT_FALSE(entry.file_specific_info().cache_state().is_pinned());

  // Pin the existing file.
  EXPECT_EQ(FILE_ERROR_OK, cache_->Pin(id));

  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id, &entry));
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_pinned());

  // Unpin the file.
  EXPECT_EQ(FILE_ERROR_OK, cache_->Unpin(id));

  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id, &entry));
  EXPECT_FALSE(entry.file_specific_info().cache_state().is_pinned());

  // Pin a non-present file.
  std::string id_non_present = "id_non_present";
  entry.Clear();
  entry.set_local_id(id_non_present);
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry));
  EXPECT_EQ(FILE_ERROR_OK, cache_->Pin(id_non_present));

  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id_non_present, &entry));
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_pinned());

  // Unpin the previously pinned non-existent file.
  EXPECT_EQ(FILE_ERROR_OK, cache_->Unpin(id_non_present));

  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id_non_present, &entry));
  EXPECT_FALSE(entry.file_specific_info().has_cache_state());

  // Unpin a file that doesn't exist in cache and is not pinned.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND, cache_->Unpin("id_non_existent"));
}

TEST_F(FileCacheTest, MountUnmount) {
  const base::FilePath src_file_path = temp_dir_.path().Append("test.dat");
  const std::string src_contents = "test";
  EXPECT_TRUE(google_apis::test_util::WriteStringToFile(src_file_path,
                                                        src_contents));
  std::string id("id_present");
  std::string md5(base::MD5String(src_contents));

  // Store a file.
  ResourceEntry entry;
  entry.set_local_id(id);
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry));
  EXPECT_EQ(FILE_ERROR_OK, cache_->Store(
      id, md5, src_file_path, FileCache::FILE_OPERATION_COPY));

  // Mark the file mounted.
  base::FilePath cache_file_path;
  EXPECT_EQ(FILE_ERROR_OK, cache_->MarkAsMounted(id, &cache_file_path));

  // Try to remove it.
  EXPECT_EQ(FILE_ERROR_IN_USE, cache_->Remove(id));

  // Clear mounted state of the file.
  EXPECT_EQ(FILE_ERROR_OK, cache_->MarkAsUnmounted(cache_file_path));

  // Try to remove again.
  EXPECT_EQ(FILE_ERROR_OK, cache_->Remove(id));
}

TEST_F(FileCacheTest, OpenForWrite) {
  // Prepare a file.
  base::FilePath src_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(), &src_file));

  const std::string id = "id";
  ResourceEntry entry;
  entry.set_local_id(id);
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry));
  ASSERT_EQ(FILE_ERROR_OK, cache_->Store(id, "md5", src_file,
                                         FileCache::FILE_OPERATION_COPY));
  EXPECT_EQ(0, entry.file_info().last_modified());

  // Entry is not dirty nor opened.
  EXPECT_FALSE(cache_->IsOpenedForWrite(id));
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id, &entry));
  EXPECT_FALSE(entry.file_specific_info().cache_state().is_dirty());

  // Open (1).
  scoped_ptr<base::ScopedClosureRunner> file_closer1;
  EXPECT_EQ(FILE_ERROR_OK, cache_->OpenForWrite(id, &file_closer1));
  EXPECT_TRUE(cache_->IsOpenedForWrite(id));

  // Entry is dirty.
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id, &entry));
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_dirty());

  // Open (2).
  scoped_ptr<base::ScopedClosureRunner> file_closer2;
  EXPECT_EQ(FILE_ERROR_OK, cache_->OpenForWrite(id, &file_closer2));
  EXPECT_TRUE(cache_->IsOpenedForWrite(id));

  // Close (1).
  file_closer1.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(cache_->IsOpenedForWrite(id));

  // last_modified is updated.
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id, &entry));
  EXPECT_NE(0, entry.file_info().last_modified());

  // Close (2).
  file_closer2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(cache_->IsOpenedForWrite(id));

  // Try to open non-existent file.
  EXPECT_EQ(FILE_ERROR_NOT_FOUND,
            cache_->OpenForWrite("nonexistent_id", &file_closer1));
}

TEST_F(FileCacheTest, UpdateMd5) {
  // Store test data.
  const base::FilePath src_file_path = temp_dir_.path().Append("test.dat");
  const std::string contents_before = "before";
  EXPECT_TRUE(google_apis::test_util::WriteStringToFile(src_file_path,
                                                        contents_before));
  std::string id("id1");
  ResourceEntry entry;
  entry.set_local_id(id);
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry));
  EXPECT_EQ(FILE_ERROR_OK, cache_->Store(id, base::MD5String(contents_before),
                                         src_file_path,
                                         FileCache::FILE_OPERATION_COPY));

  // Modify the cache file.
  scoped_ptr<base::ScopedClosureRunner> file_closer;
  EXPECT_EQ(FILE_ERROR_OK, cache_->OpenForWrite(id, &file_closer));
  base::FilePath cache_file_path;
  EXPECT_EQ(FILE_ERROR_OK, cache_->GetFile(id, &cache_file_path));
  const std::string contents_after = "after";
  EXPECT_TRUE(google_apis::test_util::WriteStringToFile(cache_file_path,
                                                        contents_after));

  // Cannot update MD5 of an opend file.
  EXPECT_EQ(FILE_ERROR_IN_USE, cache_->UpdateMd5(id));

  // Close file.
  file_closer.reset();
  base::RunLoop().RunUntilIdle();

  // MD5 was cleared by OpenForWrite().
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id, &entry));
  EXPECT_TRUE(entry.file_specific_info().cache_state().md5().empty());

  // Update MD5.
  EXPECT_EQ(FILE_ERROR_OK, cache_->UpdateMd5(id));
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id, &entry));
  EXPECT_EQ(base::MD5String(contents_after),
            entry.file_specific_info().cache_state().md5());
}

TEST_F(FileCacheTest, ClearDirty) {
  // Prepare a file.
  base::FilePath src_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(), &src_file));

  const std::string id = "id";
  ResourceEntry entry;
  entry.set_local_id(id);
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry));
  ASSERT_EQ(FILE_ERROR_OK, cache_->Store(id, "md5", src_file,
                                         FileCache::FILE_OPERATION_COPY));

  // Open the file.
  scoped_ptr<base::ScopedClosureRunner> file_closer;
  EXPECT_EQ(FILE_ERROR_OK, cache_->OpenForWrite(id, &file_closer));

  // Entry is dirty.
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id, &entry));
  EXPECT_TRUE(entry.file_specific_info().cache_state().is_dirty());

  // Cannot clear the dirty bit of an opened entry.
  EXPECT_EQ(FILE_ERROR_IN_USE, cache_->ClearDirty(id));

  // Close the file and clear the dirty bit.
  file_closer.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, cache_->ClearDirty(id));

  // Entry is not dirty.
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id, &entry));
  EXPECT_FALSE(entry.file_specific_info().cache_state().is_dirty());
}

TEST_F(FileCacheTest, Remove) {
  const base::FilePath src_file_path = temp_dir_.path().Append("test.dat");
  const std::string src_contents = "test";
  EXPECT_TRUE(google_apis::test_util::WriteStringToFile(src_file_path,
                                                        src_contents));
  std::string id("id");
  std::string md5(base::MD5String(src_contents));

  // First store a file to cache.
  ResourceEntry entry;
  entry.set_local_id(id);
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry));
  base::FilePath src_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(), &src_file));
  EXPECT_EQ(FILE_ERROR_OK, cache_->Store(
      id, md5, src_file_path, FileCache::FILE_OPERATION_COPY));

  base::FilePath cache_file_path;
  EXPECT_EQ(FILE_ERROR_OK, cache_->GetFile(id, &cache_file_path));

  // Then try to remove existing file from cache.
  EXPECT_EQ(FILE_ERROR_OK, cache_->Remove(id));
  EXPECT_FALSE(base::PathExists(cache_file_path));
}

TEST_F(FileCacheTest, RenameCacheFilesToNewFormat) {
  const base::FilePath file_directory =
      temp_dir_.path().Append(kCacheFileDirectory);

  // File with an old style "<prefix>:<ID>.<MD5>" name.
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(
      file_directory.AppendASCII("file:id_koo.md5"), "koo"));

  // File with multiple extensions should be removed.
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(
      file_directory.AppendASCII("id_kyu.md5.mounted"), "kyu (mounted)"));
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(
      file_directory.AppendASCII("id_kyu.md5"), "kyu"));

  // Rename and verify the result.
  EXPECT_TRUE(RenameCacheFilesToNewFormat(cache_.get()));
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(file_directory.AppendASCII("id_koo"),
                                     &contents));
  EXPECT_EQ("koo", contents);
  contents.clear();
  EXPECT_TRUE(base::ReadFileToString(file_directory.AppendASCII("id_kyu"),
                                     &contents));
  EXPECT_EQ("kyu", contents);

  // Rename again.
  EXPECT_TRUE(RenameCacheFilesToNewFormat(cache_.get()));

  // Files with new style names are not affected.
  contents.clear();
  EXPECT_TRUE(base::ReadFileToString(file_directory.AppendASCII("id_koo"),
                                     &contents));
  EXPECT_EQ("koo", contents);
  contents.clear();
  EXPECT_TRUE(base::ReadFileToString(file_directory.AppendASCII("id_kyu"),
                                     &contents));
  EXPECT_EQ("kyu", contents);
}

// Test for migrating cache files from files to blobs.
TEST_F(FileCacheTest, MigrateCacheFiles) {
  // Create test files and metadata.
  base::FilePath temp_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(), &temp_file));

  const base::FilePath old_cache_dir =
      temp_dir_.path().Append(kCacheFileDirectory);
  const base::FilePath new_cache_dir =
      temp_dir_.path().Append(kNewCacheFileDirectory);
  ASSERT_TRUE(base::CreateDirectory(old_cache_dir));
  ASSERT_TRUE(base::CreateDirectory(new_cache_dir));

  // Entry A: cache file in old cache directory with metadata.
  const std::string id_a = "id_a";
  ResourceEntry entry_a;
  entry_a.set_local_id(id_a);
  entry_a.mutable_file_specific_info()->mutable_cache_state()->set_is_present(
      true);
  ASSERT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry_a));
  const base::FilePath old_file_path_a = old_cache_dir.AppendASCII(id_a);
  const base::FilePath new_file_path_a = new_cache_dir.AppendASCII(id_a);
  ASSERT_TRUE(base::CopyFile(temp_file, old_file_path_a));

  // Entry B: cache file in old cache directory without metadata.
  const std::string id_b = "id_b";
  const base::FilePath old_file_path_b = old_cache_dir.AppendASCII(id_b);
  ASSERT_TRUE(base::CopyFile(temp_file, old_file_path_b));

  // Entry C: already migrated cache file.
  const std::string id_c = "id_c";
  ResourceEntry entry_c;
  entry_c.set_local_id(id_c);
  entry_c.mutable_file_specific_info()->mutable_cache_state()->set_is_present(
      true);
  ASSERT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry_c));
  const base::FilePath new_file_path_c = new_cache_dir.AppendASCII(id_c);
  ASSERT_TRUE(base::CopyFile(temp_file, new_file_path_c));

  // Entry D: metadata entry without cache file.
  const std::string id_d = "id_d";
  ResourceEntry entry_d;
  entry_d.set_local_id(id_d);
  entry_d.mutable_file_specific_info()->mutable_cache_state()->set_is_present(
      true);
  ASSERT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry_d));

  // Run migration.
  ASSERT_TRUE(FileCache::MigrateCacheFiles(old_cache_dir, new_cache_dir,
                                           metadata_storage_.get()));

  // Check result.
  EXPECT_FALSE(base::PathExists(old_file_path_a));
  EXPECT_TRUE(base::PathExists(new_file_path_a));
  // MigrateCacheFiles doesn't delete invalid cache file.
  EXPECT_TRUE(base::PathExists(old_file_path_b));
  EXPECT_TRUE(base::PathExists(new_file_path_c));
}

TEST_F(FileCacheTest, ClearAll) {
  const std::string id("1a2b");
  const std::string md5("abcdef0123456789");

  // Store an existing file.
  ResourceEntry entry;
  entry.set_local_id(id);
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry));
  base::FilePath src_file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(), &src_file));
  ASSERT_EQ(FILE_ERROR_OK,
            cache_->Store(id, md5, src_file, FileCache::FILE_OPERATION_COPY));

  // Clear cache.
  EXPECT_TRUE(cache_->ClearAll());

  // Verify that the cache is removed.
  EXPECT_TRUE(base::IsDirectoryEmpty(cache_files_dir_));
}

}  // namespace internal
}  // namespace drive
