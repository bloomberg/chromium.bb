// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"

using content::BrowserContext;

#if defined(OS_CHROMEOS)

namespace {
const char kExpectedWriteError[] =
    "Got unexpected error: File handler error: SECURITY_ERR";
}

class FileSystemExtensionApiTest : public ExtensionApiTest {
 public:
  FileSystemExtensionApiTest() : test_mount_point_("/tmp") {
  }

  virtual ~FileSystemExtensionApiTest() {}

  // Sets up test environment
  void AddTmpMountPoint() {
    // Add tmp mount point.
    fileapi::ExternalFileSystemMountPointProvider* provider =
        BrowserContext::GetFileSystemContext(browser()->profile())->
            external_provider();
    provider->AddLocalMountPoint(test_mount_point_);
  }

 private:
  FilePath test_mount_point_;
};

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest, LocalFileSystem) {
  AddTmpMountPoint();
  ASSERT_TRUE(RunComponentExtensionTest("local_filesystem")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest, FileBrowserTest) {
  AddTmpMountPoint();
  ASSERT_TRUE(RunExtensionTest("filesystem_handler")) << message_;
  ASSERT_TRUE(RunComponentExtensionSubtest("filebrowser_component",
                                           "read.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest, FileBrowserTestWrite) {
  AddTmpMountPoint();
  ASSERT_TRUE(RunExtensionTest("filesystem_handler_write")) << message_;
  ASSERT_TRUE(RunComponentExtensionSubtest("filebrowser_component",
                                            "write.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest,
                       FileBrowserTestWriteComponent) {
  AddTmpMountPoint();
  ASSERT_TRUE(RunComponentExtensionTest("filesystem_handler_write"))
      << message_;
  ASSERT_TRUE(RunComponentExtensionSubtest("filebrowser_component",
                                           "write.html")) << message_;
}

#endif
