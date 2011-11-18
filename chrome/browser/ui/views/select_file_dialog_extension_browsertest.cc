// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_file_dialog_extension.h"

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
#include "chrome/browser/ui/select_file_dialog.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
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

class SelectFileDialogExtensionBrowserTest : public ExtensionBrowserTest {
 public:
  enum DialogButtonType {
    DIALOG_BTN_OK,
    DIALOG_BTN_CANCEL
  };

  virtual void SetUp() OVERRIDE {
    // Create the dialog wrapper object, but don't show it yet.
    listener_.reset(new MockSelectFileDialogListener());
    dialog_ = new SelectFileDialogExtension(listener_.get());

    // Must run after our setup because it actually runs the test.
    ExtensionBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    ExtensionBrowserTest::TearDown();
    // Delete the dialog first, as it holds a pointer to the listener.
    dialog_ = NULL;
    listener_.reset();

    second_dialog_ = NULL;
    second_listener_.reset();
  }

  // Creates a file system mount point for a directory.
  void AddMountPoint(const FilePath& path) {
    fileapi::FileSystemPathManager* path_manager =
        browser()->profile()->GetFileSystemContext()->path_manager();
    fileapi::ExternalFileSystemMountPointProvider* provider =
        path_manager->external_provider();
    provider->AddMountPoint(path);
  }

  void OpenDialog(SelectFileDialog::Type dialog_type,
                  const FilePath& file_path,
                  const gfx::NativeWindow& owning_window,
                  const std::string& additional_message) {
    // Spawn a dialog to open a file.  The dialog will signal that it is ready
    // via chrome.test.sendMessage() in the extension JavaScript.
    ExtensionTestMessageListener init_listener("worker-initialized",
                                               false /* will_reply */);

    scoped_ptr<ExtensionTestMessageListener> additional_listener;
    if (!additional_message.empty()) {
      additional_listener.reset(
          new ExtensionTestMessageListener(additional_message, false));
    }

    dialog_->SelectFile(dialog_type,
                        string16() /* title */,
                        file_path,
                        NULL /* file_types */,
                         0 /* file_type_index */,
                        FILE_PATH_LITERAL("") /* default_extension */,
                        NULL /* source_contents */,
                        owning_window,
                        this /* params */);

    LOG(INFO) << "Waiting for JavaScript ready message.";
    ASSERT_TRUE(init_listener.WaitUntilSatisfied());

    if (additional_listener.get()) {
      LOG(INFO) << "Waiting for JavaScript " << additional_message
                << " message.";
      ASSERT_TRUE(additional_listener->WaitUntilSatisfied());
    }

    // Dialog should be running now.
    ASSERT_TRUE(dialog_->IsRunning(owning_window));
  }

  void TryOpeningSecondDialog(const gfx::NativeWindow& owning_window) {
    second_listener_.reset(new MockSelectFileDialogListener());
    second_dialog_ = new SelectFileDialogExtension(second_listener_.get());

    // At the moment we don't really care about dialog type, but we have to put
    // some dialog type.
    second_dialog_->SelectFile(SelectFileDialog::SELECT_OPEN_FILE,
                               string16() /* title */,
                               FilePath() /* default_path */,
                               NULL /* file_types */,
                               0 /* file_type_index */,
                               FILE_PATH_LITERAL("") /* default_extension */,
                               NULL /* source_contents */,
                               owning_window,
                               this /* params */);


  }

  void CloseDialog(DialogButtonType button_type,
                   const gfx::NativeWindow& owning_window) {
    // Inject JavaScript to click the cancel button and wait for notification
    // that the window has closed.
    ui_test_utils::WindowedNotificationObserver host_destroyed(
        content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
        content::NotificationService::AllSources());
    RenderViewHost* host = dialog_->GetRenderViewHost();
    string16 main_frame;
    std::string button_class =
        (button_type == DIALOG_BTN_OK) ? ".ok" : ".cancel";
    string16 script = ASCIIToUTF16(
        "console.log(\'Test JavaScript injected.\');"
        "document.querySelector(\'" + button_class + "\').click();");
    // The file selection handler closes the dialog and does not return control
    // to JavaScript, so do not wait for return values.
    host->ExecuteJavascriptInWebFrame(main_frame, script);
    LOG(INFO) << "Waiting for window close notification.";
    host_destroyed.Wait();

    // Dialog no longer believes it is running.
    ASSERT_FALSE(dialog_->IsRunning(owning_window));
  }

