// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api.h"

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <utime.h>

#include <map>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/scoped_vector.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/logging.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api_factory.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_handler_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_manager_event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_manager_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_drive.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_tasks.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/zip_file_creator.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/page_zoom.h"
#include "grit/app_locale_settings.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/webui/web_ui_util.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_util.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

using extensions::app_file_handler_util::FindFileHandlersForFiles;
using extensions::app_file_handler_util::PathAndMimeTypeSet;
using chromeos::disks::DiskMountManager;
using content::BrowserContext;
using content::BrowserThread;
using content::ChildProcessSecurityPolicy;
using content::WebContents;
using extensions::Extension;
using extensions::ZipFileCreator;
using fileapi::FileSystemURL;

namespace file_manager {
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
    file_manager_util::ConvertFileToRelativeFileSystemPath(
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
  volume_info->SetString("deviceType",
      DiskMountManager::DeviceTypeToString(volume->device_type()));
  volume_info->SetInteger("totalSize", volume->total_size_in_bytes());
  volume_info->SetBoolean("isParent", volume->is_parent());
  volume_info->SetBoolean("isReadOnly", volume->is_read_only());
  volume_info->SetBoolean("hasMedia", volume->has_media());
  volume_info->SetBoolean("isOnBootDevice", volume->on_boot_device());

  return volume_info;
}

base::DictionaryValue* CreateValueFromMountPoint(Profile* profile,
    const DiskMountManager::MountPointInfo& mount_point_info,
    const std::string& extension_id,
    const GURL& extension_source_url) {

  base::DictionaryValue *mount_info = new base::DictionaryValue();

  mount_info->SetString("mountType",
                        DiskMountManager::MountTypeToString(
                            mount_point_info.mount_type));
  mount_info->SetString("sourcePath", mount_point_info.source_path);

  base::FilePath relative_mount_path;
  // Convert mount point path to relative path with the external file system
  // exposed within File API.
  if (file_manager_util::ConvertFileToRelativeFileSystemPath(
          profile,
          extension_id,
          base::FilePath(mount_point_info.mount_path),
          &relative_mount_path)) {
    mount_info->SetString("mountPath", relative_mount_path.value());
  }

  mount_info->SetString("mountCondition",
      DiskMountManager::MountConditionToString(
          mount_point_info.mount_condition));

  return mount_info;
}

void SetDriveMountPointPermissions(
    Profile* profile,
    const std::string& extension_id,
    content::RenderViewHost* render_view_host) {
  if (!render_view_host ||
      !render_view_host->GetSiteInstance() || !render_view_host->GetProcess()) {
    return;
  }

  content::SiteInstance* site_instance = render_view_host->GetSiteInstance();
  fileapi::ExternalFileSystemBackend* backend =
      BrowserContext::GetStoragePartition(profile, site_instance)->
      GetFileSystemContext()->external_backend();
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
                                size_t* total_size_kb,
                                size_t* remaining_size_kb) {
  uint64_t total_size_in_bytes = 0;
  uint64_t remaining_size_in_bytes = 0;

  struct statvfs stat = {};  // Zero-clear
  if (HANDLE_EINTR(statvfs(mount_path.c_str(), &stat)) == 0) {
    total_size_in_bytes =
        static_cast<uint64_t>(stat.f_blocks) * stat.f_frsize;
    remaining_size_in_bytes =
        static_cast<uint64_t>(stat.f_bfree) * stat.f_frsize;
  }
  *total_size_kb = static_cast<size_t>(total_size_in_bytes / 1024);
  *remaining_size_kb = static_cast<size_t>(remaining_size_in_bytes / 1024);
}

// Retrieves the maximum file name length of the file system of |path|.
// Returns 0 if it could not be queried.
size_t GetFileNameMaxLengthOnBlockingPool(const std::string& path) {
  struct statvfs stat = {};
  if (statvfs(path.c_str(), &stat) != 0) {
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

// The typedefs are used for GetSelectedFileInfo().
typedef std::vector<GURL> UrlList;
typedef std::vector<ui::SelectedFileInfo> SelectedFileInfoList;
typedef base::Callback<void(const SelectedFileInfoList&)>
    GetSelectedFileInfoCallback;

// The struct is used for GetSelectedFileInfo().
struct GetSelectedFileInfoParams {
  bool for_opening;
  GetSelectedFileInfoCallback callback;
  std::vector<base::FilePath> file_paths;
  SelectedFileInfoList selected_files;
};

// Forward declarations of helper functions for GetSelectedFileInfo().
void GetSelectedFileInfoInternal(Profile* profile,
                                 scoped_ptr<GetSelectedFileInfoParams> params);
void ContinueGetSelectedFileInfo(Profile* profile,
                                 scoped_ptr<GetSelectedFileInfoParams> params,
                                 drive::FileError error,
                                 const base::FilePath& local_file_path,
                                 scoped_ptr<drive::ResourceEntry> entry);

// Runs |callback| with SelectedFileInfoList created from |file_urls|.
void GetSelectedFileInfo(content::RenderViewHost* render_view_host,
                         Profile* profile,
                         const UrlList& file_urls,
                         bool for_opening,
                         GetSelectedFileInfoCallback callback) {
  DCHECK(render_view_host);
  DCHECK(profile);

  scoped_ptr<GetSelectedFileInfoParams> params(new GetSelectedFileInfoParams);
  params->for_opening = for_opening;
  params->callback = callback;

  for (size_t i = 0; i < file_urls.size(); ++i) {
    const GURL& file_url = file_urls[i];
    const base::FilePath path = GetLocalPathFromURL(
        render_view_host, profile, file_url);
    if (!path.empty()) {
      DVLOG(1) << "Selected: file path: " << path.value();
      params->file_paths.push_back(path);
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetSelectedFileInfoInternal,
                 profile,
                 base::Passed(&params)));
}

// Part of GetSelectedFileInfo().
void GetSelectedFileInfoInternal(Profile* profile,
                                 scoped_ptr<GetSelectedFileInfoParams> params) {
  DCHECK(profile);

  for (size_t i = params->selected_files.size();
       i < params->file_paths.size(); ++i) {
    const base::FilePath& file_path = params->file_paths[i];
    // When opening a drive file, we should get local file path.
    if (params->for_opening &&
        drive::util::IsUnderDriveMountPoint(file_path)) {
      drive::DriveIntegrationService* integration_service =
          drive::DriveIntegrationServiceFactory::GetForProfile(profile);
      // |integration_service| is NULL if Drive is disabled.
      if (!integration_service) {
        ContinueGetSelectedFileInfo(profile,
                                    params.Pass(),
                                    drive::FILE_ERROR_FAILED,
                                    base::FilePath(),
                                    scoped_ptr<drive::ResourceEntry>());
        return;
      }
      integration_service->file_system()->GetFileByPath(
          drive::util::ExtractDrivePath(file_path),
          base::Bind(&ContinueGetSelectedFileInfo,
                     profile,
                     base::Passed(&params)));
      return;
    } else {
      params->selected_files.push_back(
          ui::SelectedFileInfo(file_path, base::FilePath()));
    }
  }
  params->callback.Run(params->selected_files);
}

// Part of GetSelectedFileInfo().
void ContinueGetSelectedFileInfo(Profile* profile,
                                 scoped_ptr<GetSelectedFileInfoParams> params,
                                 drive::FileError error,
                                 const base::FilePath& local_file_path,
                                 scoped_ptr<drive::ResourceEntry> entry) {
  DCHECK(profile);

  const int index = params->selected_files.size();
  const base::FilePath& file_path = params->file_paths[index];
  base::FilePath local_path;
  if (error == drive::FILE_ERROR_OK) {
    local_path = local_file_path;
  } else {
    DLOG(ERROR) << "Failed to get " << file_path.value()
                << " with error code: " << error;
  }
  params->selected_files.push_back(ui::SelectedFileInfo(file_path, local_path));
  GetSelectedFileInfoInternal(profile, params.Pass());
}

}  // namespace

FileBrowserPrivateAPI::FileBrowserPrivateAPI(Profile* profile)
    : event_router_(new FileManagerEventRouter(profile)) {
  ExtensionFunctionRegistry* registry =
      ExtensionFunctionRegistry::GetInstance();
  // Tasks related functions.
  registry->RegisterFunction<ExecuteTasksFunction>();
  registry->RegisterFunction<GetFileTasksFunction>();
  registry->RegisterFunction<SetDefaultTaskFunction>();

  // Drive related functions.
  registry->RegisterFunction<GetDriveEntryPropertiesFunction>();
  registry->RegisterFunction<PinDriveFileFunction>();
  registry->RegisterFunction<GetDriveFilesFunction>();
  registry->RegisterFunction<CancelFileTransfersFunction>();
  registry->RegisterFunction<TransferFileFunction>();
  registry->RegisterFunction<SearchDriveFunction>();
  registry->RegisterFunction<SearchDriveMetadataFunction>();
  registry->RegisterFunction<ClearDriveCacheFunction>();
  registry->RegisterFunction<GetDriveConnectionStateFunction>();
  registry->RegisterFunction<RequestAccessTokenFunction>();
  registry->RegisterFunction<GetShareUrlFunction>();

  registry->RegisterFunction<LogoutUserFunction>();
  registry->RegisterFunction<CancelFileDialogFunction>();
  registry->RegisterFunction<FileDialogStringsFunction>();
  registry->RegisterFunction<GetVolumeMetadataFunction>();
  registry->RegisterFunction<RequestFileSystemFunction>();
  registry->RegisterFunction<AddFileWatchBrowserFunction>();
  registry->RegisterFunction<RemoveFileWatchBrowserFunction>();
  registry->RegisterFunction<SelectFileFunction>();
  registry->RegisterFunction<SelectFilesFunction>();
  registry->RegisterFunction<AddMountFunction>();
  registry->RegisterFunction<RemoveMountFunction>();
  registry->RegisterFunction<GetMountPointsFunction>();
  registry->RegisterFunction<GetSizeStatsFunction>();
  registry->RegisterFunction<FormatDeviceFunction>();
  registry->RegisterFunction<ViewFilesFunction>();
  registry->RegisterFunction<GetPreferencesFunction>();
  registry->RegisterFunction<SetPreferencesFunction>();
  registry->RegisterFunction<SetLastModifiedFunction>();
  registry->RegisterFunction<ZipSelectionFunction>();
  registry->RegisterFunction<ValidatePathNameLengthFunction>();
  registry->RegisterFunction<ZoomFunction>();
  event_router_->ObserveFileSystemEvents();
}

FileBrowserPrivateAPI::~FileBrowserPrivateAPI() {
}

void FileBrowserPrivateAPI::Shutdown() {
  event_router_->Shutdown();
}

// static
FileBrowserPrivateAPI* FileBrowserPrivateAPI::Get(Profile* profile) {
  return FileBrowserPrivateAPIFactory::GetForProfile(profile);
}

LogoutUserFunction::LogoutUserFunction() {
}

LogoutUserFunction::~LogoutUserFunction() {
}

bool LogoutUserFunction::RunImpl() {
  chrome::AttemptUserExit();
  return true;
}

RequestFileSystemFunction::RequestFileSystemFunction() {
}

RequestFileSystemFunction::~RequestFileSystemFunction() {
}

void RequestFileSystemFunction::DidOpenFileSystem(
    scoped_refptr<fileapi::FileSystemContext> file_system_context,
    base::PlatformFileError result,
    const std::string& name,
    const GURL& root_path) {
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

  // Add drive mount point immediately when we kick of first instance of file
  // manager. The actual mount event will be sent to UI only when we perform
  // proper authentication.
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  if (integration_service)
    SetDriveMountPointPermissions(profile_, extension_id(), render_view_host());
  DictionaryValue* dict = new DictionaryValue();
  SetResult(dict);
  dict->SetString("name", name);
  dict->SetString("path", root_path.spec());
  dict->SetInteger("error", drive::FILE_ERROR_OK);
  SendResponse(true);
}

void RequestFileSystemFunction::DidFail(
    base::PlatformFileError error_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  error_ = base::StringPrintf(kFileError, static_cast<int>(error_code));
  SendResponse(false);
}

bool RequestFileSystemFunction::SetupFileSystemAccessPermissions(
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

bool RequestFileSystemFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!dispatcher() || !render_view_host() || !render_view_host()->GetProcess())
    return false;

  set_log_on_completion(true);

  content::SiteInstance* site_instance = render_view_host()->GetSiteInstance();
  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      BrowserContext::GetStoragePartition(profile_, site_instance)->
          GetFileSystemContext();

  const GURL origin_url = source_url_.GetOrigin();
  file_system_context->OpenFileSystem(
      origin_url,
      fileapi::kFileSystemTypeExternal,
      fileapi::OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT,
      base::Bind(&RequestFileSystemFunction::DidOpenFileSystem,
                 this,
                 file_system_context));
  return true;
}

FileWatchBrowserFunctionBase::FileWatchBrowserFunctionBase() {
}

FileWatchBrowserFunctionBase::~FileWatchBrowserFunctionBase() {
}

void FileWatchBrowserFunctionBase::Respond(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  SetResult(Value::CreateBooleanValue(success));
  SendResponse(success);
}

bool FileWatchBrowserFunctionBase::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!render_view_host() || !render_view_host()->GetProcess())
    return false;

  // First param is url of a file to watch.
  std::string url;
  if (!args_->GetString(0, &url) || url.empty())
    return false;

  content::SiteInstance* site_instance = render_view_host()->GetSiteInstance();
  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      BrowserContext::GetStoragePartition(profile(), site_instance)->
          GetFileSystemContext();

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

