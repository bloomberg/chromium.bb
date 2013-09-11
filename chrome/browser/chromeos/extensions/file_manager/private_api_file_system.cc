// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_file_system.h"

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <utime.h>

#include "base/path_service.h"
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
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/file_browser_private.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_util.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

using chromeos::disks::DiskMountManager;
using content::BrowserContext;
using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::WebContents;
using fileapi::FileSystemURL;

namespace extensions {
namespace {

// Error messages.
const char kFileError[] = "File error %d";

const DiskMountManager::Disk* GetVolumeAsDisk(const std::string& mount_path) {
  DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();

  DiskMountManager::MountPointMap::const_iterator mount_point_it =
      disk_mount_manager->mount_points().find(mount_path);
  if (mount_point_it == disk_mount_manager->mount_points().end())
    return NULL;

  const DiskMountManager::Disk* disk = disk_mount_manager->FindDiskBySourcePath(
      mount_point_it->second.source_path);

  return (disk && disk->is_hidden()) ? NULL : disk;
}

base::DictionaryValue* CreateValueFromDisk(
    Profile* profile,
    const std::string& extension_id,
    const DiskMountManager::Disk* volume) {
  base::DictionaryValue* volume_info = new base::DictionaryValue();

  std::string mount_path;
  if (!volume->mount_path().empty()) {
    base::FilePath relative_mount_path;
    file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
        profile, extension_id, base::FilePath(volume->mount_path()),
        &relative_mount_path);
    mount_path = relative_mount_path.value();
  }

  volume_info->SetString("devicePath", volume->device_path());
  volume_info->SetString("mountPath", mount_path);
  volume_info->SetString("systemPath", volume->system_path());
  volume_info->SetString("filePath", volume->file_path());
  volume_info->SetString("deviceLabel", volume->device_label());
  volume_info->SetString("driveLabel", volume->drive_label());
  volume_info->SetString(
      "deviceType",
      DiskMountManager::DeviceTypeToString(volume->device_type()));
  volume_info->SetDouble("totalSize",
                         static_cast<double>(volume->total_size_in_bytes()));
  volume_info->SetBoolean("isParent", volume->is_parent());
  volume_info->SetBoolean("isReadOnly", volume->is_read_only());
  volume_info->SetBoolean("hasMedia", volume->has_media());
  volume_info->SetBoolean("isOnBootDevice", volume->on_boot_device());

  return volume_info;
}

base::DictionaryValue* CreateDownloadsVolumeMetadata() {
  base::DictionaryValue* volume_info = new base::DictionaryValue;
  volume_info->SetString("mountPath", "Downloads");
  volume_info->SetBoolean("isReadOnly", false);
  return volume_info;
}

// Sets permissions for the Drive mount point so Files.app can access files
// in the mount point directory. It's safe to call this function even if
// Drive is disabled by the setting (i.e. prefs::kDisableDrive is true).
void SetDriveMountPointPermissions(
    Profile* profile,
    const std::string& extension_id,
    content::RenderViewHost* render_view_host) {
  if (!render_view_host ||
      !render_view_host->GetSiteInstance() || !render_view_host->GetProcess()) {
    return;
  }

  fileapi::ExternalFileSystemBackend* backend =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          profile, render_view_host)->external_backend();
  if (!backend)
    return;

  const base::FilePath mount_point = drive::util::GetDriveMountPointPath();
  // Grant R/W permissions to drive 'folder'. File API layer still
  // expects this to be satisfied.
  ChildProcessSecurityPolicy::GetInstance()->GrantCreateReadWriteFile(
      render_view_host->GetProcess()->GetID(), mount_point);

  base::FilePath mount_point_virtual;
  if (backend->GetVirtualPath(mount_point, &mount_point_virtual))
    backend->GrantFileAccessToExtension(extension_id, mount_point_virtual);
}

