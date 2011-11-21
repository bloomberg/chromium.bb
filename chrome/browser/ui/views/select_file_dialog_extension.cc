// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_file_dialog_extension.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "chrome/browser/extensions/extension_file_browser_private_api.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/file_manager_util.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/extensions/extension_dialog.h"
#include "chrome/browser/ui/views/window.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

const int kFileManagerWidth = 720;  // pixels
const int kFileManagerHeight = 580;  // pixels

// Holds references to file manager dialogs that have callbacks pending
// to their listeners.
class PendingDialog {
 public:
  static PendingDialog* GetInstance();
  void Add(int32 tab_id, scoped_refptr<SelectFileDialogExtension> dialog);
  void Remove(int32 tab_id);
  // Returns scoped_refptr because in some cases, when the listener receives
  // the callback, it deletes itself and the reference to the dialog, and
  // otherwise we end up calling Close on a deleted dialog.
  scoped_refptr<SelectFileDialogExtension> Find(int32 tab_id);

 private:
  friend struct DefaultSingletonTraits<PendingDialog>;
  typedef std::map<int32, scoped_refptr<SelectFileDialogExtension> > Map;
  Map map_;
};

// static
PendingDialog* PendingDialog::GetInstance() {
  return Singleton<PendingDialog>::get();
}

void PendingDialog::Add(int32 tab_id,
                         scoped_refptr<SelectFileDialogExtension> dialog) {
  DCHECK(dialog);
  if (map_.find(tab_id) == map_.end())
    map_.insert(std::make_pair(tab_id, dialog));
  else
    LOG(WARNING) << "Duplicate pending dialog " << tab_id;
}

void PendingDialog::Remove(int32 tab_id) {
  map_.erase(tab_id);
}

scoped_refptr<SelectFileDialogExtension> PendingDialog::Find(int32 tab_id) {
  Map::const_iterator it = map_.find(tab_id);
  if (it == map_.end()) {
    LOG(WARNING) << "Pending dialog not found " << tab_id;
    return NULL;
  }
  return it->second;
}

}  // namespace

// Linking this implementation of SelectFileDialog::Create into the target
// selects FileManagerDialog as the dialog of choice.
// TODO(jamescook): Move this into a new file shell_dialogs_chromeos.cc
// TODO(jamescook): Change all instances of SelectFileDialog::Create to return
// scoped_refptr<SelectFileDialog> as object is ref-counted.
// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return SelectFileDialogExtension::Create(listener);
}

/////////////////////////////////////////////////////////////////////////////

// static
SelectFileDialogExtension* SelectFileDialogExtension::Create(
    Listener* listener) {
  return new SelectFileDialogExtension(listener);
}

SelectFileDialogExtension::SelectFileDialogExtension(Listener* listener)
    : SelectFileDialog(listener),
      params_(NULL),
      tab_id_(0),
      owner_window_(0) {
}

SelectFileDialogExtension::~SelectFileDialogExtension() {
  if (extension_dialog_)
    extension_dialog_->ObserverDestroyed();
}

bool SelectFileDialogExtension::IsRunning(
    gfx::NativeWindow owner_window) const {
  return owner_window_ == owner_window;
}

void SelectFileDialogExtension::ListenerDestroyed() {
  listener_ = NULL;
  params_ = NULL;
  PendingDialog::GetInstance()->Remove(tab_id_);
}

void SelectFileDialogExtension::ExtensionDialogIsClosing(
    ExtensionDialog* dialog) {
  owner_window_ = NULL;
  // Release our reference to the dialog to allow it to close.
  extension_dialog_ = NULL;
  PendingDialog::GetInstance()->Remove(tab_id_);
}

void SelectFileDialogExtension::Close() {
  if (extension_dialog_)
    extension_dialog_->Close();
  PendingDialog::GetInstance()->Remove(tab_id_);
}

// static
void SelectFileDialogExtension::OnFileSelected(
    int32 tab_id, const FilePath& path, int index) {
  scoped_refptr<SelectFileDialogExtension> dialog =
      PendingDialog::GetInstance()->Find(tab_id);
  if (dialog) {
    DCHECK(dialog->listener_);
    dialog->listener_->FileSelected(path, index, dialog->params_);
    dialog->Close();
  }
}

// static
void SelectFileDialogExtension::OnMultiFilesSelected(
    int32 tab_id, const std::vector<FilePath>& files) {
  scoped_refptr<SelectFileDialogExtension> dialog =
      PendingDialog::GetInstance()->Find(tab_id);
  if (dialog) {
    DCHECK(dialog->listener_);
    dialog->listener_->MultiFilesSelected(files, dialog->params_);
    dialog->Close();
  }
}

// static
void SelectFileDialogExtension::OnFileSelectionCanceled(int32 tab_id) {
  scoped_refptr<SelectFileDialogExtension> dialog =
      PendingDialog::GetInstance()->Find(tab_id);
  if (dialog) {
    DCHECK(dialog->listener_);
    dialog->listener_->FileSelectionCanceled(dialog->params_);
    dialog->Close();
  }
}

RenderViewHost* SelectFileDialogExtension::GetRenderViewHost() {
  if (extension_dialog_)
    return extension_dialog_->host()->render_view_host();
  return NULL;
}

void SelectFileDialogExtension::AddPending(int32 tab_id) {
  PendingDialog::GetInstance()->Add(tab_id, this);
}

// static
bool SelectFileDialogExtension::PendingExists(int32 tab_id) {
  return PendingDialog::GetInstance()->Find(tab_id) != NULL;
}

bool SelectFileDialogExtension::HasMultipleFileTypeChoicesImpl() {
  return hasMultipleFileTypeChoices_;
}

void SelectFileDialogExtension::SelectFileImpl(
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
  // Extension background pages may not supply an owner_window.
  Browser* owner_browser = (owner_window ?
      BrowserList::FindBrowserWithWindow(owner_window) :
      BrowserList::GetLastActive());
  if (!owner_browser) {
    NOTREACHED() << "Can't find owning browser";
    return;
  }

  TabContentsWrapper* tab = owner_browser->GetSelectedTabContentsWrapper();

  // Check if we have another dialog opened in the tab. It's unlikely, but
  // possible.
  int32 tab_id = tab ? tab->restore_tab_helper()->session_id().id() : 0;
  if (PendingExists(tab_id))
    return;

  FilePath virtual_path;
  if (!file_manager_util::ConvertFileToRelativeFileSystemPath(
          owner_browser->profile(), default_path, &virtual_path)) {
    virtual_path = default_path.BaseName();
  }

  hasMultipleFileTypeChoices_ =
      file_types ? file_types->extensions.size() > 1 : true;

  GURL file_browser_url = file_manager_util::GetFileBrowserUrlWithParams(
      type, title, virtual_path, file_types, file_type_index,
      default_extension);

  ExtensionDialog* dialog = ExtensionDialog::Show(file_browser_url,
      owner_browser, tab->tab_contents(),
      kFileManagerWidth, kFileManagerHeight, string16(),
      this /* ExtensionDialog::Observer */);
  if (!dialog) {
    LOG(ERROR) << "Unable to create extension dialog";
    return;
  }

  // Connect our listener to FileDialogFunction's per-tab callbacks.
  AddPending(tab_id);

  extension_dialog_ = dialog;
  params_ = params;
  tab_id_ = tab_id;
  owner_window_ = owner_window;
}