AddFileWatchBrowserFunction::AddFileWatchBrowserFunction() {
}

AddFileWatchBrowserFunction::~AddFileWatchBrowserFunction() {
}

void AddFileWatchBrowserFunction::PerformFileWatchOperation(
    const base::FilePath& local_path,
    const base::FilePath& virtual_path,
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FileManagerEventRouter* event_router =
      FileBrowserPrivateAPI::Get(profile_)->event_router();
  event_router->AddFileWatch(
      local_path,
      virtual_path,
      extension_id,
      base::Bind(&AddFileWatchBrowserFunction::Respond, this));
}

RemoveFileWatchBrowserFunction::RemoveFileWatchBrowserFunction() {
}

RemoveFileWatchBrowserFunction::~RemoveFileWatchBrowserFunction() {
}

void RemoveFileWatchBrowserFunction::PerformFileWatchOperation(
    const base::FilePath& local_path,
    const base::FilePath& unused,
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FileManagerEventRouter* event_router =
      FileBrowserPrivateAPI::Get(profile_)->event_router();
  event_router->RemoveFileWatch(local_path, extension_id);
  Respond(true);
}

ViewFilesFunction::ViewFilesFunction() {
}

ViewFilesFunction::~ViewFilesFunction() {
}

bool ViewFilesFunction::RunImpl() {
  if (args_->GetSize() < 1) {
    return false;
  }

  ListValue* path_list = NULL;
  args_->GetList(0, &path_list);
  DCHECK(path_list);

  std::string internal_task_id;
  args_->GetString(1, &internal_task_id);

  std::vector<base::FilePath> files;
  for (size_t i = 0; i < path_list->GetSize(); ++i) {
    std::string url_as_string;
    path_list->GetString(i, &url_as_string);
    base::FilePath path = GetLocalPathFromURL(
        render_view_host(), profile(), GURL(url_as_string));
    if (path.empty())
      return false;
    files.push_back(path);
  }

  Browser* browser = chrome::FindOrCreateTabbedBrowser(
      profile_, chrome::HOST_DESKTOP_TYPE_ASH);
  bool success = browser;

  if (browser) {
    for (size_t i = 0; i < files.size(); ++i) {
      bool handled = file_manager_util::ExecuteBuiltinHandler(
          browser, files[i], internal_task_id);
      if (!handled && files.size() == 1)
        success = false;
    }
  }

  SetResult(Value::CreateBooleanValue(success));
  SendResponse(true);
  return true;
}

SelectFileFunction::SelectFileFunction() {
}

SelectFileFunction::~SelectFileFunction() {
}

bool SelectFileFunction::RunImpl() {
  if (args_->GetSize() != 4) {
    return false;
  }
  std::string file_url;
  args_->GetString(0, &file_url);
  UrlList file_paths;
  file_paths.push_back(GURL(file_url));
  bool for_opening = false;
  args_->GetBoolean(2, &for_opening);

  GetSelectedFileInfo(
      render_view_host(),
      profile(),
      file_paths,
      for_opening,
      base::Bind(&SelectFileFunction::GetSelectedFileInfoResponse, this));
  return true;
}

void SelectFileFunction::GetSelectedFileInfoResponse(
    const std::vector<ui::SelectedFileInfo>& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (files.size() != 1) {
    SendResponse(false);
    return;
  }
  int index;
  args_->GetInteger(1, &index);
  int32 tab_id = GetTabId(dispatcher());
  SelectFileDialogExtension::OnFileSelected(tab_id, files[0], index);
  SendResponse(true);
}

SelectFilesFunction::SelectFilesFunction() {
}

SelectFilesFunction::~SelectFilesFunction() {
}

bool SelectFilesFunction::RunImpl() {
  if (args_->GetSize() != 2) {
    return false;
  }

  ListValue* path_list = NULL;
  args_->GetList(0, &path_list);
  DCHECK(path_list);

  std::string virtual_path;
  size_t len = path_list->GetSize();
  UrlList file_urls;
  file_urls.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    path_list->GetString(i, &virtual_path);
    file_urls.push_back(GURL(virtual_path));
  }

  GetSelectedFileInfo(
      render_view_host(),
      profile(),
      file_urls,
      true,  // for_opening
      base::Bind(&SelectFilesFunction::GetSelectedFileInfoResponse, this));
  return true;
}

void SelectFilesFunction::GetSelectedFileInfoResponse(
    const std::vector<ui::SelectedFileInfo>& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  int32 tab_id = GetTabId(dispatcher());
  SelectFileDialogExtension::OnMultiFilesSelected(tab_id, files);
  SendResponse(true);
}

CancelFileDialogFunction::CancelFileDialogFunction() {
}

CancelFileDialogFunction::~CancelFileDialogFunction() {
}

bool CancelFileDialogFunction::RunImpl() {
  int32 tab_id = GetTabId(dispatcher());
  SelectFileDialogExtension::OnFileSelectionCanceled(tab_id);
  SendResponse(true);
  return true;
}

AddMountFunction::AddMountFunction() {
}

AddMountFunction::~AddMountFunction() {
}

bool AddMountFunction::RunImpl() {
  // The third argument is simply ignored.
  if (args_->GetSize() != 2 && args_->GetSize() != 3) {
    error_ = "Invalid argument count";
    return false;
  }

  std::string file_url;
  if (!args_->GetString(0, &file_url)) {
    return false;
  }

  std::string mount_type_str;
  if (!args_->GetString(1, &mount_type_str)) {
    return false;
  }

  drive::util::Log("%s[%d] called. (source: '%s', type:'%s')",
                   name().c_str(),
                   request_id(),
                   file_url.empty() ? "(none)" : file_url.c_str(),
                   mount_type_str.c_str());
  set_log_on_completion(true);

  // Set default return source path to the empty string.
  SetResult(new base::StringValue(""));

  chromeos::MountType mount_type =
      DiskMountManager::MountTypeFromString(mount_type_str);
  switch (mount_type) {
    case chromeos::MOUNT_TYPE_INVALID: {
      error_ = "Invalid mount type";
      SendResponse(false);
      break;
    }
    case chromeos::MOUNT_TYPE_GOOGLE_DRIVE: {
      // Dispatch fake 'mounted' event because JS code depends on it.
      // TODO(hashimoto): Remove this redanduncy.
      FileBrowserPrivateAPI::Get(profile_)->event_router()->
          OnFileSystemMounted();

      // Pass back the drive mount point path as source path.
      const std::string& drive_path =
          drive::util::GetDriveMountPointPathAsString();
      SetResult(new base::StringValue(drive_path));
      SendResponse(true);
      break;
    }
    default: {
      const base::FilePath path = GetLocalPathFromURL(
          render_view_host(), profile(), GURL(file_url));

      if (path.empty()) {
        SendResponse(false);
        break;
      }

      const base::FilePath::StringType display_name = path.BaseName().value();

      // Check if the source path is under Drive cache directory.
      if (drive::util::IsUnderDriveMountPoint(path)) {
        drive::DriveIntegrationService* integration_service =
            drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
        drive::FileSystemInterface* file_system =
            integration_service ? integration_service->file_system() : NULL;
        if (!file_system) {
          SendResponse(false);
          break;
        }
        file_system->MarkCacheFileAsMounted(
            drive::util::ExtractDrivePath(path),
            base::Bind(&AddMountFunction::OnMountedStateSet,
                       this, mount_type_str, display_name));
      } else {
        OnMountedStateSet(mount_type_str, display_name,
                          drive::FILE_ERROR_OK, path);
      }
      break;
    }
  }

  return true;
}

