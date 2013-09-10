// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/file_system_api.h"

#include "apps/saved_files_service.h"
#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/apps/directory_access_confirmation_dialog.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/file_system.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#include "base/mac/foundation_util.h"
#endif

using apps::SavedFileEntry;
using apps::SavedFilesService;
using apps::ShellWindow;
using fileapi::IsolatedContext;

const char kInvalidParameters[] = "Invalid parameters";
const char kSecurityError[] = "Security error";
const char kInvalidCallingPage[] = "Invalid calling page. This function can't "
    "be called from a background page.";
const char kUserCancelled[] = "User cancelled";
const char kWritableFileErrorFormat[] = "Error opening %s";
const char kRequiresFileSystemWriteError[] =
    "Operation requires fileSystem.write permission";
const char kRequiresFileSystemDirectoryError[] =
    "Operation requires fileSystem.directory permission";
const char kMultipleUnsupportedError[] =
    "acceptsMultiple: true is not supported for 'saveFile'";
const char kUnknownIdError[] = "Unknown id";

namespace file_system = extensions::api::file_system;
namespace ChooseEntry = file_system::ChooseEntry;

namespace {

#if defined(OS_MACOSX)
// Retrieves the localized display name for the base name of the given path.
// If the path is not localized, this will just return the base name.
std::string GetDisplayBaseName(const base::FilePath& path) {
  base::ScopedCFTypeRef<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(
      NULL, (const UInt8*)path.value().c_str(), path.value().length(), true));
  if (!url)
    return path.BaseName().value();

  CFStringRef str;
  if (LSCopyDisplayNameForURL(url, &str) != noErr)
    return path.BaseName().value();

  std::string result(base::SysCFStringRefToUTF8(str));
  CFRelease(str);
  return result;
}

// Prettifies |source_path| for OS X, by localizing every component of the
// path. Additionally, if the path is inside the user's home directory, then
// replace the home directory component with "~".
base::FilePath PrettifyPath(const base::FilePath& source_path) {
  base::FilePath home_path;
  PathService::Get(base::DIR_HOME, &home_path);
  DCHECK(source_path.IsAbsolute());

  // Break down the incoming path into components, and grab the display name
  // for every component. This will match app bundles, ".localized" folders,
  // and localized subfolders of the user's home directory.
  // Don't grab the display name of the first component, i.e., "/", as it'll
  // show up as the HDD name.
  std::vector<base::FilePath::StringType> components;
  source_path.GetComponents(&components);
  base::FilePath display_path = base::FilePath(components[0]);
  base::FilePath actual_path = display_path;
  for (std::vector<base::FilePath::StringType>::iterator i =
           components.begin() + 1; i != components.end(); ++i) {
    actual_path = actual_path.Append(*i);
    if (actual_path == home_path) {
      display_path = base::FilePath("~");
      home_path = base::FilePath();
      continue;
    }
    std::string display = GetDisplayBaseName(actual_path);
    display_path = display_path.Append(display);
  }
  DCHECK_EQ(actual_path.value(), source_path.value());
  return display_path;
}
#else  // defined(OS_MACOSX)
// Prettifies |source_path|, by replacing the user's home directory with "~"
// (if applicable).
base::FilePath PrettifyPath(const base::FilePath& source_path) {
#if defined(OS_WIN) || defined(OS_POSIX)
#if defined(OS_WIN)
  int home_key = base::DIR_PROFILE;
#elif defined(OS_POSIX)
  int home_key = base::DIR_HOME;
#endif
  base::FilePath home_path;
  base::FilePath display_path = base::FilePath::FromUTF8Unsafe("~");
  if (PathService::Get(home_key, &home_path)
      && home_path.AppendRelativePath(source_path, &display_path))
    return display_path;
#endif
  return source_path;
}
#endif  // defined(OS_MACOSX)

bool g_skip_picker_for_test = false;
bool g_use_suggested_path_for_test = false;
base::FilePath* g_path_to_be_picked_for_test;
std::vector<base::FilePath>* g_paths_to_be_picked_for_test;
bool g_skip_directory_confirmation_for_test = false;
bool g_allow_directory_access_for_test = false;

