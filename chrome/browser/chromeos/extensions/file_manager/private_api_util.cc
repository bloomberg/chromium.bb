// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"

#include <string>

#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/file_manager/snapshot_manager.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_browser_private.h"
#include "content/public/browser/child_process_security_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace file_browser_private = extensions::api::file_browser_private;

namespace file_manager {
namespace util {
namespace {

// The struct is used for GetSelectedFileInfo().
struct GetSelectedFileInfoParams {
  GetSelectedFileInfoLocalPathOption local_path_option;
  GetSelectedFileInfoCallback callback;
  std::vector<base::FilePath> file_paths;
  std::vector<ui::SelectedFileInfo> selected_files;
};

// The callback type for GetFileNativeLocalPathFor{Opening,Saving}. It receives
// the resolved local path when successful, and receives empty path for failure.
typedef base::Callback<void(const base::FilePath&)> LocalPathCallback;

// Converts a callback from Drive file system to LocalPathCallback.
void OnDriveGetFile(const base::FilePath& path,
                    const LocalPathCallback& callback,
                    drive::FileError error,
                    const base::FilePath& local_file_path,
                    scoped_ptr<drive::ResourceEntry> entry) {
  if (error != drive::FILE_ERROR_OK)
    DLOG(ERROR) << "Failed to get " << path.value() << " with: " << error;
  callback.Run(local_file_path);
}

// Gets a resolved local file path of a non native |path| for file opening.
void GetFileNativeLocalPathForOpening(Profile* profile,
                                      const base::FilePath& path,
                                      const LocalPathCallback& callback) {
  if (drive::util::IsUnderDriveMountPoint(path)) {
    drive::FileSystemInterface* file_system =
        drive::util::GetFileSystemByProfile(profile);
    if (!file_system) {
      DLOG(ERROR) << "Drive file selected while disabled: " << path.value();
      callback.Run(base::FilePath());
      return;
    }
    file_system->GetFile(drive::util::ExtractDrivePath(path),
                         base::Bind(&OnDriveGetFile, path, callback));
    return;
  }

  VolumeManager::Get(profile)->snapshot_manager()->CreateManagedSnapshot(
      path, callback);
}

// Gets a resolved local file path of a non native |path| for file saving.
void GetFileNativeLocalPathForSaving(Profile* profile,
                                     const base::FilePath& path,
                                     const LocalPathCallback& callback) {
  if (drive::util::IsUnderDriveMountPoint(path)) {
    drive::FileSystemInterface* file_system =
        drive::util::GetFileSystemByProfile(profile);
    if (!file_system) {
      DLOG(ERROR) << "Drive file selected while disabled: " << path.value();
      callback.Run(base::FilePath());
      return;
    }
    file_system->GetFileForSaving(drive::util::ExtractDrivePath(path),
                                  base::Bind(&OnDriveGetFile, path, callback));
    return;
  }

  // TODO(kinaba): For now, the only writable non-local volume is Drive.
  NOTREACHED();
  callback.Run(base::FilePath());
}

// Forward declarations of helper functions for GetSelectedFileInfo().
void ContinueGetSelectedFileInfo(Profile* profile,
                                 scoped_ptr<GetSelectedFileInfoParams> params,
                                 const base::FilePath& local_file_path);

// Part of GetSelectedFileInfo().
void GetSelectedFileInfoInternal(Profile* profile,
                                 scoped_ptr<GetSelectedFileInfoParams> params) {
  DCHECK(profile);

  for (size_t i = params->selected_files.size();
       i < params->file_paths.size(); ++i) {
    const base::FilePath& file_path = params->file_paths[i];

    if (file_manager::util::IsUnderNonNativeLocalPath(profile, file_path)) {
      // When the caller of the select file dialog wants local file paths, and
      // the selected path does not point to a native local path (e.g., Drive,
      // MTP, or provided file system), we should resolve the path.
      switch (params->local_path_option) {
        case NO_LOCAL_PATH_RESOLUTION:
          break;  // No special handling needed.
        case NEED_LOCAL_PATH_FOR_OPENING:
          GetFileNativeLocalPathForOpening(
              profile,
              file_path,
              base::Bind(&ContinueGetSelectedFileInfo,
                         profile,
                         base::Passed(&params)));
          return;  // Remaining work is done in ContinueGetSelectedFileInfo.
        case NEED_LOCAL_PATH_FOR_SAVING:
          GetFileNativeLocalPathForSaving(
              profile,
              file_path,
              base::Bind(&ContinueGetSelectedFileInfo,
                         profile,
                         base::Passed(&params)));
          return;  // Remaining work is done in ContinueGetSelectedFileInfo.
      }
    }
    params->selected_files.push_back(
        ui::SelectedFileInfo(file_path, base::FilePath()));
  }
  params->callback.Run(params->selected_files);
}

// Part of GetSelectedFileInfo().
void ContinueGetSelectedFileInfo(Profile* profile,
                                 scoped_ptr<GetSelectedFileInfoParams> params,
                                 const base::FilePath& local_path) {
  const int index = params->selected_files.size();
  const base::FilePath& file_path = params->file_paths[index];
  params->selected_files.push_back(ui::SelectedFileInfo(file_path, local_path));
  GetSelectedFileInfoInternal(profile, params.Pass());
}

}  // namespace

void VolumeInfoToVolumeMetadata(
    Profile* profile,
    const VolumeInfo& volume_info,
    file_browser_private::VolumeMetadata* volume_metadata) {
  DCHECK(volume_metadata);

  volume_metadata->volume_id = volume_info.volume_id;

  // TODO(kinaba): fill appropriate information once multi-profile support is
  // implemented.
  volume_metadata->profile.display_name = profile->GetProfileName();
  volume_metadata->profile.is_current_profile = true;

  if (!volume_info.source_path.empty()) {
    volume_metadata->source_path.reset(
        new std::string(volume_info.source_path.AsUTF8Unsafe()));
  }

  if (volume_info.type == VOLUME_TYPE_PROVIDED) {
    volume_metadata->extension_id.reset(
        new std::string(volume_info.extension_id));

    volume_metadata->file_system_id.reset(
        new std::string(volume_info.file_system_id));
  }

  volume_metadata->volume_label.reset(
      new std::string(volume_info.volume_label));

  switch (volume_info.type) {
    case VOLUME_TYPE_GOOGLE_DRIVE:
      volume_metadata->volume_type =
          file_browser_private::VOLUME_TYPE_DRIVE;
      break;
    case VOLUME_TYPE_DOWNLOADS_DIRECTORY:
      volume_metadata->volume_type =
          file_browser_private::VOLUME_TYPE_DOWNLOADS;
      break;
    case VOLUME_TYPE_REMOVABLE_DISK_PARTITION:
      volume_metadata->volume_type =
          file_browser_private::VOLUME_TYPE_REMOVABLE;
      break;
    case VOLUME_TYPE_MOUNTED_ARCHIVE_FILE:
      volume_metadata->volume_type = file_browser_private::VOLUME_TYPE_ARCHIVE;
      break;
    case VOLUME_TYPE_CLOUD_DEVICE:
      volume_metadata->volume_type =
          file_browser_private::VOLUME_TYPE_CLOUD_DEVICE;
      break;
    case VOLUME_TYPE_PROVIDED:
      volume_metadata->volume_type = file_browser_private::VOLUME_TYPE_PROVIDED;
      break;
    case VOLUME_TYPE_MTP:
      volume_metadata->volume_type = file_browser_private::VOLUME_TYPE_MTP;
      break;
    case VOLUME_TYPE_TESTING:
      volume_metadata->volume_type =
          file_browser_private::VOLUME_TYPE_TESTING;
      break;
    case NUM_VOLUME_TYPE:
      NOTREACHED();
      break;
  }

  // Fill device_type iff the volume is removable partition.
  if (volume_info.type == VOLUME_TYPE_REMOVABLE_DISK_PARTITION) {
    switch (volume_info.device_type) {
      case chromeos::DEVICE_TYPE_UNKNOWN:
        volume_metadata->device_type =
            file_browser_private::DEVICE_TYPE_UNKNOWN;
        break;
      case chromeos::DEVICE_TYPE_USB:
        volume_metadata->device_type = file_browser_private::DEVICE_TYPE_USB;
        break;
      case chromeos::DEVICE_TYPE_SD:
        volume_metadata->device_type = file_browser_private::DEVICE_TYPE_SD;
        break;
      case chromeos::DEVICE_TYPE_OPTICAL_DISC:
      case chromeos::DEVICE_TYPE_DVD:
        volume_metadata->device_type =
            file_browser_private::DEVICE_TYPE_OPTICAL;
        break;
      case chromeos::DEVICE_TYPE_MOBILE:
        volume_metadata->device_type = file_browser_private::DEVICE_TYPE_MOBILE;
        break;
    }
    volume_metadata->device_path.reset(
        new std::string(volume_info.system_path_prefix.AsUTF8Unsafe()));
    volume_metadata->is_parent_device.reset(
        new bool(volume_info.is_parent));
  } else {
    volume_metadata->device_type =
        file_browser_private::DEVICE_TYPE_NONE;
  }

  volume_metadata->is_read_only = volume_info.is_read_only;

  switch (volume_info.mount_condition) {
    case chromeos::disks::MOUNT_CONDITION_NONE:
      volume_metadata->mount_condition =
          file_browser_private::MOUNT_CONDITION_NONE;
      break;
    case chromeos::disks::MOUNT_CONDITION_UNKNOWN_FILESYSTEM:
      volume_metadata->mount_condition =
          file_browser_private::MOUNT_CONDITION_UNKNOWN;
      break;
    case chromeos::disks::MOUNT_CONDITION_UNSUPPORTED_FILESYSTEM:
      volume_metadata->mount_condition =
          file_browser_private::MOUNT_CONDITION_UNSUPPORTED;
      break;
  }
}

base::FilePath GetLocalPathFromURL(content::RenderViewHost* render_view_host,
                                   Profile* profile,
                                   const GURL& url) {
  DCHECK(render_view_host);
  DCHECK(profile);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      util::GetFileSystemContextForRenderViewHost(profile, render_view_host);

  const storage::FileSystemURL filesystem_url(
      file_system_context->CrackURL(url));
  base::FilePath path;
  if (!chromeos::FileSystemBackend::CanHandleURL(filesystem_url))
    return base::FilePath();
  return filesystem_url.path();
}

void GetSelectedFileInfo(content::RenderViewHost* render_view_host,
                         Profile* profile,
                         const std::vector<GURL>& file_urls,
                         GetSelectedFileInfoLocalPathOption local_path_option,
                         GetSelectedFileInfoCallback callback) {
  DCHECK(render_view_host);
  DCHECK(profile);

  scoped_ptr<GetSelectedFileInfoParams> params(new GetSelectedFileInfoParams);
  params->local_path_option = local_path_option;
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

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&GetSelectedFileInfoInternal, profile, base::Passed(&params)));
}

void SetupProfileFileAccessPermissions(int render_view_process_id,
                                       Profile* profile) {
  const base::FilePath paths[] = {
    drive::util::GetDriveMountPointPath(profile),
    util::GetDownloadsFolderForProfile(profile),
  };
  for (size_t i = 0; i < arraysize(paths); ++i) {
    content::ChildProcessSecurityPolicy::GetInstance(
        )->GrantCreateReadWriteFile(render_view_process_id, paths[i]);
  }
}

drive::EventLogger* GetLogger(Profile* profile) {
  drive::DriveIntegrationService* service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  return service ? service->event_logger() : NULL;
}

}  // namespace util
}  // namespace file_manager