void AddMountFunction::OnMountedStateSet(
    const std::string& mount_type,
    const base::FilePath::StringType& file_name,
    drive::FileError error,
    const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != drive::FILE_ERROR_OK) {
    SendResponse(false);
    return;
  }

  DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();
  // Pass back the actual source path of the mount point.
  SetResult(new base::StringValue(file_path.value()));
  SendResponse(true);
  // MountPath() takes a std::string.
  disk_mount_manager->MountPath(
      file_path.AsUTF8Unsafe(), base::FilePath(file_name).Extension(),
      file_name, DiskMountManager::MountTypeFromString(mount_type));
}

RemoveMountFunction::RemoveMountFunction() {
}

RemoveMountFunction::~RemoveMountFunction() {
}

bool RemoveMountFunction::RunImpl() {
  if (args_->GetSize() != 1) {
    return false;
  }

  std::string mount_path;
  if (!args_->GetString(0, &mount_path)) {
    return false;
  }

  drive::util::Log("%s[%d] called. (mount_path: '%s')",
                   name().c_str(),
                   request_id(),
                   mount_path.c_str());
  set_log_on_completion(true);

  UrlList file_paths;
  file_paths.push_back(GURL(mount_path));
  GetSelectedFileInfo(
      render_view_host(),
      profile(),
      file_paths,
      true,  // for_opening
      base::Bind(&RemoveMountFunction::GetSelectedFileInfoResponse, this));
  return true;
}

void RemoveMountFunction::GetSelectedFileInfoResponse(
    const std::vector<ui::SelectedFileInfo>& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (files.size() != 1) {
    SendResponse(false);
    return;
  }

  // TODO(tbarzic): Send response when callback is received, it would make more
  // sense than remembering issued unmount requests in file manager and showing
  // errors for them when MountCompleted event is received.
  DiskMountManager::GetInstance()->UnmountPath(
      files[0].local_path.value(),
      chromeos::UNMOUNT_OPTIONS_NONE,
      DiskMountManager::UnmountPathCallback());
  SendResponse(true);
}

GetMountPointsFunction::GetMountPointsFunction() {
}

GetMountPointsFunction::~GetMountPointsFunction() {
}

bool GetMountPointsFunction::RunImpl() {
  if (args_->GetSize())
    return false;

  base::ListValue *mounts = new base::ListValue();
  SetResult(mounts);

  DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();
  DiskMountManager::MountPointMap mount_points =
      disk_mount_manager->mount_points();

  std::string log_string = "[";
  const char *separator = "";
  for (DiskMountManager::MountPointMap::const_iterator it =
           mount_points.begin();
       it != mount_points.end();
       ++it) {
    mounts->Append(CreateValueFromMountPoint(profile_, it->second,
        extension_->id(), source_url_));
    log_string += separator + it->first;
    separator = ", ";
  }

  log_string += "]";

  drive::util::Log("%s[%d] succeeded. (results: '%s', %" PRIuS " mount points)",
                   name().c_str(),
                   request_id(),
                   log_string.c_str(),
                   mount_points.size());

  SendResponse(true);
  return true;
}

SetLastModifiedFunction::SetLastModifiedFunction() {
}

SetLastModifiedFunction::~SetLastModifiedFunction() {
}

bool SetLastModifiedFunction::RunImpl() {
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

  base::FilePath local_path = GetLocalPathFromURL(
      render_view_host(), profile(), GURL(file_url));

  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&SetLastModifiedOnBlockingPool,
                 local_path,
                 strtoul(timestamp.c_str(), NULL, 0)),
      base::Bind(&SetLastModifiedFunction::SendResponse,
                 this));
  return true;
}

GetSizeStatsFunction::GetSizeStatsFunction() {
}

GetSizeStatsFunction::~GetSizeStatsFunction() {
}

bool GetSizeStatsFunction::RunImpl() {
  if (args_->GetSize() != 1) {
    return false;
  }

  std::string mount_url;
  if (!args_->GetString(0, &mount_url))
    return false;

  base::FilePath file_path = GetLocalPathFromURL(
      render_view_host(), profile(), GURL(mount_url));
  if (file_path.empty())
    return false;

  if (file_path == drive::util::GetDriveMountPointPath()) {
    drive::DriveIntegrationService* integration_service =
        drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
    // |integration_service| is NULL if Drive is disabled.
    if (!integration_service) {
      // If stats couldn't be gotten for drive, result should be left
      // undefined. See comments in GetDriveAvailableSpaceCallback().
      SendResponse(true);
      return true;
    }

    drive::FileSystemInterface* file_system =
        integration_service->file_system();

    file_system->GetAvailableSpace(
        base::Bind(&GetSizeStatsFunction::GetDriveAvailableSpaceCallback,
                   this));

  } else {
    size_t* total_size_kb = new size_t(0);
    size_t* remaining_size_kb = new size_t(0);
    BrowserThread::PostBlockingPoolTaskAndReply(
        FROM_HERE,
        base::Bind(&GetSizeStatsOnBlockingPool,
                   file_path.value(),
                   total_size_kb,
                   remaining_size_kb),
        base::Bind(&GetSizeStatsFunction::GetSizeStatsCallback,
                   this,
                   base::Owned(total_size_kb),
                   base::Owned(remaining_size_kb)));
  }
  return true;
}

void GetSizeStatsFunction::GetDriveAvailableSpaceCallback(
    drive::FileError error,
    int64 bytes_total,
    int64 bytes_used) {
  if (error == drive::FILE_ERROR_OK) {
    int64 bytes_remaining = bytes_total - bytes_used;
    const size_t total_size_kb = static_cast<size_t>(bytes_total/1024);
    const size_t remaining_size_kb = static_cast<size_t>(bytes_remaining/1024);
    GetSizeStatsCallback(&total_size_kb, &remaining_size_kb);
  } else {
    // If stats couldn't be gotten for drive, result should be left undefined.
    SendResponse(true);
  }
}

void GetSizeStatsFunction::GetSizeStatsCallback(
    const size_t* total_size_kb,
    const size_t* remaining_size_kb) {
  base::DictionaryValue* sizes = new base::DictionaryValue();
  SetResult(sizes);

  sizes->SetInteger("totalSizeKB", *total_size_kb);
  sizes->SetInteger("remainingSizeKB", *remaining_size_kb);

  SendResponse(true);
}

FormatDeviceFunction::FormatDeviceFunction() {
}

FormatDeviceFunction::~FormatDeviceFunction() {
}

bool FormatDeviceFunction::RunImpl() {
  if (args_->GetSize() != 1) {
    return false;
  }

  std::string volume_file_url;
  if (!args_->GetString(0, &volume_file_url)) {
    NOTREACHED();
    return false;
  }

  base::FilePath file_path = GetLocalPathFromURL(
      render_view_host(), profile(), GURL(volume_file_url));
  if (file_path.empty())
    return false;

  DiskMountManager::GetInstance()->FormatMountedDevice(file_path.value());
  SendResponse(true);
  return true;
}

GetVolumeMetadataFunction::GetVolumeMetadataFunction() {
}

GetVolumeMetadataFunction::~GetVolumeMetadataFunction() {
}

bool GetVolumeMetadataFunction::RunImpl() {
  if (args_->GetSize() != 1) {
    error_ = "Invalid argument count";
    return false;
  }

  std::string volume_mount_url;
  if (!args_->GetString(0, &volume_mount_url)) {
    NOTREACHED();
    return false;
  }

  base::FilePath file_path = GetLocalPathFromURL(
      render_view_host(), profile(), GURL(volume_mount_url));
  if (file_path.empty()) {
    error_ = "Invalid mount path.";
    return false;
  }

  results_.reset();

  const DiskMountManager::Disk* volume = GetVolumeAsDisk(file_path.value());
  if (volume) {
    DictionaryValue* volume_info =
        CreateValueFromDisk(profile_, extension_->id(), volume);
    SetResult(volume_info);
  }

  SendResponse(true);
  return true;
}

FileDialogStringsFunction::FileDialogStringsFunction() {
}

FileDialogStringsFunction::~FileDialogStringsFunction() {
}