bool ValidateFileEntryAndGetPath(
    const std::string& filesystem_name,
    const std::string& filesystem_path,
    const content::RenderViewHost* render_view_host,
    base::FilePath* file_path,
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

  IsolatedContext* context = IsolatedContext::GetInstance();
  base::FilePath relative_path =
      base::FilePath::FromUTF8Unsafe(filesystem_path);
  base::FilePath virtual_path = context->CreateVirtualRootPath(filesystem_id)
      .Append(relative_path);
  fileapi::FileSystemType type;
  if (!context->CrackVirtualPath(
          virtual_path, &filesystem_id, &type, file_path)) {
    *error = kInvalidParameters;
    return false;
  }

  // The file system API is only intended to operate on file entries that
  // correspond to a native file, selected by the user so only allow file
  // systems returned by the file system API or from a drag and drop operation.
  if (type != fileapi::kFileSystemTypeNativeForPlatformApp &&
      type != fileapi::kFileSystemTypeDragged) {
    *error = kInvalidParameters;
    return false;
  }

  return true;
}

// Expand the mime-types and extensions provided in an AcceptOption, returning
// them within the passed extension vector. Returns false if no valid types
// were found.
bool GetFileTypesFromAcceptOption(
    const file_system::AcceptOption& accept_option,
    std::vector<base::FilePath::StringType>* extensions,
    string16* description) {
  std::set<base::FilePath::StringType> extension_set;
  int description_id = 0;

  if (accept_option.mime_types.get()) {
    std::vector<std::string>* list = accept_option.mime_types.get();
    bool valid_type = false;
    for (std::vector<std::string>::const_iterator iter = list->begin();
         iter != list->end(); ++iter) {
      std::vector<base::FilePath::StringType> inner;
      std::string accept_type = *iter;
      StringToLowerASCII(&accept_type);
      net::GetExtensionsForMimeType(accept_type, &inner);
      if (inner.empty())
        continue;

      if (valid_type)
        description_id = 0;  // We already have an accept type with label; if
                             // we find another, give up and use the default.
      else if (accept_type == "image/*")
        description_id = IDS_IMAGE_FILES;
      else if (accept_type == "audio/*")
        description_id = IDS_AUDIO_FILES;
      else if (accept_type == "video/*")
        description_id = IDS_VIDEO_FILES;

      extension_set.insert(inner.begin(), inner.end());
      valid_type = true;
    }
  }

  if (accept_option.extensions.get()) {
    std::vector<std::string>* list = accept_option.extensions.get();
    for (std::vector<std::string>::const_iterator iter = list->begin();
         iter != list->end(); ++iter) {
      std::string extension = *iter;
      StringToLowerASCII(&extension);
#if defined(OS_WIN)
      extension_set.insert(UTF8ToWide(*iter));
#else
      extension_set.insert(*iter);
#endif
    }
  }

  extensions->assign(extension_set.begin(), extension_set.end());
  if (extensions->empty())
    return false;

  if (accept_option.description.get())
    *description = UTF8ToUTF16(*accept_option.description.get());
  else if (description_id)
    *description = l10n_util::GetStringUTF16(description_id);

  return true;
}

// Key for the path of the directory of the file last chosen by the user in
// response to a chrome.fileSystem.chooseEntry() call.
const char kLastChooseEntryDirectory[] = "last_choose_file_directory";

const int kGraylistedPaths[] = {
#if defined(OS_WIN)
  base::DIR_PROFILE,
  base::DIR_PROGRAM_FILES,
  base::DIR_PROGRAM_FILESX86,
  base::DIR_WINDOWS,
#elif defined(OS_POSIX)
  base::DIR_HOME,
#endif
};

}  // namespace

