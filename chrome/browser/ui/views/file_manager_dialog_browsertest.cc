// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_manager_dialog.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"  // ASCIIToUTF16
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/shell_dialogs.h"  // SelectFileDialog
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/common/content_notification_types.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_path_manager.h"

// Mock listener used by test below.
class MockSelectFileDialogListener : public SelectFileDialog::Listener {
 public:
  MockSelectFileDialogListener()
    : file_selected_(false),
      canceled_(false),
      params_(NULL) {
  }

  bool file_selected() const { return file_selected_; }
  bool canceled() const { return canceled_; }
  FilePath path() const { return path_; }
  void* params() const { return params_; }

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path, int index, void* params) {
    file_selected_ = true;
    path_ = path;
    params_ = params;
  }
  virtual void MultiFilesSelected(
      const std::vector<FilePath>& files, void* params) {}
  virtual void FileSelectionCanceled(void* params) {
    canceled_ = true;
    params_ = params;
  }

 private:
  bool file_selected_;
  bool canceled_;
  FilePath path_;
  void* params_;

  DISALLOW_COPY_AND_ASSIGN(MockSelectFileDialogListener);
};

class FileManagerDialogTest : public ExtensionBrowserTest {
 public:
  void SetUp() {
    // Create the dialog wrapper object, but don't show it yet.
    listener_.reset(new MockSelectFileDialogListener());
    dialog_ = new FileManagerDialog(listener_.get());
    // Must run after our setup because it actually runs the test.
    ExtensionBrowserTest::SetUp();
  }

  void TearDown() {
    ExtensionBrowserTest::TearDown();
    // Release the dialog first, as it holds a pointer to the listener.
    dialog_.release();
    listener_.reset();
  }

  // Creates a file system mount point for a directory.
  void AddMountPoint(const FilePath& path) {
    fileapi::FileSystemPathManager* path_manager =
        browser()->profile()->GetFileSystemContext()->path_manager();
    fileapi::ExternalFileSystemMountPointProvider* provider =
        path_manager->external_provider();
    provider->AddMountPoint(path);
  }

  scoped_ptr<MockSelectFileDialogListener> listener_;
  scoped_refptr<FileManagerDialog> dialog_;
};

IN_PROC_BROWSER_TEST_F(FileManagerDialogTest, FileManagerCreateAndDestroy) {
  // Browser window must be up for us to test dialog window parent.
  gfx::NativeWindow native_window = browser()->window()->GetNativeHandle();
  ASSERT_TRUE(native_window != NULL);

  // Before we call SelectFile, dialog is not running/visible.
  ASSERT_FALSE(dialog_->IsRunning(native_window));
}

IN_PROC_BROWSER_TEST_F(FileManagerDialogTest, FileManagerDestroyListener) {
  // Some users of SelectFileDialog destroy their listener before cleaning
  // up the dialog.  Make sure we don't crash.
  dialog_->ListenerDestroyed();
  listener_.reset();
}

IN_PROC_BROWSER_TEST_F(FileManagerDialogTest, SelectFileAndCancel) {
  // Add tmp mount point even though this test won't use it directly.
  // We need this to make sure that at least one top-level directory exists
  // in the file browser.
  FilePath tmp_dir("/tmp");
  AddMountPoint(tmp_dir);

  // Spawn a dialog to open a file.  The dialog will signal that it is ready
  // via chrome.test.sendMessage() in the extension JavaScript.
  ExtensionTestMessageListener init_listener("worker-initialized",
                                             false /* will_reply */);
  gfx::NativeWindow owning_window = browser()->window()->GetNativeHandle();
  dialog_->SelectFile(SelectFileDialog::SELECT_OPEN_FILE,
                      string16() /* title */,
                      FilePath() /* default_path */,
                      NULL /* file_types */,
                       0 /* file_type_index */,
                      FILE_PATH_LITERAL("") /* default_extension */,
                      NULL /* source_contents */,
                      owning_window,
                      this /* params */);
  LOG(INFO) << "Waiting for JavaScript ready message.";
  ASSERT_TRUE(init_listener.WaitUntilSatisfied());

  // Dialog should be running now.
  ASSERT_TRUE(dialog_->IsRunning(owning_window));

  // Inject JavaScript to click the cancel button and wait for notification
  // that the window has closed.
  ui_test_utils::WindowedNotificationObserver host_destroyed(
      content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      NotificationService::AllSources());
  RenderViewHost* host = dialog_->GetRenderViewHost();
  string16 main_frame;
  string16 script = ASCIIToUTF16(
      "console.log(\'Test JavaScript injected.\');"
      "document.querySelector(\'.cancel\').click();");
  // The file selection handler closes the dialog and does not return control
  // to JavaScript, so do not wait for return values.
  host->ExecuteJavascriptInWebFrame(main_frame, script);
  LOG(INFO) << "Waiting for window close notification.";
  host_destroyed.Wait();

  // Dialog no longer believes it is running.
  ASSERT_FALSE(dialog_->IsRunning(owning_window));

  // Listener should have been informed of the cancellation.
  ASSERT_FALSE(listener_->file_selected());
  ASSERT_TRUE(listener_->canceled());
  ASSERT_EQ(this, listener_->params());
}

