// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/platform_file.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system_proxy.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "webkit/chromeos/fileapi/remote_file_system_proxy.h"

using ::testing::_;
using content::BrowserContext;

// These should match the counterparts in remote.js.
const char kTestDirPath[] = "/test_dir";
const char kTestFilePath[] = "/test_dir/hello.txt";
const char kTestFileContents[] = "hello, world";

namespace {

// The ID of the file browser extension.
const char kFileBrowserExtensionId[] = "ddammdhioacbehjngdmkjcjbnfginlla";

// Flags used to run the tests with a COMPONENT extension.
const int kComponentFlags = ExtensionApiTest::kFlagEnableFileAccess |
                            ExtensionApiTest::kFlagLoadAsComponent;

// Returns the expected URL for the given path.
GURL GetExpectedURL(const std::string& path) {
  return GURL(
      base::StringPrintf(
          "filesystem:chrome-extension://%s/external/%s",
          kFileBrowserExtensionId,
          path.c_str()));
}

// Action used to set mock expectations for CreateDirectory().
ACTION_P(MockCreateDirectory, status) {
  arg3.Run(status);
}

// Action used to set mock expectations for GetFileInfo().
ACTION_P3(MockGetFileInfo, status, file_info, path) {
  arg1.Run(status, file_info, path);
}

// Action used to set mock expectations for CreateSnapshotFile().
ACTION_P4(MockCreateSnapshotFile, status, file_info, path, file_ref) {
  arg1.Run(status, file_info, path, file_ref);
}

// The mock is used to add a remote mount point, and write tests for it.
class MockRemoteFileSystemProxy :
      public fileapi::RemoteFileSystemProxyInterface {
 public:
  MockRemoteFileSystemProxy() {}
  virtual ~MockRemoteFileSystemProxy() {}

  MOCK_METHOD2(
      GetFileInfo,
      void(const GURL& path,
           const fileapi::FileSystemOperationInterface::GetMetadataCallback&
           callback));
  MOCK_METHOD3(
      Copy,
      void(const GURL& src_path,
           const GURL& dest_path,
           const fileapi::FileSystemOperationInterface::StatusCallback&
           callback));
  MOCK_METHOD3(
      Move,
      void(const GURL& src_path,
           const GURL& dest_path,
           const fileapi::FileSystemOperationInterface::StatusCallback&
           callback));
  MOCK_METHOD2(
      ReadDirectory,
      void(const GURL& path,
           const fileapi::FileSystemOperationInterface::ReadDirectoryCallback&
           callback));
  MOCK_METHOD3(
      Remove,
      void(const GURL& path,
           bool recursive,
           const fileapi::FileSystemOperationInterface::StatusCallback&
           callback));
  MOCK_METHOD4(
      CreateDirectory,
      void(const GURL& file_url,
           bool exclusive,
           bool recursive,
           const fileapi::FileSystemOperationInterface::StatusCallback&
           callback));
  MOCK_METHOD2(
      CreateSnapshotFile,
      void(const GURL& path,
           const fileapi::FileSystemOperationInterface::SnapshotFileCallback&
           callback));
};

const char kExpectedWriteError[] =
    "Got unexpected error: File handler error: SECURITY_ERR";
}

class FileSystemExtensionApiTest : public ExtensionApiTest {
 public:
  FileSystemExtensionApiTest() : test_mount_point_("/tmp") {
  }

  virtual ~FileSystemExtensionApiTest() {}

  // Adds a local mount point at at mount point /tmp.
  void AddTmpMountPoint() {
    fileapi::ExternalFileSystemMountPointProvider* provider =
        BrowserContext::GetFileSystemContext(browser()->profile())->
            external_provider();
    provider->AddLocalMountPoint(test_mount_point_);
  }

 private:
  FilePath test_mount_point_;
};

class RemoteFileSystemExtensionApiTest : public ExtensionApiTest {
 public:
  RemoteFileSystemExtensionApiTest()
      : mock_remote_file_system_proxy_(NULL) {
  }

  virtual ~RemoteFileSystemExtensionApiTest() {}

