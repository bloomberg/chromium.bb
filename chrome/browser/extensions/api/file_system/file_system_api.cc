// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/file_system_api.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "chrome/common/extensions/api/file_system.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"

const char kInvalidParameters[] = "Invalid parameters";
const char kSecurityError[] = "Security error";
const char kInvalidCallingPage[] = "Invalid calling page";
const char kUserCancelled[] = "User cancelled";
const char kWritableFileError[] = "Invalid file for writing";

const char kSaveFileOption[] = "saveFile";

namespace file_system = extensions::api::file_system;
namespace ChooseFile = file_system::ChooseFile;

namespace {

bool GetFilePathOfFileEntry(const std::string& filesystem_name,
                            const std::string& filesystem_path,
                            const content::RenderViewHost* render_view_host,
                            FilePath* file_path,
                            std::string* error) {
  std::string filesystem_id;
  if (!fileapi::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id)) {
    *error = kInvalidParameters;
    return false;
  }

  // Only return the display path if the process has read access to the
  // filesystem.
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  if (!policy->CanReadFileSystem(render_view_host->GetProcess()->GetID(),
                                 filesystem_id)) {
    *error = kSecurityError;
    return false;
  }

  fileapi::IsolatedContext* context = fileapi::IsolatedContext::GetInstance();
  FilePath relative_path = FilePath::FromUTF8Unsafe(filesystem_path);
  FilePath virtual_path = context->CreateVirtualPath(filesystem_id,
                                                     relative_path);
  if (!context->CrackIsolatedPath(virtual_path,
                                  &filesystem_id,
                                  NULL,
                                  file_path)) {
    *error = kInvalidParameters;
    return false;
  }

  return true;
}

bool DoCheckWritableFile(const FilePath& path) {
  // Don't allow links.
  if (file_util::PathExists(path) && file_util::IsLink(path))
    return false;

  // Create the file if it doesn't already exist.
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  int creation_flags = base::PLATFORM_FILE_CREATE |
                       base::PLATFORM_FILE_READ |
                       base::PLATFORM_FILE_WRITE;
  base::CreatePlatformFile(path, creation_flags, NULL, &error);
  return error == base::PLATFORM_FILE_OK;
}

}  // namespace

namespace extensions {

bool FileSystemGetDisplayPathFunction::RunImpl() {
  std::string filesystem_name;
  std::string filesystem_path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_path));

  FilePath file_path;
  if (!GetFilePathOfFileEntry(filesystem_name, filesystem_path,
                              render_view_host_, &file_path, &error_)) {
    return false;
  }

  result_.reset(base::Value::CreateStringValue(file_path.value()));
  return true;
}

// Handles showing a dialog to the user to ask for the filename for a file to
// save.
class FileSystemPickerFunction::FilePicker : public SelectFileDialog::Listener {
 public:
  FilePicker(FileSystemPickerFunction* function,
             content::WebContents* web_contents,
             const FilePath& suggested_path,
             bool for_save)
      : suggested_path_(suggested_path),
        for_save_(for_save),
        function_(function) {
    select_file_dialog_ = SelectFileDialog::Create(this);
    SelectFileDialog::FileTypeInfo file_type_info;
    FilePath::StringType extension = suggested_path.Extension();
    if (!extension.empty()) {
      extension.erase(extension.begin());  // drop the .
      file_type_info.extensions.resize(1);
      file_type_info.extensions[0].push_back(extension);
    }
    file_type_info.include_all_files = true;
    gfx::NativeWindow owning_window = web_contents ?
        platform_util::GetTopLevel(web_contents->GetNativeView()) : NULL;

    select_file_dialog_->SelectFile(for_save ?
                                        SelectFileDialog::SELECT_SAVEAS_FILE :
                                        SelectFileDialog::SELECT_OPEN_FILE,
                                    string16(),
                                    suggested_path,
                                    &file_type_info, 0, FILE_PATH_LITERAL(""),
                                    web_contents, owning_window, NULL);
  }

  virtual ~FilePicker() {}

 private:
  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE {
    function_->FileSelected(path, for_save_);
    delete this;
  }