namespace extensions {

namespace file_system_api {

bool GetLastChooseEntryDirectory(const ExtensionPrefs* prefs,
                                 const std::string& extension_id,
                                 base::FilePath* path) {
  std::string string_path;
  if (!prefs->ReadPrefAsString(extension_id,
                               kLastChooseEntryDirectory,
                               &string_path)) {
    return false;
  }

  *path = base::FilePath::FromUTF8Unsafe(string_path);
  return true;
}

void SetLastChooseEntryDirectory(ExtensionPrefs* prefs,
                                 const std::string& extension_id,
                                 const base::FilePath& path) {
  prefs->UpdateExtensionPref(extension_id,
                             kLastChooseEntryDirectory,
                             base::CreateFilePathValue(path));
}

}  // namespace file_system_api

bool FileSystemGetDisplayPathFunction::RunImpl() {
  std::string filesystem_name;
  std::string filesystem_path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_path));

  base::FilePath file_path;
  if (!ValidateFileEntryAndGetPath(filesystem_name, filesystem_path,
                                   render_view_host_, &file_path, &error_))
    return false;

  file_path = PrettifyPath(file_path);
  SetResult(new base::StringValue(file_path.value()));
  return true;
}

FileSystemEntryFunction::FileSystemEntryFunction()
    : multiple_(false),
      is_directory_(false),
      response_(NULL) {}

void FileSystemEntryFunction::CheckWritableFiles(
    const std::vector<base::FilePath>& paths) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  app_file_handler_util::CheckWritableFiles(
      paths,
      profile_,
      is_directory_,
      base::Bind(&FileSystemEntryFunction::RegisterFileSystemsAndSendResponse,
                 this,
                 paths),
      base::Bind(&FileSystemEntryFunction::HandleWritableFileError, this));
}

void FileSystemEntryFunction::RegisterFileSystemsAndSendResponse(
    const std::vector<base::FilePath>& paths) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!render_view_host_)
    return;

  CreateResponse();
  for (std::vector<base::FilePath>::const_iterator it = paths.begin();
       it != paths.end(); ++it) {
    AddEntryToResponse(*it, "");
  }
  SendResponse(true);
}

void FileSystemEntryFunction::CreateResponse() {
  DCHECK(!response_);
  response_ = new base::DictionaryValue();
  base::ListValue* list = new base::ListValue();
  response_->Set("entries", list);
  response_->SetBoolean("multiple", multiple_);
  SetResult(response_);
}

void FileSystemEntryFunction::AddEntryToResponse(
    const base::FilePath& path,
    const std::string& id_override) {
  DCHECK(response_);
  extensions::app_file_handler_util::GrantedFileEntry file_entry =
      extensions::app_file_handler_util::CreateFileEntry(
          profile(),
          GetExtension(),
          render_view_host_->GetProcess()->GetID(),
          path,
          is_directory_);
  base::ListValue* entries;
  bool success = response_->GetList("entries", &entries);
  DCHECK(success);

  base::DictionaryValue* entry = new base::DictionaryValue();
  entry->SetString("fileSystemId", file_entry.filesystem_id);
  entry->SetString("baseName", file_entry.registered_name);
  if (id_override.empty())
    entry->SetString("id", file_entry.id);
  else
    entry->SetString("id", id_override);
  entry->SetBoolean("isDirectory", is_directory_);
  entries->Append(entry);
}

void FileSystemEntryFunction::HandleWritableFileError(
    const base::FilePath& error_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  error_ = base::StringPrintf(kWritableFileErrorFormat,
                              error_path.BaseName().AsUTF8Unsafe().c_str());
  SendResponse(false);
}

bool FileSystemGetWritableEntryFunction::RunImpl() {
  std::string filesystem_name;
  std::string filesystem_path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_path));

  if (!app_file_handler_util::HasFileSystemWritePermission(extension_)) {
    error_ = kRequiresFileSystemWriteError;
    return false;
  }

  if (!ValidateFileEntryAndGetPath(filesystem_name, filesystem_path,
                                   render_view_host_, &path_, &error_))
    return false;

  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &FileSystemGetWritableEntryFunction::SetIsDirectoryOnFileThread,
          this),
      base::Bind(
          &FileSystemGetWritableEntryFunction::CheckPermissionAndSendResponse,
          this));
  return true;
}

