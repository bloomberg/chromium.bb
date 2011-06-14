// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_manager_dialog.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_file_browser_private_api.h"
#include "chrome/browser/extensions/file_manager_util.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/extensions/extension_dialog.h"
#include "chrome/browser/ui/views/window.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace {

const int kFileManagerWidth = 720;  // pixels
const int kFileManagerHeight = 580;  // pixels

// Returns the browser represented by |window| or NULL if not found.
// TODO(jamescook):  Move this to browser_list.h.
Browser* FindBrowserWithWindow(gfx::NativeWindow window) {
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end();
       ++it) {
    Browser* browser = *it;
    if (browser->window() && browser->window()->GetNativeHandle() == window)
      return browser;
  }
  return NULL;
}

}  // namespace

// Linking this implementation of SelectFileDialog::Create into the target
// selects FileManagerDialog as the dialog of choice.
// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return new FileManagerDialog(listener);
}

/////////////////////////////////////////////////////////////////////////////

FileManagerDialog::FileManagerDialog(Listener* listener)
    : SelectFileDialog(listener),
      tab_id_(0),
      owner_window_(0) {
}

FileManagerDialog::~FileManagerDialog() {
  if (extension_dialog_)
    extension_dialog_->ObserverDestroyed();
  FileDialogFunction::Callback::Remove(tab_id_);
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
  // Release our reference to the dialog to allow it to close.
  extension_dialog_ = NULL;
  FileDialogFunction::Callback::Remove(tab_id_);
}

RenderViewHost* FileManagerDialog::GetRenderViewHost() {
  if (extension_dialog_)
    return extension_dialog_->host()->render_view_host();
  return NULL;
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
  Browser* owner_browser = FindBrowserWithWindow(owner_window);
  if (!owner_browser) {
    NOTREACHED() << "Can't find owning browser";
    return;
  }

  GURL file_browser_url = FileManagerUtil::GetFileBrowserUrlWithParams(
      type, title, default_path, file_types, file_type_index,
      default_extension);
  extension_dialog_ = ExtensionDialog::Show(file_browser_url,
      owner_browser, kFileManagerWidth, kFileManagerHeight,
      this /* ExtensionDialog::Observer */);

  // Connect our listener to FileDialogFunction's per-tab callbacks.
  TabContentsWrapper* tab = owner_browser->GetSelectedTabContentsWrapper();
  int32 tab_id = (tab ? tab->restore_tab_helper()->session_id().id() : 0);
  FileDialogFunction::Callback::Add(tab_id, listener_, params);

  tab_id_ = tab_id;
  owner_window_ = owner_window;
}