bool FileDialogStringsFunction::RunImpl() {
  DictionaryValue* dict = new DictionaryValue();
  SetResult(dict);

#define SET_STRING(id, idr) \
  dict->SetString(id, l10n_util::GetStringUTF16(idr))

  SET_STRING("WEB_FONT_FAMILY", IDS_WEB_FONT_FAMILY);
  SET_STRING("WEB_FONT_SIZE", IDS_WEB_FONT_SIZE);

  SET_STRING("ROOT_DIRECTORY_LABEL", IDS_FILE_BROWSER_ROOT_DIRECTORY_LABEL);
  SET_STRING("ARCHIVE_DIRECTORY_LABEL",
             IDS_FILE_BROWSER_ARCHIVE_DIRECTORY_LABEL);
  SET_STRING("REMOVABLE_DIRECTORY_LABEL",
             IDS_FILE_BROWSER_REMOVABLE_DIRECTORY_LABEL);
  SET_STRING("DOWNLOADS_DIRECTORY_LABEL",
             IDS_FILE_BROWSER_DOWNLOADS_DIRECTORY_LABEL);
  SET_STRING("DRIVE_DIRECTORY_LABEL", IDS_FILE_BROWSER_DRIVE_DIRECTORY_LABEL);
  SET_STRING("DRIVE_MY_DRIVE_LABEL", IDS_FILE_BROWSER_DRIVE_MY_DRIVE_LABEL);
  SET_STRING("DRIVE_OFFLINE_COLLECTION_LABEL",
             IDS_FILE_BROWSER_DRIVE_OFFLINE_COLLECTION_LABEL);
  SET_STRING("DRIVE_SHARED_WITH_ME_COLLECTION_LABEL",
             IDS_FILE_BROWSER_DRIVE_SHARED_WITH_ME_COLLECTION_LABEL);
  SET_STRING("DRIVE_RECENT_COLLECTION_LABEL",
             IDS_FILE_BROWSER_DRIVE_RECENT_COLLECTION_LABEL);
  SET_STRING("NAME_COLUMN_LABEL", IDS_FILE_BROWSER_NAME_COLUMN_LABEL);
  SET_STRING("SIZE_COLUMN_LABEL", IDS_FILE_BROWSER_SIZE_COLUMN_LABEL);
  SET_STRING("SIZE_BYTES", IDS_FILE_BROWSER_SIZE_BYTES);
  SET_STRING("SIZE_KB", IDS_FILE_BROWSER_SIZE_KB);
  SET_STRING("SIZE_MB", IDS_FILE_BROWSER_SIZE_MB);
  SET_STRING("SIZE_GB", IDS_FILE_BROWSER_SIZE_GB);
  SET_STRING("SIZE_TB", IDS_FILE_BROWSER_SIZE_TB);
  SET_STRING("SIZE_PB", IDS_FILE_BROWSER_SIZE_PB);

  SET_STRING("SHORTCUT_CTRL", IDS_FILE_BROWSER_SHORTCUT_CTRL);
  SET_STRING("SHORTCUT_ALT", IDS_FILE_BROWSER_SHORTCUT_ALT);
  SET_STRING("SHORTCUT_SHIFT", IDS_FILE_BROWSER_SHORTCUT_SHIFT);
  SET_STRING("SHORTCUT_META", IDS_FILE_BROWSER_SHORTCUT_META);
  SET_STRING("SHORTCUT_SPACE", IDS_FILE_BROWSER_SHORTCUT_SPACE);
  SET_STRING("SHORTCUT_ENTER", IDS_FILE_BROWSER_SHORTCUT_ENTER);

  SET_STRING("TYPE_COLUMN_LABEL", IDS_FILE_BROWSER_TYPE_COLUMN_LABEL);
  SET_STRING("DATE_COLUMN_LABEL", IDS_FILE_BROWSER_DATE_COLUMN_LABEL);
  SET_STRING("PREVIEW_COLUMN_LABEL", IDS_FILE_BROWSER_PREVIEW_COLUMN_LABEL);
  SET_STRING("OFFLINE_COLUMN_LABEL", IDS_FILE_BROWSER_OFFLINE_COLUMN_LABEL);

  SET_STRING("DOWNLOADS_DIRECTORY_WARNING",
             IDS_FILE_BROWSER_DOWNLOADS_DIRECTORY_WARNING);

  SET_STRING("ERROR_CREATING_FOLDER", IDS_FILE_BROWSER_ERROR_CREATING_FOLDER);
  SET_STRING("ERROR_INVALID_CHARACTER",
             IDS_FILE_BROWSER_ERROR_INVALID_CHARACTER);
  SET_STRING("ERROR_RESERVED_NAME", IDS_FILE_BROWSER_ERROR_RESERVED_NAME);
  SET_STRING("ERROR_HIDDEN_NAME", IDS_FILE_BROWSER_ERROR_HIDDEN_NAME);
  SET_STRING("ERROR_WHITESPACE_NAME", IDS_FILE_BROWSER_ERROR_WHITESPACE_NAME);
  SET_STRING("ERROR_NEW_FOLDER_EMPTY_NAME",
             IDS_FILE_BROWSER_ERROR_NEW_FOLDER_EMPTY_NAME);
  SET_STRING("ERROR_LONG_NAME", IDS_FILE_BROWSER_ERROR_LONG_NAME);
  SET_STRING("NEW_FOLDER_BUTTON_LABEL",
             IDS_FILE_BROWSER_NEW_FOLDER_BUTTON_LABEL);
  SET_STRING("NEW_WINDOW_BUTTON_LABEL",
             IDS_FILE_BROWSER_NEW_WINDOW_BUTTON_LABEL);
  SET_STRING("CHANGE_DEFAULT_APP_BUTTON_LABEL",
             IDS_FILE_BROWSER_CHANGE_DEFAULT_APP_BUTTON_LABEL);
  SET_STRING("FILENAME_LABEL", IDS_FILE_BROWSER_FILENAME_LABEL);
  SET_STRING("PREPARING_LABEL", IDS_FILE_BROWSER_PREPARING_LABEL);
  SET_STRING("DRAGGING_MULTIPLE_ITEMS",
             IDS_FILE_BROWSER_DRAGGING_MULTIPLE_ITEMS);

  SET_STRING("DIMENSIONS_LABEL", IDS_FILE_BROWSER_DIMENSIONS_LABEL);
  SET_STRING("DIMENSIONS_FORMAT", IDS_FILE_BROWSER_DIMENSIONS_FORMAT);

  SET_STRING("IMAGE_DIMENSIONS", IDS_FILE_BROWSER_IMAGE_DIMENSIONS);
  SET_STRING("VOLUME_LABEL", IDS_FILE_BROWSER_VOLUME_LABEL);
  SET_STRING("READ_ONLY", IDS_FILE_BROWSER_READ_ONLY);

  SET_STRING("ARCHIVE_MOUNT_FAILED", IDS_FILE_BROWSER_ARCHIVE_MOUNT_FAILED);
  SET_STRING("UNMOUNT_FAILED", IDS_FILE_BROWSER_UNMOUNT_FAILED);
  SET_STRING("MOUNT_ARCHIVE", IDS_FILE_BROWSER_MOUNT_ARCHIVE);
  SET_STRING("FORMAT_DEVICE_BUTTON_LABEL",
             IDS_FILE_BROWSER_FORMAT_DEVICE_BUTTON_LABEL);
  SET_STRING("UNMOUNT_DEVICE_BUTTON_LABEL",
             IDS_FILE_BROWSER_UNMOUNT_DEVICE_BUTTON_LABEL);
  SET_STRING("CLOSE_ARCHIVE_BUTTON_LABEL",
             IDS_FILE_BROWSER_CLOSE_ARCHIVE_BUTTON_LABEL);

  SET_STRING("SEARCH_TEXT_LABEL", IDS_FILE_BROWSER_SEARCH_TEXT_LABEL);

  SET_STRING("ACTION_VIEW", IDS_FILE_BROWSER_ACTION_VIEW);
  SET_STRING("ACTION_OPEN", IDS_FILE_BROWSER_ACTION_OPEN);
  SET_STRING("ACTION_OPEN_GDOC", IDS_FILE_BROWSER_ACTION_OPEN_GDOC);
  SET_STRING("ACTION_OPEN_GSHEET", IDS_FILE_BROWSER_ACTION_OPEN_GSHEET);
  SET_STRING("ACTION_OPEN_GSLIDES", IDS_FILE_BROWSER_ACTION_OPEN_GSLIDES);
  SET_STRING("ACTION_WATCH", IDS_FILE_BROWSER_ACTION_WATCH);
  SET_STRING("ACTION_LISTEN", IDS_FILE_BROWSER_ACTION_LISTEN);
  SET_STRING("INSTALL_CRX", IDS_FILE_BROWSER_INSTALL_CRX);
  SET_STRING("SEND_TO_DRIVE", IDS_FILE_BROWSER_SEND_TO_DRIVE);

  SET_STRING("GALLERY_NO_IMAGES", IDS_FILE_BROWSER_GALLERY_NO_IMAGES);
  SET_STRING("GALLERY_ITEMS_SELECTED", IDS_FILE_BROWSER_GALLERY_ITEMS_SELECTED);
  SET_STRING("GALLERY_MOSAIC", IDS_FILE_BROWSER_GALLERY_MOSAIC);
  SET_STRING("GALLERY_SLIDE", IDS_FILE_BROWSER_GALLERY_SLIDE);
  SET_STRING("GALLERY_DELETE", IDS_FILE_BROWSER_GALLERY_DELETE);
  SET_STRING("GALLERY_SLIDESHOW", IDS_FILE_BROWSER_GALLERY_SLIDESHOW);

  SET_STRING("GALLERY_EDIT", IDS_FILE_BROWSER_GALLERY_EDIT);
  SET_STRING("GALLERY_PRINT", IDS_FILE_BROWSER_GALLERY_PRINT);
  SET_STRING("GALLERY_SHARE", IDS_FILE_BROWSER_GALLERY_SHARE);
  SET_STRING("GALLERY_ENTER_WHEN_DONE",
             IDS_FILE_BROWSER_GALLERY_ENTER_WHEN_DONE);
  SET_STRING("GALLERY_AUTOFIX", IDS_FILE_BROWSER_GALLERY_AUTOFIX);
  SET_STRING("GALLERY_FIXED", IDS_FILE_BROWSER_GALLERY_FIXED);
  SET_STRING("GALLERY_CROP", IDS_FILE_BROWSER_GALLERY_CROP);
  SET_STRING("GALLERY_EXPOSURE", IDS_FILE_BROWSER_GALLERY_EXPOSURE);
  SET_STRING("GALLERY_BRIGHTNESS", IDS_FILE_BROWSER_GALLERY_BRIGHTNESS);
  SET_STRING("GALLERY_CONTRAST", IDS_FILE_BROWSER_GALLERY_CONTRAST);
  SET_STRING("GALLERY_ROTATE_LEFT", IDS_FILE_BROWSER_GALLERY_ROTATE_LEFT);
  SET_STRING("GALLERY_ROTATE_RIGHT", IDS_FILE_BROWSER_GALLERY_ROTATE_RIGHT);
  SET_STRING("GALLERY_UNDO", IDS_FILE_BROWSER_GALLERY_UNDO);
  SET_STRING("GALLERY_REDO", IDS_FILE_BROWSER_GALLERY_REDO);
  SET_STRING("GALLERY_FILE_EXISTS", IDS_FILE_BROWSER_GALLERY_FILE_EXISTS);
  SET_STRING("GALLERY_SAVED", IDS_FILE_BROWSER_GALLERY_SAVED);
  SET_STRING("GALLERY_OVERWRITE_ORIGINAL",
             IDS_FILE_BROWSER_GALLERY_OVERWRITE_ORIGINAL);
  SET_STRING("GALLERY_OVERWRITE_BUBBLE",
             IDS_FILE_BROWSER_GALLERY_OVERWRITE_BUBBLE);
  SET_STRING("GALLERY_UNSAVED_CHANGES",
             IDS_FILE_BROWSER_GALLERY_UNSAVED_CHANGES);
  SET_STRING("GALLERY_READONLY_WARNING",
             IDS_FILE_BROWSER_GALLERY_READONLY_WARNING);
  SET_STRING("GALLERY_IMAGE_ERROR", IDS_FILE_BROWSER_GALLERY_IMAGE_ERROR);
  SET_STRING("GALLERY_IMAGE_TOO_BIG_ERROR",
             IDS_FILE_BROWSER_GALLERY_IMAGE_TOO_BIG_ERROR);
  SET_STRING("GALLERY_VIDEO_ERROR", IDS_FILE_BROWSER_GALLERY_VIDEO_ERROR);
  SET_STRING("GALLERY_VIDEO_DECODING_ERROR",
             IDS_FILE_BROWSER_GALLERY_VIDEO_DECODING_ERROR);
  SET_STRING("GALLERY_VIDEO_LOOPED_MODE",
             IDS_FILE_BROWSER_GALLERY_VIDEO_LOOPED_MODE);
  SET_STRING("AUDIO_ERROR", IDS_FILE_BROWSER_AUDIO_ERROR);
  SET_STRING("GALLERY_IMAGE_OFFLINE", IDS_FILE_BROWSER_GALLERY_IMAGE_OFFLINE);
  SET_STRING("GALLERY_VIDEO_OFFLINE", IDS_FILE_BROWSER_GALLERY_VIDEO_OFFLINE);
  SET_STRING("AUDIO_OFFLINE", IDS_FILE_BROWSER_AUDIO_OFFLINE);
  // Reusing strings, but with alias starting with GALLERY.
  dict->SetString("GALLERY_FILE_HIDDEN_NAME",
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_ERROR_HIDDEN_NAME));
  dict->SetString("GALLERY_OK_LABEL",
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_OK_LABEL));
  dict->SetString("GALLERY_CANCEL_LABEL",
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_CANCEL_LABEL));
  dict->SetString("GALLERY_CONFIRM_DELETE_ONE",
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_CONFIRM_DELETE_ONE));
  dict->SetString("GALLERY_CONFIRM_DELETE_SOME",
      l10n_util::GetStringUTF16(IDS_FILE_BROWSER_CONFIRM_DELETE_SOME));

  SET_STRING("ACTION_CHOICE_OPENING_METHOD",
             IDS_FILE_BROWSER_ACTION_CHOICE_OPENING_METHOD);
  SET_STRING("ACTION_CHOICE_PHOTOS_DRIVE",
             IDS_FILE_BROWSER_ACTION_CHOICE_PHOTOS_DRIVE);
  SET_STRING("ACTION_CHOICE_DRIVE_NOT_REACHED",
             IDS_FILE_BROWSER_ACTION_CHOICE_DRIVE_NOT_REACHED);
  SET_STRING("ACTION_CHOICE_VIEW_FILES",
             IDS_FILE_BROWSER_ACTION_CHOICE_VIEW_FILES);
  SET_STRING("ACTION_CHOICE_WATCH_SINGLE_VIDEO",
             IDS_FILE_BROWSER_ACTION_CHOICE_WATCH_SINGLE_VIDEO);
  SET_STRING("ACTION_CHOICE_ONCE", IDS_FILE_BROWSER_ACTION_CHOICE_ONCE);
  SET_STRING("ACTION_CHOICE_ALWAYS", IDS_FILE_BROWSER_ACTION_CHOICE_ALWAYS);
  SET_STRING("ACTION_CHOICE_COUNTER_NO_MEDIA",
             IDS_FILE_BROWSER_ACTION_CHOICE_COUNTER_NO_MEDIA);
  SET_STRING("ACTION_CHOICE_COUNTER", IDS_FILE_BROWSER_ACTION_CHOICE_COUNTER);
  SET_STRING("ACTION_CHOICE_LOADING_USB",
             IDS_FILE_BROWSER_ACTION_CHOICE_LOADING_USB);
  SET_STRING("ACTION_CHOICE_LOADING_SD",
             IDS_FILE_BROWSER_ACTION_CHOICE_LOADING_SD);

  SET_STRING("PHOTO_IMPORT_TITLE", IDS_FILE_BROWSER_PHOTO_IMPORT_TITLE);
  SET_STRING("PHOTO_IMPORT_IMPORT_BUTTON",
             IDS_FILE_BROWSER_PHOTO_IMPORT_IMPORT_BUTTON);
  SET_STRING("PHOTO_IMPORT_CANCEL_BUTTON",
             IDS_FILE_BROWSER_PHOTO_IMPORT_CANCEL_BUTTON);
  SET_STRING("PHOTO_IMPORT_DRIVE_ERROR",
             IDS_FILE_BROWSER_PHOTO_IMPORT_DRIVE_ERROR);
  SET_STRING("PHOTO_IMPORT_DESTINATION_ERROR",
             IDS_FILE_BROWSER_PHOTO_IMPORT_DESTINATION_ERROR);
  SET_STRING("PHOTO_IMPORT_SOURCE_ERROR",
             IDS_FILE_BROWSER_PHOTO_IMPORT_SOURCE_ERROR);
  SET_STRING("PHOTO_IMPORT_UNKNOWN_DATE",
             IDS_FILE_BROWSER_PHOTO_IMPORT_UNKNOWN_DATE);
  SET_STRING("PHOTO_IMPORT_NEW_ALBUM_NAME",
             IDS_FILE_BROWSER_PHOTO_IMPORT_NEW_ALBUM_NAME);
  SET_STRING("PHOTO_IMPORT_SELECT_ALBUM_CAPTION",
             IDS_FILE_BROWSER_PHOTO_IMPORT_SELECT_ALBUM_CAPTION);
  SET_STRING("PHOTO_IMPORT_SELECT_ALBUM_CAPTION_PLURAL",
             IDS_FILE_BROWSER_PHOTO_IMPORT_SELECT_ALBUM_CAPTION_PLURAL);
  SET_STRING("PHOTO_IMPORT_IMPORTING_ERROR",
             IDS_FILE_BROWSER_PHOTO_IMPORT_IMPORTING_ERROR);
  SET_STRING("PHOTO_IMPORT_IMPORTING", IDS_FILE_BROWSER_PHOTO_IMPORT_IMPORTING);
  SET_STRING("PHOTO_IMPORT_IMPORT_COMPLETE",
             IDS_FILE_BROWSER_PHOTO_IMPORT_IMPORT_COMPLETE);
  SET_STRING("PHOTO_IMPORT_CAPTION", IDS_FILE_BROWSER_PHOTO_IMPORT_CAPTION);
  SET_STRING("PHOTO_IMPORT_ONE_SELECTED",
             IDS_FILE_BROWSER_PHOTO_IMPORT_ONE_SELECTED);
  SET_STRING("PHOTO_IMPORT_MANY_SELECTED",
             IDS_FILE_BROWSER_PHOTO_IMPORT_MANY_SELECTED);
  SET_STRING("PHOTO_IMPORT_SELECT_ALL",
             IDS_FILE_BROWSER_PHOTO_IMPORT_SELECT_ALL);
  SET_STRING("PHOTO_IMPORT_SELECT_NONE",
             IDS_FILE_BROWSER_PHOTO_IMPORT_SELECT_NONE);
  SET_STRING("PHOTO_IMPORT_DELETE_AFTER",
             IDS_FILE_BROWSER_PHOTO_IMPORT_DELETE_AFTER);
  SET_STRING("PHOTO_IMPORT_MY_PHOTOS_DIRECTORY_NAME",
             IDS_FILE_BROWSER_PHOTO_IMPORT_MY_PHOTOS_DIRECTORY_NAME);

  SET_STRING("CONFIRM_OVERWRITE_FILE", IDS_FILE_BROWSER_CONFIRM_OVERWRITE_FILE);
  SET_STRING("FILE_ALREADY_EXISTS", IDS_FILE_BROWSER_FILE_ALREADY_EXISTS);
  SET_STRING("DIRECTORY_ALREADY_EXISTS",
             IDS_FILE_BROWSER_DIRECTORY_ALREADY_EXISTS);
  SET_STRING("ERROR_RENAMING", IDS_FILE_BROWSER_ERROR_RENAMING);
  SET_STRING("RENAME_PROMPT", IDS_FILE_BROWSER_RENAME_PROMPT);
  SET_STRING("RENAME_BUTTON_LABEL", IDS_FILE_BROWSER_RENAME_BUTTON_LABEL);

  SET_STRING("ERROR_DELETING", IDS_FILE_BROWSER_ERROR_DELETING);
  SET_STRING("DELETE_BUTTON_LABEL", IDS_FILE_BROWSER_DELETE_BUTTON_LABEL);

  SET_STRING("PASTE_BUTTON_LABEL", IDS_FILE_BROWSER_PASTE_BUTTON_LABEL);

  SET_STRING("COPY_BUTTON_LABEL", IDS_FILE_BROWSER_COPY_BUTTON_LABEL);
  SET_STRING("CUT_BUTTON_LABEL", IDS_FILE_BROWSER_CUT_BUTTON_LABEL);
  SET_STRING("ZIP_SELECTION_BUTTON_LABEL",
             IDS_FILE_BROWSER_ZIP_SELECTION_BUTTON_LABEL);
  SET_STRING("CREATE_FOLDER_SHORTCUT_BUTTON_LABEL",
             IDS_FILE_BROWSER_CREATE_FOLDER_SHORTCUT_BUTTON_LABEL);
  SET_STRING("REMOVE_FOLDER_SHORTCUT_BUTTON_LABEL",
             IDS_FILE_BROWSER_REMOVE_FOLDER_SHORTCUT_BUTTON_LABEL);
  SET_STRING("SHARE_BUTTON_LABEL",
             IDS_FILE_BROWSER_SHARE_BUTTON_LABEL);

  SET_STRING("OPEN_WITH_BUTTON_LABEL", IDS_FILE_BROWSER_OPEN_WITH_BUTTON_LABEL);

  SET_STRING("TRANSFER_ITEMS_REMAINING",
             IDS_FILE_BROWSER_TRANSFER_ITEMS_REMAINING);
  SET_STRING("TRANSFER_CANCELLED", IDS_FILE_BROWSER_TRANSFER_CANCELLED);
  SET_STRING("TRANSFER_TARGET_EXISTS_ERROR",
             IDS_FILE_BROWSER_TRANSFER_TARGET_EXISTS_ERROR);
  SET_STRING("TRANSFER_FILESYSTEM_ERROR",
             IDS_FILE_BROWSER_TRANSFER_FILESYSTEM_ERROR);
  SET_STRING("TRANSFER_UNEXPECTED_ERROR",
             IDS_FILE_BROWSER_TRANSFER_UNEXPECTED_ERROR);
  SET_STRING("COPY_FILE_NAME", IDS_FILE_BROWSER_COPY_FILE_NAME);
  SET_STRING("COPY_ITEMS_REMAINING", IDS_FILE_BROWSER_COPY_ITEMS_REMAINING);
  SET_STRING("COPY_CANCELLED", IDS_FILE_BROWSER_COPY_CANCELLED);
  SET_STRING("COPY_TARGET_EXISTS_ERROR",
             IDS_FILE_BROWSER_COPY_TARGET_EXISTS_ERROR);
  SET_STRING("COPY_FILESYSTEM_ERROR", IDS_FILE_BROWSER_COPY_FILESYSTEM_ERROR);
  SET_STRING("COPY_UNEXPECTED_ERROR", IDS_FILE_BROWSER_COPY_UNEXPECTED_ERROR);
  SET_STRING("MOVE_FILE_NAME", IDS_FILE_BROWSER_MOVE_FILE_NAME);
  SET_STRING("MOVE_ITEMS_REMAINING", IDS_FILE_BROWSER_MOVE_ITEMS_REMAINING);
  SET_STRING("MOVE_CANCELLED", IDS_FILE_BROWSER_MOVE_CANCELLED);
  SET_STRING("MOVE_TARGET_EXISTS_ERROR",
             IDS_FILE_BROWSER_MOVE_TARGET_EXISTS_ERROR);
  SET_STRING("MOVE_FILESYSTEM_ERROR", IDS_FILE_BROWSER_MOVE_FILESYSTEM_ERROR);
  SET_STRING("MOVE_UNEXPECTED_ERROR", IDS_FILE_BROWSER_MOVE_UNEXPECTED_ERROR);
  SET_STRING("ZIP_FILE_NAME", IDS_FILE_BROWSER_ZIP_FILE_NAME);
  SET_STRING("ZIP_ITEMS_REMAINING", IDS_FILE_BROWSER_ZIP_ITEMS_REMAINING);
  SET_STRING("ZIP_CANCELLED", IDS_FILE_BROWSER_ZIP_CANCELLED);
  SET_STRING("ZIP_TARGET_EXISTS_ERROR",
             IDS_FILE_BROWSER_ZIP_TARGET_EXISTS_ERROR);
  SET_STRING("ZIP_FILESYSTEM_ERROR", IDS_FILE_BROWSER_ZIP_FILESYSTEM_ERROR);
  SET_STRING("ZIP_UNEXPECTED_ERROR", IDS_FILE_BROWSER_ZIP_UNEXPECTED_ERROR);
  SET_STRING("SHARE_ERROR", IDS_FILE_BROWSER_SHARE_ERROR);

  SET_STRING("DELETED_MESSAGE_PLURAL", IDS_FILE_BROWSER_DELETED_MESSAGE_PLURAL);
  SET_STRING("DELETED_MESSAGE", IDS_FILE_BROWSER_DELETED_MESSAGE);
  SET_STRING("DELETE_ERROR", IDS_FILE_BROWSER_DELETE_ERROR);
  SET_STRING("UNDO_DELETE", IDS_FILE_BROWSER_UNDO_DELETE);

  SET_STRING("CANCEL_LABEL", IDS_FILE_BROWSER_CANCEL_LABEL);
  SET_STRING("OPEN_LABEL", IDS_FILE_BROWSER_OPEN_LABEL);
  SET_STRING("SAVE_LABEL", IDS_FILE_BROWSER_SAVE_LABEL);
  SET_STRING("OK_LABEL", IDS_FILE_BROWSER_OK_LABEL);
  SET_STRING("VIEW_LABEL", IDS_FILE_BROWSER_VIEW_LABEL);

  SET_STRING("DEFAULT_NEW_FOLDER_NAME",
             IDS_FILE_BROWSER_DEFAULT_NEW_FOLDER_NAME);
  SET_STRING("MORE_FILES", IDS_FILE_BROWSER_MORE_FILES);

  SET_STRING("CONFIRM_DELETE_ONE", IDS_FILE_BROWSER_CONFIRM_DELETE_ONE);
  SET_STRING("CONFIRM_DELETE_SOME", IDS_FILE_BROWSER_CONFIRM_DELETE_SOME);

  SET_STRING("UNKNOWN_FILESYSTEM_WARNING",
             IDS_FILE_BROWSER_UNKNOWN_FILESYSTEM_WARNING);
  SET_STRING("UNSUPPORTED_FILESYSTEM_WARNING",
             IDS_FILE_BROWSER_UNSUPPORTED_FILESYSTEM_WARNING);
  SET_STRING("FORMATTING_WARNING", IDS_FILE_BROWSER_FORMATTING_WARNING);

  SET_STRING("DRIVE_MENU_HELP", IDS_FILE_BROWSER_DRIVE_MENU_HELP);
  SET_STRING("DRIVE_SHOW_HOSTED_FILES_OPTION",
             IDS_FILE_BROWSER_DRIVE_SHOW_HOSTED_FILES_OPTION);
  SET_STRING("DRIVE_MOBILE_CONNECTION_OPTION",
             IDS_FILE_BROWSER_DRIVE_MOBILE_CONNECTION_OPTION);
  SET_STRING("DRIVE_CLEAR_LOCAL_CACHE",
             IDS_FILE_BROWSER_DRIVE_CLEAR_LOCAL_CACHE);
  SET_STRING("DRIVE_SPACE_AVAILABLE_LONG",
             IDS_FILE_BROWSER_DRIVE_SPACE_AVAILABLE_LONG);
  SET_STRING("DRIVE_BUY_MORE_SPACE", IDS_FILE_BROWSER_DRIVE_BUY_MORE_SPACE);
  SET_STRING("DRIVE_BUY_MORE_SPACE_LINK",
             IDS_FILE_BROWSER_DRIVE_BUY_MORE_SPACE_LINK);
  SET_STRING("DRIVE_VISIT_DRIVE_GOOGLE_COM",
             IDS_FILE_BROWSER_DRIVE_VISIT_DRIVE_GOOGLE_COM);

  SET_STRING("SELECT_FOLDER_TITLE", IDS_FILE_BROWSER_SELECT_FOLDER_TITLE);
  SET_STRING("SELECT_OPEN_FILE_TITLE", IDS_FILE_BROWSER_SELECT_OPEN_FILE_TITLE);
  SET_STRING("SELECT_OPEN_MULTI_FILE_TITLE",
             IDS_FILE_BROWSER_SELECT_OPEN_MULTI_FILE_TITLE);
  SET_STRING("SELECT_SAVEAS_FILE_TITLE",
             IDS_FILE_BROWSER_SELECT_SAVEAS_FILE_TITLE);

  SET_STRING("MANY_FILES_SELECTED", IDS_FILE_BROWSER_MANY_FILES_SELECTED);
  SET_STRING("MANY_DIRECTORIES_SELECTED",
             IDS_FILE_BROWSER_MANY_DIRECTORIES_SELECTED);
  SET_STRING("MANY_ENTRIES_SELECTED", IDS_FILE_BROWSER_MANY_ENTRIES_SELECTED);
  SET_STRING("CALCULATING_SIZE", IDS_FILE_BROWSER_CALCULATING_SIZE);

  SET_STRING("OFFLINE_HEADER", IDS_FILE_BROWSER_OFFLINE_HEADER);
  SET_STRING("OFFLINE_MESSAGE", IDS_FILE_BROWSER_OFFLINE_MESSAGE);
  SET_STRING("OFFLINE_MESSAGE_PLURAL", IDS_FILE_BROWSER_OFFLINE_MESSAGE_PLURAL);
  SET_STRING("HOSTED_OFFLINE_MESSAGE", IDS_FILE_BROWSER_HOSTED_OFFLINE_MESSAGE);
  SET_STRING("HOSTED_OFFLINE_MESSAGE_PLURAL",
             IDS_FILE_BROWSER_HOSTED_OFFLINE_MESSAGE_PLURAL);
  SET_STRING("CONFIRM_MOBILE_DATA_USE",
             IDS_FILE_BROWSER_CONFIRM_MOBILE_DATA_USE);
  SET_STRING("CONFIRM_MOBILE_DATA_USE_PLURAL",
             IDS_FILE_BROWSER_CONFIRM_MOBILE_DATA_USE_PLURAL);
  SET_STRING("DRIVE_OUT_OF_SPACE_HEADER",
             IDS_FILE_BROWSER_DRIVE_OUT_OF_SPACE_HEADER);
  SET_STRING("DRIVE_OUT_OF_SPACE_MESSAGE",
             IDS_FILE_BROWSER_DRIVE_OUT_OF_SPACE_MESSAGE);
  SET_STRING("DRIVE_SERVER_OUT_OF_SPACE_HEADER",
             IDS_FILE_BROWSER_DRIVE_SERVER_OUT_OF_SPACE_HEADER);
  SET_STRING("DRIVE_SERVER_OUT_OF_SPACE_MESSAGE",
             IDS_FILE_BROWSER_DRIVE_SERVER_OUT_OF_SPACE_MESSAGE);
  SET_STRING("DRIVE_WELCOME_TITLE", IDS_FILE_BROWSER_DRIVE_WELCOME_TITLE);
  SET_STRING("DRIVE_WELCOME_TEXT_SHORT",
             IDS_FILE_BROWSER_DRIVE_WELCOME_TEXT_SHORT);
  SET_STRING("DRIVE_WELCOME_TEXT_LONG",
             IDS_FILE_BROWSER_DRIVE_WELCOME_TEXT_LONG);
  SET_STRING("DRIVE_WELCOME_DISMISS", IDS_FILE_BROWSER_DRIVE_WELCOME_DISMISS);
  SET_STRING("DRIVE_WELCOME_TITLE_ALTERNATIVE",
             IDS_FILE_BROWSER_DRIVE_WELCOME_TITLE_ALTERNATIVE);
  SET_STRING("DRIVE_WELCOME_TITLE_ALTERNATIVE_1TB",
             IDS_FILE_BROWSER_DRIVE_WELCOME_TITLE_ALTERNATIVE_1TB);
  SET_STRING("DRIVE_WELCOME_CHECK_ELIGIBILITY",
             IDS_FILE_BROWSER_DRIVE_WELCOME_CHECK_ELIGIBILITY);
  SET_STRING("NO_ACTION_FOR_FILE", IDS_FILE_BROWSER_NO_ACTION_FOR_FILE);
  SET_STRING("NO_ACTION_FOR_EXECUTABLE",
             IDS_FILE_BROWSER_NO_ACTION_FOR_EXECUTABLE);

  // MP3 metadata extractor plugin
  SET_STRING("ID3_ALBUM", IDS_FILE_BROWSER_ID3_ALBUM);                // TALB
  SET_STRING("ID3_BPM", IDS_FILE_BROWSER_ID3_BPM);                    // TBPM
  SET_STRING("ID3_COMPOSER", IDS_FILE_BROWSER_ID3_COMPOSER);          // TCOM
  SET_STRING("ID3_COPYRIGHT_MESSAGE",
             IDS_FILE_BROWSER_ID3_COPYRIGHT_MESSAGE);                 // TCOP
  SET_STRING("ID3_DATE", IDS_FILE_BROWSER_ID3_DATE);                  // TDAT
  SET_STRING("ID3_PLAYLIST_DELAY",
             IDS_FILE_BROWSER_ID3_PLAYLIST_DELAY);                    // TDLY
  SET_STRING("ID3_ENCODED_BY", IDS_FILE_BROWSER_ID3_ENCODED_BY);      // TENC
  SET_STRING("ID3_LYRICIST", IDS_FILE_BROWSER_ID3_LYRICIST);          // TEXT
  SET_STRING("ID3_FILE_TYPE", IDS_FILE_BROWSER_ID3_FILE_TYPE);        // TFLT
  SET_STRING("ID3_TIME", IDS_FILE_BROWSER_ID3_TIME);                  // TIME
  SET_STRING("ID3_TITLE", IDS_FILE_BROWSER_ID3_TITLE);                // TIT2
  SET_STRING("ID3_LENGTH", IDS_FILE_BROWSER_ID3_LENGTH);              // TLEN
  SET_STRING("ID3_FILE_OWNER", IDS_FILE_BROWSER_ID3_FILE_OWNER);      // TOWN
  SET_STRING("ID3_LEAD_PERFORMER",
             IDS_FILE_BROWSER_ID3_LEAD_PERFORMER);                    // TPE1
  SET_STRING("ID3_BAND", IDS_FILE_BROWSER_ID3_BAND);                  // TPE2
  SET_STRING("ID3_TRACK_NUMBER", IDS_FILE_BROWSER_ID3_TRACK_NUMBER);  // TRCK
  SET_STRING("ID3_YEAR", IDS_FILE_BROWSER_ID3_YEAR);                  // TYER
  SET_STRING("ID3_COPYRIGHT", IDS_FILE_BROWSER_ID3_COPYRIGHT);        // WCOP
  SET_STRING("ID3_OFFICIAL_AUDIO_FILE_WEBPAGE",
             IDS_FILE_BROWSER_ID3_OFFICIAL_AUDIO_FILE_WEBPAGE);       // WOAF
  SET_STRING("ID3_OFFICIAL_ARTIST",
             IDS_FILE_BROWSER_ID3_OFFICIAL_ARTIST);                   // WOAR
  SET_STRING("ID3_OFFICIAL_AUDIO_SOURCE_WEBPAGE",
             IDS_FILE_BROWSER_ID3_OFFICIAL_AUDIO_SOURCE_WEBPAGE);     // WOAS
  SET_STRING("ID3_PUBLISHERS_OFFICIAL_WEBPAGE",
             IDS_FILE_BROWSER_ID3_PUBLISHERS_OFFICIAL_WEBPAGE);       // WPUB
  SET_STRING("ID3_USER_DEFINED_URL_LINK_FRAME",
             IDS_FILE_BROWSER_ID3_USER_DEFINED_URL_LINK_FRAME);       // WXXX

  // File types
  SET_STRING("FOLDER", IDS_FILE_BROWSER_FOLDER);
  SET_STRING("GENERIC_FILE_TYPE", IDS_FILE_BROWSER_GENERIC_FILE_TYPE);
  SET_STRING("NO_EXTENSION_FILE_TYPE", IDS_FILE_BROWSER_NO_EXTENSION_FILE_TYPE);
  SET_STRING("DEVICE", IDS_FILE_BROWSER_DEVICE);
  SET_STRING("IMAGE_FILE_TYPE", IDS_FILE_BROWSER_IMAGE_FILE_TYPE);
  SET_STRING("VIDEO_FILE_TYPE", IDS_FILE_BROWSER_VIDEO_FILE_TYPE);
  SET_STRING("AUDIO_FILE_TYPE", IDS_FILE_BROWSER_AUDIO_FILE_TYPE);
  SET_STRING("HTML_DOCUMENT_FILE_TYPE",
             IDS_FILE_BROWSER_HTML_DOCUMENT_FILE_TYPE);
  SET_STRING("ZIP_ARCHIVE_FILE_TYPE", IDS_FILE_BROWSER_ZIP_ARCHIVE_FILE_TYPE);
  SET_STRING("RAR_ARCHIVE_FILE_TYPE", IDS_FILE_BROWSER_RAR_ARCHIVE_FILE_TYPE);
  SET_STRING("TAR_ARCHIVE_FILE_TYPE", IDS_FILE_BROWSER_TAR_ARCHIVE_FILE_TYPE);
  SET_STRING("TAR_BZIP2_ARCHIVE_FILE_TYPE",
             IDS_FILE_BROWSER_TAR_BZIP2_ARCHIVE_FILE_TYPE);
  SET_STRING("TAR_GZIP_ARCHIVE_FILE_TYPE",
             IDS_FILE_BROWSER_TAR_GZIP_ARCHIVE_FILE_TYPE);
  SET_STRING("PLAIN_TEXT_FILE_TYPE", IDS_FILE_BROWSER_PLAIN_TEXT_FILE_TYPE);
  SET_STRING("PDF_DOCUMENT_FILE_TYPE", IDS_FILE_BROWSER_PDF_DOCUMENT_FILE_TYPE);
  SET_STRING("WORD_DOCUMENT_FILE_TYPE",
             IDS_FILE_BROWSER_WORD_DOCUMENT_FILE_TYPE);
  SET_STRING("POWERPOINT_PRESENTATION_FILE_TYPE",
             IDS_FILE_BROWSER_POWERPOINT_PRESENTATION_FILE_TYPE);
  SET_STRING("EXCEL_FILE_TYPE", IDS_FILE_BROWSER_EXCEL_FILE_TYPE);

  SET_STRING("GDOC_DOCUMENT_FILE_TYPE",
             IDS_FILE_BROWSER_GDOC_DOCUMENT_FILE_TYPE);
  SET_STRING("GSHEET_DOCUMENT_FILE_TYPE",
             IDS_FILE_BROWSER_GSHEET_DOCUMENT_FILE_TYPE);
  SET_STRING("GSLIDES_DOCUMENT_FILE_TYPE",
             IDS_FILE_BROWSER_GSLIDES_DOCUMENT_FILE_TYPE);
  SET_STRING("GDRAW_DOCUMENT_FILE_TYPE",
             IDS_FILE_BROWSER_GDRAW_DOCUMENT_FILE_TYPE);
  SET_STRING("GTABLE_DOCUMENT_FILE_TYPE",
             IDS_FILE_BROWSER_GTABLE_DOCUMENT_FILE_TYPE);
  SET_STRING("GLINK_DOCUMENT_FILE_TYPE",
             IDS_FILE_BROWSER_GLINK_DOCUMENT_FILE_TYPE);

  SET_STRING("DRIVE_LOADING", IDS_FILE_BROWSER_DRIVE_LOADING);
  SET_STRING("DRIVE_CANNOT_REACH", IDS_FILE_BROWSER_DRIVE_CANNOT_REACH);
  SET_STRING("DRIVE_LEARN_MORE", IDS_FILE_BROWSER_DRIVE_LEARN_MORE);
  SET_STRING("DRIVE_RETRY", IDS_FILE_BROWSER_DRIVE_RETRY);

  SET_STRING("AUDIO_PLAYER_TITLE", IDS_FILE_BROWSER_AUDIO_PLAYER_TITLE);
  SET_STRING("AUDIO_PLAYER_DEFAULT_ARTIST",
             IDS_FILE_BROWSER_AUDIO_PLAYER_DEFAULT_ARTIST);

  SET_STRING("FILE_ERROR_GENERIC", IDS_FILE_BROWSER_FILE_ERROR_GENERIC);
  SET_STRING("FILE_ERROR_NOT_FOUND", IDS_FILE_BROWSER_FILE_ERROR_NOT_FOUND);
  SET_STRING("FILE_ERROR_SECURITY", IDS_FILE_BROWSER_FILE_ERROR_SECURITY);
  SET_STRING("FILE_ERROR_NOT_READABLE",
             IDS_FILE_BROWSER_FILE_ERROR_NOT_READABLE);
  SET_STRING("FILE_ERROR_NO_MODIFICATION_ALLOWED",
             IDS_FILE_BROWSER_FILE_ERROR_NO_MODIFICATION_ALLOWED);
  SET_STRING("FILE_ERROR_INVALID_STATE",
             IDS_FILE_BROWSER_FILE_ERROR_INVALID_STATE);
  SET_STRING("FILE_ERROR_INVALID_MODIFICATION",
             IDS_FILE_BROWSER_FILE_ERROR_INVALID_MODIFICATION);
  SET_STRING("FILE_ERROR_PATH_EXISTS", IDS_FILE_BROWSER_FILE_ERROR_PATH_EXISTS);
  SET_STRING("FILE_ERROR_QUOTA_EXCEEDED",
             IDS_FILE_BROWSER_FILE_ERROR_QUOTA_EXCEEDED);

  SET_STRING("SEARCH_DRIVE_HTML", IDS_FILE_BROWSER_SEARCH_DRIVE_HTML);
  SET_STRING("SEARCH_NO_MATCHING_FILES_HTML",
             IDS_FILE_BROWSER_SEARCH_NO_MATCHING_FILES_HTML);
  SET_STRING("SEARCH_EXPAND", IDS_FILE_BROWSER_SEARCH_EXPAND);
  SET_STRING("SEARCH_SPINNER", IDS_FILE_BROWSER_SEARCH_SPINNER);

  SET_STRING("CHANGE_DEFAULT_MENU_ITEM",
             IDS_FILE_BROWSER_CHANGE_DEFAULT_MENU_ITEM);
  SET_STRING("CHANGE_DEFAULT_CAPTION", IDS_FILE_BROWSER_CHANGE_DEFAULT_CAPTION);
  SET_STRING("DEFAULT_ACTION_LABEL", IDS_FILE_BROWSER_DEFAULT_ACTION_LABEL);

  SET_STRING("DETAIL_VIEW_TOOLTIP", IDS_FILE_BROWSER_DETAIL_VIEW_TOOLTIP);
  SET_STRING("THUMBNAIL_VIEW_TOOLTIP", IDS_FILE_BROWSER_THUMBNAIL_VIEW_TOOLTIP);

  SET_STRING("TIME_TODAY", IDS_FILE_BROWSER_TIME_TODAY);
  SET_STRING("TIME_YESTERDAY", IDS_FILE_BROWSER_TIME_YESTERDAY);

  SET_STRING("ALL_FILES_FILTER", IDS_FILE_BROWSER_ALL_FILES_FILTER);

  SET_STRING("SPACE_AVAILABLE", IDS_FILE_BROWSER_SPACE_AVAILABLE);
  SET_STRING("WAITING_FOR_SPACE_INFO", IDS_FILE_BROWSER_WAITING_FOR_SPACE_INFO);
  SET_STRING("FAILED_SPACE_INFO", IDS_FILE_BROWSER_FAILED_SPACE_INFO);

  SET_STRING("DRIVE_NOT_REACHED", IDS_FILE_BROWSER_DRIVE_NOT_REACHED);

  SET_STRING("HELP_LINK_LABEL", IDS_FILE_BROWSER_HELP_LINK_LABEL);
