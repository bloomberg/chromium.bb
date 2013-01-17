// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The file contains the implementation of
// fileBrowserHandlerInternal.selectFile extension function.
// When invoked, the function does the following:
//  - Verifies that the extension function was invoked as a result of user
//    gesture.
//  - Display 'save as' dialog using FileSelectorImpl which waits for the user
//    feedback.
//  - Once the user selects the file path (or cancels the selection),
//    FileSelectorImpl notifies FileBrowserHandlerInternalSelectFileFunction of
//    the selection result by calling FileHandlerSelectFile::OnFilePathSelected.
//  - If the selection was canceled,
//    FileBrowserHandlerInternalSelectFileFunction returns reporting failure.
//  - If the file path was selected, the function opens external file system
//    needed to create FileEntry object for the selected path
//    (opening file system will create file system name and root url for the
//    caller's external file system).
//  - The function grants permissions needed to read/write/create file under the
//    selected path. To grant permissions to the caller, caller's extension ID
//    has to be allowed to access the files virtual path (e.g. /Downloads/foo)
//    in ExternalFileSystemMountPointProvider. Additionally, the callers render
//    process ID has to be granted read, write and create permissions for the
//    selected file's full filesystem path (e.g.
//    /home/chronos/user/Downloads/foo) in ChildProcessSecurityPolicy.
//  - After the required file access permissions are granted, result object is
//    created and returned back.

#include "chrome/browser/chromeos/extensions/file_browser_handler_api.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/file_handler_util.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/file_browser_handler_internal.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "googleurl/src/gurl.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"

using content::BrowserContext;
using content::BrowserThread;
using extensions::api::file_browser_handler_internal::FileEntryInfo;
using file_handler::FileSelector;
using file_handler::FileSelectorFactory;

namespace SelectFile =
    extensions::api::file_browser_handler_internal::SelectFile;

namespace {

const char kNoUserGestureError[] =
    "This method can only be called in response to user gesture, such as a "
    "mouse click or key press.";

// Converts file extensions to a ui::SelectFileDialog::FileTypeInfo.
ui::SelectFileDialog::FileTypeInfo ConvertExtensionsToFileTypeInfo(
    const std::vector<std::string>& extensions) {
  ui::SelectFileDialog::FileTypeInfo file_type_info;

  for (size_t i = 0; i < extensions.size(); ++i) {
    FilePath::StringType allowed_extension =
        FilePath::FromUTF8Unsafe(extensions[i]).value();

    // FileTypeInfo takes a nested vector like [["htm", "html"], ["txt"]] to
    // group equivalent extensions, but we don't use this feature here.
    std::vector<FilePath::StringType> inner_vector;
    inner_vector.push_back(allowed_extension);
    file_type_info.extensions.push_back(inner_vector);
  }

  return file_type_info;
}

// File selector implementation.
// When |SelectFile| is invoked, it will show save as dialog and listen for user
// action. When user selects the file (or closes the dialog), the function's
// |OnFilePathSelected| method will be called with the result.
// SelectFile should be called only once, because the class instance takes
// ownership of itself after the first call. It will delete itself after the
// extension function is notified of file selection result.
// Since the extension function object is ref counted, FileSelectorImpl holds
// a reference to it to ensure that the extension function doesn't go away while
// waiting for user action. The reference is released after the function is
// notified of the selection result.
class FileSelectorImpl : public FileSelector,
                         public ui::SelectFileDialog::Listener {
 public:
  explicit FileSelectorImpl();
  virtual ~FileSelectorImpl() OVERRIDE;

 protected:
  // file_handler::FileSelectr overrides.
  // Shows save as dialog with suggested name in window bound to |browser|.
  // |allowed_extensions| specifies the file extensions allowed to be shown,
  // and selected. Extensions should not include '.'.
  //
  // After this method is called, the selector implementation should not be
  // deleted by the caller. It will delete itself after it receives response
  // from SelectFielDialog.
  virtual void SelectFile(
      const FilePath& suggested_name,
      const std::vector<std::string>& allowed_extensions,
      Browser* browser,
      FileBrowserHandlerInternalSelectFileFunction* function) OVERRIDE;

  // ui::SelectFileDialog::Listener overrides.
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;
  virtual void MultiFilesSelected(const std::vector<FilePath>& files,
                                  void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

 private:
  // Initiates and shows 'save as' dialog which will be used to prompt user to
  // select a file path. The initial selected file name in the dialog will be
  // set to |suggested_name|. The dialog will be bound to the tab active in
  // |browser|.
  // |allowed_extensions| specifies the file extensions allowed to be shown,
  // and selected. Extensions should not include '.'.
  //
  // Returns boolean indicating whether the dialog has been successfully shown
  // to the user.
  bool StartSelectFile(const FilePath& suggested_name,
                       const std::vector<std::string>& allowed_extensions,
                       Browser* browser);

  // Reacts to the user action reported by the dialog and notifies |function_|
  // about file selection result (by calling |OnFilePathSelected()|).
  // The |this| object is self destruct after the function is notified.
  // |success| indicates whether user has selectd the file.
  // |selected_path| is path that was selected. It is empty if the file wasn't
  // selected.
  void SendResponse(bool success, const FilePath& selected_path);

  // Dialog that is shown by selector.
  scoped_refptr<ui::SelectFileDialog> dialog_;

  // Extension function that uses the selector.
  scoped_refptr<FileBrowserHandlerInternalSelectFileFunction> function_;

  DISALLOW_COPY_AND_ASSIGN(FileSelectorImpl);
};

FileSelectorImpl::FileSelectorImpl() {}

FileSelectorImpl::~FileSelectorImpl() {
  if (dialog_.get())
    dialog_->ListenerDestroyed();
  // Send response if needed.
  if (function_)
    SendResponse(false, FilePath());
}

void FileSelectorImpl::SelectFile(
    const FilePath& suggested_name,
    const std::vector<std::string>& allowed_extensions,
    Browser* browser,
    FileBrowserHandlerInternalSelectFileFunction* function) {
  // We will hold reference to the function until it is notified of selection
  // result.
  function_ = function;

  if (!StartSelectFile(suggested_name, allowed_extensions, browser)) {
    // If the dialog wasn't launched, let's asynchronously report failure to the
    // function.
    base::MessageLoopProxy::current()->PostTask(FROM_HERE,
        base::Bind(&FileSelectorImpl::FileSelectionCanceled,
                   base::Unretained(this), reinterpret_cast<void*>(NULL)));
  }
}

bool FileSelectorImpl::StartSelectFile(
    const FilePath& suggested_name,
    const std::vector<std::string>& allowed_extensions,
    Browser* browser) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!dialog_.get());
  DCHECK(browser);

