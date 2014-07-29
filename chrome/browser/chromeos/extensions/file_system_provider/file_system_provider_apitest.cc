// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

namespace extensions {

class FileSystemProviderApiTest : public ExtensionApiTest {
 public:
  FileSystemProviderApiTest()
      // Set the channel to "trunk" since this API is restricted to trunk.
      : current_channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {
  }

  // Loads a helper testing extension.
  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionApiTest::SetUpOnMainThread();
    const extensions::Extension* extension = LoadExtensionWithFlags(
        test_data_dir_.AppendASCII("file_system_provider/test_util"),
        kFlagEnableIncognito);
    ASSERT_TRUE(extension);
  }

 private:
  extensions::ScopedCurrentChannel current_channel_;
};

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Mount) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/mount",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Unmount) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/unmount",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, GetMetadata) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/get_metadata",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, ReadDirectory) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/read_directory",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, ReadFile) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/read_file",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, BigFile) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/big_file",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Evil) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/evil",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, MimeType) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/mime_type",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, CreateDirectory) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags(
      "file_system_provider/create_directory", kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, DeleteEntry) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/delete_entry",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, CreateFile) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/create_file",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, CopyEntry) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/copy_entry",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, MoveEntry) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/move_entry",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, Truncate) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/truncate",
                                          kFlagLoadAsComponent))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemProviderApiTest, WriteFile) {
  ASSERT_TRUE(RunPlatformAppTestWithFlags("file_system_provider/write_file",
                                          kFlagLoadAsComponent))
      << message_;
}

}  // namespace extensions
