// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_event_router.h"

#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/extensions/file_browser_notifications.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/file_manager_util.h"
#include "chrome/browser/profiles/profile.h"
#include "grit/generated_resources.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

namespace {
  const char kDiskAddedEventType[] = "added";
  const char kDiskRemovedEventType[] = "removed";

  const char kPathChanged[] = "changed";
  const char kPathWatchError[] = "error";

  const char* DeviceTypeToString(chromeos::DeviceType type) {
    switch (type) {
      case chromeos::FLASH:
        return "flash";
      case chromeos::HDD:
        return "hdd";
      case chromeos::OPTICAL:
        return "optical";
      default:
        break;
    }
    return "undefined";
  }

  DictionaryValue* DiskToDictionaryValue(
      const chromeos::MountLibrary::Disk* disk) {
    DictionaryValue* result = new DictionaryValue();
    result->SetString("mountPath", disk->mount_path());
    result->SetString("devicePath", disk->device_path());
    result->SetString("label", disk->device_label());
    result->SetString("deviceType", DeviceTypeToString(disk->device_type()));
    result->SetInteger("totalSizeKB", disk->total_size() / 1024);
    result->SetBoolean("readOnly", disk->is_read_only());
    return result;
  }
}

const char* MountErrorToString(chromeos::MountError error) {
  switch (error) {
    case chromeos::MOUNT_ERROR_NONE:
      return "success";
    case chromeos::MOUNT_ERROR_UNKNOWN:
      return "error_unknown";
    case chromeos::MOUNT_ERROR_INTERNAL:
      return "error_internal";
    case chromeos::MOUNT_ERROR_UNKNOWN_FILESYSTEM:
      return "error_unknown_filesystem";
    case chromeos::MOUNT_ERROR_UNSUPORTED_FILESYSTEM:
      return "error_unsuported_filesystem";
    case chromeos::MOUNT_ERROR_INVALID_ARCHIVE:
      return "error_invalid_archive";
    case chromeos::MOUNT_ERROR_LIBRARY_NOT_LOADED:
      return "error_libcros_missing";
    default:
      NOTREACHED();
  }
  return "";
}

ExtensionFileBrowserEventRouter::ExtensionFileBrowserEventRouter(
    Profile* profile)
    : delegate_(new ExtensionFileBrowserEventRouter::FileWatcherDelegate(this)),
      notifications_(new FileBrowserNotifications(profile)),
      profile_(profile) {
}

ExtensionFileBrowserEventRouter::~ExtensionFileBrowserEventRouter() {
  DCHECK(file_watchers_.empty());
  STLDeleteValues(&file_watchers_);

  if (!profile_) {
    NOTREACHED();
    return;
  }
  profile_ = NULL;
  if (!chromeos::CrosLibrary::Get()->EnsureLoaded())
    return;
  chromeos::MountLibrary* lib =
      chromeos::CrosLibrary::Get()->GetMountLibrary();
  lib->RemoveObserver(this);
}

void ExtensionFileBrowserEventRouter::ObserveFileSystemEvents() {
  if (!profile_) {
    NOTREACHED();
    return;
  }
  if (!chromeos::CrosLibrary::Get()->EnsureLoaded())
    return;
  if (chromeos::UserManager::Get()->user_is_logged_in()) {
    chromeos::MountLibrary* lib =
        chromeos::CrosLibrary::Get()->GetMountLibrary();
    lib->RemoveObserver(this);
    lib->AddObserver(this);
    lib->RequestMountInfoRefresh();
  }
}

// File watch setup routines.
bool ExtensionFileBrowserEventRouter::AddFileWatch(
    const FilePath& local_path,
    const FilePath& virtual_path,
    const std::string& extension_id) {
  base::AutoLock lock(lock_);
  WatcherMap::iterator iter = file_watchers_.find(local_path);
  if (iter == file_watchers_.end()) {
    FileWatcherExtensions* watch = new FileWatcherExtensions(virtual_path,
                                                             extension_id);
    file_watchers_[local_path] = watch;
    if (!watch->file_watcher->Watch(local_path, delegate_.get())) {
      delete iter->second;
      file_watchers_.erase(iter);
      return false;
    }
  } else {
    iter->second->extensions.insert(extension_id);
  }
  return true;
}

