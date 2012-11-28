// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/threading/worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive_file_system.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/mock_drive_service.h"
#include "chrome/browser/google_apis/time_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"

using ::testing::_;
using ::testing::Return;
using content::BrowserContext;

namespace {

// These should match the counterparts in remote.js.
// Also, the size of the file in |kTestRootFeed| has to be set to
// length of kTestFileContent string.
const char kTestFileContent[] = "hello, world!";

// Contains a folder entry for the folder 'Folder' that will be 'created'.
const char kTestDirectory[] = "new_folder_entry.json";

// Contains a folder named Folder that has a file File.aBc inside of it.
const char kTestRootFeed[] = "remote_file_system_apitest_root_feed.json";

// Contains metadata of the  document that will be "downloaded" in test.
const char kTestDocumentToDownloadEntry[] =
    "remote_file_system_apitest_document_to_download.json";

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
  content::WindowedNotificationObserver page_created_;
  content::WindowedNotificationObserver page_closed_;
};

// TODO(tbarzic): We should probably share GetTestFilePath and LoadJSONFile
// with drive_file_system_unittest.
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

// Adds a next feed URL property to the given feed value.
bool AddNextFeedURLToFeedValue(const std::string& url, base::Value* feed) {
  DictionaryValue* feed_as_dictionary;
  if (!feed->GetAsDictionary(&feed_as_dictionary))
    return false;

  ListValue* links;
  if (!feed_as_dictionary->GetList("feed.link", &links))
    return false;

  DictionaryValue* link_value = new DictionaryValue();
  link_value->SetString("href", url);
  link_value->SetString("rel", "next");
  link_value->SetString("type", "application/atom_xml");

  links->Append(link_value);

  return true;
}

// Action used to set mock expectations for CreateDirectory().
ACTION_P2(MockCreateDirectoryCallback, status, value) {
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(arg2, status, base::Passed(value)));
}

// Action used to set mock expecteations for GetDocuments.
ACTION_P2(MockGetDocumentsCallback, status, value) {
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(arg5, status, base::Passed(value)));
}

// Action used to mock expectations fo GetDocumentEntry.
ACTION_P2(MockGetDocumentEntryCallback, status, value) {
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(arg1, status, base::Passed(value)));
}

// Creates a cache representation of the test file with predetermined content.
void CreateFileWithContent(const FilePath& path, const std::string& content) {
  int content_size = static_cast<int>(content.length());
  ASSERT_EQ(content_size,
            file_util::WriteFile(path, content.c_str(), content_size));
}

// Action used to set mock expectations for DownloadFile().
ACTION_P(MockDownloadFileCallback, status) {
  ASSERT_TRUE(content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&CreateFileWithContent, arg1, kTestFileContent),
      base::Bind(arg3, status, arg1)));
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
        BrowserContext::GetDefaultStoragePartition(
            browser()->profile())->GetFileSystemContext()->external_provider();
    provider->AddLocalMountPoint(test_mount_point_);
  }

  // Loads the extension, which temporarily starts the lazy background page
  // to dispatch the onInstalled event. We wait until it shuts down again.
  const extensions::Extension* LoadExtensionAndWait(
      const std::string& test_name) {
    BackgroundObserver page_complete;
    FilePath extdir = test_data_dir_.AppendASCII(test_name);
    const extensions::Extension* extension = LoadExtension(extdir);
    if (extension)
      page_complete.WaitUntilClosed();
    return extension;
  }

 private:
  FilePath test_mount_point_;
};

class RestrictedFileSystemExtensionApiTest : public ExtensionApiTest {
 public:
  RestrictedFileSystemExtensionApiTest() {}

  virtual ~RestrictedFileSystemExtensionApiTest() {}

  virtual void SetUp() OVERRIDE {
    FilePath tmp_path;
    PathService::Get(base::DIR_TEMP, &tmp_path);
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDirUnderPath(tmp_path));
    mount_point_dir_ = tmp_dir_.path().Append("mount");
    // Create the mount point.
    file_util::CreateDirectory(mount_point_dir_);

    FilePath test_dir = mount_point_dir_.Append("test_dir");
    file_util::CreateDirectory(test_dir);

    FilePath test_file = test_dir.AppendASCII("test_file.foo");
    CreateFileWithContent(test_file, kTestFileContent);

    test_file = test_dir.AppendASCII("mutable_test_file.foo");
    CreateFileWithContent(test_file, kTestFileContent);

