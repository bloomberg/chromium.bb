// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "components/filesystem/files_test_base.h"

namespace mojo {
namespace files {
namespace {

using DirectoryImplTest = FilesTestBase;

TEST_F(DirectoryImplTest, Read) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  Error error;

  // Make some files.
  const struct {
    const char* name;
    uint32_t open_flags;
  } files_to_create[] = {
      {"my_file1", kOpenFlagRead | kOpenFlagWrite | kOpenFlagCreate},
      {"my_file2", kOpenFlagWrite | kOpenFlagCreate | kOpenFlagExclusive},
      {"my_file3", kOpenFlagWrite | kOpenFlagCreate | kOpenFlagAppend},
      {"my_file4", kOpenFlagWrite | kOpenFlagCreate | kOpenFlagTruncate}};
  for (size_t i = 0; i < arraysize(files_to_create); i++) {
    error = ERROR_INTERNAL;
    directory->OpenFile(files_to_create[i].name, nullptr,
                        files_to_create[i].open_flags, Capture(&error));
    ASSERT_TRUE(directory.WaitForIncomingMethodCall());
    EXPECT_EQ(ERROR_OK, error);
  }
  // Make a directory.
  error = ERROR_INTERNAL;
  directory->OpenDirectory("my_dir", nullptr,
                           kOpenFlagRead | kOpenFlagWrite | kOpenFlagCreate,
                           Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  error = ERROR_INTERNAL;
  Array<DirectoryEntryPtr> directory_contents;
  directory->Read(Capture(&error, &directory_contents));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Expected contents of the directory.
  std::map<std::string, FileType> expected_contents;
  expected_contents["my_file1"] = FILE_TYPE_REGULAR_FILE;
  expected_contents["my_file2"] = FILE_TYPE_REGULAR_FILE;
  expected_contents["my_file3"] = FILE_TYPE_REGULAR_FILE;
  expected_contents["my_file4"] = FILE_TYPE_REGULAR_FILE;
  expected_contents["my_dir"] = FILE_TYPE_DIRECTORY;
  expected_contents["."] = FILE_TYPE_DIRECTORY;
  expected_contents[".."] = FILE_TYPE_DIRECTORY;

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

// Note: Ignore nanoseconds, since it may not always be supported. We expect at
// least second-resolution support though.
// TODO(vtl): Maybe share this with |FileImplTest.StatTouch| ... but then it'd
// be harder to split this file.
TEST_F(DirectoryImplTest, StatTouch) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  Error error;

  // Stat it.
  error = ERROR_INTERNAL;
  FileInformationPtr file_info;
  directory->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  ASSERT_FALSE(file_info.is_null());
  EXPECT_EQ(FILE_TYPE_DIRECTORY, file_info->type);
  EXPECT_EQ(0, file_info->size);
  ASSERT_FALSE(file_info->atime.is_null());
  EXPECT_GT(file_info->atime->seconds, 0);  // Expect that it's not 1970-01-01.
  ASSERT_FALSE(file_info->mtime.is_null());
  EXPECT_GT(file_info->mtime->seconds, 0);
  int64_t first_mtime = file_info->mtime->seconds;

  // Touch only the atime.
  error = ERROR_INTERNAL;
  TimespecOrNowPtr t(TimespecOrNow::New());
  t->now = false;
  t->timespec = Timespec::New();
  const int64_t kPartyTime1 = 1234567890;  // Party like it's 2009-02-13.
  t->timespec->seconds = kPartyTime1;
  directory->Touch(t.Pass(), nullptr, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Stat again.
  error = ERROR_INTERNAL;
  file_info.reset();
  directory->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  ASSERT_FALSE(file_info.is_null());
  ASSERT_FALSE(file_info->atime.is_null());
  EXPECT_EQ(kPartyTime1, file_info->atime->seconds);
  ASSERT_FALSE(file_info->mtime.is_null());
  EXPECT_EQ(first_mtime, file_info->mtime->seconds);

  // Touch only the mtime.
  t = TimespecOrNow::New();
  t->now = false;
  t->timespec = Timespec::New();
  const int64_t kPartyTime2 = 1425059525;  // No time like the present.
  t->timespec->seconds = kPartyTime2;
  directory->Touch(nullptr, t.Pass(), Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Stat again.
  error = ERROR_INTERNAL;
  file_info.reset();
  directory->Stat(Capture(&error, &file_info));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);
  ASSERT_FALSE(file_info.is_null());
  ASSERT_FALSE(file_info->atime.is_null());
  EXPECT_EQ(kPartyTime1, file_info->atime->seconds);
  ASSERT_FALSE(file_info->mtime.is_null());
  EXPECT_EQ(kPartyTime2, file_info->mtime->seconds);

  // TODO(vtl): Also test Touch() "now" options.
  // TODO(vtl): Also test touching both atime and mtime.
}

// TODO(vtl): Properly test OpenFile() and OpenDirectory() (including flags).

TEST_F(DirectoryImplTest, BasicRenameDelete) {
  DirectoryPtr directory;
  GetTemporaryRoot(&directory);
  Error error;

  // Create my_file.
  error = ERROR_INTERNAL;
  directory->OpenFile("my_file", nullptr, kOpenFlagWrite | kOpenFlagCreate,
                      Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Opening my_file should succeed.
  error = ERROR_INTERNAL;
  directory->OpenFile("my_file", nullptr, kOpenFlagRead, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Rename my_file to my_new_file.
  directory->Rename("my_file", "my_new_file", Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Opening my_file should fail.
  error = ERROR_INTERNAL;
  directory->OpenFile("my_file", nullptr, kOpenFlagRead, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_UNKNOWN, error);

  // Opening my_new_file should succeed.
  error = ERROR_INTERNAL;
  directory->OpenFile("my_new_file", nullptr, kOpenFlagRead, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Delete my_new_file (no flags).
  directory->Delete("my_new_file", 0, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_OK, error);

  // Opening my_new_file should fail.
  error = ERROR_INTERNAL;
  directory->OpenFile("my_new_file", nullptr, kOpenFlagRead, Capture(&error));
  ASSERT_TRUE(directory.WaitForIncomingMethodCall());
  EXPECT_EQ(ERROR_UNKNOWN, error);
}

// TODO(vtl): Test that an open file can be moved (by someone else) without
// operations on it being affected.
// TODO(vtl): Test delete flags.

}  // namespace
}  // namespace files
}  // namespace mojo