void ExtensionFileBrowserEventRouter::RemoveFileWatch(
    const FilePath& local_path,
    const std::string& extension_id) {
  base::AutoLock lock(lock_);
  WatcherMap::iterator iter = file_watchers_.find(local_path);
  if (iter == file_watchers_.end())
    return;
  // Remove the renderer process for this watch.
  iter->second->extensions.erase(extension_id);
  if (iter->second->extensions.empty()) {
    delete iter->second;
    file_watchers_.erase(iter);
  }
}

void ExtensionFileBrowserEventRouter::DiskChanged(
    chromeos::MountLibraryEventType event,
    const chromeos::MountLibrary::Disk* disk) {
  if (event == chromeos::MOUNT_DISK_ADDED) {
    OnDiskAdded(disk);
  } else if (event == chromeos::MOUNT_DISK_REMOVED) {
    OnDiskRemoved(disk);
  }
}

void ExtensionFileBrowserEventRouter::DeviceChanged(
    chromeos::MountLibraryEventType event,
    const std::string& device_path) {
  if (event == chromeos::MOUNT_DEVICE_ADDED) {
    OnDeviceAdded(device_path);
  } else if (event == chromeos::MOUNT_DEVICE_REMOVED) {
    OnDeviceRemoved(device_path);
  } else if (event == chromeos::MOUNT_DEVICE_SCANNED) {
    OnDeviceScanned(device_path);
  } else if (event == chromeos::MOUNT_FORMATTING_STARTED) {
    if (device_path[0] == '!') {
      OnFormattingStarted(device_path.substr(1), false);
    } else {
      OnFormattingStarted(device_path, true);
    }
  } else if (event == chromeos::MOUNT_FORMATTING_FINISHED) {
    if (device_path[0] == '!') {
      OnFormattingFinished(device_path.substr(1), false);
    } else {
      OnFormattingFinished(device_path, true);
    }
  }
}

void ExtensionFileBrowserEventRouter::MountCompleted(
    chromeos::MountLibrary::MountEvent event_type,
    chromeos::MountError error_code,
    const chromeos::MountLibrary::MountPointInfo& mount_info) {
  DispatchMountCompletedEvent(event_type, error_code, mount_info);

  if (mount_info.mount_type == chromeos::MOUNT_TYPE_DEVICE &&
      event_type == chromeos::MountLibrary::MOUNTING) {
    chromeos::MountLibrary* mount_lib =
        chromeos::CrosLibrary::Get()->GetMountLibrary();
    chromeos::MountLibrary::DiskMap::const_iterator disk_it =
        mount_lib->disks().find(mount_info.source_path);
    if (disk_it == mount_lib->disks().end()) {
      return;
    }
    chromeos::MountLibrary::Disk* disk = disk_it->second;

     notifications_->ManageNotificationsOnMountCompleted(
        disk->system_path_prefix(), disk->drive_label(), disk->is_parent(),
        error_code == chromeos::MOUNT_ERROR_NONE,
        error_code == chromeos::MOUNT_ERROR_UNSUPORTED_FILESYSTEM);
  }
}

void ExtensionFileBrowserEventRouter::HandleFileWatchNotification(
    const FilePath& local_path, bool got_error) {
  base::AutoLock lock(lock_);
  WatcherMap::const_iterator iter = file_watchers_.find(local_path);
  if (iter == file_watchers_.end()) {
    NOTREACHED();
    return;
  }
  DispatchFolderChangeEvent(iter->second->virtual_path, got_error,
                            iter->second->extensions);
}

void ExtensionFileBrowserEventRouter::DispatchFolderChangeEvent(
    const FilePath& virtual_path, bool got_error,
    const std::set<std::string>& extensions) {
  if (!profile_) {
    NOTREACHED();
    return;
  }

  for (std::set<std::string>::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    GURL target_origin_url(Extension::GetBaseURLFromExtensionId(
        *iter));
    GURL base_url = fileapi::GetFileSystemRootURI(target_origin_url,
        fileapi::kFileSystemTypeExternal);
    GURL target_file_url = GURL(base_url.spec() + virtual_path.value());
    ListValue args;
    DictionaryValue* watch_info = new DictionaryValue();
    args.Append(watch_info);
    watch_info->SetString("fileUrl", target_file_url.spec());
    watch_info->SetString("eventType",
                          got_error ? kPathWatchError : kPathChanged);

    std::string args_json;
    base::JSONWriter::Write(&args, false /* pretty_print */, &args_json);

    profile_->GetExtensionEventRouter()->DispatchEventToExtension(
        *iter, extension_event_names::kOnFileChanged, args_json,
        NULL, GURL());
  }
}

