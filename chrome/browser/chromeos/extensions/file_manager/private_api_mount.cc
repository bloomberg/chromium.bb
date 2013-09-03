// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_mount.h"

#include "base/format_macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/logging.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api.h"
#include "chrome/browser/chromeos/extensions/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/volume_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/shell_dialogs/selected_file_info.h"

using chromeos::disks::DiskMountManager;
using content::BrowserThread;

namespace extensions {
namespace {

// Returns string representaion of VolumeType.
std::string VolumeTypeToString(file_manager::VolumeType type) {
  switch (type) {
    case file_manager::VOLUME_TYPE_GOOGLE_DRIVE:
      return "drive";
    case file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY:
      // Return empty string here for backword compatibility.
      // TODO(hidehiko): Fix this to return "downloads".
      return "";
    case file_manager::VOLUME_TYPE_REMOVABLE_DISK_PARTITION:
      return "device";
    case file_manager::VOLUME_TYPE_MOUNTED_ARCHIVE_FILE:
      return "file";
  }

  NOTREACHED();
  return "";
}

// Returns the Value of the |volume_info|.
base::Value* CreateValueFromVolumeInfo(
    const file_manager::VolumeInfo& volume_info,
    Profile* profile,
    const std::string& extension_id) {
  base::DictionaryValue* result = new base::DictionaryValue;
  std::string mount_type = VolumeTypeToString(volume_info.type);
  if (!mount_type.empty())
    result->SetString("mountType", mount_type);

  if (!volume_info.source_path.empty())
    result->SetString("sourcePath", volume_info.source_path.value());

  // Convert mount point path to relative path with the external file system
  // exposed within File API.
  base::FilePath relative_path;
  if (file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
          profile, extension_id, volume_info.mount_path, &relative_path))
    result->SetString("mountPath", relative_path.value());

  result->SetString(
      "mountCondition",
      DiskMountManager::MountConditionToString(volume_info.mount_condition));
  return result;
}

}  // namespace

FileBrowserPrivateAddMountFunction::FileBrowserPrivateAddMountFunction() {
}

FileBrowserPrivateAddMountFunction::~FileBrowserPrivateAddMountFunction() {
}

bool FileBrowserPrivateAddMountFunction::RunImpl() {
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

  drive::util::Log(logging::LOG_INFO,
                   "%s[%d] called. (source: '%s', type:'%s')",
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
      file_manager::FileBrowserPrivateAPI::Get(profile_)->event_router()->
          OnFileSystemMounted();

      // Pass back the drive mount point path as source path.
      const std::string& drive_path =
          drive::util::GetDriveMountPointPathAsString();
      SetResult(new base::StringValue(drive_path));
      SendResponse(true);
      break;
    }
    default: {
      const base::FilePath path = file_manager::util::GetLocalPathFromURL(
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
            base::Bind(&FileBrowserPrivateAddMountFunction::OnMountedStateSet,
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

void FileBrowserPrivateAddMountFunction::OnMountedStateSet(
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

FileBrowserPrivateRemoveMountFunction::FileBrowserPrivateRemoveMountFunction() {
}

FileBrowserPrivateRemoveMountFunction::
    ~FileBrowserPrivateRemoveMountFunction() {
}

bool FileBrowserPrivateRemoveMountFunction::RunImpl() {
  if (args_->GetSize() != 1) {
    return false;
  }

  std::string mount_path;
  if (!args_->GetString(0, &mount_path)) {
    return false;
  }

  drive::util::Log(logging::LOG_INFO,
                   "%s[%d] called. (mount_path: '%s')",
                   name().c_str(),
                   request_id(),
                   mount_path.c_str());
  set_log_on_completion(true);

  std::vector<GURL> file_paths;
  file_paths.push_back(GURL(mount_path));
  file_manager::util::GetSelectedFileInfo(
      render_view_host(),
      profile(),
      file_paths,
      file_manager::util::NEED_LOCAL_PATH_FOR_OPENING,
      base::Bind(&FileBrowserPrivateRemoveMountFunction::
                     GetSelectedFileInfoResponse, this));
  return true;
}

void FileBrowserPrivateRemoveMountFunction::GetSelectedFileInfoResponse(
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

FileBrowserPrivateGetMountPointsFunction::
    FileBrowserPrivateGetMountPointsFunction() {
}

FileBrowserPrivateGetMountPointsFunction::
    ~FileBrowserPrivateGetMountPointsFunction() {
}

bool FileBrowserPrivateGetMountPointsFunction::RunImpl() {
  if (args_->GetSize())
    return false;

  const std::vector<file_manager::VolumeInfo>& volume_info_list =
      file_manager::VolumeManager::Get(profile_)->GetVolumeInfoList();

  std::string log_string;
  base::ListValue* result = new base::ListValue();
  for (size_t i = 0; i < volume_info_list.size(); ++i) {
    result->Append(CreateValueFromVolumeInfo(
        volume_info_list[i], profile(), extension_id()));
    if (!log_string.empty())
      log_string += ", ";
    log_string += volume_info_list[i].mount_path.AsUTF8Unsafe();
  }

  drive::util::Log(
      logging::LOG_INFO,
      "%s[%d] succeeded. (results: '[%s]', %" PRIuS " mount points)",
      name().c_str(), request_id(), log_string.c_str(), result->GetSize());

  SetResult(result);
  SendResponse(true);
  return true;
}

}  // namespace extensions
