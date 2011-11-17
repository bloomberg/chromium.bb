// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_event_router.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
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

using content::BrowserThread;

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
      const chromeos::disks::DiskMountManager::Disk* disk) {
    DictionaryValue* result = new DictionaryValue();
    result->SetString("mountPath", disk->mount_path());
    result->SetString("devicePath", disk->device_path());
    result->SetString("label", disk->device_label());
    result->SetString("deviceType", DeviceTypeToString(disk->device_type()));
    result->SetInteger("totalSizeKB", disk->total_size_in_bytes() / 1024);
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
  chromeos::disks::DiskMountManager::GetInstance()->RemoveObserver(this);
}

void ExtensionFileBrowserEventRouter::ObserveFileSystemEvents() {
  if (!profile_) {
    NOTREACHED();
    return;
  }
  if (chromeos::UserManager::Get()->user_is_logged_in()) {
    chromeos::disks::DiskMountManager* disk_mount_manager =
        chromeos::disks::DiskMountManager::GetInstance();
    disk_mount_manager->RemoveObserver(this);
    disk_mount_manager->AddObserver(this);
    disk_mount_manager->RequestMountInfoRefresh();
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
    scoped_ptr<FileWatcherExtensions>
        watch(new FileWatcherExtensions(virtual_path, extension_id));

    if (watch->Watch(local_path, delegate_.get()))
      file_watchers_[local_path] = watch.release();
    else
      return false;
  } else {
    iter->second->AddExtension(extension_id);
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
  iter->second->RemoveExtension(extension_id);
  if (iter->second->GetRefCount() == 0) {
    delete iter->second;
    file_watchers_.erase(iter);
  }
}

void ExtensionFileBrowserEventRouter::DiskChanged(
    chromeos::disks::DiskMountManagerEventType event,
    const chromeos::disks::DiskMountManager::Disk* disk) {
  // Disregard hidden devices.
  if (disk->is_hidden())
    return;
  if (event == chromeos::disks::MOUNT_DISK_ADDED) {
    OnDiskAdded(disk);
  } else if (event == chromeos::disks::MOUNT_DISK_REMOVED) {
    OnDiskRemoved(disk);
  }
}

void ExtensionFileBrowserEventRouter::DeviceChanged(
    chromeos::disks::DiskMountManagerEventType event,
    const std::string& device_path) {
  if (event == chromeos::disks::MOUNT_DEVICE_ADDED) {
    OnDeviceAdded(device_path);
  } else if (event == chromeos::disks::MOUNT_DEVICE_REMOVED) {
    OnDeviceRemoved(device_path);
  } else if (event == chromeos::disks::MOUNT_DEVICE_SCANNED) {
    OnDeviceScanned(device_path);
  } else if (event == chromeos::disks::MOUNT_FORMATTING_STARTED) {
  // TODO(tbarzic): get rid of '!'.
    if (device_path[0] == '!') {
      OnFormattingStarted(device_path.substr(1), false);
    } else {
      OnFormattingStarted(device_path, true);
    }
  } else if (event == chromeos::disks::MOUNT_FORMATTING_FINISHED) {
    if (device_path[0] == '!') {
      OnFormattingFinished(device_path.substr(1), false);
    } else {
      OnFormattingFinished(device_path, true);
    }
  }
}

void ExtensionFileBrowserEventRouter::MountCompleted(
    chromeos::disks::DiskMountManager::MountEvent event_type,
    chromeos::MountError error_code,
    const chromeos::disks::DiskMountManager::MountPointInfo& mount_info) {
  DispatchMountCompletedEvent(event_type, error_code, mount_info);

  if (mount_info.mount_type == chromeos::MOUNT_TYPE_DEVICE &&
      event_type == chromeos::disks::DiskMountManager::MOUNTING) {
    chromeos::disks::DiskMountManager* disk_mount_manager =
        chromeos::disks::DiskMountManager::GetInstance();
    chromeos::disks::DiskMountManager::DiskMap::const_iterator disk_it =
        disk_mount_manager->disks().find(mount_info.source_path);
    if (disk_it == disk_mount_manager->disks().end()) {
      return;
    }
    chromeos::disks::DiskMountManager::Disk* disk = disk_it->second;

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
  DispatchFolderChangeEvent(iter->second->GetVirtualPath(), got_error,
                            iter->second->GetExtensions());
}

void ExtensionFileBrowserEventRouter::DispatchFolderChangeEvent(
    const FilePath& virtual_path, bool got_error,
    const ExtensionFileBrowserEventRouter::ExtensionUsageRegistry& extensions) {
  if (!profile_) {
    NOTREACHED();
    return;
  }

  for (ExtensionUsageRegistry::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    GURL target_origin_url(Extension::GetBaseURLFromExtensionId(
        iter->first));
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
        iter->first, extension_event_names::kOnFileChanged, args_json,
        NULL, GURL());
  }
}

void ExtensionFileBrowserEventRouter::DispatchDiskEvent(
    const chromeos::disks::DiskMountManager::Disk* disk, bool added) {
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
    chromeos::disks::DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const chromeos::disks::DiskMountManager::MountPointInfo& mount_info) {
  if (!profile_ || mount_info.mount_type == chromeos::MOUNT_TYPE_INVALID) {
    NOTREACHED();
    return;
  }

  ListValue args;
  DictionaryValue* mount_info_value = new DictionaryValue();
  args.Append(mount_info_value);
  if (event == chromeos::disks::DiskMountManager::MOUNTING) {
    mount_info_value->SetString("eventType", "mount");
  } else {
    mount_info_value->SetString("eventType", "unmount");
  }
  mount_info_value->SetString("status", MountErrorToString(error_code));
  mount_info_value->SetString(
      "mountType",
      chromeos::disks::DiskMountManager::MountTypeToString(
          mount_info.mount_type));

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
      event == chromeos::disks::DiskMountManager::MOUNTING) {
    FileManagerUtil::ViewFolder(FilePath(mount_info.mount_path));
  }
}