#undef SET_STRING

  dict->SetBoolean("PDF_VIEW_ENABLED",
      file_manager_util::ShouldBeOpenedWithPlugin(profile(), ".pdf"));
  dict->SetBoolean("SWF_VIEW_ENABLED",
      file_manager_util::ShouldBeOpenedWithPlugin(profile(), ".swf"));

  webui::SetFontAndTextDirection(dict);

  std::string board;
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  if (!provider->GetMachineStatistic(chromeos::system::kMachineInfoBoard,
                                     &board)) {
    board = "unknown";
  }
  dict->SetString(chromeos::system::kMachineInfoBoard, board);
  return true;
}

GetPreferencesFunction::GetPreferencesFunction() {
}

GetPreferencesFunction::~GetPreferencesFunction() {
}

bool GetPreferencesFunction::RunImpl() {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());

  const PrefService* service = profile_->GetPrefs();

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
  bool drive_enabled = (integration_service != NULL);

  if (drive_enabled)
    SetDriveMountPointPermissions(profile_, extension_id(), render_view_host());

  value->SetBoolean("driveEnabled", drive_enabled);

  value->SetBoolean("cellularDisabled",
                    service->GetBoolean(prefs::kDisableDriveOverCellular));

  value->SetBoolean("hostedFilesDisabled",
                    service->GetBoolean(prefs::kDisableDriveHostedFiles));

  value->SetBoolean("use24hourClock",
                    service->GetBoolean(prefs::kUse24HourClock));

  {
    bool allow = true;
    if (!chromeos::CrosSettings::Get()->GetBoolean(
            chromeos::kAllowRedeemChromeOsRegistrationOffers, &allow)) {
      allow = true;
    }
    value->SetBoolean("allowRedeemOffers", allow);
  }

  SetResult(value.release());

  drive::util::Log("%s succeeded.", name().c_str());
  return true;
}

