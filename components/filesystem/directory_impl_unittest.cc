// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>

#include "base/macros.h"
#include "components/filesystem/files_test_base.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/util/capture_util.h"

using mojo::Capture;

namespace filesystem {
namespace {

using DirectoryImplTest = FilesTestBase;

TEST_F(DirectoryImplTest, Read) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  // Make some files.
  const struct {
    const char* name;
    uint32_t open_flags;
  } files_to_create[] = {
      {"my_file1", kFlagRead | kFlagWrite | kFlagCreate},
      {"my_file2", kFlagWrite | kFlagCreate},
      {"my_file3", kFlagAppend | kFlagCreate }};
  for (size_t i = 0; i < arraysize(files_to_create); i++) {
    error = FileError::FAILED;
    directory->OpenFile(files_to_create[i].name, nullptr,
                        files_to_create[i].open_flags, Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FileError::OK, error);
  }
  // Make a directory.
  error = FileError::FAILED;
  directory->OpenDirectory(
      "my_dir", nullptr, kFlagRead | kFlagWrite | kFlagCreate, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FileError::OK, error);

  error = FileError::FAILED;
  mojo::Array<DirectoryEntryPtr> directory_contents;
  directory->Read(Capture(&error, &directory_contents));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FileError::OK, error);

  // Expected contents of the directory.
  std::map<std::string, FsFileType> expected_contents;
  expected_contents["my_file1"] = FsFileType::REGULAR_FILE;
  expected_contents["my_file2"] = FsFileType::REGULAR_FILE;
  expected_contents["my_file3"] = FsFileType::REGULAR_FILE;
  expected_contents["my_dir"] = FsFileType::DIRECTORY;
  // Note: We don't expose ".." or ".".

  EXPECT_EQ(expected_contents.size(), directory_contents.size());
  for (size_t i = 0; i < directory_contents.size(); i++) {
    ASSERT_TRUE(directory_contents[i]);
    ASSERT_TRUE(directory_contents[i]->name);
    auto it = expected_contents.find(directory_contents[i]->name.get());
    ASSERT_TRUE(it != expected_contents.end());
    EXPECT_EQ(it->second, directory_contents[i]->type);
    expected_contents.erase(it);
  }
}

// TODO(vtl): Properly test OpenFile() and OpenDirectory() (including flags).

TEST_F(DirectoryImplTest, BasicRenameDelete) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  // Create my_file.
  error = FileError::FAILED;
  directory->OpenFile("my_file", nullptr, kFlagWrite | kFlagCreate,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FileError::OK, error);

  // Opening my_file should succeed.
  error = FileError::FAILED;
  directory->OpenFile("my_file", nullptr, kFlagRead | kFlagOpen,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FileError::OK, error);

  // Rename my_file to my_new_file.
  directory->Rename("my_file", "my_new_file", Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FileError::OK, error);

  // Opening my_file should fail.

  error = FileError::FAILED;
  directory->OpenFile("my_file", nullptr, kFlagRead | kFlagOpen,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FileError::NOT_FOUND, error);

  // Opening my_new_file should succeed.
  error = FileError::FAILED;
  directory->OpenFile("my_new_file", nullptr, kFlagRead | kFlagOpen,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FileError::OK, error);

  // Delete my_new_file (no flags).
  directory->Delete("my_new_file", 0, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FileError::OK, error);

  // Opening my_new_file should fail.
  error = FileError::FAILED;
  directory->OpenFile("my_new_file", nullptr, kFlagRead | kFlagOpen,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FileError::NOT_FOUND, error);
}

TEST_F(DirectoryImplTest, CantOpenDirectoriesAsFiles) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  {
    // Create a directory called 'my_file'
    DirectoryPtr my_file_directory;
    error = FileError::FAILED;
    directory->OpenDirectory(
        "my_file", GetProxy(&my_file_directory),
        kFlagRead | kFlagWrite | kFlagCreate,
        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FileError::OK, error);
  }

  {
    // Attempt to open that directory as a file. This must fail!
    FilePtr file;
    error = FileError::FAILED;
    directory->OpenFile("my_file", GetProxy(&file), kFlagRead | kFlagOpen,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FileError::NOT_A_FILE, error);
  }
}

TEST_F(DirectoryImplTest, Clone) {
  DirectoryPtr clone_one;
  DirectoryPtr clone_two;
  FileError error;

  {
    DirectoryPtr directory;
    GetTemporaryRoot(&directory);

    directory->Clone(GetProxy(&clone_one));
    directory->Clone(GetProxy(&clone_two));

    // Original temporary directory goes out of scope here; shouldn't be
    // deleted since it has clones.
  }

  std::string data("one two three");
  {
    clone_one->WriteFile("data", mojo::Array<uint8_t>::From(data),
                         Capture(&error));
    ASSERT_TRUE(clone_one.WaitForIncomingResponse());
    EXPECT_EQ(FileError::OK, error);
  }

  {
    mojo::Array<uint8_t> file_contents;
    clone_two->ReadEntireFile("data", Capture(&error, &file_contents));
    ASSERT_TRUE(clone_two.WaitForIncomingResponse());
    EXPECT_EQ(FileError::OK, error);

    EXPECT_EQ(data, file_contents.To<std::string>());
  }
}

TEST_F(DirectoryImplTest, WriteFileReadFile) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  std::string data("one two three");
  {
    directory->WriteFile("data", mojo::Array<uint8_t>::From(data),
                         Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FileError::OK, error);
  }

  {
    mojo::Array<uint8_t> file_contents;
    directory->ReadEntireFile("data", Capture(&error, &file_contents));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FileError::OK, error);

    EXPECT_EQ(data, file_contents.To<std::string>());
  }
}

TEST_F(DirectoryImplTest, ReadEmptyFileIsNotFoundError) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  {
    mojo::Array<uint8_t> file_contents;
    directory->ReadEntireFile("doesnt_exist", Capture(&error, &file_contents));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FileError::NOT_FOUND, error);
  }
}

TEST_F(DirectoryImplTest, CantReadEntireFileOnADirectory) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  // Create a directory
  {
    DirectoryPtr my_file_directory;
    error = FileError::FAILED;
    directory->OpenDirectory(
        "my_dir", GetProxy(&my_file_directory),
        kFlagRead | kFlagWrite | kFlagCreate,
        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FileError::OK, error);
  }

  // Try to read it as a file
  {
    mojo::Array<uint8_t> file_contents;
    directory->ReadEntireFile("my_dir", Capture(&error, &file_contents));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FileError::NOT_A_FILE, error);
  }
}

TEST_F(DirectoryImplTest, CantWriteFileOnADirectory) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  // Create a directory
  {
    DirectoryPtr my_file_directory;
    error = FileError::FAILED;
    directory->OpenDirectory(
        "my_dir", GetProxy(&my_file_directory),
        kFlagRead | kFlagWrite | kFlagCreate,
        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FileError::OK, error);
  }

  {
    std::string data("one two three");
    directory->WriteFile("my_dir", mojo::Array<uint8_t>::From(data),
                         Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FileError::NOT_A_FILE, error);
  }
}

// TODO(vtl): Test delete flags.

}  // namespace
}  // namespace filesystem
