// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_handler_api.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/file_handler_util.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/extensions/api/file_browser_handler_internal.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"

using content::BrowserContext;
using extensions::api::file_browser_handler_internal::FileEntryInfo;
using file_handler::FileSelector;

namespace SelectFile =
    extensions::api::file_browser_handler_internal::SelectFile;

namespace {

const char kNoUserGestureError[] =
    "This method can only be called in response to user gesture, such as a "
    "mouse click or key press.";

// File selector implementation. It is bound to SelectFileFunction instance.
// When |SelectFile| is invoked, it will show save as dialog and listen for user
// action. When user selects the file (or closes the dialog), the function's
// |OnFilePathSelected| method will be called with the result.
// When |SelectFile| is called, the class instance takes ownership of itself.
class FileSelectorImpl : public FileSelector,
                         public SelectFileDialog::Listener {
 public:
  explicit FileSelectorImpl(FileHandlerSelectFileFunction* function);
  virtual ~FileSelectorImpl() OVERRIDE;

 protected:
  // file_handler::FileSelectr overrides.
  // Shows save as dialog with suggested name in window bound to |browser|.
  // After this method is called, the selector implementation should not be
  // deleted. It will delete itself after it receives response from
  // SelectFielDialog.
  virtual void SelectFile(const FilePath& suggested_name,
                          Browser* browser) OVERRIDE;
  virtual void set_function_for_test(
      FileHandlerSelectFileFunction* function) OVERRIDE;

  // SelectFileDialog::Listener overrides.
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;
  virtual void MultiFilesSelected(const std::vector<FilePath>& files,
                                  void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

 private:
  bool DoSelectFile(const FilePath& suggeste_name, Browser* browser);
  void SendResponse(bool success, const FilePath& selected_path);

  // Dialog that is shown by selector.
  scoped_refptr<SelectFileDialog> dialog_;

  // Extension function that uses the selector.
  scoped_refptr<FileHandlerSelectFileFunction> function_;

  DISALLOW_COPY_AND_ASSIGN(FileSelectorImpl);
};

FileSelectorImpl::FileSelectorImpl(FileHandlerSelectFileFunction* function)
    : function_(function) {
}

FileSelectorImpl::~FileSelectorImpl() {
  if (dialog_.get())
    dialog_->ListenerDestroyed();
  // Send response if needed.
  if (function_)
    SendResponse(false, FilePath());
}

void FileSelectorImpl::SelectFile(const FilePath& suggested_name,
                                  Browser* browser) {
  if (!DoSelectFile(suggested_name, browser)) {
    // If the dialog wasn't launched, let's asynchronously report failure to the
    // function.
    base::MessageLoopProxy::current()->PostTask(FROM_HERE,
        base::Bind(&FileSelectorImpl::FileSelectionCanceled,
                   base::Unretained(this), reinterpret_cast<void*>(NULL)));
  }
}

// This should be used in testing only.
void FileSelectorImpl::set_function_for_test(
    FileHandlerSelectFileFunction* function) {
  NOTREACHED();
}

bool FileSelectorImpl::DoSelectFile(const FilePath& suggested_name,
                                    Browser* browser) {
  DCHECK(!dialog_.get());
  DCHECK(browser);

  if (!browser->window())
    return false;

  TabContents* tab_contents = chrome::GetActiveTabContents(browser);
  if (!tab_contents)
    return false;

  dialog_ = SelectFileDialog::Create(this);
  dialog_->SelectFile(SelectFileDialog::SELECT_SAVEAS_FILE,
      string16() /* dialog title*/, suggested_name,
      NULL /* allowed file types */, 0 /* file type index */,
      std::string() /* default file extension */, tab_contents->web_contents(),
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
  // We don't want to send multiple responses.
  if (function_.get())
    function_->OnFilePathSelected(success, selected_path);
  function_ = NULL;
}

typedef base::Callback<void (bool success,
                             const std::string& file_system_name,
                             const GURL& file_system_root)>
    FileSystemOpenCallback;

void RelayOpenFileSystemCallbackToFileThread(
    const FileSystemOpenCallback& callback,
    base::PlatformFileError error,
    const std::string& file_system_name,
    const GURL& file_system_root) {
  bool success = (error == base::PLATFORM_FILE_OK);
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(callback, success, file_system_name, file_system_root));
}

}  // namespace

FileHandlerSelectFileFunction::FileHandlerSelectFileFunction() {}

void FileHandlerSelectFileFunction::OnFilePathSelected(
    bool success,
    const FilePath& full_path) {
  if (!success) {
    Respond(false, std::string(), GURL(), FilePath());
    return;
  }

  full_path_ = full_path;

  BrowserContext::GetFileSystemContext(profile_)->OpenFileSystem(
      source_url_.GetOrigin(), fileapi::kFileSystemTypeExternal, false,
      base::Bind(&RelayOpenFileSystemCallbackToFileThread,
          base::Bind(&FileHandlerSelectFileFunction::CreateFileOnFileThread,
                     this)));
};

// static
void FileHandlerSelectFileFunction::set_file_selector_for_test(
    FileSelector* file_selector) {
  FileHandlerSelectFileFunction::file_selector_for_test_ = file_selector;
}

// static
void FileHandlerSelectFileFunction::set_gesture_check_disabled_for_test(
    bool disabled) {
  FileHandlerSelectFileFunction::gesture_check_disabled_for_test_ = disabled;
}

FileHandlerSelectFileFunction::~FileHandlerSelectFileFunction() {}

bool FileHandlerSelectFileFunction::RunImpl() {
  scoped_ptr<SelectFile::Params> params(SelectFile::Params::Create(*args_));

  FilePath suggested_name(params->selection_params.suggested_name);

  if (!user_gesture() && !gesture_check_disabled_for_test_) {
    error_ = kNoUserGestureError;
    return false;
  }

  // If |file_selector_| is set (e.g. in test), use it instesad of creating new
  // file selector.
  FileSelector* file_selector = GetFileSelector();
  file_selector->SelectFile(suggested_name.BaseName(), GetCurrentBrowser());
  return true;
}

void FileHandlerSelectFileFunction::CreateFileOnFileThread(
    bool success,
    const std::string& file_system_name,
    const GURL& file_system_root) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  if (success)
    success = DoCreateFile();
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&FileHandlerSelectFileFunction::OnFileCreated, this,
          success, file_system_name, file_system_root));
}