    test_file = test_dir.AppendASCII("test_file_to_delete.foo");
    CreateFileWithContent(test_file, kTestFileContent);

    test_file = test_dir.AppendASCII("test_file_to_move.foo");
    CreateFileWithContent(test_file, kTestFileContent);

    // Create test files.
    ExtensionApiTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    ExtensionApiTest::TearDown();
  }

  void AddRestrictedMountPoint() {
    fileapi::ExternalFileSystemMountPointProvider* provider =
        BrowserContext::GetDefaultStoragePartition(
            browser()->profile())->GetFileSystemContext()->external_provider();
    provider->AddRestrictedLocalMountPoint(mount_point_dir_);
  }

 protected:
  base::ScopedTempDir tmp_dir_;
  FilePath mount_point_dir_;
};


class RemoteFileSystemExtensionApiTest : public ExtensionApiTest {
 public:
  RemoteFileSystemExtensionApiTest() {}

  virtual ~RemoteFileSystemExtensionApiTest() {}

  virtual void SetUp() OVERRIDE {
    // Set up cache root and documents service to be used when creating gdata
    // system service. This has to be done early on (before the browser is
    // created) because the system service instance is initialized very early
    // by FileBrowserEventRouter.
    FilePath tmp_dir_path;
    PathService::Get(base::DIR_TEMP, &tmp_dir_path);
    ASSERT_TRUE(test_cache_root_.CreateUniqueTempDirUnderPath(tmp_dir_path));
    drive::DriveSystemServiceFactory::set_cache_root_for_test(
        test_cache_root_.path().value());

    mock_drive_service_ = new google_apis::MockDriveService();

    // |mock_drive_service_| will eventually get owned by a system service.
    drive::DriveSystemServiceFactory::set_drive_service_for_test(
        mock_drive_service_);

    ExtensionApiTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    // Let's make sure we don't leak documents service.
    drive::DriveSystemServiceFactory::set_drive_service_for_test(NULL);
    drive::DriveSystemServiceFactory::set_cache_root_for_test(std::string());
    ExtensionApiTest::TearDown();
  }

 protected:
  base::ScopedTempDir test_cache_root_;
  google_apis::MockDriveService* mock_drive_service_;
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

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest, FileBrowserWebIntentTest) {
  AddTmpMountPoint();

  ResultCatcher catcher;
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());

  // Create a test file inside the base::ScopedTempDir.
  FilePath test_file = tmp_dir.path().AppendASCII("text_file.xul");
  CreateFileWithContent(test_file, kTestFileContent);

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("webintent_handler"))) << message_;

  // Load the source component, with the fileUrl within the virtual mount
  // point.
  const extensions::Extension* extension = LoadExtensionAsComponent(
      test_data_dir_.AppendASCII("filebrowser_component"));
  ASSERT_TRUE(extension) << message_;
  std::string path = "filesystem:chrome-extension://" + extension->id() +
      "/external" + test_file.value();
  GURL url = extension->GetResourceURL("intent.html#" + path);
  ui_test_utils::NavigateToURL(browser(), url);

  // The webintent_handler sends chrome.test.succeed() on successful receipt
  // of the incoming Web Intent.
  ASSERT_TRUE(catcher.GetNextResult()) << message_;
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

IN_PROC_BROWSER_TEST_F(RestrictedFileSystemExtensionApiTest, Basic) {
  AddRestrictedMountPoint();
  ASSERT_TRUE(RunExtensionSubtest(
      "filebrowser_component", "restricted.html", kComponentFlags)) << message_;
}

