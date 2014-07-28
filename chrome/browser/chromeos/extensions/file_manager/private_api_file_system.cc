// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_file_system.h"

#include <sys/statvfs.h>

#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/file_browser_private.h"
#include "chrome/common/extensions/api/file_browser_private_internal.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_util.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/fileapi/file_system_info.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

using chromeos::disks::DiskMountManager;
using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using file_manager::util::EntryDefinition;
using file_manager::util::FileDefinition;
using fileapi::FileSystemURL;

namespace extensions {
namespace {

// Retrieves total and remaining available size on |mount_path|.
void GetSizeStatsOnBlockingPool(const std::string& mount_path,
                                uint64* total_size,
                                uint64* remaining_size) {
  struct statvfs stat = {};  // Zero-clear
  if (HANDLE_EINTR(statvfs(mount_path.c_str(), &stat)) == 0) {
    *total_size = static_cast<uint64>(stat.f_blocks) * stat.f_frsize;
    *remaining_size = static_cast<uint64>(stat.f_bavail) * stat.f_frsize;
  }
}

// Retrieves the maximum file name length of the file system of |path|.
// Returns 0 if it could not be queried.
size_t GetFileNameMaxLengthOnBlockingPool(const std::string& path) {
  struct statvfs stat = {};
  if (HANDLE_EINTR(statvfs(path.c_str(), &stat)) != 0) {
    // The filesystem seems not supporting statvfs(). Assume it to be a commonly
    // used bound 255, and log the failure.
    LOG(ERROR) << "Cannot statvfs() the name length limit for: " << path;
    return 255;
  }
  return stat.f_namemax;
}

// Returns EventRouter for the |profile_id| if available.
file_manager::EventRouter* GetEventRouterByProfileId(void* profile_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // |profile_id| needs to be checked with ProfileManager::IsValidProfile
  // before using it.
  Profile* profile = reinterpret_cast<Profile*>(profile_id);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return NULL;

  return file_manager::FileBrowserPrivateAPI::Get(profile)->event_router();
}

// Notifies the copy progress to extensions via event router.
void NotifyCopyProgress(
    void* profile_id,
    fileapi::FileSystemOperationRunner::OperationID operation_id,
    fileapi::FileSystemOperation::CopyProgressType type,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    int64 size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_manager::EventRouter* event_router =
      GetEventRouterByProfileId(profile_id);
  if (event_router) {
    event_router->OnCopyProgress(
        operation_id, type,
        source_url.ToGURL(), destination_url.ToGURL(), size);
  }
}

// Callback invoked periodically on progress update of Copy().
void OnCopyProgress(
    void* profile_id,
    fileapi::FileSystemOperationRunner::OperationID* operation_id,
    fileapi::FileSystemOperation::CopyProgressType type,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    int64 size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NotifyCopyProgress,
                 profile_id, *operation_id, type,
                 source_url, destination_url, size));
}

// Notifies the copy completion to extensions via event router.
void NotifyCopyCompletion(
    void* profile_id,
    fileapi::FileSystemOperationRunner::OperationID operation_id,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    base::File::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_manager::EventRouter* event_router =
      GetEventRouterByProfileId(profile_id);
  if (event_router)
    event_router->OnCopyCompleted(
        operation_id,
        source_url.ToGURL(), destination_url.ToGURL(), error);
}

// Callback invoked upon completion of Copy() (regardless of succeeded or
// failed).
void OnCopyCompleted(
    void* profile_id,
    fileapi::FileSystemOperationRunner::OperationID* operation_id,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url,
    base::File::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NotifyCopyCompletion,
                 profile_id, *operation_id,
                 source_url, destination_url, error));
}

