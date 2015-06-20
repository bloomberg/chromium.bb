// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file.h"
#include "components/filesystem/files_test_base.h"
#include "mojo/platform_handle/platform_handle_functions.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/util/capture_util.h"

using mojo::Capture;

namespace filesystem {
namespace {

using FileImplTest = FilesTestBase;

TEST_F(FileImplTest, CreateWriteCloseRenameOpenRead) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  {
    // Create my_file.
    FilePtr file;
    error = FILE_ERROR_FAILED;
    directory->OpenFile("my_file", GetProxy(&file), kFlagWrite | kFlagCreate,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('h'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    error = FILE_ERROR_FAILED;
    uint32_t num_bytes_written = 0;
    file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
                WHENCE_FROM_CURRENT, Capture(&error, &num_bytes_written));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = FILE_ERROR_FAILED;
    file->Close(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
  }

  // Rename it.
  error = FILE_ERROR_FAILED;
  directory->Rename("my_file", "your_file", Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  {
    // Open my_file again.
    FilePtr file;
    error = FILE_ERROR_FAILED;
    directory->OpenFile("your_file", GetProxy(&file), kFlagRead | kFlagOpen,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Read from it.
    mojo::Array<uint8_t> bytes_read;
    error = FILE_ERROR_FAILED;
    file->Read(3, 1, WHENCE_FROM_BEGIN, Capture(&error, &bytes_read));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
    ASSERT_EQ(3u, bytes_read.size());
    EXPECT_EQ(static_cast<uint8_t>('e'), bytes_read[0]);
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read[1]);
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read[2]);
  }

  // TODO(vtl): Test read/write offset options.
}

TEST_F(FileImplTest, CantWriteInReadMode) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  std::vector<uint8_t> bytes_to_write;
  bytes_to_write.push_back(static_cast<uint8_t>('h'));
  bytes_to_write.push_back(static_cast<uint8_t>('e'));
  bytes_to_write.push_back(static_cast<uint8_t>('l'));
  bytes_to_write.push_back(static_cast<uint8_t>('l'));
  bytes_to_write.push_back(static_cast<uint8_t>('o'));

  {
    // Create my_file.
    FilePtr file;
    error = FILE_ERROR_FAILED;
    directory->OpenFile("my_file", GetProxy(&file), kFlagWrite | kFlagCreate,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Write to it.
    error = FILE_ERROR_FAILED;
    uint32_t num_bytes_written = 0;
    file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
                WHENCE_FROM_CURRENT, Capture(&error, &num_bytes_written));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = FILE_ERROR_FAILED;
    file->Close(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
  }

  {
    // Open my_file again, this time with read only mode.
    FilePtr file;
    error = FILE_ERROR_FAILED;
    directory->OpenFile("my_file", GetProxy(&file), kFlagRead | kFlagOpen,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Try to write in read mode; it should fail.
    error = FILE_ERROR_OK;
    uint32_t num_bytes_written = 0;
    file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
                WHENCE_FROM_CURRENT, Capture(&error, &num_bytes_written));

    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_FAILED, error);
    EXPECT_EQ(0u, num_bytes_written);

    // Close it.
    error = FILE_ERROR_FAILED;
    file->Close(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
  }
}

TEST_F(FileImplTest, OpenInAppendMode) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  {
    // Create my_file.
    FilePtr file;
    error = FILE_ERROR_FAILED;
    directory->OpenFile("my_file", GetProxy(&file), kFlagWrite | kFlagCreate,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('h'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    error = FILE_ERROR_FAILED;
    uint32_t num_bytes_written = 0;
    file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
                WHENCE_FROM_CURRENT, Capture(&error, &num_bytes_written));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = FILE_ERROR_FAILED;
    file->Close(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
  }

  {
    // Append to my_file.
    FilePtr file;
    error = FILE_ERROR_FAILED;
    directory->OpenFile("my_file", GetProxy(&file), kFlagAppend | kFlagOpen,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('g'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    bytes_to_write.push_back(static_cast<uint8_t>('d'));
    bytes_to_write.push_back(static_cast<uint8_t>('b'));
    bytes_to_write.push_back(static_cast<uint8_t>('y'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    error = FILE_ERROR_FAILED;
    uint32_t num_bytes_written = 0;
    file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
                WHENCE_FROM_CURRENT, Capture(&error, &num_bytes_written));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = FILE_ERROR_FAILED;
    file->Close(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
  }

  {
    // Open my_file again.
    FilePtr file;
    error = FILE_ERROR_FAILED;
    directory->OpenFile("my_file", GetProxy(&file), kFlagRead | kFlagOpen,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Read from it.
    mojo::Array<uint8_t> bytes_read;
    error = FILE_ERROR_FAILED;
    file->Read(12, 0, WHENCE_FROM_BEGIN, Capture(&error, &bytes_read));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
    ASSERT_EQ(12u, bytes_read.size());
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read[3]);
    EXPECT_EQ(static_cast<uint8_t>('o'), bytes_read[4]);
    EXPECT_EQ(static_cast<uint8_t>('g'), bytes_read[5]);
    EXPECT_EQ(static_cast<uint8_t>('o'), bytes_read[6]);
  }
}

TEST_F(FileImplTest, OpenInTruncateMode) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  {
    // Create my_file.
    FilePtr file;
    error = FILE_ERROR_FAILED;
    directory->OpenFile("my_file", GetProxy(&file), kFlagWrite | kFlagCreate,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('h'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('l'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    error = FILE_ERROR_FAILED;
    uint32_t num_bytes_written = 0;
    file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
                WHENCE_FROM_CURRENT, Capture(&error, &num_bytes_written));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = FILE_ERROR_FAILED;
    file->Close(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
  }

  {
    // Append to my_file.
    FilePtr file;
    error = FILE_ERROR_FAILED;
    directory->OpenFile("my_file", GetProxy(&file),
                        kFlagWrite | kFlagOpenTruncated, Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Write to it.
    std::vector<uint8_t> bytes_to_write;
    bytes_to_write.push_back(static_cast<uint8_t>('g'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    bytes_to_write.push_back(static_cast<uint8_t>('o'));
    bytes_to_write.push_back(static_cast<uint8_t>('d'));
    bytes_to_write.push_back(static_cast<uint8_t>('b'));
    bytes_to_write.push_back(static_cast<uint8_t>('y'));
    bytes_to_write.push_back(static_cast<uint8_t>('e'));
    error = FILE_ERROR_FAILED;
    uint32_t num_bytes_written = 0;
    file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
                WHENCE_FROM_CURRENT, Capture(&error, &num_bytes_written));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
    EXPECT_EQ(bytes_to_write.size(), num_bytes_written);

    // Close it.
    error = FILE_ERROR_FAILED;
    file->Close(Capture(&error));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
  }

  {
    // Open my_file again.
    FilePtr file;
    error = FILE_ERROR_FAILED;
    directory->OpenFile("my_file", GetProxy(&file), kFlagRead | kFlagOpen,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Read from it.
    mojo::Array<uint8_t> bytes_read;
    error = FILE_ERROR_FAILED;
    file->Read(7, 0, WHENCE_FROM_BEGIN, Capture(&error, &bytes_read));
    ASSERT_TRUE(file.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
    ASSERT_EQ(7u, bytes_read.size());
    EXPECT_EQ(static_cast<uint8_t>('g'), bytes_read[0]);
    EXPECT_EQ(static_cast<uint8_t>('o'), bytes_read[1]);
    EXPECT_EQ(static_cast<uint8_t>('o'), bytes_read[2]);
    EXPECT_EQ(static_cast<uint8_t>('d'), bytes_read[3]);
  }
}

// Note: Ignore nanoseconds, since it may not always be supported. We expect at
// least second-resolution support though.
TEST_F(FileImplTest, StatTouch) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  // Create my_file.
  FilePtr file;
  error = FILE_ERROR_FAILED;
  directory->OpenFile("my_file", GetProxy(&file), kFlagWrite | kFlagCreate,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Stat it.
  error = FILE_ERROR_FAILED;
  FileInformationPtr file_info;
  file->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(FS_FILE_TYPE_REGULAR_FILE, file_info->type);
  EXPECT_EQ(0, file_info->size);
  EXPECT_GT(file_info->atime, 0);  // Expect that it's not 1970-01-01.
  EXPECT_GT(file_info->mtime, 0);
  double first_mtime = file_info->mtime;

  // Touch only the atime.
  error = FILE_ERROR_FAILED;
  TimespecOrNowPtr t(TimespecOrNow::New());
  t->now = false;
  const int64_t kPartyTime1 = 1234567890;  // Party like it's 2009-02-13.
  t->seconds = kPartyTime1;
  file->Touch(t.Pass(), nullptr, Capture(&error));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Stat again.
  error = FILE_ERROR_FAILED;
  file_info.reset();
  file->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(kPartyTime1, file_info->atime);
  EXPECT_EQ(first_mtime, file_info->mtime);

  // Touch only the mtime.
  t = TimespecOrNow::New();
  t->now = false;
  const int64_t kPartyTime2 = 1425059525;  // No time like the present.
  t->seconds = kPartyTime2;
  file->Touch(nullptr, t.Pass(), Capture(&error));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Stat again.
  error = FILE_ERROR_FAILED;
  file_info.reset();
  file->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(kPartyTime1, file_info->atime);
  EXPECT_EQ(kPartyTime2, file_info->mtime);

  // TODO(vtl): Also test non-zero file size.
  // TODO(vtl): Also test Touch() "now" options.
  // TODO(vtl): Also test touching both atime and mtime.
}

TEST_F(FileImplTest, TellSeek) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  // Create my_file.
  FilePtr file;
  error = FILE_ERROR_FAILED;
  directory->OpenFile("my_file", GetProxy(&file), kFlagWrite | kFlagCreate,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Write to it.
  std::vector<uint8_t> bytes_to_write(1000, '!');
  error = FILE_ERROR_FAILED;
  uint32_t num_bytes_written = 0;
  file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
              WHENCE_FROM_CURRENT, Capture(&error, &num_bytes_written));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(bytes_to_write.size(), num_bytes_written);
  const int size = static_cast<int>(num_bytes_written);

  // Tell.
  error = FILE_ERROR_FAILED;
  int64_t position = -1;
  file->Tell(Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  // Should be at the end.
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(size, position);

  // Seek back 100.
  error = FILE_ERROR_FAILED;
  position = -1;
  file->Seek(-100, WHENCE_FROM_CURRENT, Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(size - 100, position);

  // Tell.
  error = FILE_ERROR_FAILED;
  position = -1;
  file->Tell(Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(size - 100, position);

  // Seek to 123 from start.
  error = FILE_ERROR_FAILED;
  position = -1;
  file->Seek(123, WHENCE_FROM_BEGIN, Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(123, position);

  // Tell.
  error = FILE_ERROR_FAILED;
  position = -1;
  file->Tell(Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(123, position);

  // Seek to 123 back from end.
  error = FILE_ERROR_FAILED;
  position = -1;
  file->Seek(-123, WHENCE_FROM_END, Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(size - 123, position);

  // Tell.
  error = FILE_ERROR_FAILED;
  position = -1;
  file->Tell(Capture(&error, &position));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(size - 123, position);

  // TODO(vtl): Check that seeking actually affects reading/writing.
  // TODO(vtl): Check that seeking can extend the file?
}

TEST_F(FileImplTest, Dup) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  // Create my_file.
  FilePtr file1;
  error = FILE_ERROR_FAILED;
  directory->OpenFile("my_file", GetProxy(&file1),
                      kFlagRead | kFlagWrite | kFlagCreate, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Write to it.
  std::vector<uint8_t> bytes_to_write;
  bytes_to_write.push_back(static_cast<uint8_t>('h'));
  bytes_to_write.push_back(static_cast<uint8_t>('e'));
  bytes_to_write.push_back(static_cast<uint8_t>('l'));
  bytes_to_write.push_back(static_cast<uint8_t>('l'));
  bytes_to_write.push_back(static_cast<uint8_t>('o'));
  error = FILE_ERROR_FAILED;
  uint32_t num_bytes_written = 0;
  file1->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
               WHENCE_FROM_CURRENT, Capture(&error, &num_bytes_written));
  ASSERT_TRUE(file1.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(bytes_to_write.size(), num_bytes_written);
  const int end_hello_pos = static_cast<int>(num_bytes_written);

  // Dup it.
  FilePtr file2;
  error = FILE_ERROR_FAILED;
  file1->Dup(GetProxy(&file2), Capture(&error));
  ASSERT_TRUE(file1.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  // |file2| should have the same position.
  error = FILE_ERROR_FAILED;
  int64_t position = -1;
  file2->Tell(Capture(&error, &position));
  ASSERT_TRUE(file2.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(end_hello_pos, position);

  // Write using |file2|.
  std::vector<uint8_t> more_bytes_to_write;
  more_bytes_to_write.push_back(static_cast<uint8_t>('w'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('o'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('r'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('l'));
  more_bytes_to_write.push_back(static_cast<uint8_t>('d'));
  error = FILE_ERROR_FAILED;
  num_bytes_written = 0;
  file2->Write(mojo::Array<uint8_t>::From(more_bytes_to_write), 0,
               WHENCE_FROM_CURRENT, Capture(&error, &num_bytes_written));
  ASSERT_TRUE(file2.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(more_bytes_to_write.size(), num_bytes_written);
  const int end_world_pos = end_hello_pos + static_cast<int>(num_bytes_written);

  // |file1| should have the same position.
  error = FILE_ERROR_FAILED;
  position = -1;
  file1->Tell(Capture(&error, &position));
  ASSERT_TRUE(file1.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(end_world_pos, position);

  // Close |file1|.
  error = FILE_ERROR_FAILED;
  file1->Close(Capture(&error));
  ASSERT_TRUE(file1.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Read everything using |file2|.
  mojo::Array<uint8_t> bytes_read;
  error = FILE_ERROR_FAILED;
  file2->Read(1000, 0, WHENCE_FROM_BEGIN, Capture(&error, &bytes_read));
  ASSERT_TRUE(file2.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(static_cast<size_t>(end_world_pos), bytes_read.size());
  // Just check the first and last bytes.
  EXPECT_EQ(static_cast<uint8_t>('h'), bytes_read[0]);
  EXPECT_EQ(static_cast<uint8_t>('d'), bytes_read[end_world_pos - 1]);

  // TODO(vtl): Test that |file2| has the same open options as |file1|.
}

TEST_F(FileImplTest, Truncate) {
  const uint32_t kInitialSize = 1000;
  const uint32_t kTruncatedSize = 654;

  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  // Create my_file.
  FilePtr file;
  error = FILE_ERROR_FAILED;
  directory->OpenFile("my_file", GetProxy(&file), kFlagWrite | kFlagCreate,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Write to it.
  std::vector<uint8_t> bytes_to_write(kInitialSize, '!');
  error = FILE_ERROR_FAILED;
  uint32_t num_bytes_written = 0;
  file->Write(mojo::Array<uint8_t>::From(bytes_to_write), 0,
              WHENCE_FROM_CURRENT, Capture(&error, &num_bytes_written));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(kInitialSize, num_bytes_written);

  // Stat it.
  error = FILE_ERROR_FAILED;
  FileInformationPtr file_info;
  file->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(kInitialSize, file_info->size);

  // Truncate it.
  error = FILE_ERROR_FAILED;
  file->Truncate(kTruncatedSize, Capture(&error));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Stat again.
  error = FILE_ERROR_FAILED;
  file_info.reset();
  file->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(file.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(kTruncatedSize, file_info->size);
}

TEST_F(FileImplTest, AsHandle) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  {
    // Create my_file.
    FilePtr file1;
    error = FILE_ERROR_FAILED;
    directory->OpenFile("my_file", GetProxy(&file1),
                        kFlagRead | kFlagWrite | kFlagCreate, Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Fetch the handle
    error = FILE_ERROR_FAILED;
    mojo::ScopedHandle handle;
    file1->AsHandle(Capture(&error, &handle));
    ASSERT_TRUE(file1.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Pull a file descriptor out of the scoped handle.
    MojoPlatformHandle platform_handle;
    MojoResult extract_result =
        MojoExtractPlatformHandle(handle.release().value(), &platform_handle);
    EXPECT_EQ(MOJO_RESULT_OK, extract_result);

    // Pass this raw file descriptor to a base::File.
    base::File raw_file(platform_handle);
    ASSERT_TRUE(raw_file.IsValid());
    EXPECT_EQ(5, raw_file.WriteAtCurrentPos("hello", 5));
  }

  {
    // Reopen my_file.
    FilePtr file2;
    error = FILE_ERROR_FAILED;
    directory->OpenFile("my_file", GetProxy(&file2), kFlagRead | kFlagOpen,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);

    // Verify that we wrote data raw on the file descriptor.
    mojo::Array<uint8_t> bytes_read;
    error = FILE_ERROR_FAILED;
    file2->Read(5, 0, WHENCE_FROM_BEGIN, Capture(&error, &bytes_read));
    ASSERT_TRUE(file2.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
    ASSERT_EQ(5u, bytes_read.size());
    EXPECT_EQ(static_cast<uint8_t>('h'), bytes_read[0]);
    EXPECT_EQ(static_cast<uint8_t>('e'), bytes_read[1]);
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read[2]);
    EXPECT_EQ(static_cast<uint8_t>('l'), bytes_read[3]);
    EXPECT_EQ(static_cast<uint8_t>('o'), bytes_read[4]);
  }
}

}  // namespace
}  // namespace filesystem
