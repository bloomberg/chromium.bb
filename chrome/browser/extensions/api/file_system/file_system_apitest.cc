// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "chrome/browser/extensions/api/file_system/file_system_api.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"

using extensions::FileSystemChooseFileFunction;

class FileSystemApiTest : public PlatformAppBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    test_root_folder_ = test_data_dir_.AppendASCII("api_test")
        .AppendASCII("file_system");
  }

  virtual void TearDown() OVERRIDE {
    FileSystemChooseFileFunction::StopSkippingPickerForTest();
    PlatformAppBrowserTest::TearDown();
  };

 protected:
  FilePath TempFilePath(const std::string& destination_name, bool copy_gold) {
    if (!temp_dir_.CreateUniqueTempDir()) {
      ADD_FAILURE() << "CreateUniqueTempDir failed";
      return FilePath();
    }
    FilePath destination = temp_dir_.path().AppendASCII(destination_name);
    if (copy_gold) {
      FilePath source = test_root_folder_.AppendASCII("gold.txt");
      EXPECT_TRUE(file_util::CopyFile(source, destination));
    }
    return destination;
  }

  FilePath test_root_folder_;
  ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiGetDisplayPath) {
  FilePath test_file = test_root_folder_.AppendASCII("gold.txt");
  FileSystemChooseFileFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/get_display_path"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiOpenExistingFileTest) {
  FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseFileFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_existing"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiInvalidChooseFileTypeTest) {
  FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseFileFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/invalid_choose_file_type")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiOpenExistingFileWithWriteTest) {
  FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseFileFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_existing_with_write")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiOpenWritableExistingFileTest) {
  FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseFileFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_writable_existing")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiOpenWritableExistingFileWithWriteTest) {
  FilePath test_file = TempFilePath("open_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseFileFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/open_writable_existing_with_write")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiOpenCancelTest) {
  FileSystemChooseFileFunction::SkipPickerAndAlwaysCancelForTest();
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_cancel"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiOpenBackgroundTest) {
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/open_background"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiSaveNewFileTest) {
  FilePath test_file = TempFilePath("save_new.txt", false);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseFileFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_new"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiSaveExistingFileTest) {
  FilePath test_file = TempFilePath("save_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseFileFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_existing"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiSaveNewFileWithWriteTest) {
  FilePath test_file = TempFilePath("save_new.txt", false);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseFileFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_new_with_write"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiSaveExistingFileWithWriteTest) {
  FilePath test_file = TempFilePath("save_existing.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseFileFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/save_existing_with_write")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiSaveCancelTest) {
  FileSystemChooseFileFunction::SkipPickerAndAlwaysCancelForTest();
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_cancel"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiSaveBackgroundTest) {
  ASSERT_TRUE(RunPlatformAppTest("api_test/file_system/save_background"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest, FileSystemApiGetWritableTest) {
  FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseFileFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/get_writable_file_entry")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemApiTest,
    FileSystemApiGetWritableWithWriteTest) {
  FilePath test_file = TempFilePath("writable.txt", true);
  ASSERT_FALSE(test_file.empty());
  FileSystemChooseFileFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest(
      "api_test/file_system/get_writable_file_entry_with_write")) << message_;
}