void FileSystemGetWritableEntryFunction::CheckPermissionAndSendResponse() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (is_directory_ &&
      !extension_->HasAPIPermission(APIPermission::kFileSystemDirectory)) {
    error_ = kRequiresFileSystemDirectoryError;
    SendResponse(false);
  }
  std::vector<base::FilePath> paths;
  paths.push_back(path_);
  CheckWritableFiles(paths);
}

void FileSystemGetWritableEntryFunction::SetIsDirectoryOnFileThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  if (base::DirectoryExists(path_)) {
    is_directory_ = true;
  }
}

bool FileSystemIsWritableEntryFunction::RunImpl() {
  std::string filesystem_name;
  std::string filesystem_path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_path));

  std::string filesystem_id;
  if (!fileapi::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id)) {
    error_ = kInvalidParameters;
    return false;
  }

  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  int renderer_id = render_view_host_->GetProcess()->GetID();
  bool is_writable = policy->CanReadWriteFileSystem(renderer_id,
                                                    filesystem_id);

  SetResult(new base::FundamentalValue(is_writable));
  return true;
}

// Handles showing a dialog to the user to ask for the filename for a file to
// save or open.
class FileSystemChooseEntryFunction::FilePicker
    : public ui::SelectFileDialog::Listener {
 public:
  FilePicker(FileSystemChooseEntryFunction* function,
             content::WebContents* web_contents,
             const base::FilePath& suggested_name,
             const ui::SelectFileDialog::FileTypeInfo& file_type_info,
             ui::SelectFileDialog::Type picker_type)
      : function_(function) {
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, new ChromeSelectFilePolicy(web_contents));
    gfx::NativeWindow owning_window = web_contents ?
        platform_util::GetTopLevel(web_contents->GetView()->GetNativeView()) :
        NULL;

    if (g_skip_picker_for_test) {
      if (g_use_suggested_path_for_test) {
        content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
            base::Bind(
                &FileSystemChooseEntryFunction::FilePicker::FileSelected,
                base::Unretained(this), suggested_name, 1,
                static_cast<void*>(NULL)));
      } else if (g_path_to_be_picked_for_test) {
        content::BrowserThread::PostTask(
            content::BrowserThread::UI, FROM_HERE,
            base::Bind(
                &FileSystemChooseEntryFunction::FilePicker::FileSelected,
                base::Unretained(this), *g_path_to_be_picked_for_test, 1,
                static_cast<void*>(NULL)));
      } else if (g_paths_to_be_picked_for_test) {
        content::BrowserThread::PostTask(
            content::BrowserThread::UI,
            FROM_HERE,
            base::Bind(
                &FileSystemChooseEntryFunction::FilePicker::MultiFilesSelected,
                base::Unretained(this),
                *g_paths_to_be_picked_for_test,
                static_cast<void*>(NULL)));
      } else {
        content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
            base::Bind(
                &FileSystemChooseEntryFunction::FilePicker::
                    FileSelectionCanceled,
                base::Unretained(this), static_cast<void*>(NULL)));
      }
      return;
    }

    select_file_dialog_->SelectFile(picker_type,
                                    string16(),
                                    suggested_name,
                                    &file_type_info,
                                    0,
                                    base::FilePath::StringType(),
                                    owning_window,
                                    NULL);
  }

  virtual ~FilePicker() {}

 private:
  // ui::SelectFileDialog::Listener implementation.
  virtual void FileSelected(const base::FilePath& path,
                            int index,
                            void* params) OVERRIDE {
    std::vector<base::FilePath> paths;
    paths.push_back(path);
    MultiFilesSelected(paths, params);
  }

  virtual void FileSelectedWithExtraInfo(const ui::SelectedFileInfo& file,
                                         int index,
                                         void* params) OVERRIDE {
    // Normally, file.local_path is used because it is a native path to the
    // local read-only cached file in the case of remote file system like
    // Chrome OS's Google Drive integration. Here, however, |file.file_path| is
    // necessary because we need to create a FileEntry denoting the remote file,
    // not its cache. On other platforms than Chrome OS, they are the same.
    //
    // TODO(kinaba): remove this, once after the file picker implements proper
    // switch of the path treatment depending on the |support_drive| flag.
    FileSelected(file.file_path, index, params);
  }

  virtual void MultiFilesSelected(const std::vector<base::FilePath>& files,
                                  void* params) OVERRIDE {
    function_->FilesSelected(files);
    delete this;
  }

  virtual void MultiFilesSelectedWithExtraInfo(
      const std::vector<ui::SelectedFileInfo>& files,
      void* params) OVERRIDE {
    std::vector<base::FilePath> paths;
    for (std::vector<ui::SelectedFileInfo>::const_iterator it = files.begin();
         it != files.end(); ++it) {
      paths.push_back(it->file_path);
    }
    MultiFilesSelected(paths, params);
  }

  virtual void FileSelectionCanceled(void* params) OVERRIDE {
    function_->FileSelectionCanceled();
    delete this;
  }

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  scoped_refptr<FileSystemChooseEntryFunction> function_;

  DISALLOW_COPY_AND_ASSIGN(FilePicker);
};

