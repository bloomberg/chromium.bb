// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/select_file_dialog_extension.h"

#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/select_file_dialog_util.h"
#include "chrome/browser/chromeos/file_manager/url_util.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/extensions/extension_dialog.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/base_window.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/views/widget/widget.h"

using apps::ShellWindow;
using content::BrowserThread;

namespace {

const int kFileManagerWidth = 972;  // pixels
const int kFileManagerHeight = 640;  // pixels

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
  DCHECK(dialog.get());
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
  if (extension_dialog_.get())
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
    ExtensionDialog* /*dialog*/) {
  profile_ = NULL;
  owner_window_ = NULL;
  // Release our reference to the underlying dialog to allow it to close.
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
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ExtensionService::ReloadExtension,
                   base::Unretained(extensions::ExtensionSystem::Get(profile_)
                                        ->extension_service()),
                   extension_id));
  }

  dialog->GetWidget()->Close();
}

// static
void SelectFileDialogExtension::OnFileSelected(
    int32 tab_id,
    const ui::SelectedFileInfo& file,
    int index) {
  scoped_refptr<SelectFileDialogExtension> dialog =
      PendingDialog::GetInstance()->Find(tab_id);
  if (!dialog.get())
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
  if (!dialog.get())
    return;
  dialog->selection_type_ = MULTIPLE_FILES;
  dialog->selection_files_ = files;
  dialog->selection_index_ = 0;
}

// static
void SelectFileDialogExtension::OnFileSelectionCanceled(int32 tab_id) {
  scoped_refptr<SelectFileDialogExtension> dialog =
      PendingDialog::GetInstance()->Find(tab_id);
  if (!dialog.get())
    return;
  dialog->selection_type_ = CANCEL;
  dialog->selection_files_.clear();
  dialog->selection_index_ = 0;
}

content::RenderViewHost* SelectFileDialogExtension::GetRenderViewHost() {
  if (extension_dialog_.get())
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
  return PendingDialog::GetInstance()->Find(tab_id).get() != NULL;
}

bool SelectFileDialogExtension::HasMultipleFileTypeChoicesImpl() {
  return has_multiple_file_type_choices_;
}

void SelectFileDialogExtension::SelectFileImpl(
    Type type,
    const string16& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owner_window,
    void* params) {
  if (owner_window_) {
    LOG(ERROR) << "File dialog already in use!";
    return;
  }

  // The base window to associate the dialog with.
  ui::BaseWindow* base_window = NULL;

  // The web contents to associate the dialog with.
  content::WebContents* web_contents = NULL;

  // To get the base_window and profile, either a Browser or ShellWindow is
  // needed.
  Browser* owner_browser =  NULL;
  ShellWindow* shell_window = NULL;

  // If owner_window is supplied, use that to find a browser or a shell window.
  if (owner_window) {
    owner_browser = chrome::FindBrowserWithWindow(owner_window);
    if (!owner_browser) {
      // If an owner_window was supplied but we couldn't find a browser, this
      // could be for a shell window.
      shell_window = apps::ShellWindowRegistry::
          GetShellWindowForNativeWindowAnyProfile(owner_window);
    }
  }

  if (shell_window) {
    if (shell_window->window_type_is_panel()) {
      NOTREACHED() << "File dialog opened by panel.";
      return;
    }
    base_window = shell_window->GetBaseWindow();
    web_contents = shell_window->web_contents();
  } else {
    // If the owning window is still unknown, this could be a background page or
    // and extension popup. Use the last active browser.
    if (!owner_browser) {
      owner_browser =
          chrome::FindLastActiveWithHostDesktopType(chrome::GetActiveDesktop());
    }
    DCHECK(owner_browser);
    if (!owner_browser) {
      LOG(ERROR) << "Could not find browser or shell window for popup.";
      return;
    }
    base_window = owner_browser->window();
    web_contents = owner_browser->tab_strip_model()->GetActiveWebContents();
  }

  DCHECK(base_window);
  DCHECK(web_contents);
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile_);

  // Check if we have another dialog opened for the contents. It's unlikely, but
  // possible. If there is no web contents use a tab_id of -1. A dialog without
  // an associated web contents will be shown full-screen; only one at a time
  // is allowed in this state.
  int32 tab_id = SessionID::IdForTab(web_contents);
  if (PendingExists(tab_id)) {
    DLOG(WARNING) << "Pending dialog exists with id " << tab_id;
    return;
  }

  base::FilePath default_dialog_path;

  const PrefService* pref_service = profile_->GetPrefs();

  if (default_path.empty() && pref_service) {
    default_dialog_path =
        pref_service->GetFilePath(prefs::kDownloadDefaultDirectory);
  } else {
    default_dialog_path = default_path;
  }

  base::FilePath virtual_path;
  base::FilePath fallback_path = profile_->last_selected_directory().Append(
      default_dialog_path.BaseName());
  // If an absolute path is specified as the default path, convert it to the
  // virtual path in the file browser extension. Due to the current design,
  // an invalid temporal cache file path may passed as |default_dialog_path|
  // (crbug.com/178013 #9-#11). In such a case, we use the last selected
  // directory as a workaround. Real fix is tracked at crbug.com/110119.
  using file_manager::kFileManagerAppId;
  if (default_dialog_path.IsAbsolute() &&
      (file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
           profile_, kFileManagerAppId, default_dialog_path, &virtual_path) ||
       file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
           profile_, kFileManagerAppId, fallback_path, &virtual_path))) {
    virtual_path = base::FilePath("/").Append(virtual_path);
  } else {
    // If the path was relative, or failed to convert, just use the base name,
    virtual_path = default_dialog_path.BaseName();
  }

  has_multiple_file_type_choices_ =
      file_types ? file_types->extensions.size() > 1 : true;

  GURL file_manager_url =
      file_manager::util::GetFileManagerMainPageUrlWithParams(
          type, title, virtual_path, file_types, file_type_index,
          default_extension);

  ExtensionDialog* dialog = ExtensionDialog::Show(file_manager_url,
      base_window, profile_, web_contents,
      kFileManagerWidth, kFileManagerHeight,
      kFileManagerWidth,
      kFileManagerHeight,
#if defined(USE_AURA)
      file_manager::util::GetSelectFileDialogTitle(type),
#else
      // HTML-based header used.
      string16(),
#endif
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
