// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/extension_file_browser_private_api.h"
#include "chrome/browser/extensions/file_manager_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/extensions/extension_dialog.h"
#include "chrome/browser/ui/views/window.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/user_metrics.h"
#include "views/window/window.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace {

const int kFileManagerWidth = 720;  // pixels
const int kFileManagerHeight = 580;  // pixels

}

// Shows a dialog box for selecting a file or a folder.
class FileManagerDialog
    : public SelectFileDialog,
      public ExtensionDialog::Observer {

 public:
  explicit FileManagerDialog(Listener* listener);

  // BaseShellDialog implementation.
  virtual bool IsRunning(gfx::NativeWindow owner_window) const OVERRIDE;
  virtual void ListenerDestroyed() OVERRIDE;

  // ExtensionDialog::Observer implementation.
  virtual void ExtensionDialogIsClosing(ExtensionDialog* dialog) OVERRIDE;

 protected:
  // SelectFileDialog implementation.
  virtual void SelectFileImpl(Type type,
                              const string16& title,
                              const FilePath& default_path,
                              const FileTypeInfo* file_types,
                              int file_type_index,
                              const FilePath::StringType& default_extension,
                              gfx::NativeWindow owning_window,
                              void* params) OVERRIDE;

 private:
  virtual ~FileManagerDialog() {}

  int32 tab_id_;

  gfx::NativeWindow owner_window_;

  DISALLOW_COPY_AND_ASSIGN(FileManagerDialog);
};

// Linking this implementation of SelectFileDialog::Create into the target
// selects FileManagerDialog as the dialog of choice.
// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return new FileManagerDialog(listener);
}

FileManagerDialog::FileManagerDialog(Listener* listener)
    : SelectFileDialog(listener),
      tab_id_(0),
      owner_window_(0) {
}

bool FileManagerDialog::IsRunning(gfx::NativeWindow owner_window) const {
  return owner_window_ == owner_window;
}

void FileManagerDialog::ListenerDestroyed() {
  listener_ = NULL;
  FileDialogFunction::Callback::Remove(tab_id_);
}

void FileManagerDialog::ExtensionDialogIsClosing(ExtensionDialog* dialog) {
  owner_window_ = NULL;
}

void FileManagerDialog::SelectFileImpl(
    Type type,
    const string16& title,
    const FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension,
    gfx::NativeWindow owner_window,
    void* params) {
  if (owner_window_) {
    LOG(ERROR) << "File dialog already in use!";
    return;
  }
  Browser* active_browser = BrowserList::GetLastActive();
  if (!active_browser)
    return;

  GURL file_browser_url = FileManagerUtil::GetFileBrowserUrlWithParams(
      type, title, default_path, file_types, file_type_index,
      default_extension);
  ExtensionDialog* dialog = ExtensionDialog::Show(file_browser_url,
      active_browser, kFileManagerWidth, kFileManagerHeight,
      this /* ExtensionDialog::Observer */);

  // Connect our listener to FileDialogFunction's per-tab callbacks.
  Browser* extension_browser = dialog->host()->view()->browser();
  TabContents* contents = extension_browser->GetSelectedTabContents();
  int32 tab_id = (contents ? contents->controller().session_id().id() : 0);
  FileDialogFunction::Callback::Add(tab_id, listener_, params);

  tab_id_ = tab_id;
  owner_window_ = owner_window;
}
