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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"

namespace utils = extension_function_test_utils;

using ::testing::_;
using ::testing::Invoke;
using content::BrowserContext;
using extensions::Extension;

namespace {

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

  FilePath GetFullPathOnTmpMountPoint(const FilePath& relative_path) {
    return tmp_mount_point_.Append(relative_path);
  }

 private:
  ScopedTempDir scoped_tmp_dir_;
  FilePath tmp_mount_point_;
};

// Action used to set mock expectations for SelectFile().
ACTION_P3(MockFileSelectorResponse, fun, success, selected_path) {
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(&FileHandlerSelectFileFunction::OnFilePathSelected,
          base::Unretained(fun), success, selected_path));
}

class MockFileSelector : public file_handler::FileSelector {
 public:
  MockFileSelector() {}
  virtual ~MockFileSelector() {}

  MOCK_METHOD2(SelectFile, void(const FilePath&, Browser*));

  virtual void set_function_for_test(
      FileHandlerSelectFileFunction* function) OVERRIDE {
    ASSERT_TRUE(!function_);
    function_ = function;
  }

  void InitStubSelectFile(bool success, const FilePath& selected_path) {
    success_ = success;
    selected_path_ = selected_path;
  }

  void StubSelectFile(const FilePath& suggested_name, Browser* browser) {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE,
        base::Bind(&FileHandlerSelectFileFunction::OnFilePathSelected,
            function_, success_, selected_path_));
    function_ = NULL;
  }

  void StubFail(const FilePath& suggested_name, Browser* browser) {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE,
        base::Bind(&FileHandlerSelectFileFunction::OnFilePathSelected,
            function_, false, FilePath()));
    function_ = NULL;
  }

 private:
  scoped_refptr<FileHandlerSelectFileFunction> function_;
  bool success_;
  FilePath selected_path_;
};

void CheckSelectedFileContents(const FilePath& selected_path,
                               const std::string& expected_contents) {
  std::string test_file_contents;
  ASSERT_TRUE(file_util::ReadFileToString(selected_path, &test_file_contents));
  EXPECT_EQ(expected_contents, test_file_contents);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(FileBrowserHandlerExtensionTest, EndToEnd) {
  AddTmpMountPoint();

  // Path that will be "selected" by file selector.
  const FilePath selected_path =
      GetFullPathOnTmpMountPoint(FilePath("test_file.txt"));

  // Setup file selector to return |selected_path|.
  scoped_ptr<MockFileSelector> mock_file_selector(new MockFileSelector());
  mock_file_selector->InitStubSelectFile(true, selected_path);
  EXPECT_CALL(*mock_file_selector,
              SelectFile(FilePath("some_file_name.txt"), _))
      .WillOnce(Invoke(mock_file_selector.get(),
                       &MockFileSelector::StubSelectFile));

  // Let's make file selector fail for suggested name "fail".
  EXPECT_CALL(*mock_file_selector,
              SelectFile(FilePath("fail"), _))
      .WillOnce(Invoke(mock_file_selector.get(), &MockFileSelector::StubFail));

  // Setup extension function parameters for the test.
  FileHandlerSelectFileFunction::set_file_selector_for_test(
      mock_file_selector.get());
  FileHandlerSelectFileFunction::set_gesture_check_disabled_for_test(true);

  // Selected path should still not exist.
  ASSERT_FALSE(file_util::PathExists(selected_path));

  ASSERT_TRUE(RunExtensionTest("file_browser/filehandler_create")) << message_;

  // Path should have been created by function call.
  ASSERT_TRUE(file_util::PathExists(selected_path));

  const std::string expected_contents = "hello from test extension.";
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&CheckSelectedFileContents, selected_path, expected_contents));

  FileHandlerSelectFileFunction::set_file_selector_for_test(NULL);
  FileHandlerSelectFileFunction::set_gesture_check_disabled_for_test(false);
}

IN_PROC_BROWSER_TEST_F(FileBrowserHandlerTest, NoUserGesture) {
  scoped_refptr<FileHandlerSelectFileFunction> select_file_function(
      new FileHandlerSelectFileFunction());

  scoped_ptr<MockFileSelector> mock_file_selector(new MockFileSelector());
  EXPECT_CALL(*mock_file_selector, SelectFile(_, _))
      .Times(0);

  FileHandlerSelectFileFunction::set_file_selector_for_test(
      mock_file_selector.get());

  std::string error =
      utils::RunFunctionAndReturnError(
          select_file_function.get(),
          "[{\"suggestedName\": \"foo\"}]",
          browser());

  const std::string expected_error =
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

  FileHandlerSelectFileFunction::set_file_selector_for_test(
      mock_file_selector.get());

  select_file_function->set_has_callback(true);
  select_file_function->set_user_gesture(true);

  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          select_file_function.get(),
          "[{\"suggestedName\": \"some_file_name.txt\"}]",
          browser())));

  EXPECT_FALSE(utils::GetBoolean(result.get(), "success"));
  DictionaryValue* entry_info;
  EXPECT_FALSE(result->GetDictionary("entry", &entry_info));
}

IN_PROC_BROWSER_TEST_F(FileBrowserHandlerTest, SuggestedFullPath) {
  scoped_refptr<FileHandlerSelectFileFunction> select_file_function(
      new FileHandlerSelectFileFunction());

  scoped_ptr<MockFileSelector> mock_file_selector(new MockFileSelector());
  EXPECT_CALL(*mock_file_selector,
              SelectFile(FilePath("some_file_name.txt"), _))
      .WillOnce(MockFileSelectorResponse(select_file_function.get(), false,
                                         FilePath()));

  FileHandlerSelectFileFunction::set_file_selector_for_test(
      mock_file_selector.get());

  select_file_function->set_has_callback(true);
  select_file_function->set_user_gesture(true);

  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnResult(
          select_file_function.get(),
          "[{\"suggestedName\": \"/path_to_file/some_file_name.txt\"}]",
          browser())));

  EXPECT_FALSE(utils::GetBoolean(result.get(), "success"));
  DictionaryValue* entry_info;
  EXPECT_FALSE(result->GetDictionary("entry", &entry_info));
}