  if (!browser->window())
    return false;

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return false;

  dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_contents));

  // Convert |allowed_extensions| to ui::SelectFileDialog::FileTypeInfo.
  ui::SelectFileDialog::FileTypeInfo allowed_file_info =
      ConvertExtensionsToFileTypeInfo(allowed_extensions);
  allowed_file_info.support_gdata = true;

  dialog_->SelectFile(ui::SelectFileDialog::SELECT_SAVEAS_FILE,
                      string16() /* dialog title*/,
                      suggested_name,
                      &allowed_file_info,
                      0 /* file type index */,
                      std::string() /* default file extension */,
                      browser->window()->GetNativeWindow(), NULL /* params */);

  return dialog_->IsRunning(browser->window()->GetNativeWindow());
}

void FileSelectorImpl::FileSelected(
    const FilePath& path, int index, void* params) {
  SendResponse(true, path);
  delete this;
}

void FileSelectorImpl::MultiFilesSelected(
    const std::vector<FilePath>& files,
    void* params) {
  // Only single file should be selected in save-as dialog.
  NOTREACHED();
}

void FileSelectorImpl::FileSelectionCanceled(
    void* params) {
  SendResponse(false, FilePath());
  delete this;
}

void FileSelectorImpl::SendResponse(bool success,
                                    const FilePath& selected_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We don't want to send multiple responses.
  if (function_.get())
    function_->OnFilePathSelected(success, selected_path);
  function_ = NULL;
}

// FileSelectorFactory implementation.
class FileSelectorFactoryImpl : public FileSelectorFactory {
 public:
  FileSelectorFactoryImpl() {}
  virtual ~FileSelectorFactoryImpl() {}

  // FileSelectorFactory implementation.
  // Creates new FileSelectorImplementation for the function.
  virtual FileSelector* CreateFileSelector() const OVERRIDE {
    return new FileSelectorImpl();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FileSelectorFactoryImpl);
};

typedef base::Callback<void (bool success,
                             const std::string& file_system_name,
                             const GURL& file_system_root)>
    FileSystemOpenCallback;