SetPreferencesFunction::SetPreferencesFunction() {
}

SetPreferencesFunction::~SetPreferencesFunction() {
}

bool SetPreferencesFunction::RunImpl() {
  base::DictionaryValue* value = NULL;

  if (!args_->GetDictionary(0, &value) || !value)
    return false;

  PrefService* service = profile_->GetPrefs();

  bool tmp;

  if (value->GetBoolean("cellularDisabled", &tmp))
    service->SetBoolean(prefs::kDisableDriveOverCellular, tmp);

  if (value->GetBoolean("hostedFilesDisabled", &tmp))
    service->SetBoolean(prefs::kDisableDriveHostedFiles, tmp);

  drive::util::Log("%s succeeded.", name().c_str());
  return true;
}

ZipSelectionFunction::ZipSelectionFunction() {
}

ZipSelectionFunction::~ZipSelectionFunction() {
}

bool ZipSelectionFunction::RunImpl() {
  if (args_->GetSize() < 3) {
    return false;
  }

  // First param is the source directory URL.
  std::string dir_url;
  if (!args_->GetString(0, &dir_url) || dir_url.empty())
    return false;

  base::FilePath src_dir = GetLocalPathFromURL(
      render_view_host(), profile(), GURL(dir_url));
  if (src_dir.empty())
    return false;

  // Second param is the list of selected file URLs.
  ListValue* selection_urls = NULL;
  args_->GetList(1, &selection_urls);
  if (!selection_urls || !selection_urls->GetSize())
    return false;

  std::vector<base::FilePath> files;
  for (size_t i = 0; i < selection_urls->GetSize(); ++i) {
    std::string file_url;
    selection_urls->GetString(i, &file_url);
    base::FilePath path = GetLocalPathFromURL(
        render_view_host(), profile(), GURL(file_url));
    if (path.empty())
      return false;
    files.push_back(path);
  }

  // Third param is the name of the output zip file.
  std::string dest_name;
  if (!args_->GetString(2, &dest_name) || dest_name.empty())
    return false;

  // Check if the dir path is under Drive mount point.
  // TODO(hshi): support create zip file on Drive (crbug.com/158690).
  if (drive::util::IsUnderDriveMountPoint(src_dir))
    return false;

  base::FilePath dest_file = src_dir.Append(dest_name);
  std::vector<base::FilePath> src_relative_paths;
  for (size_t i = 0; i != files.size(); ++i) {
    const base::FilePath& file_path = files[i];

    // Obtain the relative path of |file_path| under |src_dir|.
    base::FilePath relative_path;
    if (!src_dir.AppendRelativePath(file_path, &relative_path))
      return false;
    src_relative_paths.push_back(relative_path);
  }

  zip_file_creator_ = new ZipFileCreator(this, src_dir, src_relative_paths,
                                         dest_file);

  // Keep the refcount until the zipping is complete on utility process.
  AddRef();

  zip_file_creator_->Start();
  return true;
}