  virtual void FileSelectionCanceled(void* params) OVERRIDE {
    function_->FileSelectionCanceled();
    delete this;
  }

  FilePath suggested_path_;

  // for_save_ is false when using the picker to open a file, and true
  // when saving. It affects the style of the dialog and also what happens
  // after a file is selected by the user.
  bool for_save_;

  scoped_refptr<SelectFileDialog> select_file_dialog_;
  scoped_refptr<FileSystemPickerFunction> function_;

  DISALLOW_COPY_AND_ASSIGN(FilePicker);
};

bool FileSystemPickerFunction::ShowPicker(const FilePath& suggested_path,
                                          bool for_save) {
  ShellWindowRegistry* registry = ShellWindowRegistry::Get(profile());
  DCHECK(registry);
  ShellWindow* shell_window = registry->GetShellWindowForRenderViewHost(
      render_view_host());
  if (!shell_window) {
    error_ = kInvalidCallingPage;
    return false;
  }

  // The file picker will hold a reference to this function instance, preventing
  // its destruction (and subsequent sending of the function response) until the
  // user has selected a file or cancelled the picker. At that point, the picker
  // will delete itself, which will also free the function instance.
  new FilePicker(this, shell_window->web_contents(), suggested_path, for_save);
  return true;
}

void FileSystemPickerFunction::FileSelected(const FilePath& path,
                                            bool for_save) {
  if (for_save) {
    content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
        base::Bind(&FileSystemPickerFunction::CheckWritableFile, this, path));
    return;
  }

  // Don't need to check the file, it's for reading.
  RegisterFileSystemAndSendResponse(path, false);
}

void FileSystemPickerFunction::FileSelectionCanceled() {
  error_ = kUserCancelled;
  SendResponse(false);
}

void FileSystemPickerFunction::CheckWritableFile(const FilePath& path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  if (DoCheckWritableFile(path)) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&FileSystemPickerFunction::RegisterFileSystemAndSendResponse,
            this, path, true));
    return;
  }

  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&FileSystemPickerFunction::HandleWritableFileError, this));
}

void FileSystemPickerFunction::RegisterFileSystemAndSendResponse(
    const FilePath& path, bool for_save) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::set<FilePath> filesets;
  filesets.insert(path);

  fileapi::IsolatedContext* isolated_context =
      fileapi::IsolatedContext::GetInstance();
  DCHECK(isolated_context);
  std::string filesystem_id = isolated_context->RegisterIsolatedFileSystem(
      filesets);

  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  int renderer_id = render_view_host_->GetProcess()->GetID();
  if (for_save)
    policy->GrantReadWriteFileSystem(renderer_id, filesystem_id);
  else
    policy->GrantReadFileSystem(renderer_id, filesystem_id);

  // We only need file level access for reading FileEntries. Saving FileEntries
  // just needs the file system to have read/write access, which is granted
  // above if required.
  if (!policy->CanReadFile(renderer_id, path))
    policy->GrantReadFile(renderer_id, path);

  DictionaryValue* dict = new DictionaryValue();
  result_.reset(dict);
  dict->SetString("fileSystemId", filesystem_id);
  dict->SetString("baseName", path.BaseName().AsUTF8Unsafe());
  SendResponse(true);
}

void FileSystemPickerFunction::HandleWritableFileError() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  error_ = kWritableFileError;
  SendResponse(false);
}

bool FileSystemGetWritableFileEntryFunction::RunImpl() {
  std::string filesystem_name;
  std::string filesystem_path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_path));

  FilePath suggested_path;
  if (!GetFilePathOfFileEntry(filesystem_name, filesystem_path,
                              render_view_host_, &suggested_path, &error_)) {
    return false;
  }

  return ShowPicker(suggested_path, true);
}

bool FileSystemChooseFileFunction::RunImpl() {
  scoped_ptr<ChooseFile::Params> params(ChooseFile::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool for_save = false;
  file_system::ChooseFileOptions* options = params->options.get();
  if (options) {
    if (options->type.get() &&  *options->type == kSaveFileOption)
      for_save = true;
  }

  return ShowPicker(FilePath(), for_save);
}

}  // namespace extensions