// Relays callback from file system open operation by translating file error
// returned by the operation to success boolean.
void RunOpenFileSystemCallback(
    const FileSystemOpenCallback& callback,
    base::PlatformFileError error,
    const std::string& file_system_name,
    const GURL& file_system_root) {
  bool success = (error == base::PLATFORM_FILE_OK);
  callback.Run(success, file_system_name, file_system_root);
}

}  // namespace

FileBrowserHandlerInternalSelectFileFunction::
    FileBrowserHandlerInternalSelectFileFunction()
        : file_selector_factory_(new FileSelectorFactoryImpl()),
          user_gesture_check_enabled_(true) {
}

FileBrowserHandlerInternalSelectFileFunction::
    FileBrowserHandlerInternalSelectFileFunction(
        FileSelectorFactory* file_selector_factory,
        bool enable_user_gesture_check)
        : file_selector_factory_(file_selector_factory),
          user_gesture_check_enabled_(enable_user_gesture_check) {
  DCHECK(file_selector_factory);
}

FileBrowserHandlerInternalSelectFileFunction::
    ~FileBrowserHandlerInternalSelectFileFunction() {}

bool FileBrowserHandlerInternalSelectFileFunction::RunImpl() {
  scoped_ptr<SelectFile::Params> params(SelectFile::Params::Create(*args_));

  FilePath suggested_name(params->selection_params.suggested_name);
  std::vector<std::string> allowed_extensions;
  if (params->selection_params.allowed_file_extensions.get())
    allowed_extensions = *params->selection_params.allowed_file_extensions;

  if (!user_gesture() && user_gesture_check_enabled_) {
    error_ = kNoUserGestureError;
    return false;
  }

  FileSelector* file_selector = file_selector_factory_->CreateFileSelector();
  file_selector->SelectFile(suggested_name.BaseName(),
                            allowed_extensions,
                            GetCurrentBrowser(),
                            this);
  return true;
}

void FileBrowserHandlerInternalSelectFileFunction::OnFilePathSelected(
    bool success,
    const FilePath& full_path) {
  if (!success) {
    Respond(false);
    return;
  }

  full_path_ = full_path;

  // We have to open file system in order to create a FileEntry object for the
  // selected file path.
  content::SiteInstance* site_instance = render_view_host()->GetSiteInstance();
  BrowserContext::GetStoragePartition(profile_, site_instance)->
      GetFileSystemContext()->OpenFileSystem(
          source_url_.GetOrigin(), fileapi::kFileSystemTypeExternal, false,
          base::Bind(
              &RunOpenFileSystemCallback,
              base::Bind(&FileBrowserHandlerInternalSelectFileFunction::
                             OnFileSystemOpened,
                         this)));
};

void FileBrowserHandlerInternalSelectFileFunction::OnFileSystemOpened(
    bool success,
    const std::string& file_system_name,
    const GURL& file_system_root) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!success) {
    Respond(false);
    return;
  }

  // Remember opened file system's parameters.
  file_system_name_ = file_system_name;
  file_system_root_ = file_system_root;

  GrantPermissions();

  Respond(true);
}

void FileBrowserHandlerInternalSelectFileFunction::GrantPermissions() {
  content::SiteInstance* site_instance = render_view_host()->GetSiteInstance();
  fileapi::ExternalFileSystemMountPointProvider* external_provider =
      BrowserContext::GetStoragePartition(profile_, site_instance)->
      GetFileSystemContext()->external_provider();
  DCHECK(external_provider);

  external_provider->GetVirtualPath(full_path_, &virtual_path_);
  DCHECK(!virtual_path_.empty());

  // Grant access to this particular file to target extension. This will
  // ensure that the target extension can access only this FS entry and
  // prevent from traversing FS hierarchy upward.
  external_provider->GrantFileAccessToExtension(extension_id(), virtual_path_);

  // Grant access to the selected file to target extensions render view process.
  content::ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
      render_view_host()->GetProcess()->GetID(),
      full_path_,
      file_handler_util::GetReadWritePermissions());
}

void FileBrowserHandlerInternalSelectFileFunction::Respond(bool success) {
  scoped_ptr<SelectFile::Results::Result> result(
      new SelectFile::Results::Result());
  result->success = success;

  // If the file was selected, add 'entry' object which will be later used to
  // create a FileEntry instance for the selected file.
  if (success) {
    result->entry.reset(new FileEntryInfo());
    result->entry->file_system_name = file_system_name_;
    result->entry->file_system_root = file_system_root_.spec();
    result->entry->file_full_path = "/" + virtual_path_.value();
    result->entry->file_is_directory = false;
  }

  results_ = SelectFile::Results::Create(*result);
  SendResponse(true);
}
