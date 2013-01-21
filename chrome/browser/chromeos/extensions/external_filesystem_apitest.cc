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
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/google_apis/dummy_drive_service.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
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
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"

using content::BrowserContext;
using extensions::Extension;

namespace {

// These should match the counterparts in remote.js.
// Also, the size of the file in |kTestRootFeed| has to be set to
// length of kTestFileContent string.
const char kTestFileContent[] = "hello, world!";

// Contains a folder entry for the folder 'Folder' that will be 'created'.
const char kTestDirectory[] = "gdata/new_folder_entry.json";

// Contains a folder named Folder that has a file File.aBc inside of it.
const char kTestRootFeed[] = "gdata/remote_file_system_apitest_root_feed.json";

// Contains metadata of the  document that will be "downloaded" in test.
const char kTestDocumentToDownloadEntry[] =
    "gdata/remote_file_system_apitest_document_to_download.json";

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

// Creates a cache representation of the test file with predetermined content.
void CreateFileWithContent(const FilePath& path, const std::string& content) {
  int content_size = static_cast<int>(content.length());
  ASSERT_EQ(content_size,
            file_util::WriteFile(path, content.c_str(), content_size));
}

// Fake google_apis::DriveServiceInterface implementation used by
// RemoteFileSystemExtensionApiTest.
class FakeDriveService : public google_apis::DummyDriveService {
 public:
  // google_apis::DriveServiceInterface overrides:
  virtual void GetResourceList(
      const GURL& feed_url,
      int64 start_changestamp,
      const std::string& search_string,
      bool shared_with_me,
      const std::string& directory_resource_id,
      const google_apis::GetResourceListCallback& callback) OVERRIDE {
    scoped_ptr<base::Value> value(
        google_apis::test_util::LoadJSONFile(kTestRootFeed));
    if (!search_string.empty()) {
      // Search results will be returned in two parts:
      // 1. Search will be given empty initial feed url. The returned feed will
      //    have next feed URL set to mock the situation when server returns
      //    partial result feed.
      // 2. Search will be given next feed URL from the first call as the
      //    initial feed url. Result feed will not have next feed url set.
      // In both cases search will return all files and directories in test root
      // feed.
      if (feed_url.is_empty()) {
        ASSERT_TRUE(
            AddNextFeedURLToFeedValue("https://next_feed", value.get()));
      } else {
        EXPECT_EQ(GURL("https://next_feed"), feed_url);
      }
    }
    scoped_ptr<google_apis::ResourceList> result(
        google_apis::ResourceList::ExtractAndParse(*value));
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, google_apis::HTTP_SUCCESS, base::Passed(&result)));
  }

  virtual void GetResourceEntry(
      const std::string& resource_id,
      const google_apis::GetResourceEntryCallback& callback) OVERRIDE {
    EXPECT_EQ("file:1_file_resource_id", resource_id);

    scoped_ptr<base::Value> file_to_download_value(
        google_apis::test_util::LoadJSONFile(kTestDocumentToDownloadEntry));
    scoped_ptr<google_apis::ResourceEntry> file_to_download(
        google_apis::ResourceEntry::ExtractAndParse(*file_to_download_value));

    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, google_apis::HTTP_SUCCESS,
                   base::Passed(&file_to_download)));
  }

  virtual void GetAccountMetadata(
      const google_apis::GetAccountMetadataCallback& callback) OVERRIDE {
    scoped_ptr<google_apis::AccountMetadataFeed> account_metadata(
        new google_apis::AccountMetadataFeed);
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, google_apis::HTTP_SUCCESS,
                   base::Passed(&account_metadata)));
  }

  virtual void AddNewDirectory(
      const GURL& parent_content_url,
      const std::string& directory_name,
      const google_apis::GetResourceEntryCallback& callback) OVERRIDE {
    scoped_ptr<base::Value> dir_value(
        google_apis::test_util::LoadJSONFile(kTestDirectory));
    scoped_ptr<google_apis::ResourceEntry> dir_resource_entry(
        google_apis::ResourceEntry::ExtractAndParse(*dir_value));
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, google_apis::HTTP_SUCCESS,
                   base::Passed(&dir_resource_entry)));
  }

  virtual void DownloadFile(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      const google_apis::DownloadActionCallback& download_action_callback,
      const google_apis::GetContentCallback& get_content_callback) OVERRIDE {
    EXPECT_EQ(GURL("https://file_content_url_changed"), content_url);
    ASSERT_TRUE(content::BrowserThread::PostBlockingPoolTaskAndReply(
        FROM_HERE,
        base::Bind(&CreateFileWithContent, local_cache_path, kTestFileContent),
        base::Bind(download_action_callback, google_apis::HTTP_SUCCESS,
                   local_cache_path)));
  }
};

class FileSystemExtensionApiTest : public ExtensionApiTest {
 public:
  FileSystemExtensionApiTest() {}

  virtual ~FileSystemExtensionApiTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    mount_point_dir_ = tmp_dir_.path().Append("tmp");
    // Create the mount point.
    file_util::CreateDirectory(mount_point_dir_);