// Starts the copy operation via FileSystemOperationRunner.
fileapi::FileSystemOperationRunner::OperationID StartCopyOnIOThread(
    void* profile_id,
    scoped_refptr<fileapi::FileSystemContext> file_system_context,
    const FileSystemURL& source_url,
    const FileSystemURL& destination_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Note: |operation_id| is owned by the callback for
  // FileSystemOperationRunner::Copy(). It is always called in the next message
  // loop or later, so at least during this invocation it should alive.
  fileapi::FileSystemOperationRunner::OperationID* operation_id =
      new fileapi::FileSystemOperationRunner::OperationID;
  *operation_id = file_system_context->operation_runner()->Copy(
      source_url, destination_url,
      fileapi::FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED,
      base::Bind(&OnCopyProgress,
                 profile_id, base::Unretained(operation_id)),
      base::Bind(&OnCopyCompleted,
                 profile_id, base::Owned(operation_id),
                 source_url, destination_url));
  return *operation_id;
}

void OnCopyCancelled(base::File::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // We just ignore the status if the copy is actually cancelled or not,
  // because failing cancellation means the operation is not running now.
  DLOG_IF(WARNING, error != base::File::FILE_OK)
      << "Failed to cancel copy: " << error;
}

// Cancels the running copy operation identified by |operation_id|.
void CancelCopyOnIOThread(
    scoped_refptr<fileapi::FileSystemContext> file_system_context,
    fileapi::FileSystemOperationRunner::OperationID operation_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  file_system_context->operation_runner()->Cancel(
      operation_id, base::Bind(&OnCopyCancelled));
}

}  // namespace

void FileBrowserPrivateRequestFileSystemFunction::DidFail(
    base::File::Error error_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetError(base::StringPrintf("File error %d", static_cast<int>(error_code)));
  SendResponse(false);
}

bool FileBrowserPrivateRequestFileSystemFunction::
    SetupFileSystemAccessPermissions(
        scoped_refptr<fileapi::FileSystemContext> file_system_context,
        int child_id,
        Profile* profile,
        scoped_refptr<const extensions::Extension> extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!extension.get())
    return false;

  // Make sure that only component extension can access the entire
  // local file system.
  if (extension_->location() != extensions::Manifest::COMPONENT) {
    NOTREACHED() << "Private method access by non-component extension "
                 << extension->id();
    return false;
  }

  fileapi::ExternalFileSystemBackend* backend =
      file_system_context->external_backend();
  if (!backend)
    return false;

  // Grant full access to File API from this component extension.
  backend->GrantFullAccessToExtension(extension_->id());

  // Grant R/W file permissions to the renderer hosting component
  // extension for all paths exposed by our local file system backend.
  std::vector<base::FilePath> root_dirs = backend->GetRootDirectories();
  for (size_t i = 0; i < root_dirs.size(); ++i) {
    ChildProcessSecurityPolicy::GetInstance()->GrantCreateReadWriteFile(
        child_id, root_dirs[i]);
  }

  // Grant R/W permissions to profile-specific directories (Drive, Downloads)
  // from other profiles. Those directories may not be mounted at this moment
  // yet, so we need to do this separately from the above loop over
  // GetRootDirectories().
  const std::vector<Profile*>& profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (size_t i = 0; i < profiles.size(); ++i) {
    if (!profiles[i]->IsOffTheRecord()) {
      file_manager::util::SetupProfileFileAccessPermissions(child_id,
                                                            profiles[i]);
    }
  }

  return true;
}