void FileSystemChooseEntryFunction::ShowPicker(
    const ui::SelectFileDialog::FileTypeInfo& file_type_info,
    ui::SelectFileDialog::Type picker_type) {
  // TODO(asargent/benwells) - As a short term remediation for crbug.com/179010
  // we're adding the ability for a whitelisted extension to use this API since
  // chrome.fileBrowserHandler.selectFile is ChromeOS-only. Eventually we'd
  // like a better solution and likely this code will go back to being
  // platform-app only.
  content::WebContents* web_contents = NULL;
  if (extension_->is_platform_app()) {
    apps::ShellWindowRegistry* registry =
        apps::ShellWindowRegistry::Get(profile());
    DCHECK(registry);
    ShellWindow* shell_window = registry->GetShellWindowForRenderViewHost(
        render_view_host());
    if (!shell_window) {
      error_ = kInvalidCallingPage;
      SendResponse(false);
      return;
    }
    web_contents = shell_window->web_contents();
  } else {
    web_contents = GetAssociatedWebContents();
  }
  // The file picker will hold a reference to this function instance, preventing
  // its destruction (and subsequent sending of the function response) until the
  // user has selected a file or cancelled the picker. At that point, the picker
  // will delete itself, which will also free the function instance.
  new FilePicker(
      this, web_contents, initial_path_, file_type_info, picker_type);
}

// static
void FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
    base::FilePath* path) {
  g_skip_picker_for_test = true;
  g_use_suggested_path_for_test = false;
  g_path_to_be_picked_for_test = path;
  g_paths_to_be_picked_for_test = NULL;
}

void FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathsForTest(
    std::vector<base::FilePath>* paths) {
  g_skip_picker_for_test = true;
  g_use_suggested_path_for_test = false;
  g_paths_to_be_picked_for_test = paths;
}

// static
void FileSystemChooseEntryFunction::SkipPickerAndSelectSuggestedPathForTest() {
  g_skip_picker_for_test = true;
  g_use_suggested_path_for_test = true;
  g_path_to_be_picked_for_test = NULL;
  g_paths_to_be_picked_for_test = NULL;
}

// static
void FileSystemChooseEntryFunction::SkipPickerAndAlwaysCancelForTest() {
  g_skip_picker_for_test = true;
  g_use_suggested_path_for_test = false;
  g_path_to_be_picked_for_test = NULL;
  g_paths_to_be_picked_for_test = NULL;
}

// static
void FileSystemChooseEntryFunction::StopSkippingPickerForTest() {
  g_skip_picker_for_test = false;
}

// static
void FileSystemChooseEntryFunction::SkipDirectoryConfirmationForTest() {
  g_skip_directory_confirmation_for_test = true;
  g_allow_directory_access_for_test = true;
}

