// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/mapped_file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Implementation of FileIOCallback for the tests.
class FileCallbackTest: public disk_cache::FileIOCallback {
 public:
  FileCallbackTest(int id, MessageLoopHelper* helper, int* max_id)
      : id_(id),
        helper_(helper),
        max_id_(max_id) {
  }
  virtual ~FileCallbackTest() {}

  virtual void OnFileIOComplete(int bytes_copied) OVERRIDE;

 private:
  int id_;
  MessageLoopHelper* helper_;
  int* max_id_;
};

void FileCallbackTest::OnFileIOComplete(int bytes_copied) {
  if (id_ > *max_id_) {
    NOTREACHED();
    helper_->set_callback_reused_error(true);
  }

  helper_->CallbackWasCalled();
}

}  // namespace

TEST_F(DiskCacheTest, MappedFile_SyncIO) {
  base::FilePath filename = cache_path_.AppendASCII("a_test");
  scoped_refptr<disk_cache::MappedFile> file(new disk_cache::MappedFile);
  ASSERT_TRUE(CreateCacheTestFile(filename));
  ASSERT_TRUE(file->Init(filename, 8192));

  char buffer1[20];
  char buffer2[20];
  CacheTestFillBuffer(buffer1, sizeof(buffer1), false);
  base::strlcpy(buffer1, "the data", arraysize(buffer1));
  EXPECT_TRUE(file->Write(buffer1, sizeof(buffer1), 8192));
  EXPECT_TRUE(file->Read(buffer2, sizeof(buffer2), 8192));
  EXPECT_STREQ(buffer1, buffer2);
}

TEST_F(DiskCacheTest, MappedFile_AsyncIO) {
  base::FilePath filename = cache_path_.AppendASCII("a_test");
  scoped_refptr<disk_cache::MappedFile> file(new disk_cache::MappedFile);
  ASSERT_TRUE(CreateCacheTestFile(filename));
  ASSERT_TRUE(file->Init(filename, 8192));

  int max_id = 0;
  MessageLoopHelper helper;
  FileCallbackTest callback(1, &helper, &max_id);

  char buffer1[20];
  char buffer2[20];
  CacheTestFillBuffer(buffer1, sizeof(buffer1), false);
  base::strlcpy(buffer1, "the data", arraysize(buffer1));
  bool completed;
  EXPECT_TRUE(file->Write(buffer1, sizeof(buffer1), 1024 * 1024, &callback,
              &completed));
  int expected = completed ? 0 : 1;

  max_id = 1;
  helper.WaitUntilCacheIoFinished(expected);

  EXPECT_TRUE(file->Read(buffer2, sizeof(buffer2), 1024 * 1024, &callback,
              &completed));
  if (!completed)
    expected++;

  helper.WaitUntilCacheIoFinished(expected);

  EXPECT_EQ(expected, helper.callbacks_called());
  EXPECT_FALSE(helper.callback_reused_error());
  EXPECT_STREQ(buffer1, buffer2);
}

TEST_F(DiskCacheTest, MappedFile_MemoryAccess) {
  const size_t page_size = 4096;
  const size_t buffer_size = 20;
  size_t file_sizes[] = { 2 * page_size,
                          8 * page_size + buffer_size};
  bool full_writes[] = { false, true };
  for (size_t i = 0; i < arraysize(file_sizes); ++i) {
    size_t file_size = file_sizes[i];
    for (size_t j = 0; j < arraysize(full_writes); ++j) {
      bool full_write = full_writes[j];
      base::FilePath filename = cache_path_.AppendASCII("a_test");
      scoped_refptr<disk_cache::MappedFile> file(new disk_cache::MappedFile);
      ASSERT_TRUE(CreateCacheTestFileWithSize(filename, file_size));
      ASSERT_TRUE(file->Init(filename, file_size));

      char buffer1[buffer_size];
      char buffer2[buffer_size];
      CacheTestFillBuffer(buffer1, buffer_size, false);

      char* buffer = reinterpret_cast<char*>(file->buffer());

      memcpy(buffer, buffer1, buffer_size);
      if (full_write) {
        for (size_t k = page_size; k <= file_size / 2; k += page_size) {
          memcpy(buffer + k, buffer1, buffer_size);
        }
      }

      file->Flush();

      file->Read(buffer2, buffer_size, 0);
      EXPECT_EQ(0, strncmp(buffer1, buffer2, buffer_size));
      if (full_write) {
        for (size_t k = page_size; k <= file_size / 2; k += page_size) {
          file->Read(buffer2, buffer_size, k);
          EXPECT_EQ(0, strncmp(buffer1, buffer2, buffer_size));
        }
      }

      // Checking writes at the end of the file.
      memcpy(buffer + file_size - buffer_size, buffer1, buffer_size);
      file->Flush();
      file->Read(buffer2, buffer_size, file_size - buffer_size);
      EXPECT_EQ(0, strncmp(buffer1, buffer2, buffer_size));
      file->Flush();

      EXPECT_EQ(file_size, file->GetLength());
    }
  }
}
