// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_manager_dialog.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/shell_dialogs.h"  // SelectFileDialog
#include "chrome/test/ui_test_utils.h"

class FileManagerDialogTest : public ExtensionBrowserTest {
};

class MockSelectFileDialogListener : public SelectFileDialog::Listener {
 public:
  MockSelectFileDialogListener()
    : canceled_(false) {
  }

  bool canceled() const { return canceled_; }

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path, int index, void* params) {}
  virtual void MultiFilesSelected(
      const std::vector<FilePath>& files, void* params) {}
  virtual void FileSelectionCanceled(void* params) {
    canceled_ = true;
  }

 private:
  bool canceled_;

  DISALLOW_COPY_AND_ASSIGN(MockSelectFileDialogListener);
};

IN_PROC_BROWSER_TEST_F(FileManagerDialogTest, FileManagerCreateAndDestroy) {
  // Browser window must be up for us to test dialog window parent.
  gfx::NativeWindow native_window = browser()->window()->GetNativeHandle();
  ASSERT_TRUE(native_window != NULL);

  // Create the dialog wrapper object, but don't show it yet.
  scoped_ptr<MockSelectFileDialogListener> listener(
      new MockSelectFileDialogListener());
  scoped_refptr<FileManagerDialog> dialog =
      new FileManagerDialog(listener.get());

  // Before we call SelectFile, dialog is not running/visible.
  ASSERT_FALSE(dialog->IsRunning(native_window));

  // Release the dialog first, as it holds a pointer to the listener.
  dialog.release();
  listener.reset();
}

IN_PROC_BROWSER_TEST_F(FileManagerDialogTest, FileManagerDestroyListener) {
  // Create the dialog wrapper object, but don't show it yet.
  scoped_ptr<MockSelectFileDialogListener> listener(
      new MockSelectFileDialogListener());
  scoped_refptr<FileManagerDialog> dialog =
      new FileManagerDialog(listener.get());

  // Some users of SelectFileDialog destroy their listener before cleaning
  // up the dialog.  Make sure we don't crash.
  dialog->ListenerDestroyed();
  listener.reset();

  dialog.release();
}

IN_PROC_BROWSER_TEST_F(FileManagerDialogTest, SelectFileAndCancel) {
  // Create the dialog wrapper object, but don't show it yet.
  scoped_ptr<MockSelectFileDialogListener> listener(
      new MockSelectFileDialogListener());
  scoped_refptr<FileManagerDialog> dialog =
      new FileManagerDialog(listener.get());

  // Spawn a dialog to open a file.  The dialog will signal that it is done
  // loading via chrome.test.sendMessage('ready') in the extension JavaScript.
  ExtensionTestMessageListener msg_listener("ready", false /* will_reply */);
  gfx::NativeWindow owning_window = browser()->window()->GetNativeHandle();
  dialog->SelectFile(SelectFileDialog::SELECT_OPEN_FILE,
                     string16() /* title */,
                     FilePath() /* default_path */,
                     NULL /* file_types */,
                     0 /* file_type_index */,
                     FILE_PATH_LITERAL("") /* default_extension */,
                     NULL /* source_contents */,
                     owning_window,
                     NULL /* params */);
  LOG(INFO) << "Waiting for JavaScript ready message.";
  ASSERT_TRUE(msg_listener.WaitUntilSatisfied());

  // Dialog should be running now.
  ASSERT_TRUE(dialog->IsRunning(owning_window));

  // Inject JavaScript to click the cancel button and wait for notification
  // that the window has closed.
  ui_test_utils::WindowedNotificationObserver host_destroyed(
      NotificationType::RENDER_WIDGET_HOST_DESTROYED,
      NotificationService::AllSources());
  RenderViewHost* host = dialog->GetRenderViewHost();
  std::wstring script =
      L"console.log('Test JavaScript injected.');"
      L"document.querySelector('.cancel').click();";
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      host, L"" /* frame_xpath */, script));
  LOG(INFO) << "Waiting for window close notification.";
  host_destroyed.Wait();

  // Dialog no longer believes it is running.
  ASSERT_FALSE(dialog->IsRunning(owning_window));

  // Listener should have been informed of the cancellation.
  ASSERT_TRUE(listener->canceled());

  // Enforce deleting the dialog first.
  dialog.release();
  listener.reset();
}
