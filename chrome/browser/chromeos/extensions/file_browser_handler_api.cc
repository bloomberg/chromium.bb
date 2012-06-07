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
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/api/file_browser_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"

using content::BrowserContext;

namespace SelectFile = extensions::api::file_browser_handler::SelectFile;

namespace {

const char kNoUserGestureError[] =
    "This method can only be called in response to user gesture, such as a "
    "mouse click or key press.";

// File selector implementation. It is bound to SelectFileFunction instance.
// When |SelectFile| is invoked, it will show save as dialog and listen for user
// action. When user selects the file (or closes the dialog), the function's
// |OnFilePathSelected| method will be called with the result.
// When |SelectFile| is called, the class instance takes ownership of itself.
class FileSelectorImpl : public file_handler::FileSelector,
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

bool FileSelectorImpl::DoSelectFile(const FilePath& suggested_name,
                                    Browser* browser) {
  DCHECK(!dialog_.get());
  DCHECK(browser);

  if (!browser->window())
    return false;

  TabContentsWrapper* tab_contents = browser->GetSelectedTabContentsWrapper();
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

}  // namespace

FileHandlerSelectFileFunction::FileHandlerSelectFileFunction()
    : file_selector_(NULL),
      render_process_host_id_(0) {
}

FileHandlerSelectFileFunction::~FileHandlerSelectFileFunction() {
}

bool FileHandlerSelectFileFunction::RunImpl() {
  scoped_ptr<SelectFile::Params> params(SelectFile::Params::Create(*args_));

  FilePath suggested_name(params->selection_params.suggested_name);

  if (!user_gesture()) {
    error_ = kNoUserGestureError;
    return false;
  }

  // If |file_selector_| is set (e.g. in test), use it instesad of creating new
  // file selector.
  file_handler::FileSelector* file_selector = file_selector_;
  file_selector_ = NULL;
  if (!file_selector)
    file_selector = new FileSelectorImpl(this);

  file_selector->SelectFile(suggested_name.BaseName(), GetCurrentBrowser());
  return true;
}

void FileHandlerSelectFileFunction::OnFilePathSelected(
    bool success,
    const FilePath& full_path) {
  if (!success) {
    Respond(false, FilePath());
    return;
  }

  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&FileHandlerSelectFileFunction::CreateFileOnFileThread,
          this, full_path,
          base::Bind(&FileHandlerSelectFileFunction::OnFileCreated, this)));
};

void FileHandlerSelectFileFunction::CreateFileOnFileThread(
    const FilePath& full_path,
    const CreateFileCallback& callback) {
  bool success = DoCreateFile(full_path);
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(callback, success, full_path));
}

bool FileHandlerSelectFileFunction::DoCreateFile(const FilePath& full_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  // Don't allow links.
  if (file_util::PathExists(full_path) && file_util::IsLink(full_path))
    return false;

  bool created = false;
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  int creation_flags = base::PLATFORM_FILE_CREATE_ALWAYS |
                       base::PLATFORM_FILE_READ |
                       base::PLATFORM_FILE_WRITE;
  base::CreatePlatformFile(full_path, creation_flags, &created, &error);
  return error == base::PLATFORM_FILE_OK;
}

void FileHandlerSelectFileFunction::OnFileCreated(bool success,
                                                  const FilePath& full_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (success)
    GrantPermissions(full_path);
  Respond(success, full_path);
}

void FileHandlerSelectFileFunction::GrantPermissions(
    const FilePath& full_path) {
  fileapi::ExternalFileSystemMountPointProvider* external_provider =
      BrowserContext::GetFileSystemContext(profile_)->external_provider();
  DCHECK(external_provider);

  FilePath virtual_path;
  external_provider->GetVirtualPath(full_path, &virtual_path);
  DCHECK(!virtual_path.empty());

  // |render_process_host_id_| might have been previously setup in api test.
  if (!render_process_host_id_)
    render_process_host_id_ = render_view_host()->GetProcess()->GetID();

  // Grant access to this particular file to target extension. This will
  // ensure that the target extension can access only this FS entry and
  // prevent from traversing FS hierarchy upward.
  external_provider->GrantFileAccessToExtension(extension_id(), virtual_path);
  content::ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
      render_process_host_id_,
      full_path,
      file_handler_util::GetReadWritePermissions());
}

void FileHandlerSelectFileFunction::Respond(bool success,
                                            const FilePath& full_path) {
  GURL file_url;
  if (success) {
    file_manager_util::ConvertFileToFileSystemUrl(profile_, full_path,
        source_url_.GetOrigin(), &file_url);
    DCHECK(!file_url.is_empty());
  }

  scoped_ptr<SelectFile::Result::Result> result(
      new SelectFile::Result::Result());
  result->success = success;
  result->file_url = file_url.spec();

  result_.reset(SelectFile::Result::Create(*result));
  SendResponse(true);
}