    // Create test files.
    ExtensionApiTest::SetUp();
  }

  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  // Adds a local mount point at at mount point /tmp.
  void AddTmpMountPoint(const std::string& extension_id) {
    Profile* profile = browser()->profile();

    GURL site = extensions::ExtensionSystem::Get(profile)->
        extension_service()->GetSiteForExtensionId(extension_id);
    fileapi::ExternalFileSystemMountPointProvider* provider =
        BrowserContext::GetStoragePartitionForSite(profile, site)->
            GetFileSystemContext()->external_provider();
    provider->AddLocalMountPoint(mount_point_dir_);
  }

  bool RunFileBrowserHandlerTest(const std::string& test_page,
                                 const std::string& file_browser_name,
                                 const std::string& handler_name) {
    // If needed, load the test file handler.
    const Extension* file_handler = NULL;
    if (!handler_name.empty()) {
      BackgroundObserver page_complete;
      file_handler = LoadExtension(test_data_dir_.AppendASCII(handler_name));
      if (!file_handler)
        return false;
      page_complete.WaitUntilLoaded();
    }

    // Load test file browser.
    const Extension* file_browser = LoadExtensionAsComponent(
        test_data_dir_.AppendASCII(file_browser_name));
    if (!file_browser)
      return false;

    // Add test mount point to file browser's and file handler's context.
    AddTmpMountPoint(file_browser->id());
    if (file_handler)
      AddTmpMountPoint(file_handler->id());

    // Run the test.
    ResultCatcher catcher;
    GURL url = file_browser->GetResourceURL(test_page);
    ui_test_utils::NavigateToURL(browser(), url);
    return catcher.GetNextResult();
  }

 protected:
  FilePath mount_point_dir_;

 private:
  base::ScopedTempDir tmp_dir_;
};

class RestrictedFileSystemExtensionApiTest : public ExtensionApiTest {
 public:
  RestrictedFileSystemExtensionApiTest() {}

  virtual ~RestrictedFileSystemExtensionApiTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
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

  void AddRestrictedMountPoint(const std::string& extension_id) {
    Profile* profile = browser()->profile();

    GURL site = extensions::ExtensionSystem::Get(profile)->
        extension_service()->GetSiteForExtensionId(extension_id);
    fileapi::ExternalFileSystemMountPointProvider* provider =
        BrowserContext::GetStoragePartitionForSite(profile, site)->
            GetFileSystemContext()->external_provider();
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

    drive::DriveSystemServiceFactory::SetFactoryForTest(
        base::Bind(&RemoteFileSystemExtensionApiTest::CreateDriveSystemService,
                   base::Unretained(this)));

    ExtensionApiTest::SetUp();
  }

 protected:
  // DriveSystemService factory function for this test.
  drive::DriveSystemService* CreateDriveSystemService(Profile* profile) {
    return new drive::DriveSystemService(profile,
                                         new FakeDriveService(),
                                         test_cache_root_.path(),
                                         NULL);
  }

  base::ScopedTempDir test_cache_root_;
};

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest, LocalFileSystem) {
  ASSERT_TRUE(RunFileBrowserHandlerTest("test.html", "local_filesystem", ""))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest, FileBrowserTest) {
  ASSERT_TRUE(RunFileBrowserHandlerTest("read.html",
                                        "filebrowser_component",
                                        "filesystem_handler"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest, FileBrowserTestLazy) {
  BackgroundObserver page_complete;
  const Extension* file_handler = LoadExtension(
      test_data_dir_.AppendASCII("filesystem_handler_lazy_background"));
  page_complete.WaitUntilClosed();

  ASSERT_TRUE(file_handler) << message_;

  AddTmpMountPoint(file_handler->id());

  ASSERT_TRUE(RunFileBrowserHandlerTest("read.html",
                                        "filebrowser_component",
                                        ""))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest, FileBrowserWebIntentTest) {
  // Create a test file inside the ScopedTempDir.
  FilePath test_file = mount_point_dir_.AppendASCII("text_file.xul");
  CreateFileWithContent(test_file, kTestFileContent);

  ASSERT_TRUE(RunFileBrowserHandlerTest("intent.html#/tmp/text_file.xul",
                                        "filebrowser_component",
                                        "webintent_handler"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest, FileBrowserTestWrite) {
  ASSERT_TRUE(RunFileBrowserHandlerTest("write.html",
                                        "filebrowser_component",
                                        "filesystem_handler_write"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest,
                       FileBrowserTestWriteReadOnly) {
  ASSERT_FALSE(RunFileBrowserHandlerTest("write.html#def",
                                         "filebrowser_component",
                                         "filesystem_handler_write"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(FileSystemExtensionApiTest,
                       FileBrowserTestWriteComponent) {
  BackgroundObserver page_complete;
  const Extension* file_handler = LoadExtensionAsComponent(
      test_data_dir_.AppendASCII("filesystem_handler_write"));
  page_complete.WaitUntilLoaded();

  ASSERT_TRUE(file_handler) << message_;

  AddTmpMountPoint(file_handler->id());

  ASSERT_TRUE(RunFileBrowserHandlerTest("write.html",
                                        "filebrowser_component",
                                        ""))
      << message_;
}

IN_PROC_BROWSER_TEST_F(RestrictedFileSystemExtensionApiTest, Basic) {
  const Extension* file_browser = LoadExtensionAsComponent(
      test_data_dir_.AppendASCII("filebrowser_component"));

  AddRestrictedMountPoint(file_browser->id());

  ResultCatcher catcher;
  GURL url = file_browser->GetResourceURL("restricted.html");
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_TRUE(catcher.GetNextResult()) << message_;
}

IN_PROC_BROWSER_TEST_F(RemoteFileSystemExtensionApiTest, RemoteMountPoint) {
  BackgroundObserver page_complete;
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("filesystem_handler")))
      << message_;
  page_complete.WaitUntilLoaded();

  EXPECT_TRUE(RunExtensionSubtest("filebrowser_component", "remote.html",
      kComponentFlags)) << message_;
}

IN_PROC_BROWSER_TEST_F(RemoteFileSystemExtensionApiTest, ContentSearch) {
  EXPECT_TRUE(RunExtensionSubtest("filebrowser_component", "remote_search.html",
      kComponentFlags)) << message_;
}

}  // namespace