bool FileBrowserPrivateRequestFileSystemFunction::RunAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  using extensions::api::file_browser_private::RequestFileSystem::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!dispatcher() || !render_view_host() || !render_view_host()->GetProcess())
    return false;

  set_log_on_completion(true);

  using file_manager::VolumeManager;
  using file_manager::VolumeInfo;
  VolumeManager* volume_manager = VolumeManager::Get(GetProfile());
  if (!volume_manager)
    return false;

  VolumeInfo volume_info;
  if (!volume_manager->FindVolumeInfoById(params->volume_id, &volume_info)) {
    DidFail(base::File::FILE_ERROR_NOT_FOUND);
    return false;
  }

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          GetProfile(), render_view_host());

  // Set up file permission access.
  const int child_id = render_view_host()->GetProcess()->GetID();
  if (!SetupFileSystemAccessPermissions(
          file_system_context, child_id, GetProfile(), extension())) {
    DidFail(base::File::FILE_ERROR_SECURITY);
    return false;
  }

  FileDefinition file_definition;
  if (!file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
           GetProfile(),
           extension_id(),
           volume_info.mount_path,
           &file_definition.virtual_path)) {
    DidFail(base::File::FILE_ERROR_INVALID_OPERATION);
    return false;
  }
  file_definition.is_directory = true;

  file_manager::util::ConvertFileDefinitionToEntryDefinition(
      GetProfile(),
      extension_id(),
      file_definition,
      base::Bind(
          &FileBrowserPrivateRequestFileSystemFunction::OnEntryDefinition,
          this));
  return true;
}

void FileBrowserPrivateRequestFileSystemFunction::OnEntryDefinition(
    const EntryDefinition& entry_definition) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (entry_definition.error != base::File::FILE_OK) {
    DidFail(entry_definition.error);
    return;
  }

  if (!entry_definition.is_directory) {
    DidFail(base::File::FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  base::DictionaryValue* dict = new base::DictionaryValue();
  SetResult(dict);
  dict->SetString("name", entry_definition.file_system_name);
  dict->SetString("root_url", entry_definition.file_system_root_url);
  dict->SetInteger("error", drive::FILE_ERROR_OK);
  SendResponse(true);
}

void FileWatchFunctionBase::Respond(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetResult(new base::FundamentalValue(success));
  SendResponse(success);
}

bool FileWatchFunctionBase::RunAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!render_view_host() || !render_view_host()->GetProcess())
    return false;

  // First param is url of a file to watch.
  std::string url;
  if (!args_->GetString(0, &url) || url.empty())
    return false;

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          GetProfile(), render_view_host());

  FileSystemURL file_watch_url = file_system_context->CrackURL(GURL(url));
  base::FilePath local_path = file_watch_url.path();
  base::FilePath virtual_path = file_watch_url.virtual_path();
  if (local_path.empty()) {
    Respond(false);
    return true;
  }
  PerformFileWatchOperation(local_path, virtual_path, extension_id());

  return true;
}

void FileBrowserPrivateAddFileWatchFunction::PerformFileWatchOperation(
    const base::FilePath& local_path,
    const base::FilePath& virtual_path,
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_manager::EventRouter* event_router =
      file_manager::FileBrowserPrivateAPI::Get(GetProfile())->event_router();
  event_router->AddFileWatch(
      local_path,
      virtual_path,
      extension_id,
      base::Bind(&FileBrowserPrivateAddFileWatchFunction::Respond, this));
}

void FileBrowserPrivateRemoveFileWatchFunction::PerformFileWatchOperation(
    const base::FilePath& local_path,
    const base::FilePath& unused,
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_manager::EventRouter* event_router =
      file_manager::FileBrowserPrivateAPI::Get(GetProfile())->event_router();
  event_router->RemoveFileWatch(local_path, extension_id);
  Respond(true);
}

bool FileBrowserPrivateGetSizeStatsFunction::RunAsync() {
  using extensions::api::file_browser_private::GetSizeStats::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  using file_manager::VolumeManager;
  using file_manager::VolumeInfo;
  VolumeManager* volume_manager = VolumeManager::Get(GetProfile());
  if (!volume_manager)
    return false;

  VolumeInfo volume_info;
  if (!volume_manager->FindVolumeInfoById(params->volume_id, &volume_info))
    return false;

  if (volume_info.type == file_manager::VOLUME_TYPE_GOOGLE_DRIVE) {
    drive::FileSystemInterface* file_system =
        drive::util::GetFileSystemByProfile(GetProfile());
    if (!file_system) {
      // |file_system| is NULL if Drive is disabled.
      // If stats couldn't be gotten for drive, result should be left
      // undefined. See comments in GetDriveAvailableSpaceCallback().
      SendResponse(true);
      return true;
    }

    file_system->GetAvailableSpace(
        base::Bind(&FileBrowserPrivateGetSizeStatsFunction::
                       GetDriveAvailableSpaceCallback,
                   this));
  } else {
    uint64* total_size = new uint64(0);
    uint64* remaining_size = new uint64(0);
    BrowserThread::PostBlockingPoolTaskAndReply(
        FROM_HERE,
        base::Bind(&GetSizeStatsOnBlockingPool,
                   volume_info.mount_path.value(),
                   total_size,
                   remaining_size),
        base::Bind(&FileBrowserPrivateGetSizeStatsFunction::
                       GetSizeStatsCallback,
                   this,
                   base::Owned(total_size),
                   base::Owned(remaining_size)));
  }
  return true;
}