IN_PROC_BROWSER_TEST_F(FileManagerDialogTest, SelectFileAndOpen) {
  // Allow the tmp directory to be mounted.  We explicitly use /tmp because
  // it it whitelisted for file system access on Chrome OS.
  FilePath tmp_dir("/tmp");
  AddMountPoint(tmp_dir);

  // Create a directory with a single file in it.  ScopedTempDir will delete
  // itself and our temp file when it goes out of scope.
  ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDirUnderPath(tmp_dir));
  FilePath temp_dir = scoped_temp_dir.path();
  FilePath test_file = temp_dir.AppendASCII("file_manager_test.html");

  // Create an empty file to give us something to select.
  FILE* fp = file_util::OpenFile(test_file, "w");
  ASSERT_TRUE(fp != NULL);
  ASSERT_TRUE(file_util::CloseFile(fp));

  // Spawn a dialog to open a file.  Provide the path to the file so the dialog
  // will automatically select it.  Ensure that the OK button is enabled by
  // waiting for chrome.test.sendMessage('selection-change-complete').
  // The extension starts a Web Worker to read file metadata, so it may send
  // 'selection-change-complete' before 'worker-initialized'.  This is OK.
  ExtensionTestMessageListener init_listener("worker-initialized",
                                             false /* will_reply */);
  ExtensionTestMessageListener selection_listener("selection-change-complete",
                                                  false /* will_reply */);
  gfx::NativeWindow owning_window = browser()->window()->GetNativeHandle();
  dialog_->SelectFile(SelectFileDialog::SELECT_OPEN_FILE,
                      string16() /* title */,
                      test_file,
                      NULL /* file_types */,
                      0 /* file_type_index */,
                      FILE_PATH_LITERAL("") /* default_extension */,
                      NULL /* source_contents */,
                      owning_window,
                      this /* params */);
  LOG(INFO) << "Waiting for JavaScript initialized message.";
  ASSERT_TRUE(init_listener.WaitUntilSatisfied());
  LOG(INFO) << "Waiting for JavaScript selection-change-complete message.";
  ASSERT_TRUE(selection_listener.WaitUntilSatisfied());

  // Dialog should be running now.
  ASSERT_TRUE(dialog_->IsRunning(owning_window));

  // Inject JavaScript to click the open button and wait for notification
  // that the window has closed.
  ui_test_utils::WindowedNotificationObserver host_destroyed(
      content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      NotificationService::AllSources());
  RenderViewHost* host = dialog_->GetRenderViewHost();
  string16 main_frame;
  string16 script = ASCIIToUTF16(
      "console.log(\'Test JavaScript injected.\');"
      "document.querySelector('.ok').click();");
  // The file selection handler closes the dialog and does not return control
  // to JavaScript, so do not wait for return values.
  host->ExecuteJavascriptInWebFrame(main_frame, script);
  LOG(INFO) << "Waiting for window close notification.";
  host_destroyed.Wait();

  // Dialog no longer believes it is running.
  ASSERT_FALSE(dialog_->IsRunning(owning_window));

  // Listener should have been informed that the file was opened.
  ASSERT_TRUE(listener_->file_selected());
  ASSERT_FALSE(listener_->canceled());
  ASSERT_EQ(test_file, listener_->path());
  ASSERT_EQ(this, listener_->params());
}