// Retrieves total and remaining available size on |mount_path|.
void GetSizeStatsOnBlockingPool(const std::string& mount_path,
                                uint64* total_size,
                                uint64* remaining_size) {
  struct statvfs stat = {};  // Zero-clear
  if (HANDLE_EINTR(statvfs(mount_path.c_str(), &stat)) == 0) {
    *total_size =
        static_cast<uint64>(stat.f_blocks) * stat.f_frsize;
    *remaining_size =
        static_cast<uint64>(stat.f_bavail) * stat.f_frsize;
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

// Sets last modified date.
bool SetLastModifiedOnBlockingPool(const base::FilePath& local_path,
                                   time_t timestamp) {
  if (local_path.empty())
    return false;

  struct stat stat_buffer;
  if (stat(local_path.value().c_str(), &stat_buffer) != 0)
    return false;

  struct utimbuf times;
  times.actime = stat_buffer.st_atime;
  times.modtime = timestamp;
  return utime(local_path.value().c_str(), &times) == 0;
}

// Notifies the copy completion to extensions via event router.
void NotifyCopyCompletion(
    void* profile_id,
    fileapi::FileSystemOperationRunner::OperationID operation_id,
    const FileSystemURL& dest_url,
    base::PlatformFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // |profile_id| needs to be checked with ProfileManager::IsValidProfile
  // before using it.
  Profile* profile = reinterpret_cast<Profile*>(profile_id);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;

  file_manager::EventRouter* event_router =
      file_manager::FileBrowserPrivateAPI::Get(profile)->event_router();
  event_router->OnCopyCompleted(operation_id, dest_url.ToGURL(), error);
}

// Callback invoked upon completion of Copy() (regardless of succeeded or
// failed).
void OnCopyCompleted(
    void* profile_id,
    fileapi::FileSystemOperationRunner::OperationID* operation_id,
    const FileSystemURL& dest_url,
    base::PlatformFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&NotifyCopyCompletion,
                 profile_id, *operation_id, dest_url, error));
}

// Starts the copy operation via FileSystemOperationRunner.
fileapi::FileSystemOperationRunner::OperationID StartCopyOnIOThread(
    void* profile_id,
    scoped_refptr<fileapi::FileSystemContext> file_system_context,
    const FileSystemURL& source_url,
    const FileSystemURL& dest_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Note: |operation_id| is owned by the callback for
  // FileSystemOperationRunner::Copy(). It is always called in the next message
  // loop or later, so at least during this invocation it should alive.
  fileapi::FileSystemOperationRunner::OperationID* operation_id =
      new fileapi::FileSystemOperationRunner::OperationID;
  *operation_id = file_system_context->operation_runner()->Copy(
      source_url, dest_url,
      fileapi::FileSystemOperationRunner::CopyProgressCallback(),
      base::Bind(&OnCopyCompleted,
                 profile_id, base::Owned(operation_id), dest_url));
  return *operation_id;
}

void OnCopyCancelled(base::PlatformFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // We just ignore the status if the copy is actually cancelled or not,
  // because failing cancellation means the operation is not running now.
  DLOG_IF(WARNING, error != base::PLATFORM_FILE_OK)
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

FileBrowserPrivateRequestFileSystemFunction::
    FileBrowserPrivateRequestFileSystemFunction() {
}

FileBrowserPrivateRequestFileSystemFunction::
    ~FileBrowserPrivateRequestFileSystemFunction() {
}

void FileBrowserPrivateRequestFileSystemFunction::DidOpenFileSystem(
    scoped_refptr<fileapi::FileSystemContext> file_system_context,
    base::PlatformFileError result,
    const std::string& name,
    const GURL& root_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (result != base::PLATFORM_FILE_OK) {
    DidFail(result);
    return;
  }

  // RenderViewHost may have gone while the task is posted asynchronously.
  if (!render_view_host()) {
    DidFail(base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }

  // Set up file permission access.
  const int child_id = render_view_host()->GetProcess()->GetID();
  if (!SetupFileSystemAccessPermissions(file_system_context,
                                        child_id,
                                        GetExtension())) {
    DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
    return;
  }

  // Set permissions for the Drive mount point immediately when we kick of
  // first instance of file manager. The actual mount event will be sent to
  // UI only when we perform proper authentication.
  //
  // Note that we call this function even when Drive is disabled by the
  // setting. Otherwise, we need to call this when the setting is changed at
  // a later time, which complicates the code.
  SetDriveMountPointPermissions(profile_, extension_id(), render_view_host());

  DictionaryValue* dict = new DictionaryValue();
  SetResult(dict);
  dict->SetString("name", name);
  dict->SetString("root_url", root_url.spec());
  dict->SetInteger("error", drive::FILE_ERROR_OK);
  SendResponse(true);
}

void FileBrowserPrivateRequestFileSystemFunction::DidFail(
    base::PlatformFileError error_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  error_ = base::StringPrintf(kFileError, static_cast<int>(error_code));
  SendResponse(false);
}

bool FileBrowserPrivateRequestFileSystemFunction::
    SetupFileSystemAccessPermissions(
        scoped_refptr<fileapi::FileSystemContext> file_system_context,
        int child_id,
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
  return true;
}

bool FileBrowserPrivateRequestFileSystemFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  using extensions::api::file_browser_private::RequestFileSystem::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // TODO(satorux): Handle the file system ID. crbug.com/284963.
  DCHECK_EQ("compatible", params->file_system_id);

  if (!dispatcher() || !render_view_host() || !render_view_host()->GetProcess())
    return false;

  set_log_on_completion(true);

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          profile(), render_view_host());

  const GURL origin_url = source_url_.GetOrigin();
  file_system_context->OpenFileSystem(
      origin_url,
      fileapi::kFileSystemTypeExternal,
      fileapi::OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT,
      base::Bind(&FileBrowserPrivateRequestFileSystemFunction::
                     DidOpenFileSystem,
                 this,
                 file_system_context));
  return true;
}