// static
void FileSystemChooseEntryFunction::AutoCancelDirectoryConfirmationForTest() {
  g_skip_directory_confirmation_for_test = true;
  g_allow_directory_access_for_test = false;
}

// static
void FileSystemChooseEntryFunction::StopSkippingDirectoryConfirmationForTest() {
  g_skip_directory_confirmation_for_test = false;
}

// static
void FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
    const std::string& name, const base::FilePath& path) {
  // For testing on Chrome OS, where to deal with remote and local paths
  // smoothly, all accessed paths need to be registered in the list of
  // external mount points.
  fileapi::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      name, fileapi::kFileSystemTypeNativeLocal, path);
}

void FileSystemChooseEntryFunction::SetInitialPathOnFileThread(
    const base::FilePath& suggested_name,
    const base::FilePath& previous_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  if (!previous_path.empty() && base::DirectoryExists(previous_path)) {
    initial_path_ = previous_path.Append(suggested_name);
  } else {
    base::FilePath documents_dir;
    if (PathService::Get(chrome::DIR_USER_DOCUMENTS, &documents_dir)) {
      initial_path_ = documents_dir.Append(suggested_name);
    } else {
      initial_path_ = suggested_name;
    }
  }
}

void FileSystemChooseEntryFunction::FilesSelected(
    const std::vector<base::FilePath>& paths) {
  DCHECK(!paths.empty());
  base::FilePath last_choose_directory;
  if (is_directory_) {
    last_choose_directory = paths[0];
  } else {
    last_choose_directory = paths[0].DirName();
  }
  file_system_api::SetLastChooseEntryDirectory(ExtensionPrefs::Get(profile()),
                                               GetExtension()->id(),
                                               last_choose_directory);
  if (is_directory_) {
    // Get the WebContents for the app window to be the parent window of the
    // confirmation dialog if necessary.
    apps::ShellWindowRegistry* registry =
        apps::ShellWindowRegistry::Get(profile());
    DCHECK(registry);
    ShellWindow* shell_window = registry->GetShellWindowForRenderViewHost(
        render_view_host());
    if (!shell_window) {
      error_ = kInvalidCallingPage;
      SendResponse(false);
      return;
    }
    content::WebContents* web_contents = shell_window->web_contents();

    content::BrowserThread::PostTask(
        content::BrowserThread::FILE,
        FROM_HERE,
        base::Bind(
            &FileSystemChooseEntryFunction::ConfirmDirectoryAccessOnFileThread,
            this,
            paths,
            web_contents));
    return;
  }

  OnDirectoryAccessConfirmed(paths);
}

void FileSystemChooseEntryFunction::FileSelectionCanceled() {
  error_ = kUserCancelled;
  SendResponse(false);
}

void FileSystemChooseEntryFunction::ConfirmDirectoryAccessOnFileThread(
    const std::vector<base::FilePath>& paths,
    content::WebContents* web_contents) {
  DCHECK_EQ(paths.size(), 1u);
  const base::FilePath path = base::MakeAbsoluteFilePath(paths[0]);
  if (path.empty()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&FileSystemChooseEntryFunction::FileSelectionCanceled,
                   this));
    return;
  }

  for (size_t i = 0; i < arraysize(kGraylistedPaths); i++) {
    base::FilePath graylisted_path;
    if (PathService::Get(kGraylistedPaths[i], &graylisted_path) &&
        (path == graylisted_path || path.IsParent(graylisted_path))) {
      if (g_skip_directory_confirmation_for_test) {
        if (g_allow_directory_access_for_test) {
          break;
        } else {
          content::BrowserThread::PostTask(
              content::BrowserThread::UI,
              FROM_HERE,
              base::Bind(&FileSystemChooseEntryFunction::FileSelectionCanceled,
                         this));
        }
        return;
      }

      content::BrowserThread::PostTask(
          content::BrowserThread::UI,
          FROM_HERE,
          base::Bind(
              CreateDirectoryAccessConfirmationDialog,
              app_file_handler_util::HasFileSystemWritePermission(extension_),
              UTF8ToUTF16(extension_->name()),
              web_contents,
              base::Bind(
                  &FileSystemChooseEntryFunction::OnDirectoryAccessConfirmed,
                  this,
                  paths),
              base::Bind(&FileSystemChooseEntryFunction::FileSelectionCanceled,
                         this)));
      return;
    }
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&FileSystemChooseEntryFunction::OnDirectoryAccessConfirmed,
                 this, paths));
}