IN_PROC_BROWSER_TEST_F(FileManagerDialogTest, SelectFileAndSave) {
  // Allow the tmp directory to be mounted.  We explicitly use /tmp because
  // it it whitelisted for file system access on Chrome OS.
  FilePath tmp_dir("/tmp");
  AddMountPoint(tmp_dir);

  // Create a directory with a single file in it.  ScopedTempDir will delete
  // itself and our temp file when it goes out of scope.
  ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDirUnderPath(tmp_dir));
  FilePath temp_dir = scoped_temp_dir.path();
  FilePath test_file = temp_dir.AppendASCII("file_manager_test.html");

  // Spawn a dialog to save a file, providing a suggested path.
  // Ensure "Save" button is enabled by waiting for notification from
  // chrome.test.sendMessage().
  // The extension starts a Web Worker to read file metadata, so it may send
  // 'directory-change-complete' before 'worker-initialized'.  This is OK.
  ExtensionTestMessageListener init_listener("worker-initialized",
                                             false /* will_reply */);
  ExtensionTestMessageListener dir_change_listener("directory-change-complete",
                                                   false /* will_reply */);
  gfx::NativeWindow owning_window = browser()->window()->GetNativeHandle();
  dialog_->SelectFile(SelectFileDialog::SELECT_SAVEAS_FILE,
                      string16() /* title */,
                      test_file,
                      NULL /* file_types */,
                      0 /* file_type_index */,
                      FILE_PATH_LITERAL("") /* default_extension */,
                      NULL /* source_contents */,
                      owning_window,
                      this /* params */);
  LOG(INFO) << "Waiting for JavaScript initialized message.";
  ASSERT_TRUE(init_listener.WaitUntilSatisfied());
  LOG(INFO) << "Waiting for JavaScript directory-change-complete message.";
  ASSERT_TRUE(dir_change_listener.WaitUntilSatisfied());

  // Dialog should be running now.
  ASSERT_TRUE(dialog_->IsRunning(owning_window));

  // Inject JavaScript to click the save button and wait for notification
  // that the window has closed.
  ui_test_utils::WindowedNotificationObserver host_destroyed(
      content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      NotificationService::AllSources());
  RenderViewHost* host = dialog_->GetRenderViewHost();
  string16 main_frame;
  string16 script = ASCIIToUTF16(
      "console.log(\'Test JavaScript injected.\');"
      "document.querySelector('.ok').click();");
  // The file selection handler closes the dialog and does not return control
  // to JavaScript, so do not wait for return values.
  host->ExecuteJavascriptInWebFrame(main_frame, script);
  LOG(INFO) << "Waiting for window close notification.";
  host_destroyed.Wait();

  // Dialog no longer believes it is running.
  ASSERT_FALSE(dialog_->IsRunning(owning_window));

  // Listener should have been informed that the file was selected.
  ASSERT_TRUE(listener_->file_selected());
  ASSERT_FALSE(listener_->canceled());
  ASSERT_EQ(test_file, listener_->path());
  ASSERT_EQ(this, listener_->params());
}

IN_PROC_BROWSER_TEST_F(FileManagerDialogTest, OpenSingletonTabAndCancel) {
  // Add tmp mount point even though this test won't use it directly.
  // We need this to make sure that at least one top-level directory exists
  // in the file browser.
  FilePath tmp_dir("/tmp");
  AddMountPoint(tmp_dir);

  // Spawn a dialog to open a file.  The dialog will signal that it is ready
  // via chrome.test.sendMessage() in the extension JavaScript.
  ExtensionTestMessageListener init_listener("worker-initialized",
                                             false /* will_reply */);
  gfx::NativeWindow owning_window = browser()->window()->GetNativeHandle();
  dialog_->SelectFile(SelectFileDialog::SELECT_OPEN_FILE,
                      string16() /* title */,
                      FilePath() /* default_path */,
                      NULL /* file_types */,
                       0 /* file_type_index */,
                      FILE_PATH_LITERAL("") /* default_extension */,
                      NULL /* source_contents */,
                      owning_window,
                      this /* params */);
  LOG(INFO) << "Waiting for JavaScript ready message.";
  ASSERT_TRUE(init_listener.WaitUntilSatisfied());

  // Dialog should be running now.
  ASSERT_TRUE(dialog_->IsRunning(owning_window));

  // Open a singleton tab in background.
  browser::NavigateParams p(browser(), GURL("www.google.com"),
                            PageTransition::LINK);
  p.window_action = browser::NavigateParams::SHOW_WINDOW;
  p.disposition = SINGLETON_TAB;
  browser::Navigate(&p);

  // Inject JavaScript to click the cancel button and wait for notification
  // that the window has closed.
  ui_test_utils::WindowedNotificationObserver host_destroyed(
      content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      NotificationService::AllSources());
  RenderViewHost* host = dialog_->GetRenderViewHost();
  string16 main_frame;
  string16 script = ASCIIToUTF16(
      "console.log(\'Test JavaScript injected.\');"
      "document.querySelector(\'.cancel\').click();");
  // The file selection handler closes the dialog and does not return control
  // to JavaScript, so do not wait for return values.
  host->ExecuteJavascriptInWebFrame(main_frame, script);
  LOG(INFO) << "Waiting for window close notification.";
  host_destroyed.Wait();

  // Dialog no longer believes it is running.
  ASSERT_FALSE(dialog_->IsRunning(owning_window));

  // Listener should have been informed of the cancellation.
  ASSERT_FALSE(listener_->file_selected());
  ASSERT_TRUE(listener_->canceled());
  ASSERT_EQ(this, listener_->params());
}
