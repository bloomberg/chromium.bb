// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_file_dialog_extension.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "chrome/common/pref_names.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/file_browser_private_api.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/base_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/views/extensions/extension_dialog.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/dialogs/selected_file_info.h"

using content::BrowserThread;

namespace {

const int kFileManagerWidth = 954;  // pixels
const int kFileManagerHeight = 640;  // pixels

const int kFileManagerMinimumWidth = kFileManagerWidth * 2 / 3;  // pixels
const int kFileManagerMinimumHeight = kFileManagerHeight * 2 / 3;  // pixels

// Holds references to file manager dialogs that have callbacks pending
// to their listeners.
class PendingDialog {
 public:
  static PendingDialog* GetInstance();
  void Add(int32 tab_id, scoped_refptr<SelectFileDialogExtension> dialog);
  void Remove(int32 tab_id);
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
    DLOG(WARNING) << "Duplicate pending dialog " << tab_id;
}

void PendingDialog::Remove(int32 tab_id) {
  map_.erase(tab_id);
}

scoped_refptr<SelectFileDialogExtension> PendingDialog::Find(int32 tab_id) {
  Map::const_iterator it = map_.find(tab_id);
  if (it == map_.end())
    return NULL;
  return it->second;
}

}  // namespace

/////////////////////////////////////////////////////////////////////////////

// TODO(jamescook): Move this into a new file shell_dialogs_chromeos.cc
// TODO(jamescook): Change all instances of SelectFileDialog::Create to return
// scoped_refptr<SelectFileDialog> as object is ref-counted.
// static
SelectFileDialogExtension* SelectFileDialogExtension::Create(
    Listener* listener,
    ui::SelectFilePolicy* policy) {
  return new SelectFileDialogExtension(listener, policy);
}

SelectFileDialogExtension::SelectFileDialogExtension(
    Listener* listener,
    ui::SelectFilePolicy* policy)
    : SelectFileDialog(listener, policy),
      has_multiple_file_type_choices_(false),
      tab_id_(0),
      profile_(NULL),
      owner_window_(NULL),
      selection_type_(CANCEL),
      selection_index_(0),
      params_(NULL) {
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

void SelectFileDialogExtension::ExtensionDialogClosing(
    ExtensionDialog* dialog) {
  profile_ = NULL;
  owner_window_ = NULL;
  // Release our reference to the dialog to allow it to close.
  extension_dialog_ = NULL;
  PendingDialog::GetInstance()->Remove(tab_id_);
  // Actually invoke the appropriate callback on our listener.
  NotifyListener();
}

void SelectFileDialogExtension::ExtensionTerminated(
    ExtensionDialog* dialog) {
  // The extension would have been unloaded because of the termination,
  // reload it.
  std::string extension_id = dialog->host()->extension()->id();
  // Reload the extension after a bit; the extension may not have been unloaded
  // yet. We don't want to try to reload the extension only to have the Unload
  // code execute after us and re-unload the extension.
  //
  // TODO(rkc): This is ugly. The ideal solution is that we shouldn't need to
  // reload the extension at all - when we try to open the extension the next
  // time, the extension subsystem would automatically reload it for us. At
  // this time though this is broken because of some faulty wiring in
  // ExtensionProcessManager::CreateViewHost. Once that is fixed, remove this.
  if (profile_) {
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&ExtensionService::ReloadExtension,
            base::Unretained(profile_->GetExtensionService()),
        extension_id));
  }

  dialog->Close();
}

// static
void SelectFileDialogExtension::OnFileSelected(
    int32 tab_id,
    const ui::SelectedFileInfo& file,
    int index) {
  scoped_refptr<SelectFileDialogExtension> dialog =
      PendingDialog::GetInstance()->Find(tab_id);
  if (!dialog)
    return;
  dialog->selection_type_ = SINGLE_FILE;
  dialog->selection_files_.clear();
  dialog->selection_files_.push_back(file);
  dialog->selection_index_ = index;
}

// static
void SelectFileDialogExtension::OnMultiFilesSelected(
    int32 tab_id,
    const std::vector<ui::SelectedFileInfo>& files) {
  scoped_refptr<SelectFileDialogExtension> dialog =
      PendingDialog::GetInstance()->Find(tab_id);
  if (!dialog)
    return;
  dialog->selection_type_ = MULTIPLE_FILES;
  dialog->selection_files_ = files;
  dialog->selection_index_ = 0;
}

// static
void SelectFileDialogExtension::OnFileSelectionCanceled(int32 tab_id) {
  scoped_refptr<SelectFileDialogExtension> dialog =
      PendingDialog::GetInstance()->Find(tab_id);
  if (!dialog)
    return;
  dialog->selection_type_ = CANCEL;
  dialog->selection_files_.clear();
  dialog->selection_index_ = 0;
}

