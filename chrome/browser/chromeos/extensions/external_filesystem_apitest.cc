// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/threading/worker_pool.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/gdata/mock_gdata_documents_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"

using ::testing::_;
using ::testing::Return;
using content::BrowserContext;

namespace {

// These should match the counterparts in remote.js.
const char kTestFileContents[] = "hello, world";

// Contains a folder entry for the folder 'Folder' that will be 'created'.
const char kTestDirectory[] = "new_folder_entry.json";

// Contains a folder named Folder that has a file File.aBc inside of it.
const char kTestRootFeed[] = "remote_file_system_apitest_root_feed.json";

// The ID of the file browser extension.
const char kFileBrowserExtensionId[] = "ddammdhioacbehjngdmkjcjbnfginlla";

// Flags used to run the tests with a COMPONENT extension.
const int kComponentFlags = ExtensionApiTest::kFlagEnableFileAccess |
                            ExtensionApiTest::kFlagLoadAsComponent;

// Helper class to wait for a background page to load or close again.
// TODO(tbarzic): We can probably share this with e.g.
//                lazy_background_page_apitest.
class BackgroundObserver {
 public:
  BackgroundObserver()
      : page_created_(chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
                      content::NotificationService::AllSources()),
        page_closed_(chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                     content::NotificationService::AllSources()) {
  }

  // TODO(tbarzic): Use this for file handlers in the rest of the tests
  // (instead of calling chrome.test.succeed in js).
  void WaitUntilLoaded() {
    page_created_.Wait();
  }

  void WaitUntilClosed() {
    page_closed_.Wait();
  }

 private:
  ui_test_utils::WindowedNotificationObserver page_created_;
  ui_test_utils::WindowedNotificationObserver page_closed_;
};

// TODO(tbarzic): We should probably share GetTestFilePath and LoadJSONFile
// with gdata_file_system_unittest.
// Generates file path in gdata test directory for a file with name |filename|.
FilePath GetTestFilePath(const FilePath::StringType& filename) {
  FilePath path;
  std::string error;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
   path = path.AppendASCII("chromeos")
       .AppendASCII("gdata")
       .AppendASCII(filename);
  EXPECT_TRUE(file_util::PathExists(path)) <<
      "Couldn't find " << path.value();
  return path;
}

// Loads and deserializes a json file in gdata test directory whose name is
// |filename|. Returns new Value object the file is deserialized to.
base::Value* LoadJSONFile(const std::string& filename) {
  FilePath path = GetTestFilePath(filename);
  std::string error;
  JSONFileValueSerializer serializer(path);
  Value* value = serializer.Deserialize(NULL, &error);
  EXPECT_TRUE(value) <<
      "Parse error " << path.value() << ": " << error;
  return value;
}

// Action used to set mock expectations for CreateDirectory().
ACTION_P2(MockCreateDirectoryCallback, status, value) {
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(arg2, status, base::Passed(value)));
}

// Action used to set mock expecteations for GetDocuments.
ACTION_P2(MockGetDocumentsCallback, status, value) {
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(arg3, status, base::Passed(value)));
}

// Creates a cache representation of the test file with predetermined content.
void CreateDownloadFile(const FilePath& path) {
  int file_content_size = static_cast<int>(sizeof(kTestFileContents));
  ASSERT_EQ(file_content_size,
            file_util::WriteFile(path, kTestFileContents, file_content_size));
}

// Action used to set mock expectations for DownloadFile().
ACTION_P(MockDownloadFileCallback, status) {
  ASSERT_TRUE(base::WorkerPool::PostTaskAndReply(FROM_HERE,
      base::Bind(&CreateDownloadFile, arg1),
      base::Bind(arg3, status, arg2, arg1),
      false));
}

}  // namespace

class FileSystemExtensionApiTest : public ExtensionApiTest {
 public:
  FileSystemExtensionApiTest() : test_mount_point_("/tmp") {
  }

  virtual ~FileSystemExtensionApiTest() {}

  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  // Adds a local mount point at at mount point /tmp.
  void AddTmpMountPoint() {
    fileapi::ExternalFileSystemMountPointProvider* provider =
        BrowserContext::GetFileSystemContext(browser()->profile())->
            external_provider();
    provider->AddLocalMountPoint(test_mount_point_);
  }

  // Loads the extension, which temporarily starts the lazy background page
  // to dispatch the onInstalled event. We wait until it shuts down again.
  const Extension* LoadExtensionAndWait(const std::string& test_name) {
    BackgroundObserver page_complete;
    FilePath extdir = test_data_dir_.AppendASCII(test_name);
    const Extension* extension = LoadExtension(extdir);
    if (extension)
      page_complete.WaitUntilClosed();
    return extension;
  }