void ExtensionFileBrowserEventRouter::DispatchDiskEvent(
    const chromeos::MountLibrary::Disk* disk, bool added) {
  if (!profile_) {
    NOTREACHED();
    return;
  }

  ListValue args;
  DictionaryValue* mount_info = new DictionaryValue();
  args.Append(mount_info);
  mount_info->SetString("eventType",
                        added ? kDiskAddedEventType : kDiskRemovedEventType);
  DictionaryValue* disk_info = DiskToDictionaryValue(disk);
  mount_info->Set("volumeInfo", disk_info);

  std::string args_json;
  base::JSONWriter::Write(&args, false /* pretty_print */, &args_json);
  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      extension_event_names::kOnFileBrowserDiskChanged, args_json, NULL,
      GURL());
}

void ExtensionFileBrowserEventRouter::DispatchMountCompletedEvent(
    chromeos::MountLibrary::MountEvent event,
    chromeos::MountError error_code,
    const chromeos::MountLibrary::MountPointInfo& mount_info) {
  if (!profile_ || mount_info.mount_type == chromeos::MOUNT_TYPE_INVALID) {
    NOTREACHED();
    return;
  }

  ListValue args;
  DictionaryValue* mount_info_value = new DictionaryValue();
  args.Append(mount_info_value);
  if (event == chromeos::MountLibrary::MOUNTING) {
    mount_info_value->SetString("eventType", "mount");
  } else {
    mount_info_value->SetString("eventType", "unmount");
  }
  mount_info_value->SetString("status", MountErrorToString(error_code));
  mount_info_value->SetString("mountType",
      chromeos::MountLibrary::MountTypeToString(mount_info.mount_type));

  if (mount_info.mount_type == chromeos::MOUNT_TYPE_ARCHIVE) {
    GURL source_url;
    if (FileManagerUtil::ConvertFileToFileSystemUrl(profile_,
            FilePath(mount_info.source_path),
            FileManagerUtil::GetFileBrowserExtensionUrl().GetOrigin(),
            &source_url)) {
      mount_info_value->SetString("sourceUrl", source_url.spec());
    }
  } else {
    mount_info_value->SetString("sourceUrl", mount_info.source_path);
  }

  FilePath relative_mount_path;
  bool relative_mount_path_set = false;

  // If there were no error or some special conditions occured, add mountPath
  // to the event.
  if (error_code == chromeos::MOUNT_ERROR_NONE ||
      mount_info.mount_condition) {
    // Convert mount point path to relative path with the external file system
    // exposed within File API.
    if (FileManagerUtil::ConvertFileToRelativeFileSystemPath(profile_,
            FilePath(mount_info.mount_path),
            &relative_mount_path)) {
      mount_info_value->SetString("mountPath",
                                  "/" + relative_mount_path.value());
      relative_mount_path_set = true;
    }
  }

  std::string args_json;
  base::JSONWriter::Write(&args, false /* pretty_print */, &args_json);
  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      extension_event_names::kOnFileBrowserMountCompleted, args_json, NULL,
      GURL());

  if (relative_mount_path_set &&
      mount_info.mount_type == chromeos::MOUNT_TYPE_DEVICE &&
      !mount_info.mount_condition &&
      event == chromeos::MountLibrary::MOUNTING) {
    FileManagerUtil::ShowFullTabUrl(profile_, FilePath(mount_info.mount_path));
  }
}