content::RenderViewHost* SelectFileDialogExtension::GetRenderViewHost() {
  if (extension_dialog_)
    return extension_dialog_->host()->render_view_host();
  return NULL;
}

void SelectFileDialogExtension::NotifyListener() {
  if (!listener_)
    return;
  switch (selection_type_) {
    case CANCEL:
      listener_->FileSelectionCanceled(params_);
      break;
    case SINGLE_FILE:
      listener_->FileSelectedWithExtraInfo(selection_files_[0],
                                           selection_index_,
                                           params_);
      break;
    case MULTIPLE_FILES:
      listener_->MultiFilesSelectedWithExtraInfo(selection_files_, params_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void SelectFileDialogExtension::AddPending(int32 tab_id) {
  PendingDialog::GetInstance()->Add(tab_id, this);
}

// static
bool SelectFileDialogExtension::PendingExists(int32 tab_id) {
  return PendingDialog::GetInstance()->Find(tab_id) != NULL;
}

bool SelectFileDialogExtension::HasMultipleFileTypeChoicesImpl() {
  return has_multiple_file_type_choices_;
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

  // The base window to associate the dialog with.
  BaseWindow* base_window = NULL;

  // The tab contents to associate the dialog with.
  const TabContents* tab = NULL;

  // First try to find a Browser using the supplied owner_window. If no owner
  // window has been supplied, this is running from a background page and should
  // be associated with the last active browser.
  Browser* owner_browser = (owner_window ?
      browser::FindBrowserWithWindow(owner_window) :
          BrowserList::GetLastActive());
  if (owner_browser) {
    base_window = owner_browser->window();
    tab = chrome::GetActiveTabContents(owner_browser);
    profile_ = tab->profile();
  } else if (owner_window) {
    // If an owner_window was supplied but we couldn't find a browser, this
    // could be for a shell window.
    // TODO(benwells): Find a better way to get a shell window from a native
    // window.
    std::vector<Profile*> profiles =
        g_browser_process->profile_manager()->GetLoadedProfiles();
    for (std::vector<Profile*>::const_iterator i(profiles.begin());
         i < profiles.end(); ++i) {
      extensions::ShellWindowRegistry* registry =
          extensions::ShellWindowRegistry::Get(*i);
      DCHECK(registry);
      ShellWindow* shell_window = registry->GetShellWindowForNativeWindow(
          owner_window);
      if (shell_window) {
        base_window = shell_window->GetBaseWindow();
        tab = shell_window->tab_contents();
        profile_ = *i;
        break;
      }
    }
  }

  if (!base_window) {
    NOTREACHED() << "Can't find owning window.";
    return;
  }
  DCHECK(profile_);

  // Check if we have another dialog opened in the tab. It's unlikely, but
  // possible. If there is no tab contents use a tab_id of 0. A dialog without
  // an associated tab contents will be shown fully screen; only one at a time
  // is allowed in this state.
  int32 tab_id = tab ? SessionTabHelper::FromWebContents(tab->web_contents())->
                           session_id().id()
                     : 0;
  if (PendingExists(tab_id)) {
    DLOG(WARNING) << "Pending dialog exists with id " << tab_id;
    return;
  }

  FilePath default_dialog_path;

  const PrefService* pref_service = profile_->GetPrefs();

  if (default_path.empty() && pref_service) {
    default_dialog_path =
        pref_service->GetFilePath(prefs::kDownloadDefaultDirectory);
  } else {
    default_dialog_path = default_path;
  }

  FilePath virtual_path;
  if (file_manager_util::ConvertFileToRelativeFileSystemPath(
          profile_, default_dialog_path, &virtual_path)) {
    virtual_path = FilePath("/").Append(virtual_path);
  } else {
    virtual_path = default_dialog_path.BaseName();
  }

  has_multiple_file_type_choices_ =
      file_types ? file_types->extensions.size() > 1 : true;

  GURL file_browser_url = file_manager_util::GetFileBrowserUrlWithParams(
      type, title, virtual_path, file_types, file_type_index,
      default_extension);

  ExtensionDialog* dialog = ExtensionDialog::Show(file_browser_url,
      base_window, profile_, tab->web_contents(),
      kFileManagerWidth, kFileManagerHeight,
#if defined(USE_AURA)
      file_manager_util::GetTitleFromType(type),
#else
      // HTML-based header used.
      string16(),
#endif
      this /* ExtensionDialog::Observer */);
  if (!dialog) {
    LOG(ERROR) << "Unable to create extension dialog";
    return;
  }

  dialog->SetMinimumContentsSize(kFileManagerMinimumWidth,
                                 kFileManagerMinimumHeight);

  // Connect our listener to FileDialogFunction's per-tab callbacks.
  AddPending(tab_id);

  extension_dialog_ = dialog;
  params_ = params;
  tab_id_ = tab_id;
  owner_window_ = owner_window;
}