 private:
  FilePath test_mount_point_;
};

class RemoteFileSystemExtensionApiTest : public ExtensionApiTest {
 public:
  RemoteFileSystemExtensionApiTest() {}

  virtual ~RemoteFileSystemExtensionApiTest() {}

  virtual void SetUp() OVERRIDE {
    FilePath tmp_dir_path;
    PathService::Get(base::DIR_TEMP, &tmp_dir_path);
    ASSERT_TRUE(test_cache_root_.CreateUniqueTempDirUnderPath(tmp_dir_path));

    ExtensionApiTest::SetUp();
  }

  // Sets up GDataFileSystem that will be used in the test.
  // NOTE: Remote mount point should get added to mount poitn provider when
  // getLocalFileSystem is called from filebrowser_component extension.
  virtual void SetupGDataFileSystemForTest() {
    gdata::GDataSystemService* system_service =
        gdata::GDataSystemServiceFactory::GetForProfile(browser()->profile());
    EXPECT_TRUE(system_service && system_service->file_system());

    mock_documents_service_ = new gdata::MockDocumentsService();
    operation_registry_.reset(new gdata::GDataOperationRegistry());
    system_service->file_system()->SetDocumentsServiceForTesting(
        mock_documents_service_);

    EXPECT_TRUE(system_service->file_system()->SetCacheRootPathForTesting(
        test_cache_root_.path()));
  }

 protected:
  ScopedTempDir test_cache_root_;
  gdata::MockDocumentsService* mock_documents_service_;
  scoped_ptr<gdata::GDataOperationRegistry> operation_registry_;
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

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest, FileBrowserTestLazy) {
  AddTmpMountPoint();
  ASSERT_TRUE(LoadExtensionAndWait("filesystem_handler_lazy_background"))
      << message_;
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
                       FileBrowserTestWriteReadOnly) {
  AddTmpMountPoint();
  ASSERT_TRUE(RunExtensionTest("filesystem_handler_write")) << message_;
  ASSERT_FALSE(RunExtensionSubtest(
      "filebrowser_component", "write.html#def", kComponentFlags)) << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest,
                       FileBrowserTestWriteComponent) {
  AddTmpMountPoint();
  ASSERT_TRUE(RunComponentExtensionTest("filesystem_handler_write"))
      << message_;
  ASSERT_TRUE(RunExtensionSubtest(
      "filebrowser_component", "write.html", kComponentFlags)) << message_;
}

// Failing on Linux ChromeOS builds.
// http://crbug.com/128757
IN_PROC_BROWSER_TEST_F(RemoteFileSystemExtensionApiTest,
                       DISABLED_RemoteMountPoint) {
  SetupGDataFileSystemForTest();

  EXPECT_CALL(*mock_documents_service_, GetAccountMetadata(_)).Times(1);

  // First, file browser will try to create new directory.
  scoped_ptr<base::Value> dir_value(LoadJSONFile(kTestDirectory));
  EXPECT_CALL(*mock_documents_service_,
              CreateDirectory(_, _, _))
      .WillOnce(MockCreateDirectoryCallback(gdata::HTTP_SUCCESS, &dir_value));

  // Then the test will try to read an existing file file.
  // Remote filesystem should first request root feed from gdata server.
  scoped_ptr<base::Value> documents_value(LoadJSONFile(kTestRootFeed));
  EXPECT_CALL(*mock_documents_service_,
              GetDocuments(_, _, _, _))
      .WillOnce(MockGetDocumentsCallback(gdata::HTTP_SUCCESS,
                                         &documents_value));

  // When file browser tries to read the file, remote filesystem should detect
  // that the cached file is not present on the disk and download it. Mocked
  // download file will create file with the cached name and predetermined
  // content. This is the file file browser will read content from.
  // Later in the test, file handler will try to open the same file on gdata
  // mount point. This time, DownloadFile should not be called because local
  // copy is already present in the cache.
  EXPECT_CALL(*mock_documents_service_,
              DownloadFile(_, _, _, _, _))
      .WillOnce(MockDownloadFileCallback(gdata::HTTP_SUCCESS));

  // On exit, all operations in progress should be cancelled.
  EXPECT_CALL(*mock_documents_service_, CancelAll());
  // This one is called on exit, but we don't care much about it, as long as it
  // retunrs something valid (i.e. not NULL).
  EXPECT_CALL(*mock_documents_service_, operation_registry()).
      WillOnce(Return(operation_registry_.get()));

  // All is set... RUN THE TEST.
  EXPECT_TRUE(RunExtensionTest("filesystem_handler")) << message_;
  EXPECT_TRUE(RunExtensionSubtest("filebrowser_component", "remote.html",
      kComponentFlags)) << message_;
}