void ZipSelectionFunction::OnZipDone(bool success) {
  SetResult(new base::FundamentalValue(success));
  SendResponse(true);
  Release();
}

ValidatePathNameLengthFunction::ValidatePathNameLengthFunction() {
}

ValidatePathNameLengthFunction::~ValidatePathNameLengthFunction() {
}

bool ValidatePathNameLengthFunction::RunImpl() {
  std::string parent_url;
  if (!args_->GetString(0, &parent_url))
    return false;

  std::string name;
  if (!args_->GetString(1, &name))
    return false;

  content::SiteInstance* site_instance = render_view_host()->GetSiteInstance();
  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      BrowserContext::GetStoragePartition(profile(), site_instance)->
          GetFileSystemContext();
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
      base::Bind(&ValidatePathNameLengthFunction::OnFilePathLimitRetrieved,
                 this, name.size()));
  return true;
}

void ValidatePathNameLengthFunction::OnFilePathLimitRetrieved(
    size_t current_length,
    size_t max_length) {
  SetResult(new base::FundamentalValue(current_length <= max_length));
  SendResponse(true);
}

ZoomFunction::ZoomFunction() {
}

ZoomFunction::~ZoomFunction() {
}

bool ZoomFunction::RunImpl() {
  content::RenderViewHost* const view_host = render_view_host();
  std::string operation;
  args_->GetString(0, &operation);
  content::PageZoom zoom_type;
  if (operation == "in") {
    zoom_type = content::PAGE_ZOOM_IN;
  } else if (operation == "out") {
    zoom_type = content::PAGE_ZOOM_OUT;
  } else if (operation == "reset") {
    zoom_type = content::PAGE_ZOOM_RESET;
  } else {
    NOTREACHED();
    return false;
  }
  view_host->Zoom(zoom_type);
  return true;
}

}  // namespace file_manager