  scoped_ptr<MockSelectFileDialogListener> listener_;
  scoped_refptr<SelectFileDialogExtension> dialog_;

  scoped_ptr<MockSelectFileDialogListener> second_listener_;
  scoped_refptr<SelectFileDialogExtension> second_dialog_;
};

IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest, CreateAndDestroy) {
  // Browser window must be up for us to test dialog window parent.
  gfx::NativeWindow native_window = browser()->window()->GetNativeHandle();
  ASSERT_TRUE(native_window != NULL);

  // Before we call SelectFile, dialog is not running/visible.
  ASSERT_FALSE(dialog_->IsRunning(native_window));
}

IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest, DestroyListener) {
  // Some users of SelectFileDialog destroy their listener before cleaning
  // up the dialog.  Make sure we don't crash.
  dialog_->ListenerDestroyed();
  listener_.reset();
}

// TODO(jamescook): Add a test for selecting a file for an <input type='file'/>
// page element, as that uses different memory management pathways.
// crbug.com/98791

// TODO(jamescook): Make dialog work on non-ChromeOS platforms. crbug.com/97424
#if !defined(OS_CHROMEOS)
#define MAYBE_SelectFileAndCancel DISABLED_SelectFileAndCancel
#else
#define MAYBE_SelectFileAndCancel SelectFileAndCancel
#endif
IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest,
                       MAYBE_SelectFileAndCancel) {
  // Add tmp mount point even though this test won't use it directly.
  // We need this to make sure that at least one top-level directory exists
  // in the file browser.
  FilePath tmp_dir("/tmp");
  AddMountPoint(tmp_dir);

  gfx::NativeWindow owning_window = browser()->window()->GetNativeHandle();

  // FilePath() for default path.
  OpenDialog(SelectFileDialog::SELECT_OPEN_FILE, FilePath(), owning_window, "");

  // Press cancel button.
  CloseDialog(DIALOG_BTN_CANCEL, owning_window);

  // Listener should have been informed of the cancellation.
  ASSERT_FALSE(listener_->file_selected());
  ASSERT_TRUE(listener_->canceled());
  ASSERT_EQ(this, listener_->params());
}

// TODO(jamescook): Make dialog work on non-ChromeOS platforms. crbug.com/97424
#if !defined(OS_CHROMEOS)
#define MAYBE_SelectFileAndOpen DISABLED_SelectFileAndOpen
#else
#define MAYBE_SelectFileAndOpen SelectFileAndOpen
#endif
IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest,
                       MAYBE_SelectFileAndOpen) {
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

  gfx::NativeWindow owning_window = browser()->window()->GetNativeHandle();

  // Spawn a dialog to open a file.  Provide the path to the file so the dialog
  // will automatically select it.  Ensure that the OK button is enabled by
  // waiting for chrome.test.sendMessage('selection-change-complete').
  // The extension starts a Web Worker to read file metadata, so it may send
  // 'selection-change-complete' before 'worker-initialized'.  This is OK.
  OpenDialog(SelectFileDialog::SELECT_OPEN_FILE, test_file, owning_window,
             "selection-change-complete");

  // Click open button.
  CloseDialog(DIALOG_BTN_OK, owning_window);

  // Listener should have been informed that the file was opened.
  ASSERT_TRUE(listener_->file_selected());
  ASSERT_FALSE(listener_->canceled());
  ASSERT_EQ(test_file, listener_->path());
  ASSERT_EQ(this, listener_->params());
}