void FileBrowserPrivateGetSizeStatsFunction::GetDriveAvailableSpaceCallback(
    drive::FileError error,
    int64 bytes_total,
    int64 bytes_used) {
  if (error == drive::FILE_ERROR_OK) {
    const uint64 bytes_total_unsigned = bytes_total;
    const uint64 bytes_remaining_unsigned = bytes_total - bytes_used;
    GetSizeStatsCallback(&bytes_total_unsigned,
                         &bytes_remaining_unsigned);
  } else {
    // If stats couldn't be gotten for drive, result should be left undefined.
    SendResponse(true);
  }
}

void FileBrowserPrivateGetSizeStatsFunction::GetSizeStatsCallback(
    const uint64* total_size,
    const uint64* remaining_size) {
  base::DictionaryValue* sizes = new base::DictionaryValue();
  SetResult(sizes);

  sizes->SetDouble("totalSize", static_cast<double>(*total_size));
  sizes->SetDouble("remainingSize", static_cast<double>(*remaining_size));

  SendResponse(true);
}

bool FileBrowserPrivateValidatePathNameLengthFunction::RunAsync() {
  using extensions::api::file_browser_private::ValidatePathNameLength::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          GetProfile(), render_view_host());

  fileapi::FileSystemURL filesystem_url(
      file_system_context->CrackURL(GURL(params->parent_directory_url)));
  if (!chromeos::FileSystemBackend::CanHandleURL(filesystem_url))
    return false;

  // No explicit limit on the length of Drive file names.
  if (filesystem_url.type() == fileapi::kFileSystemTypeDrive) {
    SetResult(new base::FundamentalValue(true));
    SendResponse(true);
    return true;
  }

  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&GetFileNameMaxLengthOnBlockingPool,
                 filesystem_url.path().AsUTF8Unsafe()),
      base::Bind(&FileBrowserPrivateValidatePathNameLengthFunction::
                     OnFilePathLimitRetrieved,
                 this, params->name.size()));
  return true;
}

void FileBrowserPrivateValidatePathNameLengthFunction::OnFilePathLimitRetrieved(
    size_t current_length,
    size_t max_length) {
  SetResult(new base::FundamentalValue(current_length <= max_length));
  SendResponse(true);
}

bool FileBrowserPrivateFormatVolumeFunction::RunAsync() {
  using extensions::api::file_browser_private::FormatVolume::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  using file_manager::VolumeManager;
  using file_manager::VolumeInfo;
  VolumeManager* volume_manager = VolumeManager::Get(GetProfile());
  if (!volume_manager)
    return false;

  VolumeInfo volume_info;
  if (!volume_manager->FindVolumeInfoById(params->volume_id, &volume_info))
    return false;

  DiskMountManager::GetInstance()->FormatMountedDevice(
      volume_info.mount_path.AsUTF8Unsafe());
  SendResponse(true);
  return true;
}

