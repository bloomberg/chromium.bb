// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_event_router.h"

#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/notifications/system_notification.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/file_manager_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

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

ExtensionFileBrowserEventRouter::ExtensionFileBrowserEventRouter(
    Profile* profile)
    : delegate_(new ExtensionFileBrowserEventRouter::FileWatcherDelegate(this)),
      profile_(profile) {
}

ExtensionFileBrowserEventRouter::~ExtensionFileBrowserEventRouter() {
  DCHECK(file_watchers_.empty());
  STLDeleteValues(&file_watchers_);

  if (!profile_) {
    NOTREACHED();
    return;
  }
  if (!chromeos::CrosLibrary::Get()->EnsureLoaded())
    return;
  chromeos::MountLibrary* lib =
      chromeos::CrosLibrary::Get()->GetMountLibrary();
  lib->RemoveObserver(this);
  profile_ = NULL;
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
  } else if (event == chromeos::MOUNT_DISK_CHANGED) {
    OnDiskChanged(disk);
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
  }
}
void ExtensionFileBrowserEventRouter::MountCompleted(
    chromeos::MountLibrary::MountEvent event_type,
    chromeos::MountError error_code,
    const chromeos::MountLibrary::MountPointInfo& mount_info) {
  DispatchMountCompletedEvent(event_type, error_code, mount_info);
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

void ExtensionFileBrowserEventRouter::DispatchMountEvent(
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

  // If there were no error, add mountPath to the event.
  if (error_code == chromeos::MOUNT_ERROR_NONE) {
    FilePath relative_mount_path;
    // Convert mount point path to relative path with the external file system
    // exposed within File API.
    if (FileManagerUtil::ConvertFileToRelativeFileSystemPath(profile_,
            FilePath(mount_info.mount_path),
            &relative_mount_path)) {
      mount_info_value->SetString("mountPath", relative_mount_path.value());
    }
  }

  std::string args_json;
  base::JSONWriter::Write(&args, false /* pretty_print */, &args_json);
  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      extension_event_names::kOnFileBrowserMountCompleted, args_json, NULL,
      GURL());
}

void ExtensionFileBrowserEventRouter::OnDiskAdded(
    const chromeos::MountLibrary::Disk* disk) {
  VLOG(1) << "Disk added: " << disk->device_path();
  if (disk->device_path().empty()) {
    VLOG(1) << "Empty system path for " << disk->device_path();
    return;
  }
  if (disk->is_parent()) {
    if (!disk->has_media()) {
      HideDeviceNotification(disk->system_path());
      return;
    }
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
}

void ExtensionFileBrowserEventRouter::OnDiskRemoved(
    const chromeos::MountLibrary::Disk* disk) {
  VLOG(1) << "Disk removed: " << disk->device_path();
  HideDeviceNotification(disk->system_path());
  MountPointMap::iterator iter = mounted_devices_.find(disk->device_path());
  if (iter == mounted_devices_.end())
    return;

  chromeos::MountLibrary* lib =
      chromeos::CrosLibrary::Get()->GetMountLibrary();
  // TODO(zelidrag): This for some reason does not work as advertized.
  // we might need to clean up mount directory on FILE thread here as well.
  lib->UnmountPath(disk->device_path().c_str());

  DispatchMountEvent(disk, false);
  mounted_devices_.erase(iter);
}

void ExtensionFileBrowserEventRouter::OnDiskChanged(
    const chromeos::MountLibrary::Disk* disk) {
  VLOG(1) << "Disk changed : " << disk->device_path();
  if (!disk->mount_path().empty()) {
    HideDeviceNotification(disk->system_path());
    // Remember this mount point.
    if (mounted_devices_.find(disk->device_path()) == mounted_devices_.end()) {
      mounted_devices_.insert(
          std::pair<std::string, std::string>(disk->device_path(),
                                              disk->mount_path()));
      DispatchMountEvent(disk, true);
      HideDeviceNotification(disk->system_path());
      FileManagerUtil::ShowFullTabUrl(profile_, FilePath(disk->mount_path()));
    }
  }
}

void ExtensionFileBrowserEventRouter::OnDeviceAdded(
    const std::string& device_path) {
  VLOG(1) << "Device added : " << device_path;
  // TODO(zelidrag): Find better icon here.
  ShowDeviceNotification(device_path, IDR_PAGEINFO_INFO,
      l10n_util::GetStringUTF16(IDS_REMOVABLE_DEVICE_SCANNING_MESSAGE));

}

void ExtensionFileBrowserEventRouter::OnDeviceRemoved(
    const std::string& system_path) {
  HideDeviceNotification(system_path);
}

void ExtensionFileBrowserEventRouter::OnDeviceScanned(
    const std::string& device_path) {
  VLOG(1) << "Device scanned : " << device_path;
}

void ExtensionFileBrowserEventRouter::ShowDeviceNotification(
    const std::string& system_path, int icon_resource_id,
    const string16& message) {
  NotificationMap::iterator iter = FindNotificationForPath(system_path);
  std::string mount_path;
  if (iter != notifications_.end()) {
    iter->second->Show(message, false, false);
  } else {
    if (!profile_) {
      NOTREACHED();
      return;
    }
    chromeos::SystemNotification* notification =
        new chromeos::SystemNotification(
            profile_,
            system_path,
            icon_resource_id,
            l10n_util::GetStringUTF16(IDS_REMOVABLE_DEVICE_DETECTION_TITLE));
    notifications_.insert(NotificationMap::value_type(system_path,
        linked_ptr<chromeos::SystemNotification>(notification)));
    notification->Show(message, false, false);
  }
}

void ExtensionFileBrowserEventRouter::HideDeviceNotification(
    const std::string& system_path) {
  NotificationMap::iterator iter = FindNotificationForPath(system_path);
  if (iter != notifications_.end()) {
    iter->second->Hide();
    notifications_.erase(iter);
  }
}

ExtensionFileBrowserEventRouter::NotificationMap::iterator
    ExtensionFileBrowserEventRouter::FindNotificationForPath(
        const std::string& system_path) {
  for (NotificationMap::iterator iter = notifications_.begin();
       iter != notifications_.end();
       ++iter) {
    const std::string& notification_device_path = iter->first;
    // Doing a sub string match so that we find if this new one is a subdevice
    // of another already inserted device.
    if (StartsWithASCII(system_path, notification_device_path, true)) {
      return iter;
    }
  }
  return notifications_.end();
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
