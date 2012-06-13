// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/file_system_api.h"

#include "base/file_path.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/select_file_dialog.h"
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
class FileSystemGetWritableFileEntryFunction::FilePicker
    : public SelectFileDialog::Listener {
 public:
  FilePicker(FileSystemGetWritableFileEntryFunction* function,
             content::WebContents* web_contents,
             const FilePath& suggested_path)
      : suggested_path_(suggested_path),
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

    select_file_dialog_->SelectFile(SelectFileDialog::SELECT_SAVEAS_FILE,
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
    function_->FileSelected(path);
    delete this;
  }

  virtual void FileSelectionCanceled(void* params) OVERRIDE {
    function_->FileSelectionCanceled();
    delete this;
  }

  FilePath suggested_path_;

  // For managing select file dialogs.
  scoped_refptr<SelectFileDialog> select_file_dialog_;
  scoped_refptr<FileSystemGetWritableFileEntryFunction> function_;

  DISALLOW_COPY_AND_ASSIGN(FilePicker);
};

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
  new FilePicker(this, shell_window->web_contents(), suggested_path);
  return true;
}

void FileSystemGetWritableFileEntryFunction::FileSelected(
    const FilePath& path) {
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
  policy->GrantReadWriteFileSystem(renderer_id, filesystem_id);

  DictionaryValue* dict = new DictionaryValue();
  result_.reset(dict);
  dict->SetString("fileSystemId", filesystem_id);
  dict->SetString("baseName", path.BaseName().AsUTF8Unsafe());
  SendResponse(true);
}

void FileSystemGetWritableFileEntryFunction::FileSelectionCanceled() {
  error_ = kUserCancelled;
  SendResponse(false);
}

}  // namespace extensions
