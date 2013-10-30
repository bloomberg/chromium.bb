// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_browser_private.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

using content::BrowserThread;
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

// Forward declarations of helper functions for GetSelectedFileInfo().
void ContinueGetSelectedFileInfo(Profile* profile,
                                 scoped_ptr<GetSelectedFileInfoParams> params,
                                 drive::FileError error,
                                 const base::FilePath& local_file_path,
                                 scoped_ptr<drive::ResourceEntry> entry);

// Part of GetSelectedFileInfo().
void GetSelectedFileInfoInternal(Profile* profile,
                                 scoped_ptr<GetSelectedFileInfoParams> params) {
  DCHECK(profile);
  drive::FileSystemInterface* file_system =
      drive::util::GetFileSystemByProfile(profile);

  for (size_t i = params->selected_files.size();
       i < params->file_paths.size(); ++i) {
    const base::FilePath& file_path = params->file_paths[i];

    if (!drive::util::IsUnderDriveMountPoint(file_path)) {
      params->selected_files.push_back(
          ui::SelectedFileInfo(file_path, base::FilePath()));
    } else {
      // |file_system| is NULL if Drive is disabled.
      if (!file_system) {
        ContinueGetSelectedFileInfo(profile,
                                    params.Pass(),
                                    drive::FILE_ERROR_FAILED,
                                    base::FilePath(),
                                    scoped_ptr<drive::ResourceEntry>());
        return;
      }
      // When the caller of the select file dialog wants local file paths,
      // we should retrieve Drive files onto the local cache.
      switch (params->local_path_option) {
        case NO_LOCAL_PATH_RESOLUTION:
          params->selected_files.push_back(
              ui::SelectedFileInfo(file_path, base::FilePath()));
          break;
        case NEED_LOCAL_PATH_FOR_OPENING:
          file_system->GetFileByPath(
              drive::util::ExtractDrivePath(file_path),
              base::Bind(&ContinueGetSelectedFileInfo,
                         profile,
                         base::Passed(&params)));
          return;  // Remaining work is done in ContinueGetSelectedFileInfo.
        case NEED_LOCAL_PATH_FOR_SAVING:
          file_system->GetFileByPathForSaving(
              drive::util::ExtractDrivePath(file_path),
              base::Bind(&ContinueGetSelectedFileInfo,
                         profile,
                         base::Passed(&params)));
          return;  // Remaining work is done in ContinueGetSelectedFileInfo.
      }
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

void VolumeInfoToVolumeMetadata(
    Profile* profile,
    const VolumeInfo& volume_info,
    file_browser_private::VolumeMetadata* volume_metadata) {
  DCHECK(volume_metadata);

  if (!volume_info.mount_path.empty()) {
    // Convert mount point path to relative path with the external file system
    // exposed within File API.
    base::FilePath relative_mount_path;
    if (ConvertAbsoluteFilePathToRelativeFileSystemPath(
            profile, kFileManagerAppId, base::FilePath(volume_info.mount_path),
            &relative_mount_path))
      volume_metadata->mount_path = "/" + relative_mount_path.AsUTF8Unsafe();
  }

  if (!volume_info.source_path.empty()) {
    volume_metadata->source_path.reset(
        new std::string(volume_info.source_path.AsUTF8Unsafe()));
  }

  switch (volume_info.type) {
    case VOLUME_TYPE_GOOGLE_DRIVE:
      volume_metadata->volume_type =
          file_browser_private::VolumeMetadata::VOLUME_TYPE_DRIVE;
      break;
    case VOLUME_TYPE_DOWNLOADS_DIRECTORY:
      volume_metadata->volume_type =
          file_browser_private::VolumeMetadata::VOLUME_TYPE_DOWNLOADS;
      break;
    case VOLUME_TYPE_REMOVABLE_DISK_PARTITION:
      volume_metadata->volume_type =
          file_browser_private::VolumeMetadata::VOLUME_TYPE_REMOVABLE;
      break;
    case VOLUME_TYPE_MOUNTED_ARCHIVE_FILE:
      volume_metadata->volume_type =
          file_browser_private::VolumeMetadata::VOLUME_TYPE_ARCHIVE;
      break;
  }

  // Fill device_type iff the volume is removable partition.
  if (volume_info.type == VOLUME_TYPE_REMOVABLE_DISK_PARTITION) {
    switch (volume_info.device_type) {
      case chromeos::DEVICE_TYPE_UNKNOWN:
        volume_metadata->device_type =
            file_browser_private::VolumeMetadata::DEVICE_TYPE_UNKNOWN;
        break;
      case chromeos::DEVICE_TYPE_USB:
        volume_metadata->device_type =
            file_browser_private::VolumeMetadata::DEVICE_TYPE_USB;
        break;
      case chromeos::DEVICE_TYPE_SD:
        volume_metadata->device_type =
            file_browser_private::VolumeMetadata::DEVICE_TYPE_SD;
        break;
      case chromeos::DEVICE_TYPE_OPTICAL_DISC:
      case chromeos::DEVICE_TYPE_DVD:
        volume_metadata->device_type =
            file_browser_private::VolumeMetadata::DEVICE_TYPE_OPTICAL;
        break;
      case chromeos::DEVICE_TYPE_MOBILE:
        volume_metadata->device_type =
            file_browser_private::VolumeMetadata::DEVICE_TYPE_MOBILE;
        break;
    }
  } else {
    volume_metadata->device_type =
        file_browser_private::VolumeMetadata::DEVICE_TYPE_NONE;
  }

  volume_metadata->is_read_only = volume_info.is_read_only;

  switch (volume_info.mount_condition) {
    case chromeos::disks::MOUNT_CONDITION_NONE:
      volume_metadata->mount_condition =
          file_browser_private::VolumeMetadata::MOUNT_CONDITION_NONE;
      break;
    case chromeos::disks::MOUNT_CONDITION_UNKNOWN_FILESYSTEM:
      volume_metadata->mount_condition =
          file_browser_private::VolumeMetadata::MOUNT_CONDITION_UNKNOWN;
      break;
    case chromeos::disks::MOUNT_CONDITION_UNSUPPORTED_FILESYSTEM:
      volume_metadata->mount_condition =
          file_browser_private::VolumeMetadata::MOUNT_CONDITION_UNSUPPORTED;
      break;
  }
}

content::WebContents* GetWebContents(ExtensionFunctionDispatcher* dispatcher) {
  if (!dispatcher) {
    LOG(WARNING) << "No dispatcher";
    return NULL;
  }
  if (!dispatcher->delegate()) {
    LOG(WARNING) << "No delegate";
    return NULL;
  }
  content::WebContents* web_contents =
      dispatcher->delegate()->GetAssociatedWebContents();
  if (!web_contents) {
    LOG(WARNING) << "No associated tab contents";
    return NULL;
  }
  return web_contents;
}

int32 GetTabId(ExtensionFunctionDispatcher* dispatcher) {
  content::WebContents* web_contents = GetWebContents(dispatcher);
  return web_contents ? ExtensionTabUtil::GetTabId(web_contents) : 0;
}

base::FilePath GetLocalPathFromURL(
    content::RenderViewHost* render_view_host,
    Profile* profile,
    const GURL& url) {
  DCHECK(render_view_host);
  DCHECK(profile);

  scoped_refptr<fileapi::FileSystemContext> file_system_context =
      util::GetFileSystemContextForRenderViewHost(
          profile, render_view_host);

  const fileapi::FileSystemURL filesystem_url(
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

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetSelectedFileInfoInternal,
                 profile,
                 base::Passed(&params)));
}

}  // namespace util
}  // namespace file_manager
