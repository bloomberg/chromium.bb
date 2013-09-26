// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_mount.h"

#include "base/format_macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/logging.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_browser_private_api.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_browser_private.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/shell_dialogs/selected_file_info.h"

using chromeos::disks::DiskMountManager;
using content::BrowserThread;
using extensions::api::file_browser_private::MountPointInfo;

namespace extensions {
namespace {

// Returns the Value of the |volume_info|.
linked_ptr<MountPointInfo> CreateValueFromVolumeInfo(
    const file_manager::VolumeInfo& volume_info,
    Profile* profile,
    const std::string& extension_id) {
  linked_ptr<MountPointInfo> result(new MountPointInfo);

  result->volume_type = MountPointInfo::VOLUME_TYPE_NONE;
  switch (volume_info.type) {
    case file_manager::VOLUME_TYPE_GOOGLE_DRIVE:
      result->volume_type = MountPointInfo::VOLUME_TYPE_DRIVE;
      break;
    case file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY:
      result->volume_type = MountPointInfo::VOLUME_TYPE_DOWNLOADS;
      break;
    case file_manager::VOLUME_TYPE_REMOVABLE_DISK_PARTITION:
      result->volume_type = MountPointInfo::VOLUME_TYPE_REMOVABLE;
      break;
    case file_manager::VOLUME_TYPE_MOUNTED_ARCHIVE_FILE:
      result->volume_type = MountPointInfo::VOLUME_TYPE_ARCHIVE;
      break;
  }
  DCHECK_NE(result->volume_type, MountPointInfo::VOLUME_TYPE_NONE);

  if (!volume_info.source_path.empty())
    result->source_path.reset(
        new std::string(volume_info.source_path.AsUTF8Unsafe()));

  // Convert mount point path to relative path with the external file system
  // exposed within File API.
  base::FilePath relative_path;
  if (file_manager::util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
          profile, extension_id, volume_info.mount_path, &relative_path))
    result->mount_path.reset(
        new std::string(relative_path.AsUTF8Unsafe()));

  result->mount_condition =
      DiskMountManager::MountConditionToString(volume_info.mount_condition);
  return result;
}

}  // namespace

bool FileBrowserPrivateAddMountFunction::RunImpl() {
  using extensions::api::file_browser_private::AddMount::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::util::Log(logging::LOG_INFO,
                   "%s[%d] called. (source: '%s', type:'%s')",
                   name().c_str(),
                   request_id(),
                   params->source.empty() ? "(none)" : params->source.c_str(),
                   Params::ToString(params->volume_type).c_str());
  set_log_on_completion(true);

  if (params->volume_type != Params::VOLUME_TYPE_ARCHIVE) {
    error_ = "Invalid mount type";
    return false;
  }

  const base::FilePath path = file_manager::util::GetLocalPathFromURL(
      render_view_host(), profile(), GURL(params->source));

  if (path.empty())
    return false;

  const base::FilePath::StringType display_name = path.BaseName().value();

  // Check if the source path is under Drive cache directory.
  if (drive::util::IsUnderDriveMountPoint(path)) {
    drive::FileSystemInterface* file_system =
        drive::util::GetFileSystemByProfile(profile());
    if (!file_system)
      return false;

    file_system->MarkCacheFileAsMounted(
        drive::util::ExtractDrivePath(path),
        base::Bind(&FileBrowserPrivateAddMountFunction::OnMountedStateSet,
                   this, display_name));
  } else {
    OnMountedStateSet(display_name, drive::FILE_ERROR_OK, path);
  }
  return true;
}

void FileBrowserPrivateAddMountFunction::OnMountedStateSet(
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
      file_name, chromeos::MOUNT_TYPE_ARCHIVE);
}

bool FileBrowserPrivateRemoveMountFunction::RunImpl() {
  using extensions::api::file_browser_private::RemoveMount::Params;
  const scoped_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  drive::util::Log(logging::LOG_INFO,
                   "%s[%d] called. (mount_path: '%s')",
                   name().c_str(),
                   request_id(),
                   params->mount_path.c_str());
  set_log_on_completion(true);

  std::vector<GURL> file_paths;
  file_paths.push_back(GURL(params->mount_path));
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

bool FileBrowserPrivateGetMountPointsFunction::RunImpl() {
  if (args_->GetSize())
    return false;

  const std::vector<file_manager::VolumeInfo>& volume_info_list =
      file_manager::VolumeManager::Get(profile_)->GetVolumeInfoList();

  std::string log_string;
  std::vector<linked_ptr<extensions::api::file_browser_private::
                         MountPointInfo> > result;
  for (size_t i = 0; i < volume_info_list.size(); ++i) {
    result.push_back(CreateValueFromVolumeInfo(
        volume_info_list[i], profile(), extension_id()));
    if (!log_string.empty())
      log_string += ", ";
    log_string += volume_info_list[i].mount_path.AsUTF8Unsafe();
  }

  drive::util::Log(
      logging::LOG_INFO,
      "%s[%d] succeeded. (results: '[%s]', %" PRIuS " mount points)",
      name().c_str(), request_id(), log_string.c_str(), result.size());

  results_ = extensions::api::file_browser_private::
      GetMountPoints::Results::Create(result);
  SendResponse(true);
  return true;
}

}  // namespace extensions