FileWatchFunctionBase::FileWatchFunctionBase() {
}

FileWatchFunctionBase::~FileWatchFunctionBase() {
}

void FileWatchFunctionBase::Respond(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetResult(Value::CreateBooleanValue(success));
  SendResponse(success);
}

bool FileWatchFunctionBase::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!render_view_host() || !render_view_host()->GetProcess())
    return false;

  // First param is url of a file to watch.
  std::string url;
  if (!args_->GetString(0, &url) || url.empty())
    return false;

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          profile(), render_view_host());

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

FileBrowserPrivateAddFileWatchFunction::
    FileBrowserPrivateAddFileWatchFunction() {
}

FileBrowserPrivateAddFileWatchFunction::
    ~FileBrowserPrivateAddFileWatchFunction() {
}

void FileBrowserPrivateAddFileWatchFunction::PerformFileWatchOperation(
    const base::FilePath& local_path,
    const base::FilePath& virtual_path,
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_manager::EventRouter* event_router =
      file_manager::FileBrowserPrivateAPI::Get(profile_)->event_router();
  event_router->AddFileWatch(
      local_path,
      virtual_path,
      extension_id,
      base::Bind(&FileBrowserPrivateAddFileWatchFunction::Respond, this));
}

FileBrowserPrivateRemoveFileWatchFunction::
    FileBrowserPrivateRemoveFileWatchFunction() {
}

FileBrowserPrivateRemoveFileWatchFunction::
    ~FileBrowserPrivateRemoveFileWatchFunction() {
}

void FileBrowserPrivateRemoveFileWatchFunction::PerformFileWatchOperation(
    const base::FilePath& local_path,
    const base::FilePath& unused,
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_manager::EventRouter* event_router =
      file_manager::FileBrowserPrivateAPI::Get(profile_)->event_router();
  event_router->RemoveFileWatch(local_path, extension_id);
  Respond(true);
}

FileBrowserPrivateSetLastModifiedFunction::
    FileBrowserPrivateSetLastModifiedFunction() {
}

FileBrowserPrivateSetLastModifiedFunction::
    ~FileBrowserPrivateSetLastModifiedFunction() {
}

bool FileBrowserPrivateSetLastModifiedFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (args_->GetSize() != 2) {
    return false;
  }

  std::string file_url;
  if (!args_->GetString(0, &file_url))
    return false;

  std::string timestamp;
  if (!args_->GetString(1, &timestamp))
    return false;

  base::FilePath local_path = file_manager::util::GetLocalPathFromURL(
      render_view_host(), profile(), GURL(file_url));

  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&SetLastModifiedOnBlockingPool,
                 local_path,
                 strtoul(timestamp.c_str(), NULL, 0)),
      base::Bind(&FileBrowserPrivateSetLastModifiedFunction::SendResponse,
                 this));
  return true;
}

FileBrowserPrivateGetSizeStatsFunction::
    FileBrowserPrivateGetSizeStatsFunction() {
}

FileBrowserPrivateGetSizeStatsFunction::
    ~FileBrowserPrivateGetSizeStatsFunction() {
}

