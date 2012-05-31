// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_handler_api.h"

#include "base/bind.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"

namespace utils = extension_function_test_utils;

using ::testing::_;
using ::testing::Return;
using content::BrowserContext;
using content::SiteInstance;
using extensions::Extension;

namespace {

int ExtractProcessFromExtensionId(const std::string& extension_id,
                                  Profile* profile) {
  GURL extension_url =
      Extension::GetBaseURLFromExtensionId(extension_id);
  ExtensionProcessManager* manager = profile->GetExtensionProcessManager();

  SiteInstance* site_instance = manager->GetSiteInstanceForURL(extension_url);
  if (!site_instance || !site_instance->HasProcess())
    return -1;
  content::RenderProcessHost* process = site_instance->GetProcess();

  return process->GetID();
}

GURL GetSourceURLForExtension(const std::string& extension_id) {
  return GURL("chrome-extension://" + extension_id + "/");
}

GURL GetFileSystemURLForExtension(const std::string& extension_id,
                                  const std::string& virtual_path) {
  return GURL("filesystem:chrome-extension://" + extension_id + "/external/" +
              virtual_path);
}

class BackgroundObserver {
 public:
  BackgroundObserver()
      : page_created_(chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
                      content::NotificationService::AllSources()) {
  }

  void WaitUntilLoaded() {
    page_created_.Wait();
  }
 private:
  ui_test_utils::WindowedNotificationObserver page_created_;
};

class FileBrowserHandlerTest : public InProcessBrowserTest {
};

class FileBrowserHandlerExtensionTest : public ExtensionApiTest {
 protected:
  virtual void SetUp() OVERRIDE {
    // Create mount point directory that will be used in the test.
    // Mount point will be called "tmp", and it will be located in a tmp
    // directory with unique name.
    FilePath tmp_dir_path;
    PathService::Get(base::DIR_TEMP, &tmp_dir_path);
    ASSERT_TRUE(scoped_tmp_dir_.CreateUniqueTempDirUnderPath(tmp_dir_path));
    tmp_mount_point_ = scoped_tmp_dir_.path().Append("tmp");
    file_util::CreateDirectory(tmp_mount_point_);

    ExtensionApiTest::SetUp();
  }

  void AddTmpMountPoint() {
    fileapi::ExternalFileSystemMountPointProvider* provider =
        BrowserContext::GetFileSystemContext(browser()->profile())->
            external_provider();
    provider->AddLocalMountPoint(tmp_mount_point_);
  }

  // Loads the extension under path derived from |test_name|, and waits until
  // it's loaded.
  const Extension* LoadExtensionAndWait(const std::string& test_name) {
    BackgroundObserver page_complete;
    FilePath extdir = test_data_dir_.AppendASCII(test_name);
    const Extension* extension = LoadExtension(extdir);
    if (extension)
      page_complete.WaitUntilLoaded();
    return extension;
  }

  FilePath GetFullPathOnTmpMountPoint(const FilePath& relative_path) {
    return tmp_mount_point_.Append(relative_path);
  }

 private:
  ScopedTempDir scoped_tmp_dir_;
  FilePath tmp_mount_point_;
};

class MockFileSelector : public file_handler::FileSelector {
 public:
  MockFileSelector() {}
  virtual ~MockFileSelector() {}

  MOCK_METHOD2(SelectFile, void(const FilePath&, Browser*));
};

// Action used to set mock expectations for SelectFile().
ACTION_P3(MockFileSelectorResponse, fun, success, selected_path) {
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(&FileHandlerSelectFileFunction::OnFilePathSelected,
          base::Unretained(fun), success, selected_path));
}