void FileSystemChooseEntryFunction::OnDirectoryAccessConfirmed(
    const std::vector<base::FilePath>& paths) {
  if (app_file_handler_util::HasFileSystemWritePermission(extension_)) {
    CheckWritableFiles(paths);
    return;
  }

  // Don't need to check the file, it's for reading.
  RegisterFileSystemsAndSendResponse(paths);
}

void FileSystemChooseEntryFunction::BuildFileTypeInfo(
    ui::SelectFileDialog::FileTypeInfo* file_type_info,
    const base::FilePath::StringType& suggested_extension,
    const AcceptOptions* accepts,
    const bool* acceptsAllTypes) {
  file_type_info->include_all_files = true;
  if (acceptsAllTypes)
    file_type_info->include_all_files = *acceptsAllTypes;

  bool need_suggestion = !file_type_info->include_all_files &&
                         !suggested_extension.empty();

  if (accepts) {
    typedef file_system::AcceptOption AcceptOption;
    for (std::vector<linked_ptr<AcceptOption> >::const_iterator iter =
            accepts->begin(); iter != accepts->end(); ++iter) {
      string16 description;
      std::vector<base::FilePath::StringType> extensions;

      if (!GetFileTypesFromAcceptOption(**iter, &extensions, &description))
        continue;  // No extensions were found.

      file_type_info->extensions.push_back(extensions);
      file_type_info->extension_description_overrides.push_back(description);

      // If we still need to find suggested_extension, hunt for it inside the
      // extensions returned from GetFileTypesFromAcceptOption.
      if (need_suggestion && std::find(extensions.begin(),
              extensions.end(), suggested_extension) != extensions.end()) {
        need_suggestion = false;
      }
    }
  }

  // If there's nothing in our accepted extension list or we couldn't find the
  // suggested extension required, then default to accepting all types.
  if (file_type_info->extensions.empty() || need_suggestion)
    file_type_info->include_all_files = true;
}

void FileSystemChooseEntryFunction::BuildSuggestion(
    const std::string *opt_name,
    base::FilePath* suggested_name,
    base::FilePath::StringType* suggested_extension) {
  if (opt_name) {
    *suggested_name = base::FilePath::FromUTF8Unsafe(*opt_name);

    // Don't allow any path components; shorten to the base name. This should
    // result in a relative path, but in some cases may not. Clear the
    // suggestion for safety if this is the case.
    *suggested_name = suggested_name->BaseName();
    if (suggested_name->IsAbsolute())
      *suggested_name = base::FilePath();

    *suggested_extension = suggested_name->Extension();
    if (!suggested_extension->empty())
      suggested_extension->erase(suggested_extension->begin());  // drop the .
  }
}