IN_PROC_BROWSER_TEST_F(RemoteFileSystemExtensionApiTest,
                       RemoteMountPoint) {
  EXPECT_CALL(*mock_drive_service_, GetAccountMetadata(_)).Times(1);

  // First, file browser will try to create new directory.
  scoped_ptr<base::Value> dir_value(LoadJSONFile(kTestDirectory));
  EXPECT_CALL(*mock_drive_service_, AddNewDirectory(_, _, _))
      .WillOnce(MockCreateDirectoryCallback(
          google_apis::HTTP_SUCCESS, &dir_value));

  // Then the test will try to read an existing file file.
  // Remote filesystem should first request root feed from gdata server.
  scoped_ptr<base::Value> documents_value(LoadJSONFile(kTestRootFeed));
  EXPECT_CALL(*mock_drive_service_,
              GetDocuments(_, _, _, _, _, _))
      .WillOnce(MockGetDocumentsCallback(google_apis::HTTP_SUCCESS,
                                         &documents_value));

  // When file browser tries to read the file, remote filesystem should detect
  // that the cached file is not present on the disk and download it. Mocked
  // download file will create file with the cached name and predetermined
  // content. This is the file file browser will read content from.
  // Later in the test, file handler will try to open the same file on gdata
  // mount point. This time, DownloadFile should not be called because local
  // copy is already present in the cache.
  scoped_ptr<base::Value> document_to_download_value(
      LoadJSONFile(kTestDocumentToDownloadEntry));
  EXPECT_CALL(*mock_drive_service_,
              GetDocumentEntry("file:1_file_resource_id", _))
      .WillOnce(MockGetDocumentEntryCallback(google_apis::HTTP_SUCCESS,
                                             &document_to_download_value));

  // We expect to download url defined in document entry returned by
  // GetDocumentEntry mock implementation.
  EXPECT_CALL(*mock_drive_service_,
              DownloadFile(_, _, GURL("https://file_content_url_changed"),
                           _, _))
      .WillOnce(MockDownloadFileCallback(google_apis::HTTP_SUCCESS));

  // On exit, all operations in progress should be cancelled.
  EXPECT_CALL(*mock_drive_service_, CancelAll());

  // All is set... RUN THE TEST.
  EXPECT_TRUE(RunExtensionTest("filesystem_handler")) << message_;
  EXPECT_TRUE(RunExtensionSubtest("filebrowser_component", "remote.html",
      kComponentFlags)) << message_;
}

IN_PROC_BROWSER_TEST_F(RemoteFileSystemExtensionApiTest, ContentSearch) {
  EXPECT_CALL(*mock_drive_service_, GetAccountMetadata(_)).Times(1);

  // First, test will get drive root directory, to init file system.
  scoped_ptr<base::Value> documents_value(LoadJSONFile(kTestRootFeed));
  EXPECT_CALL(*mock_drive_service_,
              GetDocuments(_, _, "", _, _, _))
      .WillOnce(MockGetDocumentsCallback(google_apis::HTTP_SUCCESS,
                                         &documents_value));

  // Search results will be returned in two parts:
  // 1. Search will be given empty initial feed url. The returned feed will
  //    have next feed URL set to mock the situation when server returns
  //    partial result feed.
  // 2. Search will be given next feed URL from the first call as the initial
  //    feed url. Result feed will not have next feed url set.
  // In both cases search will return all files and directories in test root
  // feed.
  scoped_ptr<base::Value> first_search_value(LoadJSONFile(kTestRootFeed));

  ASSERT_TRUE(
      AddNextFeedURLToFeedValue("https://next_feed", first_search_value.get()));

  EXPECT_CALL(*mock_drive_service_,
              GetDocuments(GURL(), _, "foo", _, _, _))
      .WillOnce(MockGetDocumentsCallback(google_apis::HTTP_SUCCESS,
                                         &first_search_value));

  scoped_ptr<base::Value> second_search_value(LoadJSONFile(kTestRootFeed));
  EXPECT_CALL(*mock_drive_service_,
              GetDocuments(GURL("https://next_feed"), _, "foo", _, _, _))
      .WillOnce(MockGetDocumentsCallback(google_apis::HTTP_SUCCESS,
                                         &second_search_value));

  // Test will try to create a snapshot of the returned file.
  scoped_ptr<base::Value> document_to_download_value(
      LoadJSONFile(kTestDocumentToDownloadEntry));
  EXPECT_CALL(*mock_drive_service_,
              GetDocumentEntry("file:1_file_resource_id", _))
      .WillOnce(MockGetDocumentEntryCallback(google_apis::HTTP_SUCCESS,
                                             &document_to_download_value));

  // We expect to download url defined in document entry returned by
  // GetDocumentEntry mock implementation.
  EXPECT_CALL(*mock_drive_service_,
              DownloadFile(_, _, GURL("https://file_content_url_changed"),
                           _, _))
      .WillOnce(MockDownloadFileCallback(google_apis::HTTP_SUCCESS));

  // On exit, all operations in progress should be cancelled.
  EXPECT_CALL(*mock_drive_service_, CancelAll());

  // All is set... RUN THE TEST.
  EXPECT_TRUE(RunExtensionSubtest("filebrowser_component", "remote_search.html",
      kComponentFlags)) << message_;
}