// TODO(jamescook): Make dialog work on non-ChromeOS platforms. crbug.com/97424
#if !defined(OS_CHROMEOS)
#define MAYBE_SelectFileAndSave DISABLED_SelectFileAndSave
#else
#define MAYBE_SelectFileAndSave SelectFileAndSave
#endif
IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest,
                       MAYBE_SelectFileAndSave) {
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

  gfx::NativeWindow owning_window = browser()->window()->GetNativeHandle();

  // Spawn a dialog to save a file, providing a suggested path.
  // Ensure "Save" button is enabled by waiting for notification from
  // chrome.test.sendMessage().
  // The extension starts a Web Worker to read file metadata, so it may send
  // 'directory-change-complete' before 'worker-initialized'.  This is OK.
  OpenDialog(SelectFileDialog::SELECT_SAVEAS_FILE, test_file, owning_window,
             "directory-change-complete");

  // Click save button.
  CloseDialog(DIALOG_BTN_OK, owning_window);

  // Listener should have been informed that the file was selected.
  ASSERT_TRUE(listener_->file_selected());
  ASSERT_FALSE(listener_->canceled());
  ASSERT_EQ(test_file, listener_->path());
  ASSERT_EQ(this, listener_->params());
}

// TODO(jamescook): Make dialog work on non-ChromeOS platforms. crbug.com/97424
#if !defined(OS_CHROMEOS)
#define MAYBE_OpenSingletonTabAndCancel DISABLED_OpenSingletonTabAndCancel
#else
#define MAYBE_OpenSingletonTabAndCancel OpenSingletonTabAndCancel
#endif
IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest,
                       MAYBE_OpenSingletonTabAndCancel) {
  // Add tmp mount point even though this test won't use it directly.
  // We need this to make sure that at least one top-level directory exists
  // in the file browser.
  FilePath tmp_dir("/tmp");
  AddMountPoint(tmp_dir);

  gfx::NativeWindow owning_window = browser()->window()->GetNativeHandle();

  OpenDialog(SelectFileDialog::SELECT_OPEN_FILE, FilePath(), owning_window, "");

  // Open a singleton tab in background.
  browser::NavigateParams p(browser(), GURL("www.google.com"),
                            content::PAGE_TRANSITION_LINK);
  p.window_action = browser::NavigateParams::SHOW_WINDOW;
  p.disposition = SINGLETON_TAB;
  browser::Navigate(&p);

  // Press cancel button.
  CloseDialog(DIALOG_BTN_CANCEL, owning_window);

  // Listener should have been informed of the cancellation.
  ASSERT_FALSE(listener_->file_selected());
  ASSERT_TRUE(listener_->canceled());
  ASSERT_EQ(this, listener_->params());
}

// TODO(jamescook): Make dialog work on non-ChromeOS platforms. crbug.com/97424
#if !defined(OS_CHROMEOS)
#define MAYBE_OpenTwoDialogs DISABLED_OpenTwoDialogs
#else
#define MAYBE_OpenTwoDialogs OpenTwoDialogs
#endif
IN_PROC_BROWSER_TEST_F(SelectFileDialogExtensionBrowserTest,
                       MAYBE_OpenTwoDialogs) {
  // Add tmp mount point even though this test won't use it directly.
  // We need this to make sure that at least one top-level directory exists
  // in the file browser.
  FilePath tmp_dir("/tmp");
  AddMountPoint(tmp_dir);

  gfx::NativeWindow owning_window = browser()->window()->GetNativeHandle();

  OpenDialog(SelectFileDialog::SELECT_OPEN_FILE, FilePath(), owning_window, "");

  TryOpeningSecondDialog(owning_window);

  // Second dialog should not be running.
  ASSERT_FALSE(second_dialog_->IsRunning(owning_window));

  // Click cancel button.
  CloseDialog(DIALOG_BTN_CANCEL, owning_window);

  // Listener should have been informed of the cancellation.
  ASSERT_FALSE(listener_->file_selected());
  ASSERT_TRUE(listener_->canceled());
  ASSERT_EQ(this, listener_->params());
}