bool FileBrowserPrivateGetSizeStatsFunction::RunImpl() {
  if (args_->GetSize() != 1) {
    return false;
  }

  std::string mount_url;
  if (!args_->GetString(0, &mount_url))
    return false;

  base::FilePath file_path = file_manager::util::GetLocalPathFromURL(
      render_view_host(), profile(), GURL(mount_url));
  if (file_path.empty())
    return false;

  if (file_path == drive::util::GetDriveMountPointPath()) {
    drive::FileSystemInterface* file_system =
        drive::util::GetFileSystemByProfile(profile());
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
                   file_path.value(),
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

FileBrowserPrivateGetVolumeMetadataFunction::
    FileBrowserPrivateGetVolumeMetadataFunction() {
}

FileBrowserPrivateGetVolumeMetadataFunction::
    ~FileBrowserPrivateGetVolumeMetadataFunction() {
}

bool FileBrowserPrivateGetVolumeMetadataFunction::RunImpl() {
  if (args_->GetSize() != 1) {
    error_ = "Invalid argument count";
    return false;
  }

  std::string volume_mount_url;
  if (!args_->GetString(0, &volume_mount_url)) {
    NOTREACHED();
    return false;
  }

  base::FilePath file_path = file_manager::util::GetLocalPathFromURL(
      render_view_host(), profile(), GURL(volume_mount_url));
  if (file_path.empty()) {
    error_ = "Invalid mount path.";
    return false;
  }

  results_.reset();

  base::FilePath home_path;
  // TODO(hidehiko): Return the volume info for Drive File System.
  if (PathService::Get(base::DIR_HOME, &home_path) &&
      file_path == home_path.AppendASCII("Downloads")) {
    // Return simple (fake) volume metadata for Downloads volume.
    SetResult(CreateDownloadsVolumeMetadata());
  } else {
    const DiskMountManager::Disk* volume = GetVolumeAsDisk(file_path.value());
    if (volume)
      SetResult(CreateValueFromDisk(profile_, extension_->id(), volume));
  }

  SendResponse(true);
  return true;
}

FileBrowserPrivateValidatePathNameLengthFunction::
    FileBrowserPrivateValidatePathNameLengthFunction() {
}

FileBrowserPrivateValidatePathNameLengthFunction::
    ~FileBrowserPrivateValidatePathNameLengthFunction() {
}

bool FileBrowserPrivateValidatePathNameLengthFunction::RunImpl() {
  std::string parent_url;
  if (!args_->GetString(0, &parent_url))
    return false;

  std::string name;
  if (!args_->GetString(1, &name))
    return false;

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          profile(), render_view_host());

  fileapi::FileSystemURL filesystem_url(
      file_system_context->CrackURL(GURL(parent_url)));
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
                 this, name.size()));
  return true;
}

void FileBrowserPrivateValidatePathNameLengthFunction::OnFilePathLimitRetrieved(
    size_t current_length,
    size_t max_length) {
  SetResult(new base::FundamentalValue(current_length <= max_length));
  SendResponse(true);
}

FileBrowserPrivateFormatDeviceFunction::
    FileBrowserPrivateFormatDeviceFunction() {
}

FileBrowserPrivateFormatDeviceFunction::
    ~FileBrowserPrivateFormatDeviceFunction() {
}

bool FileBrowserPrivateFormatDeviceFunction::RunImpl() {
  if (args_->GetSize() != 1) {
    return false;
  }

  std::string volume_file_url;
  if (!args_->GetString(0, &volume_file_url)) {
    NOTREACHED();
    return false;
  }

  base::FilePath file_path = file_manager::util::GetLocalPathFromURL(
      render_view_host(), profile(), GURL(volume_file_url));
  if (file_path.empty())
    return false;

  DiskMountManager::GetInstance()->FormatMountedDevice(file_path.value());
  SendResponse(true);
  return true;
}

FileBrowserPrivateStartCopyFunction::FileBrowserPrivateStartCopyFunction() {
}

FileBrowserPrivateStartCopyFunction::~FileBrowserPrivateStartCopyFunction() {
}

bool FileBrowserPrivateStartCopyFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  using  extensions::api::file_browser_private::StartCopy::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->source_url.empty() || params->parent.empty() ||
      params->new_name.empty()) {
    error_ = base::IntToString(fileapi::PlatformFileErrorToWebFileError(
        base::PLATFORM_FILE_ERROR_INVALID_URL));
    return false;
  }

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          profile(), render_view_host());

  fileapi::FileSystemURL source_url(
      file_system_context->CrackURL(GURL(params->source_url)));
  fileapi::FileSystemURL dest_url(file_system_context->CrackURL(
      GURL(params->parent + "/" + params->new_name)));

  if (!source_url.is_valid() || !dest_url.is_valid()) {
    error_ = base::IntToString(fileapi::PlatformFileErrorToWebFileError(
        base::PLATFORM_FILE_ERROR_INVALID_URL));
    return false;
  }

  return BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&StartCopyOnIOThread,
                 profile(), file_system_context, source_url, dest_url),
      base::Bind(&FileBrowserPrivateStartCopyFunction::RunAfterStartCopy,
                 this));
}

void FileBrowserPrivateStartCopyFunction::RunAfterStartCopy(
    int operation_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetResult(Value::CreateIntegerValue(operation_id));
  SendResponse(true);
}

FileBrowserPrivateCancelCopyFunction::FileBrowserPrivateCancelCopyFunction() {
}

FileBrowserPrivateCancelCopyFunction::~FileBrowserPrivateCancelCopyFunction() {
}

bool FileBrowserPrivateCancelCopyFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  using  extensions::api::file_browser_private::CancelCopy::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      file_manager::util::GetFileSystemContextForRenderViewHost(
          profile(), render_view_host());

  // We don't much take care about the result of cancellation.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CancelCopyOnIOThread, file_system_context, params->copy_id));
  SendResponse(true);
  return true;
}

}  // namespace extensions
