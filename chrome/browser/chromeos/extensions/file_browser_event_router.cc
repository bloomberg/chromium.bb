// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_event_router.h"

#include "base/json/json_writer.h"
#include "base/memory/singleton.h"
#include "base/stl_util-inl.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/notifications/system_notification.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/extensions/file_manager_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

const char kDiskAddedEventType[] = "added";
const char kDiskRemovedEventType[] = "removed";

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
  result->SetString("label", disk->device_label());
  result->SetString("deviceType", DeviceTypeToString(disk->device_type()));
  result->SetInteger("totalSizeKB", disk->total_size() / 1024);
  result->SetBoolean("readOnly", disk->is_read_only());
  return result;
}

ExtensionFileBrowserEventRouter::ExtensionFileBrowserEventRouter()
    : profile_(NULL) {
}

ExtensionFileBrowserEventRouter::~ExtensionFileBrowserEventRouter() {
}

void ExtensionFileBrowserEventRouter::ObserveFileSystemEvents(
    Profile* profile) {
  if (!profile)
    return;
  profile_ = profile;
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

void ExtensionFileBrowserEventRouter::StopObservingFileSystemEvents() {
  if (!profile_)
    return;
  if (!chromeos::CrosLibrary::Get()->EnsureLoaded())
    return;
  chromeos::MountLibrary* lib =
      chromeos::CrosLibrary::Get()->GetMountLibrary();
  lib->RemoveObserver(this);
  profile_ = NULL;
}

// static
ExtensionFileBrowserEventRouter*
    ExtensionFileBrowserEventRouter::GetInstance() {
  return Singleton<ExtensionFileBrowserEventRouter>::get();
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

void ExtensionFileBrowserEventRouter::DispatchEvent(
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

void ExtensionFileBrowserEventRouter::OnDiskAdded(
    const chromeos::MountLibrary::Disk* disk) {
  VLOG(1) << "Disk added: " << disk->device_path();
  if (disk->device_path().empty()) {
    VLOG(1) << "Empty system path for " << disk->device_path();
    return;
  }
  if (disk->is_parent()) {
    if (!disk->has_media())
      HideDeviceNotification(disk->system_path());
    return;
  }

  // If disk is not mounted yet, give it a try.
  if (disk->mount_path().empty()) {
    // Initiate disk mount operation.
    chromeos::MountLibrary* lib =
        chromeos::CrosLibrary::Get()->GetMountLibrary();
    lib->MountPath(disk->device_path().c_str());
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

  DispatchEvent(disk, false);
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
      DispatchEvent(disk, true);
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
