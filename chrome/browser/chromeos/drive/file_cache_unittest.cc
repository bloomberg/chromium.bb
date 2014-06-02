// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_cache.h"

#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_temp_dir.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/fake_free_disk_space_getter.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/resource_metadata_storage.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {
namespace {

const char kCacheFileDirectory[] = "files";

}  // namespace

// Tests FileCache methods working with the blocking task runner.
class FileCacheTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    const base::FilePath metadata_dir = temp_dir_.path().AppendASCII("meta");
    cache_files_dir_ = temp_dir_.path().AppendASCII(kCacheFileDirectory);

    ASSERT_TRUE(base::CreateDirectory(metadata_dir));
    ASSERT_TRUE(base::CreateDirectory(cache_files_dir_));

    fake_free_disk_space_getter_.reset(new FakeFreeDiskSpaceGetter);

    metadata_storage_.reset(new ResourceMetadataStorage(
        metadata_dir,
        base::MessageLoopProxy::current().get()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    cache_.reset(new FileCache(
        metadata_storage_.get(),
        cache_files_dir_,
        base::MessageLoopProxy::current().get(),
        fake_free_disk_space_getter_.get()));
    ASSERT_TRUE(cache_->Initialize());
  }

  static bool RenameCacheFilesToNewFormat(FileCache* cache) {
    return cache->RenameCacheFilesToNewFormat();
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
      temp_dir_.path().AppendASCII(kCacheFileDirectory);
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

  ResourceEntry entry;
  entry.set_local_id(id_tmp);
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry));
  ASSERT_EQ(FILE_ERROR_OK,
            cache_->Store(id_tmp, md5_tmp, src_file,
                          FileCache::FILE_OPERATION_COPY));
  base::FilePath tmp_path;
  ASSERT_EQ(FILE_ERROR_OK, cache_->GetFile(id_tmp, &tmp_path));

  // Store a file as a pinned file and remember the path.
  const std::string id_pinned = "id_pinned", md5_pinned = "md5_pinned";
  entry.Clear();
  entry.set_local_id(id_pinned);
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->PutEntry(entry));
  ASSERT_EQ(FILE_ERROR_OK,
            cache_->Store(id_pinned, md5_pinned, src_file,
                          FileCache::FILE_OPERATION_COPY));
  ASSERT_EQ(FILE_ERROR_OK, cache_->Pin(id_pinned));
  base::FilePath pinned_path;
  ASSERT_EQ(FILE_ERROR_OK, cache_->GetFile(id_pinned, &pinned_path));

  // Call FreeDiskSpaceIfNeededFor().
  fake_free_disk_space_getter_->set_default_value(test_util::kLotsOfSpace);
  fake_free_disk_space_getter_->PushFakeValue(0);
  const int64 kNeededBytes = 1;
  EXPECT_TRUE(cache_->FreeDiskSpaceIfNeededFor(kNeededBytes));

  // Only 'temporary' file gets removed.
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

TEST_F(FileCacheTest, GetFile) {
  const base::FilePath src_file_path = temp_dir_.path().Append("test.dat");
  const std::string src_contents = "test";
  EXPECT_TRUE(google_apis::test_util::WriteStringToFile(src_file_path,
                                                        src_contents));
  std::string id("id1");
  std::string md5(base::MD5String(src_contents));

  const base::FilePath cache_file_directory =
      temp_dir_.path().AppendASCII(kCacheFileDirectory);

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
  EXPECT_TRUE(cache_->IsOpenedForWrite(id));

  // last_modified is updated.
  EXPECT_EQ(FILE_ERROR_OK, metadata_storage_->GetEntry(id, &entry));
  EXPECT_NE(0, entry.file_info().last_modified());

  // Close (2).
  file_closer2.reset();
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
      temp_dir_.path().AppendASCII(kCacheFileDirectory);

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

TEST_F(FileCacheTest, ClearAll) {
  const std::string id("pdf:1a2b");
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
