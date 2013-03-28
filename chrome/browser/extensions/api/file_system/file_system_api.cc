// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/file_system_api.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/common/extensions/api/file_system.h"
#include "chrome/common/extensions/permissions/api_permission.h"
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
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#include "base/mac/foundation_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#endif

using fileapi::IsolatedContext;

const char kInvalidParameters[] = "Invalid parameters";
const char kSecurityError[] = "Security error";
const char kInvalidCallingPage[] = "Invalid calling page. This function can't "
    "be called from a background page.";
const char kUserCancelled[] = "User cancelled";
const char kWritableFileError[] = "Invalid file for writing";
const char kRequiresFileSystemWriteError[] =
    "Operation requires fileSystem.write permission";
const char kUnknownChooseEntryType[] = "Unknown type";

const char kOpenFileOption[] = "openFile";
const char kOpenWritableFileOption[] ="openWritableFile";
const char kSaveFileOption[] = "saveFile";

namespace file_system = extensions::api::file_system;
namespace ChooseEntry = file_system::ChooseEntry;

namespace {

#if defined(OS_MACOSX)
// Retrieves the localized display name for the base name of the given path.
// If the path is not localized, this will just return the base name.
std::string GetDisplayBaseName(const base::FilePath& path) {
  base::mac::ScopedCFTypeRef<CFURLRef> url(
      CFURLCreateFromFileSystemRepresentation(
          NULL,
          (const UInt8*)path.value().c_str(),
          path.value().length(),
          true));
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
base::FilePath* g_path_to_be_picked_for_test;

bool GetFilePathOfFileEntry(const std::string& filesystem_name,
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
  if (!context->CrackVirtualPath(virtual_path,
                                 &filesystem_id,
                                 NULL,
                                 file_path)) {
    *error = kInvalidParameters;
    return false;
  }

  return true;
}

bool DoCheckWritableFile(const base::FilePath& path) {
  // Don't allow links.
  if (file_util::PathExists(path) && file_util::IsLink(path))
    return false;

  // Create the file if it doesn't already exist.
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  int creation_flags = base::PLATFORM_FILE_CREATE |
                       base::PLATFORM_FILE_READ |
                       base::PLATFORM_FILE_WRITE;
  base::PlatformFile file = base::CreatePlatformFile(path, creation_flags,
                                                     NULL, &error);
  // Close the file so we don't keep a lock open.
  if (file != base::kInvalidPlatformFileValue)
    base::ClosePlatformFile(file);
  return error == base::PLATFORM_FILE_OK ||
         error == base::PLATFORM_FILE_ERROR_EXISTS;
}

void CheckLocalWritableFile(const base::FilePath& path,
                            const base::Closure& on_success,
                            const base::Closure& on_failure) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      DoCheckWritableFile(path) ? on_success : on_failure);
}

#if defined(OS_CHROMEOS)
void CheckRemoteWritableFile(const base::Closure& on_success,
                             const base::Closure& on_failure,
                             drive::DriveFileError error,
                             const base::FilePath& path) {
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      error == drive::DRIVE_FILE_OK ? on_success : on_failure);
}
#endif

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

}  // namespace

namespace extensions {

bool FileSystemGetDisplayPathFunction::RunImpl() {
  std::string filesystem_name;
  std::string filesystem_path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_path));

  base::FilePath file_path;
  if (!GetFilePathOfFileEntry(filesystem_name, filesystem_path,
                              render_view_host_, &file_path, &error_))
    return false;

  file_path = PrettifyPath(file_path);
  SetResult(base::Value::CreateStringValue(file_path.value()));
  return true;
}

bool FileSystemEntryFunction::HasFileSystemWritePermission() {
  const extensions::Extension* extension = GetExtension();
  if (!extension)
    return false;

  return extension->HasAPIPermission(APIPermission::kFileSystemWrite);
}

void FileSystemEntryFunction::CheckWritableFile(const base::FilePath& path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::Closure on_success =
      base::Bind(&FileSystemEntryFunction::RegisterFileSystemAndSendResponse,
                 this, path, WRITABLE);
  base::Closure on_failure =
      base::Bind(&FileSystemEntryFunction::HandleWritableFileError, this);

#if defined(OS_CHROMEOS)
  if (drive::util::IsUnderDriveMountPoint(path)) {
    drive::util::PrepareWritableFileAndRun(profile_, path,
        base::Bind(&CheckRemoteWritableFile, on_success, on_failure));
    return;
  }
#endif
  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&CheckLocalWritableFile, path, on_success, on_failure));
}

void FileSystemEntryFunction::RegisterFileSystemAndSendResponse(
    const base::FilePath& path, EntryType entry_type) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  fileapi::IsolatedContext* isolated_context =
      fileapi::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  bool writable = entry_type == WRITABLE;
  extensions::app_file_handler_util::GrantedFileEntry file_entry =
      extensions::app_file_handler_util::CreateFileEntry(profile(),
          GetExtension()->id(), render_view_host_->GetProcess()->GetID(), path,
          writable);

  DictionaryValue* dict = new DictionaryValue();
  SetResult(dict);
  dict->SetString("fileSystemId", file_entry.filesystem_id);
  dict->SetString("baseName", file_entry.registered_name);
  dict->SetString("id", file_entry.id);

  SendResponse(true);
}