bool FileBrowserPrivateStartCopyFunction::RunAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  using  extensions::api::file_browser_private::StartCopy::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->source_url.empty() || params->parent.empty() ||
      params->new_name.empty()) {
    // Error code in format of DOMError.name.
    SetError("EncodingError");
    return false;
  }

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          GetProfile(), render_view_host());

  // |parent| may have a trailing slash if it is a root directory.
  std::string destination_url_string = params->parent;
  if (destination_url_string[destination_url_string.size() - 1] != '/')
    destination_url_string += '/';
  destination_url_string += net::EscapePath(params->new_name);

  fileapi::FileSystemURL source_url(
      file_system_context->CrackURL(GURL(params->source_url)));
  fileapi::FileSystemURL destination_url(
      file_system_context->CrackURL(GURL(destination_url_string)));

  if (!source_url.is_valid() || !destination_url.is_valid()) {
    // Error code in format of DOMError.name.
    SetError("EncodingError");
    return false;
  }

  return BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&StartCopyOnIOThread,
                 GetProfile(),
                 file_system_context,
                 source_url,
                 destination_url),
      base::Bind(&FileBrowserPrivateStartCopyFunction::RunAfterStartCopy,
                 this));
}

void FileBrowserPrivateStartCopyFunction::RunAfterStartCopy(
    int operation_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetResult(new base::FundamentalValue(operation_id));
  SendResponse(true);
}

bool FileBrowserPrivateCancelCopyFunction::RunAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  using extensions::api::file_browser_private::CancelCopy::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          GetProfile(), render_view_host());

  // We don't much take care about the result of cancellation.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CancelCopyOnIOThread, file_system_context, params->copy_id));
  SendResponse(true);
  return true;
}

bool FileBrowserPrivateInternalResolveIsolatedEntriesFunction::RunAsync() {
  using extensions::api::file_browser_private_internal::ResolveIsolatedEntries::
      Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          GetProfile(), render_view_host());
  DCHECK(file_system_context);

  const fileapi::ExternalFileSystemBackend* external_backend =
      file_system_context->external_backend();
  DCHECK(external_backend);

  file_manager::util::FileDefinitionList file_definition_list;
  for (size_t i = 0; i < params->urls.size(); ++i) {
    FileSystemURL fileSystemUrl =
        file_system_context->CrackURL(GURL(params->urls[i]));
    DCHECK(external_backend->CanHandleType(fileSystemUrl.type()));

    FileDefinition file_definition;
    const bool result =
        file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
            GetProfile(),
            extension_->id(),
            fileSystemUrl.path(),
            &file_definition.virtual_path);
    if (!result)
      continue;
    // The API only supports isolated files.
    file_definition.is_directory = false;
    file_definition_list.push_back(file_definition);
  }

  file_manager::util::ConvertFileDefinitionListToEntryDefinitionList(
      GetProfile(),
      extension_->id(),
      file_definition_list,  // Safe, since copied internally.
      base::Bind(
          &FileBrowserPrivateInternalResolveIsolatedEntriesFunction::
              RunAsyncAfterConvertFileDefinitionListToEntryDefinitionList,
          this));
  return true;
}

void FileBrowserPrivateInternalResolveIsolatedEntriesFunction::
    RunAsyncAfterConvertFileDefinitionListToEntryDefinitionList(scoped_ptr<
        file_manager::util::EntryDefinitionList> entry_definition_list) {
  using extensions::api::file_browser_private_internal::EntryDescription;
  std::vector<linked_ptr<EntryDescription> > entries;

  for (size_t i = 0; i < entry_definition_list->size(); ++i) {
    if (entry_definition_list->at(i).error != base::File::FILE_OK)
      continue;
    linked_ptr<EntryDescription> entry(new EntryDescription);
    entry->file_system_name = entry_definition_list->at(i).file_system_name;
    entry->file_system_root = entry_definition_list->at(i).file_system_root_url;
    entry->file_full_path =
        "/" + entry_definition_list->at(i).full_path.AsUTF8Unsafe();
    entry->file_is_directory = entry_definition_list->at(i).is_directory;
    entries.push_back(entry);
  }

  results_ = extensions::api::file_browser_private_internal::
      ResolveIsolatedEntries::Results::Create(entries);
  SendResponse(true);
}
}  // namespace extensions