void ExtensionFileBrowserEventRouter::OnDiskAdded(
    const chromeos::MountLibrary::Disk* disk) {
  VLOG(1) << "Disk added: " << disk->device_path();
  if (disk->device_path().empty()) {
    VLOG(1) << "Empty system path for " << disk->device_path();
    return;
  }

  // If disk is not mounted yet, give it a try.
  if (disk->mount_path().empty()) {
    // Initiate disk mount operation.
    chromeos::MountLibrary* lib =
        chromeos::CrosLibrary::Get()->GetMountLibrary();
    lib->MountPath(disk->device_path().c_str(),
                   chromeos::MOUNT_TYPE_DEVICE,
                   chromeos::MountPathOptions());  // Unused.
  }
  DispatchDiskEvent(disk, true);
}

void ExtensionFileBrowserEventRouter::OnDiskRemoved(
    const chromeos::MountLibrary::Disk* disk) {
  VLOG(1) << "Disk removed: " << disk->device_path();

  if (!disk->mount_path().empty()) {
    chromeos::MountLibrary* lib =
       chromeos::CrosLibrary::Get()->GetMountLibrary();
    lib->UnmountPath(disk->mount_path().c_str());
  }
  DispatchDiskEvent(disk, false);
}

void ExtensionFileBrowserEventRouter::OnDeviceAdded(
    const std::string& device_path) {
  VLOG(1) << "Device added : " << device_path;

  notifications_->RegisterDevice(device_path);
  notifications_->ShowNotificationDelayed(FileBrowserNotifications::DEVICE,
                                          device_path,
                                          4000);
}

void ExtensionFileBrowserEventRouter::OnDeviceRemoved(
    const std::string& device_path) {
  VLOG(1) << "Device removed : " << device_path;
  notifications_->HideNotification(FileBrowserNotifications::DEVICE,
                                   device_path);
  notifications_->HideNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   device_path);
  notifications_->UnregisterDevice(device_path);
}

void ExtensionFileBrowserEventRouter::OnDeviceScanned(
    const std::string& device_path) {
  VLOG(1) << "Device scanned : " << device_path;
}

void ExtensionFileBrowserEventRouter::OnFormattingStarted(
    const std::string& device_path, bool success) {
  if (success) {
    notifications_->ShowNotification(FileBrowserNotifications::FORMAT_START,
                                     device_path);
  } else {
    notifications_->ShowNotification(
        FileBrowserNotifications::FORMAT_START_FAIL, device_path);
  }
}

void ExtensionFileBrowserEventRouter::OnFormattingFinished(
    const std::string& device_path, bool success) {
  if (success) {
    notifications_->HideNotification(FileBrowserNotifications::FORMAT_START,
                                     device_path);
    notifications_->ShowNotification(FileBrowserNotifications::FORMAT_SUCCESS,
                                     device_path);
    // Hide it after a couple of seconds.
    notifications_->HideNotificationDelayed(
        FileBrowserNotifications::FORMAT_SUCCESS, device_path, 4000);

    chromeos::MountLibrary* lib =
        chromeos::CrosLibrary::Get()->GetMountLibrary();
    lib->MountPath(device_path.c_str(),
                   chromeos::MOUNT_TYPE_DEVICE,
                   chromeos::MountPathOptions());  // Unused.
  } else {
    notifications_->HideNotification(FileBrowserNotifications::FORMAT_START,
                                     device_path);
    notifications_->ShowNotification(FileBrowserNotifications::FORMAT_FAIL,
                                     device_path);
  }
}

// ExtensionFileBrowserEventRouter::WatcherDelegate methods.
ExtensionFileBrowserEventRouter::FileWatcherDelegate::FileWatcherDelegate(
    ExtensionFileBrowserEventRouter* router) : router_(router) {
}

void ExtensionFileBrowserEventRouter::FileWatcherDelegate::OnFilePathChanged(
    const FilePath& local_path) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
          &FileWatcherDelegate::HandleFileWatchOnUIThread,
          local_path,
          false));    // got_error
}

void ExtensionFileBrowserEventRouter::FileWatcherDelegate::OnFilePathError(
    const FilePath& local_path) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
          &FileWatcherDelegate::HandleFileWatchOnUIThread,
          local_path,
          true));     // got_error
}

void
ExtensionFileBrowserEventRouter::FileWatcherDelegate::HandleFileWatchOnUIThread(
     const FilePath& local_path, bool got_error) {
  router_->HandleFileWatchNotification(local_path, got_error);
}