void CheckSelectedFileContents(const FilePath& selected_path,
                               const std::string& expected_contents) {
  std::string test_file_contents;
  ASSERT_TRUE(file_util::ReadFileToString(selected_path, &test_file_contents));
  EXPECT_EQ(expected_contents, test_file_contents);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(FileBrowserHandlerExtensionTest, Simple) {
  AddTmpMountPoint();

  // Test extension that will try to write test file after we simulate
  // selectFileForSave fucntion call.
  const Extension* extension =
      LoadExtensionAndWait("file_browser/filehandler_create");

  ASSERT_TRUE(extension);

  // Path that will be "selected" by file selector.
  FilePath selected_path =
      GetFullPathOnTmpMountPoint(FilePath("test_file.txt"));

  scoped_refptr<FileHandlerSelectFileFunction> select_file_function(
      new FileHandlerSelectFileFunction());

  // Setup file selector to return |selected_path|.
  scoped_ptr<MockFileSelector> mock_file_selector(new MockFileSelector());
  EXPECT_CALL(*mock_file_selector,
              SelectFile(FilePath("some_file_name.txt"), _))
      .WillOnce(MockFileSelectorResponse(select_file_function.get(), true,
                                         selected_path));
  select_file_function->set_file_selector_for_test(
      mock_file_selector.get());

  // Let's set the extension function as it was called with user gesture from
  // the |extension|.
  select_file_function->set_has_callback(true);
  select_file_function->set_user_gesture(true);
  select_file_function->set_extension(extension);

  int render_process_host_id =
      ExtractProcessFromExtensionId(extension->id(), browser()->profile());
  select_file_function->set_render_process_host_id_for_test(
      render_process_host_id);

  GURL source_url = GetSourceURLForExtension(extension->id());
  select_file_function->set_source_url(source_url);

  // Selected path should still not exist.
  ASSERT_FALSE(file_util::PathExists(selected_path));

  // Invoke the method.
  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          select_file_function.get(),
          "[{\"suggestedName\": \"some_file_name.txt\"}]",
          browser())));

  // Check that function results are as expected.
  EXPECT_TRUE(utils::GetBoolean(result.get(), "success"));
  EXPECT_EQ(
      GetFileSystemURLForExtension(extension->id(), "tmp/test_file.txt").spec(),
      utils::GetString(result.get(), "fileURL"));

  // Path should have been created by function call.
  ASSERT_TRUE(file_util::PathExists(selected_path));

  // Run test page in test extension that will verify that the extension can
  // actually access and write the file.
  ResultCatcher catcher;
  GURL url = extension->GetResourceURL("test.html");
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  std::string expected_contents = "hello from test extension.";
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&CheckSelectedFileContents, selected_path, expected_contents));

  ui_test_utils::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(FileBrowserHandlerTest, NoUserGesture) {
  scoped_refptr<FileHandlerSelectFileFunction> select_file_function(
      new FileHandlerSelectFileFunction());

  scoped_ptr<MockFileSelector> mock_file_selector(new MockFileSelector());
  EXPECT_CALL(*mock_file_selector, SelectFile(_, _))
      .Times(0);

  select_file_function->set_file_selector_for_test(
      mock_file_selector.get());

  std::string error =
      utils::RunFunctionAndReturnError(
          select_file_function.get(),
          "[{\"suggestedName\": \"foo\"}]",
          browser());

  std::string expected_error =
      "This method can only be called in response to user gesture, such as a "
      "mouse click or key press.";
  EXPECT_EQ(expected_error, error);
}

IN_PROC_BROWSER_TEST_F(FileBrowserHandlerTest, SelectionFailed) {
  scoped_refptr<FileHandlerSelectFileFunction> select_file_function(
      new FileHandlerSelectFileFunction());

  scoped_ptr<MockFileSelector> mock_file_selector(new MockFileSelector());
  EXPECT_CALL(*mock_file_selector,
              SelectFile(FilePath("some_file_name.txt"), _))
      .WillOnce(MockFileSelectorResponse(select_file_function.get(), false,
                                         FilePath()));

  select_file_function->set_file_selector_for_test(
      mock_file_selector.get());
  // Without a callback the function will not generate a result.
  select_file_function->set_has_callback(true);
  select_file_function->set_user_gesture(true);

  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          select_file_function.get(),
          "[{\"suggestedName\": \"some_file_name.txt\"}]",
          browser())));

  EXPECT_FALSE(utils::GetBoolean(result.get(), "success"));
  EXPECT_EQ("", utils::GetString(result.get(), "fileURL"));
}

IN_PROC_BROWSER_TEST_F(FileBrowserHandlerTest, SuggestedFullPath) {
  scoped_refptr<FileHandlerSelectFileFunction> select_file_function(
      new FileHandlerSelectFileFunction());

  scoped_ptr<MockFileSelector> mock_file_selector(new MockFileSelector());
  EXPECT_CALL(*mock_file_selector,
              SelectFile(FilePath("some_file_name.txt"), _))
      .WillOnce(MockFileSelectorResponse(select_file_function.get(), false,
                                         FilePath()));

  select_file_function->set_file_selector_for_test(
      mock_file_selector.get());
  // Without a callback the function will not generate a result.
  select_file_function->set_has_callback(true);
  select_file_function->set_user_gesture(true);

  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          select_file_function.get(),
          "[{\"suggestedName\": \"/path_to_file/some_file_name.txt\"}]",
          browser())));

  EXPECT_FALSE(utils::GetBoolean(result.get(), "success"));
  EXPECT_EQ("", utils::GetString(result.get(), "fileURL"));
}

