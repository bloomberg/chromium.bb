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
#include "chrome/browser/chromeos/extensions/file_manager/file_manager_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/shell_dialogs/selected_file_info.h"

using chromeos::disks::DiskMountManager;
using content::BrowserThread;

namespace file_manager {
namespace {

// Creates a dictionary describing the mount point of |mount_point_info|.
base::DictionaryValue* CreateValueFromMountPoint(
    Profile* profile,
    const DiskMountManager::MountPointInfo& mount_point_info,
    const std::string& extension_id) {

  base::DictionaryValue *mount_info = new base::DictionaryValue();

  mount_info->SetString("mountType",
                        DiskMountManager::MountTypeToString(
                            mount_point_info.mount_type));
  mount_info->SetString("sourcePath", mount_point_info.source_path);

  base::FilePath relative_mount_path;
  // Convert mount point path to relative path with the external file system
  // exposed within File API.
  if (util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
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

}  // namespace

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
      const base::FilePath path = util::GetLocalPathFromURL(
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

  drive::util::Log(logging::LOG_INFO,
                   "%s[%d] called. (mount_path: '%s')",
                   name().c_str(),
                   request_id(),
                   mount_path.c_str());
  set_log_on_completion(true);

  std::vector<GURL> file_paths;
  file_paths.push_back(GURL(mount_path));
  util::GetSelectedFileInfo(
      render_view_host(),
      profile(),
      file_paths,
      util::NEED_LOCAL_PATH_FOR_OPENING,
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
    mounts->Append(CreateValueFromMountPoint(profile_,
                                             it->second,
                                             extension_->id()));
    log_string += separator + it->first;
    separator = ", ";
  }

  log_string += "]";

  drive::util::Log(logging::LOG_INFO,
                   "%s[%d] succeeded. (results: '%s', %" PRIuS " mount points)",
                   name().c_str(),
                   request_id(),
                   log_string.c_str(),
                   mount_points.size());

  SendResponse(true);
  return true;
}

}  // namespace file_manager