bool FileSystemChooseEntryFunction::RunImpl() {
  scoped_ptr<ChooseEntry::Params> params(ChooseEntry::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::FilePath suggested_name;
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  ui::SelectFileDialog::Type picker_type =
      ui::SelectFileDialog::SELECT_OPEN_FILE;

  file_system::ChooseEntryOptions* options = params->options.get();
  if (options) {
    multiple_ = options->accepts_multiple;
    if (multiple_)
      picker_type = ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE;

    if (options->type == file_system::CHOOSE_ENTRY_TYPE_OPENWRITABLEFILE &&
        !app_file_handler_util::HasFileSystemWritePermission(extension_)) {
      error_ = kRequiresFileSystemWriteError;
      return false;
    } else if (options->type == file_system::CHOOSE_ENTRY_TYPE_SAVEFILE) {
      if (!app_file_handler_util::HasFileSystemWritePermission(extension_)) {
        error_ = kRequiresFileSystemWriteError;
        return false;
      }
      if (multiple_) {
        error_ = kMultipleUnsupportedError;
        return false;
      }
      picker_type = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
    } else if (options->type == file_system::CHOOSE_ENTRY_TYPE_OPENDIRECTORY) {
      is_directory_ = true;
      if (!extension_->HasAPIPermission(APIPermission::kFileSystemDirectory)) {
        error_ = kRequiresFileSystemDirectoryError;
        return false;
      }
      if (multiple_) {
        error_ = kMultipleUnsupportedError;
        return false;
      }
      picker_type = ui::SelectFileDialog::SELECT_FOLDER;
    }

    base::FilePath::StringType suggested_extension;
    BuildSuggestion(options->suggested_name.get(), &suggested_name,
        &suggested_extension);

    BuildFileTypeInfo(&file_type_info, suggested_extension,
        options->accepts.get(), options->accepts_all_types.get());
  }

  file_type_info.support_drive = true;

  base::FilePath previous_path;
  file_system_api::GetLastChooseEntryDirectory(
      ExtensionPrefs::Get(profile()),
      GetExtension()->id(),
      &previous_path);

  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &FileSystemChooseEntryFunction::SetInitialPathOnFileThread, this,
          suggested_name, previous_path),
      base::Bind(
          &FileSystemChooseEntryFunction::ShowPicker, this, file_type_info,
          picker_type));
  return true;
}

bool FileSystemRetainEntryFunction::RunImpl() {
  std::string entry_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &entry_id));
  SavedFilesService* saved_files_service = SavedFilesService::Get(profile());
  // Add the file to the retain list if it is not already on there.
  if (!saved_files_service->IsRegistered(extension_->id(), entry_id)) {
    std::string filesystem_name;
    std::string filesystem_path;
    EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_name));
    EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &filesystem_path));
    if (!ValidateFileEntryAndGetPath(filesystem_name,
                                     filesystem_path,
                                     render_view_host_,
                                     &path_,
                                     &error_)) {
      return false;
    }

    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&FileSystemRetainEntryFunction::SetIsDirectoryOnFileThread,
                   this),
        base::Bind(
            &FileSystemRetainEntryFunction::RetainFileEntry, this, entry_id));
    return true;
  }

  saved_files_service->EnqueueFileEntry(extension_->id(), entry_id);
  SendResponse(true);
  return true;
}

void FileSystemRetainEntryFunction::RetainFileEntry(
    const std::string& entry_id) {
  SavedFilesService* saved_files_service = SavedFilesService::Get(profile());
  saved_files_service->RegisterFileEntry(
      extension_->id(), entry_id, path_, is_directory_);
  saved_files_service->EnqueueFileEntry(extension_->id(), entry_id);
  SendResponse(true);
}

void FileSystemRetainEntryFunction::SetIsDirectoryOnFileThread() {
  if (base::DirectoryExists(path_)) {
    is_directory_ = true;
  }
}

bool FileSystemIsRestorableFunction::RunImpl() {
  std::string entry_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &entry_id));
  SetResult(new base::FundamentalValue(SavedFilesService::Get(
      profile())->IsRegistered(extension_->id(), entry_id)));
  return true;
}

bool FileSystemRestoreEntryFunction::RunImpl() {
  std::string entry_id;
  bool needs_new_entry;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &entry_id));
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(1, &needs_new_entry));
  const SavedFileEntry* file_entry = SavedFilesService::Get(
      profile())->GetFileEntry(extension_->id(), entry_id);
  if (!file_entry) {
    error_ = kUnknownIdError;
    return false;
  }

  SavedFilesService::Get(profile())->EnqueueFileEntry(
      extension_->id(), entry_id);

  // Only create a new file entry if the renderer requests one.
  // |needs_new_entry| will be false if the renderer already has an Entry for
  // |entry_id|.
  if (needs_new_entry) {
    is_directory_ = file_entry->is_directory;
    CreateResponse();
    AddEntryToResponse(file_entry->path, file_entry->id);
  }
  SendResponse(true);
  return true;
}

}  // namespace extensions