bool FileHandlerSelectFileFunction::DoCreateFile() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  // Don't allow links.
  if (file_util::PathExists(full_path_) && file_util::IsLink(full_path_))
    return false;

  bool created = false;
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  int creation_flags = base::PLATFORM_FILE_CREATE_ALWAYS |
                       base::PLATFORM_FILE_READ |
                       base::PLATFORM_FILE_WRITE;
  base::CreatePlatformFile(full_path_, creation_flags, &created, &error);
  return error == base::PLATFORM_FILE_OK;
}

void FileHandlerSelectFileFunction::OnFileCreated(
    bool success,
    const std::string& file_system_name,
    const GURL& file_system_root) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  FilePath virtual_path;
  if (success)
    virtual_path = GrantPermissions();
  Respond(success, file_system_name, file_system_root, virtual_path);
}

FilePath FileHandlerSelectFileFunction::GrantPermissions() {
  fileapi::ExternalFileSystemMountPointProvider* external_provider =
      BrowserContext::GetFileSystemContext(profile_)->external_provider();
  DCHECK(external_provider);

  FilePath virtual_path;
  external_provider->GetVirtualPath(full_path_, &virtual_path);
  DCHECK(!virtual_path.empty());

  // Grant access to this particular file to target extension. This will
  // ensure that the target extension can access only this FS entry and
  // prevent from traversing FS hierarchy upward.
  external_provider->GrantFileAccessToExtension(extension_id(), virtual_path);
  content::ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
      render_view_host()->GetProcess()->GetID(),
      full_path_,
      file_handler_util::GetReadWritePermissions());

  return virtual_path;
}

void FileHandlerSelectFileFunction::Respond(
    bool success,
    const std::string& file_system_name,
    const GURL& file_system_root,
    const FilePath& virtual_path) {
  scoped_ptr<SelectFile::Result::Result> result(
      new SelectFile::Result::Result());
  result->success = success;
  if (success) {
    result->entry.reset(new FileEntryInfo());
    result->entry->file_system_name = file_system_name;
    result->entry->file_system_root = file_system_root.spec();
    result->entry->file_full_path = "/" + virtual_path.value();
    result->entry->file_is_directory = false;
  }

  result_.reset(SelectFile::Result::Create(*result));
  SendResponse(true);
}

FileSelector* FileHandlerSelectFileFunction::GetFileSelector() {
  FileSelector* result = file_selector_for_test_;
  if (result) {
    result->set_function_for_test(this);
    return result;
  }
  return new FileSelectorImpl(this);
}

FileSelector* FileHandlerSelectFileFunction::file_selector_for_test_ = NULL;

bool FileHandlerSelectFileFunction::gesture_check_disabled_for_test_ = false;

