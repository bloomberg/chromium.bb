// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "components/filesystem/files_test_base.h"
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
    error = FILE_ERROR_FAILED;
    directory->OpenFile(files_to_create[i].name, nullptr,
                        files_to_create[i].open_flags, Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
  }
  // Make a directory.
  error = FILE_ERROR_FAILED;
  directory->OpenDirectory(
      "my_dir", nullptr, kFlagRead | kFlagWrite | kFlagCreate, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  error = FILE_ERROR_FAILED;
  mojo::Array<DirectoryEntryPtr> directory_contents;
  directory->Read(Capture(&error, &directory_contents));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Expected contents of the directory.
  std::map<std::string, FsFileType> expected_contents;
  expected_contents["my_file1"] = FS_FILE_TYPE_REGULAR_FILE;
  expected_contents["my_file2"] = FS_FILE_TYPE_REGULAR_FILE;
  expected_contents["my_file3"] = FS_FILE_TYPE_REGULAR_FILE;
  expected_contents["my_dir"] = FS_FILE_TYPE_DIRECTORY;
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
  error = FILE_ERROR_FAILED;
  directory->OpenFile("my_file", nullptr, kFlagWrite | kFlagCreate,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Opening my_file should succeed.
  error = FILE_ERROR_FAILED;
  directory->OpenFile("my_file", nullptr, kFlagRead | kFlagOpen,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Rename my_file to my_new_file.
  directory->Rename("my_file", "my_new_file", Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Opening my_file should fail.

  error = FILE_ERROR_FAILED;
  directory->OpenFile("my_file", nullptr, kFlagRead | kFlagOpen,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_FAILED, error);

  // Opening my_new_file should succeed.
  error = FILE_ERROR_FAILED;
  directory->OpenFile("my_new_file", nullptr, kFlagRead | kFlagOpen,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Delete my_new_file (no flags).
  directory->Delete("my_new_file", 0, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_OK, error);

  // Opening my_new_file should fail.
  error = FILE_ERROR_FAILED;
  directory->OpenFile("my_new_file", nullptr, kFlagRead | kFlagOpen,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingResponse());
  EXPECT_EQ(FILE_ERROR_FAILED, error);
}

TEST_F(DirectoryImplTest, CantOpenDirectoriesAsFiles) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  FileError error;

  {
    // Create a directory called 'my_file'
    DirectoryPtr my_file_directory;
    error = FILE_ERROR_FAILED;
    directory->OpenDirectory(
        "my_file", GetProxy(&my_file_directory),
        kFlagRead | kFlagWrite | kFlagCreate,
        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_OK, error);
  }

  {
    // Attempt to open that directory as a file. This must fail!
    FilePtr file;
    error = FILE_ERROR_FAILED;
    directory->OpenFile("my_file", GetProxy(&file), kFlagRead | kFlagOpen,
                        Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingResponse());
    EXPECT_EQ(FILE_ERROR_NOT_A_FILE, error);
  }
}


// TODO(vtl): Test delete flags.

}  // namespace
}  // namespace filesystem