  virtual void SetUp() OVERRIDE {
    FilePath tmp_dir_path;
    PathService::Get(base::DIR_TEMP, &tmp_dir_path);

    ASSERT_TRUE(test_mount_point_.CreateUniqueTempDirUnderPath(tmp_dir_path));

    file_util::CreateTemporaryFileInDir(test_mount_point_.path(),
                                        &test_file_path_);
    file_util::WriteFile(test_file_path_,
                         kTestFileContents,
                         sizeof(kTestFileContents) - 1);
    file_util::GetFileInfo(test_file_path_, &test_file_info_);

    // ExtensionApiTest::SetUp() should be called at the end. For some
    // reason, ExtensionApiTest::SetUp() starts running tests, so any
    // setup has to be done before calling this.
    ExtensionApiTest::SetUp();
  }

  // Adds a remote mount point at at mount point /tmp.
  void AddTmpMountPoint() {
    fileapi::ExternalFileSystemMountPointProvider* provider =
        BrowserContext::GetFileSystemContext(browser()->profile())->
            external_provider();
    mock_remote_file_system_proxy_ = new MockRemoteFileSystemProxy;
    // Take the ownership of mock_remote_file_system_proxy_.
    provider->AddRemoteMountPoint(test_mount_point_.path(),
                                mock_remote_file_system_proxy_);
  }

  std::string GetPathOnMountPoint(const std::string& path) {
    return test_mount_point_.path().BaseName().value() + path;
  }

 protected:
  base::PlatformFileInfo test_file_info_;
  FilePath test_file_path_;
  ScopedTempDir test_mount_point_;
  MockRemoteFileSystemProxy* mock_remote_file_system_proxy_;
};

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest, LocalFileSystem) {
  AddTmpMountPoint();
  ASSERT_TRUE(RunComponentExtensionTest("local_filesystem")) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest, FileBrowserTest) {
  AddTmpMountPoint();
  ASSERT_TRUE(RunExtensionTest("filesystem_handler")) << message_;
  ASSERT_TRUE(RunExtensionSubtest(
      "filebrowser_component", "read.html", kComponentFlags)) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest, FileBrowserTestWrite) {
  AddTmpMountPoint();
  ASSERT_TRUE(RunExtensionTest("filesystem_handler_write")) << message_;
  ASSERT_TRUE(RunExtensionSubtest(
      "filebrowser_component", "write.html", kComponentFlags)) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest,
                       FileBrowserTestWriteComponent) {
  AddTmpMountPoint();
  ASSERT_TRUE(RunComponentExtensionTest("filesystem_handler_write"))
      << message_;
  ASSERT_TRUE(RunExtensionSubtest(
      "filebrowser_component", "write.html", kComponentFlags)) << message_;
}

IN_PROC_BROWSER_TEST_F(RemoteFileSystemExtensionApiTest, RemoteMountPoint) {
  AddTmpMountPoint();
  // The test directory is created first.
  const GURL test_dir_url =
      GetExpectedURL(GetPathOnMountPoint(kTestDirPath));
  EXPECT_CALL(*mock_remote_file_system_proxy_,
              CreateDirectory(test_dir_url, false, false, _))
      .WillOnce(MockCreateDirectory(base::PLATFORM_FILE_OK));

  // Then GetFileInfo() is called over "tmp/test_dir/hello.txt".
  const std::string expected_path = GetPathOnMountPoint(kTestFilePath);
  GURL expected_url = GetExpectedURL(expected_path);
  EXPECT_CALL(*mock_remote_file_system_proxy_,
              GetFileInfo(expected_url, _))
      .WillOnce(MockGetFileInfo(
          base::PLATFORM_FILE_OK,
          test_file_info_,
          FilePath::FromUTF8Unsafe(expected_path)));

  // Then CreateSnapshotFile() is called over "tmp/test_dir/hello.txt".
  EXPECT_CALL(*mock_remote_file_system_proxy_,
              CreateSnapshotFile(expected_url, _))
      .WillOnce(MockCreateSnapshotFile(
          base::PLATFORM_FILE_OK,
          test_file_info_,
          // Returns the path to the temporary file on the local drive.
          test_file_path_,
          scoped_refptr<webkit_blob::ShareableFileReference>(NULL)));
  ASSERT_TRUE(RunExtensionSubtest(
      "filebrowser_component", "remote.html#" + GetPathOnMountPoint(""),
      kComponentFlags)) << message_;
}