void FileSystemEntryFunction::HandleWritableFileError() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  error_ = kWritableFileError;
  SendResponse(false);
}

bool FileSystemGetWritableEntryFunction::RunImpl() {
  std::string filesystem_name;
  std::string filesystem_path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_path));

  if (!HasFileSystemWritePermission()) {
    error_ = kRequiresFileSystemWriteError;
    return false;
  }

  base::FilePath path;
  if (!GetFilePathOfFileEntry(filesystem_name, filesystem_path,
                              render_view_host_, &path, &error_))
    return false;

  CheckWritableFile(path);
  return true;
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

  SetResult(base::Value::CreateBooleanValue(is_writable));
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
             ui::SelectFileDialog::Type picker_type,
             EntryType entry_type)
      : suggested_name_(suggested_name),
        entry_type_(entry_type),
        function_(function) {
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, new ChromeSelectFilePolicy(web_contents));
    gfx::NativeWindow owning_window = web_contents ?
        platform_util::GetTopLevel(web_contents->GetView()->GetNativeView()) :
        NULL;

    if (g_skip_picker_for_test) {
      if (g_path_to_be_picked_for_test) {
        content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
            base::Bind(
                &FileSystemChooseEntryFunction::FilePicker::FileSelected,
                base::Unretained(this), *g_path_to_be_picked_for_test, 1,
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
                                    &file_type_info, 0, FILE_PATH_LITERAL(""),
                                    owning_window, NULL);
  }

  virtual ~FilePicker() {}

 private:
  // ui::SelectFileDialog::Listener implementation.
  virtual void FileSelected(const base::FilePath& path,
                            int index,
                            void* params) OVERRIDE {
    function_->FileSelected(path, entry_type_);
    delete this;
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
    function_->FileSelected(file.file_path, entry_type_);
    delete this;
  }

  virtual void FileSelectionCanceled(void* params) OVERRIDE {
    function_->FileSelectionCanceled();
    delete this;
  }

  base::FilePath suggested_name_;

  EntryType entry_type_;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  scoped_refptr<FileSystemChooseEntryFunction> function_;

  DISALLOW_COPY_AND_ASSIGN(FilePicker);
};

bool FileSystemChooseEntryFunction::ShowPicker(
    const base::FilePath& suggested_name,
    const ui::SelectFileDialog::FileTypeInfo& file_type_info,
    ui::SelectFileDialog::Type picker_type,
    EntryType entry_type) {
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
  new FilePicker(this, shell_window->web_contents(), suggested_name,
      file_type_info, picker_type, entry_type);
  return true;
}

// static
void FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
    base::FilePath* path) {
  g_skip_picker_for_test = true;
  g_path_to_be_picked_for_test = path;
}

// static
void FileSystemChooseEntryFunction::SkipPickerAndAlwaysCancelForTest() {
  g_skip_picker_for_test = true;
  g_path_to_be_picked_for_test = NULL;
}

// static
void FileSystemChooseEntryFunction::StopSkippingPickerForTest() {
  g_skip_picker_for_test = false;
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

void FileSystemChooseEntryFunction::FileSelected(const base::FilePath& path,
                                                EntryType entry_type) {
  if (entry_type == WRITABLE) {
    CheckWritableFile(path);
    return;
  }

  // Don't need to check the file, it's for reading.
  RegisterFileSystemAndSendResponse(path, READ_ONLY);
}

void FileSystemChooseEntryFunction::FileSelectionCanceled() {
  error_ = kUserCancelled;
  SendResponse(false);
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
  EntryType entry_type = READ_ONLY;
  ui::SelectFileDialog::Type picker_type =
      ui::SelectFileDialog::SELECT_OPEN_FILE;

  file_system::ChooseEntryOptions* options = params->options.get();
  if (options) {
    if (options->type.get()) {
      if (*options->type == kOpenWritableFileOption) {
        entry_type = WRITABLE;
      } else if (*options->type == kSaveFileOption) {
        entry_type = WRITABLE;
        picker_type = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
      } else if (*options->type != kOpenFileOption) {
        error_ = kUnknownChooseEntryType;
        return false;
      }
    }

    base::FilePath::StringType suggested_extension;
    BuildSuggestion(options->suggested_name.get(), &suggested_name,
        &suggested_extension);

    BuildFileTypeInfo(&file_type_info, suggested_extension,
        options->accepts.get(), options->accepts_all_types.get());
  }

  if (entry_type == WRITABLE && !HasFileSystemWritePermission()) {
    error_ = kRequiresFileSystemWriteError;
    return false;
  }

  file_type_info.support_drive = true;
  return ShowPicker(suggested_name, file_type_info, picker_type, entry_type);
}

}  // namespace extensions