void ExtensionFileBrowserEventRouter::OnDiskAdded(
    const chromeos::disks::DiskMountManager::Disk* disk) {
  VLOG(1) << "Disk added: " << disk->device_path();
  if (disk->device_path().empty()) {
    VLOG(1) << "Empty system path for " << disk->device_path();
    return;
  }

  // If disk is not mounted yet, give it a try.
  if (disk->mount_path().empty()) {
    // Initiate disk mount operation.
    chromeos::disks::DiskMountManager::GetInstance()->MountPath(
        disk->device_path(), chromeos::MOUNT_TYPE_DEVICE);
  }
  DispatchDiskEvent(disk, true);
}

void ExtensionFileBrowserEventRouter::OnDiskRemoved(
    const chromeos::disks::DiskMountManager::Disk* disk) {
  VLOG(1) << "Disk removed: " << disk->device_path();

  if (!disk->mount_path().empty()) {
    chromeos::disks::DiskMountManager::GetInstance()->UnmountPath(
        disk->mount_path());
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

    chromeos::disks::DiskMountManager::GetInstance()->MountPath(
        device_path, chromeos::MOUNT_TYPE_DEVICE);
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
      base::Bind(&FileWatcherDelegate::HandleFileWatchOnUIThread,
                 this, local_path, false));
}

void ExtensionFileBrowserEventRouter::FileWatcherDelegate::OnFilePathError(
    const FilePath& local_path) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
          base::Bind(&FileWatcherDelegate::HandleFileWatchOnUIThread,
                     this, local_path, true));
}

void
ExtensionFileBrowserEventRouter::FileWatcherDelegate::HandleFileWatchOnUIThread(
     const FilePath& local_path, bool got_error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  router_->HandleFileWatchNotification(local_path, got_error);
}


ExtensionFileBrowserEventRouter::FileWatcherExtensions::FileWatcherExtensions(
    const FilePath& path, const std::string& extension_id)
    : ref_count(0) {
  file_watcher.reset(new base::files::FilePathWatcher());
  virtual_path = path;
  AddExtension(extension_id);
}

void ExtensionFileBrowserEventRouter::FileWatcherExtensions::AddExtension(
    const std::string& extension_id) {
  ExtensionUsageRegistry::iterator it = extensions.find(extension_id);
  if (it != extensions.end()) {
    it->second++;
  } else {
    extensions.insert(ExtensionUsageRegistry::value_type(extension_id, 1));
  }

  ref_count++;
}

void ExtensionFileBrowserEventRouter::FileWatcherExtensions::RemoveExtension(
    const std::string& extension_id) {
  ExtensionUsageRegistry::iterator it = extensions.find(extension_id);

  if (it != extensions.end()) {
    // If entry found - decrease it's count and remove if necessary
    if (0 == it->second--) {
      extensions.erase(it);
    }

    ref_count--;
  } else {
    // Might be reference counting problem - e.g. if some component of
    // extension subscribes/unsubscribes correctly, but other component
    // only unsubscribes, developer of first one might receive this message
    LOG(FATAL) << " Extension [" << extension_id
        << "] tries to unsubscribe from folder [" << local_path.value()
        << "] it isn't subscribed";
  }
}

const ExtensionFileBrowserEventRouter::ExtensionUsageRegistry&
ExtensionFileBrowserEventRouter::FileWatcherExtensions::GetExtensions() const {
  return extensions;
}

unsigned int
ExtensionFileBrowserEventRouter::FileWatcherExtensions::GetRefCount() const {
  return ref_count;
}

const FilePath&
ExtensionFileBrowserEventRouter::FileWatcherExtensions::GetVirtualPath() const {
  return virtual_path;
}

bool ExtensionFileBrowserEventRouter::FileWatcherExtensions::Watch
    (const FilePath& path, FileWatcherDelegate* delegate) {
  return file_watcher->Watch(path, delegate);
}
