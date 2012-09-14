// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_event_router.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/api/prefs/pref_change_registrar.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/extensions/file_browser_notifications.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"
#include "chrome/browser/chromeos/gdata/drive_service_interface.h"
#include "chrome/browser/chromeos/gdata/drive_system_service.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

using chromeos::disks::DiskMountManager;
using chromeos::disks::DiskMountManagerEventType;
using content::BrowserThread;
using gdata::DriveSystemService;
using gdata::DriveSystemServiceFactory;

namespace {
  const char kDiskAddedEventType[] = "added";
  const char kDiskRemovedEventType[] = "removed";

  const char kPathChanged[] = "changed";
  const char kPathWatchError[] = "error";

  DictionaryValue* DiskToDictionaryValue(
      const DiskMountManager::Disk* disk) {
    DictionaryValue* result = new DictionaryValue();
    result->SetString("mountPath", disk->mount_path());
    result->SetString("devicePath", disk->device_path());
    result->SetString("label", disk->device_label());
    result->SetString("deviceType",
        DiskMountManager::DeviceTypeToString(disk->device_type()));
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
    case chromeos::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM:
      return "error_unsuported_filesystem";
    case chromeos::MOUNT_ERROR_INVALID_ARCHIVE:
      return "error_invalid_archive";
    case chromeos::MOUNT_ERROR_NOT_AUTHENTICATED:
      return "error_authentication";
    case chromeos::MOUNT_ERROR_PATH_UNMOUNTED:
      return "error_path_unmounted";
    default:
      NOTREACHED();
  }
  return "";
}

FileBrowserEventRouter::FileBrowserEventRouter(
    Profile* profile)
    : delegate_(new FileBrowserEventRouter::FileWatcherDelegate(this)),
      notifications_(new FileBrowserNotifications(profile)),
      pref_change_registrar_(new PrefChangeRegistrar),
      profile_(profile),
      num_remote_update_requests_(0) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

FileBrowserEventRouter::~FileBrowserEventRouter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FileBrowserEventRouter::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK(file_watchers_.empty());
  STLDeleteValues(&file_watchers_);
  if (!profile_) {
    NOTREACHED();
    return;
  }
  DiskMountManager::GetInstance()->RemoveObserver(this);

  DriveSystemService* system_service =
      DriveSystemServiceFactory::FindForProfile(profile_);
  if (system_service) {
    system_service->file_system()->RemoveObserver(this);
    system_service->drive_service()->operation_registry()->RemoveObserver(this);
  }

  chromeos::NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (network_library)
     network_library->RemoveNetworkManagerObserver(this);

  profile_ = NULL;
}

void FileBrowserEventRouter::ObserveFileSystemEvents() {
  if (!profile_) {
    NOTREACHED();
    return;
  }
  if (!chromeos::UserManager::Get()->IsUserLoggedIn())
    return;

  DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();
  disk_mount_manager->RemoveObserver(this);
  disk_mount_manager->AddObserver(this);
  disk_mount_manager->RequestMountInfoRefresh();

  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfile(profile_);
  if (!system_service) {
    NOTREACHED();
    return;
  }
  system_service->drive_service()->operation_registry()->AddObserver(this);
  system_service->file_system()->AddObserver(this);

  chromeos::NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (network_library)
     network_library->AddNetworkManagerObserver(this);

  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(prefs::kDisableGDataOverCellular, this);
  pref_change_registrar_->Add(prefs::kDisableGDataHostedFiles, this);
  pref_change_registrar_->Add(prefs::kDisableGData, this);
  pref_change_registrar_->Add(prefs::kExternalStorageDisabled, this);
}

// File watch setup routines.
bool FileBrowserEventRouter::AddFileWatch(
    const FilePath& local_path,
    const FilePath& virtual_path,
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::AutoLock lock(lock_);
  FilePath watch_path = local_path;
  bool is_remote_watch = false;
  // Tweak watch path for remote sources - we need to drop leading /special
  // directory from there in order to be able to pair these events with
  // their change notifications.
  if (gdata::util::GetSpecialRemoteRootPath().IsParent(watch_path)) {
    watch_path = gdata::util::ExtractDrivePath(watch_path);
    is_remote_watch = true;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&FileBrowserEventRouter::HandleRemoteUpdateRequestOnUIThread,
                   this, true));
  }

  WatcherMap::iterator iter = file_watchers_.find(watch_path);
  if (iter == file_watchers_.end()) {
    scoped_ptr<FileWatcherExtensions>
        watch(new FileWatcherExtensions(virtual_path,
                                        extension_id,
                                        is_remote_watch));

    if (watch->Watch(watch_path, delegate_.get()))
      file_watchers_[watch_path] = watch.release();
    else
      return false;
  } else {
    iter->second->AddExtension(extension_id);
  }
  return true;
}

void FileBrowserEventRouter::RemoveFileWatch(
    const FilePath& local_path,
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::AutoLock lock(lock_);
  FilePath watch_path = local_path;
  // Tweak watch path for remote sources - we need to drop leading /special
  // directory from there in order to be able to pair these events with
  // their change notifications.
  if (gdata::util::GetSpecialRemoteRootPath().IsParent(watch_path)) {
    watch_path = gdata::util::ExtractDrivePath(watch_path);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&FileBrowserEventRouter::HandleRemoteUpdateRequestOnUIThread,
                   this, false));
  }
  WatcherMap::iterator iter = file_watchers_.find(watch_path);
  if (iter == file_watchers_.end())
    return;
  // Remove the renderer process for this watch.
  iter->second->RemoveExtension(extension_id);
  if (iter->second->GetRefCount() == 0) {
    delete iter->second;
    file_watchers_.erase(iter);
  }
}

void FileBrowserEventRouter::MountDrive(
    const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfile(profile_);
  if (system_service) {
    system_service->drive_service()->Authenticate(
        base::Bind(&FileBrowserEventRouter::OnAuthenticated,
                   this,
                   callback));
  }
}

void FileBrowserEventRouter::OnAuthenticated(
    const base::Closure& callback,
    gdata::GDataErrorCode error,
    const std::string& token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  chromeos::MountError error_code;
  // For the file manager to work offline, GDATA_NO_CONNECTION is allowed.
  if (error == gdata::HTTP_SUCCESS || error == gdata::GDATA_NO_CONNECTION)
    error_code = chromeos::MOUNT_ERROR_NONE;
  else
    error_code = chromeos::MOUNT_ERROR_NOT_AUTHENTICATED;

  // Pass back the gdata mount point path as source path.
  const std::string& gdata_path = gdata::util::GetDriveMountPointPathAsString();
  DiskMountManager::MountPointInfo mount_info(
      gdata_path,
      gdata_path,
      chromeos::MOUNT_TYPE_GDATA,
      chromeos::disks::MOUNT_CONDITION_NONE);

  // Raise mount event.
  MountCompleted(DiskMountManager::MOUNTING, error_code, mount_info);

  if (!callback.is_null())
    callback.Run();
}

void FileBrowserEventRouter::HandleRemoteUpdateRequestOnUIThread(bool start) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  gdata::DriveFileSystemInterface* file_system = GetRemoteFileSystem();
  DCHECK(file_system);

  if (start) {
    file_system->CheckForUpdates();
    if (num_remote_update_requests_ == 0)
      file_system->StartUpdates();
    ++num_remote_update_requests_;
  } else {
    DCHECK_LE(1, num_remote_update_requests_);
    --num_remote_update_requests_;
    if (num_remote_update_requests_ == 0)
      file_system->StopUpdates();
  }
}

void FileBrowserEventRouter::DiskChanged(
    DiskMountManagerEventType event,
    const DiskMountManager::Disk* disk) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Disregard hidden devices.
  if (disk->is_hidden())
    return;
  if (event == chromeos::disks::MOUNT_DISK_ADDED) {
    OnDiskAdded(disk);
  } else if (event == chromeos::disks::MOUNT_DISK_REMOVED) {
    OnDiskRemoved(disk);
  }
}

void FileBrowserEventRouter::DeviceChanged(
    DiskMountManagerEventType event,
    const std::string& device_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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

void FileBrowserEventRouter::MountCompleted(
    DiskMountManager::MountEvent event_type,
    chromeos::MountError error_code,
    const DiskMountManager::MountPointInfo& mount_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DispatchMountCompletedEvent(event_type, error_code, mount_info);

  if (mount_info.mount_type == chromeos::MOUNT_TYPE_DEVICE &&
      event_type == DiskMountManager::MOUNTING) {
    DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();
    const DiskMountManager::Disk* disk =
        disk_mount_manager->FindDiskBySourcePath(mount_info.source_path);
    if (!disk)
      return;

    notifications_->ManageNotificationsOnMountCompleted(
        disk->system_path_prefix(), disk->drive_label(), disk->is_parent(),
        error_code == chromeos::MOUNT_ERROR_NONE,
        error_code == chromeos::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM);
  } else if (mount_info.mount_type == chromeos::MOUNT_TYPE_ARCHIVE) {
    // Clear the "mounted" state for archive files in gdata cache
    // when mounting failed or unmounting succeeded.
    if ((event_type == DiskMountManager::MOUNTING) !=
        (error_code == chromeos::MOUNT_ERROR_NONE)) {
      FilePath source_path(mount_info.source_path);
      DriveSystemService* system_service =
          DriveSystemServiceFactory::GetForProfile(profile_);
      gdata::DriveCache* cache =
          system_service ? system_service->cache() : NULL;
      if (cache) {
        cache->SetMountedStateOnUIThread(
            source_path, false, gdata::ChangeCacheStateCallback());
      }
    }
  }
}

void FileBrowserEventRouter::OnNetworkManagerChanged(
    chromeos::NetworkLibrary* network_library) {
  if (!profile_ || !profile_->GetExtensionEventRouter()) {
    NOTREACHED();
    return;
  }
  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      extensions::event_names::kOnFileBrowserNetworkConnectionChanged,
      scoped_ptr<ListValue>(new ListValue()), NULL, GURL());
}

void FileBrowserEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (!profile_ || !profile_->GetExtensionEventRouter()) {
    NOTREACHED();
    return;
  }
  if (type == chrome::NOTIFICATION_PREF_CHANGED) {
    std::string* pref_name = content::Details<std::string>(details).ptr();
    // If the policy just got disabled we have to unmount every device currently
    // mounted. The opposite is fine - we can let the user re-plug her device to
    // make it available.
    if (*pref_name == prefs::kExternalStorageDisabled &&
        profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled)) {
      DiskMountManager* manager = DiskMountManager::GetInstance();
      DiskMountManager::MountPointMap mounts(manager->mount_points());
      for (DiskMountManager::MountPointMap::const_iterator it = mounts.begin();
           it != mounts.end(); ++it) {
        LOG(INFO) << "Unmounting " << it->second.mount_path
                  << " because of policy.";
        manager->UnmountPath(it->second.mount_path);
      }
      return;
    } else if (*pref_name == prefs::kDisableGDataOverCellular ||
        *pref_name == prefs::kDisableGDataHostedFiles ||
        *pref_name == prefs::kDisableGData) {
      profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
          extensions::event_names::kOnFileBrowserGDataPreferencesChanged,
          scoped_ptr<ListValue>(new ListValue()), NULL, GURL());
    }
  }
}

void FileBrowserEventRouter::OnProgressUpdate(
    const std::vector<gdata::OperationRegistry::ProgressStatus>& list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<ListValue> event_list(
      file_manager_util::ProgressStatusVectorToListValue(
          profile_,
          file_manager_util::GetFileBrowserExtensionUrl().GetOrigin(),
          list));

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(event_list.release());

  profile_->GetExtensionEventRouter()->DispatchEventToExtension(
      std::string(kFileBrowserDomain),
      extensions::event_names::kOnFileTransfersUpdated, args.Pass(), NULL,
      GURL());
}

void FileBrowserEventRouter::OnDirectoryChanged(
    const FilePath& directory_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  HandleFileWatchNotification(directory_path, false);
}

void FileBrowserEventRouter::OnDocumentFeedFetched(
    int num_accumulated_entries) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(base::Value::CreateIntegerValue(num_accumulated_entries));

  profile_->GetExtensionEventRouter()->DispatchEventToExtension(
      std::string(kFileBrowserDomain),
      extensions::event_names::kOnDocumentFeedFetched, args.Pass(), NULL,
      GURL());
}

void FileBrowserEventRouter::OnFileSystemMounted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  MountDrive(base::Bind(&base::DoNothing));  // Callback does nothing.
}

void FileBrowserEventRouter::OnFileSystemBeingUnmounted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Raise a MountCompleted event to notify the File Manager.
  const std::string& gdata_path = gdata::util::GetDriveMountPointPathAsString();
  DiskMountManager::MountPointInfo mount_info(
      gdata_path,
      gdata_path,
      chromeos::MOUNT_TYPE_GDATA,
      chromeos::disks::MOUNT_CONDITION_NONE);
  MountCompleted(DiskMountManager::UNMOUNTING, chromeos::MOUNT_ERROR_NONE,
                 mount_info);
}

void FileBrowserEventRouter::OnAuthenticationFailed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Raise a MountCompleted event to notify the File Manager.
  const std::string& gdata_path = gdata::util::GetDriveMountPointPathAsString();
  DiskMountManager::MountPointInfo mount_info(
      gdata_path,
      gdata_path,
      chromeos::MOUNT_TYPE_GDATA,
      chromeos::disks::MOUNT_CONDITION_NONE);
  MountCompleted(DiskMountManager::UNMOUNTING, chromeos::MOUNT_ERROR_NONE,
                 mount_info);
}

void FileBrowserEventRouter::HandleFileWatchNotification(
    const FilePath& local_path, bool got_error) {
  base::AutoLock lock(lock_);
  WatcherMap::const_iterator iter = file_watchers_.find(local_path);
  if (iter == file_watchers_.end()) {
    return;
  }
  DispatchFolderChangeEvent(iter->second->GetVirtualPath(), got_error,
                            iter->second->GetExtensions());
}

void FileBrowserEventRouter::DispatchFolderChangeEvent(
    const FilePath& virtual_path, bool got_error,
    const FileBrowserEventRouter::ExtensionUsageRegistry& extensions) {
  if (!profile_) {
    NOTREACHED();
    return;
  }

  for (ExtensionUsageRegistry::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    GURL target_origin_url(extensions::Extension::GetBaseURLFromExtensionId(
        iter->first));
    GURL base_url = fileapi::GetFileSystemRootURI(target_origin_url,
        fileapi::kFileSystemTypeExternal);
    GURL target_file_url = GURL(base_url.spec() + virtual_path.value());
    scoped_ptr<ListValue> args(new ListValue());
    DictionaryValue* watch_info = new DictionaryValue();
    args->Append(watch_info);
    watch_info->SetString("fileUrl", target_file_url.spec());
    watch_info->SetString("eventType",
                          got_error ? kPathWatchError : kPathChanged);

    profile_->GetExtensionEventRouter()->DispatchEventToExtension(
        iter->first, extensions::event_names::kOnFileChanged, args.Pass(), NULL,
        GURL());
  }
}

// TODO(tbarzic): This is not used anymore. Remove it.
void FileBrowserEventRouter::DispatchDiskEvent(
    const DiskMountManager::Disk* disk, bool added) {
  if (!profile_) {
    NOTREACHED();
    return;
  }

  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* mount_info = new DictionaryValue();
  args->Append(mount_info);
  mount_info->SetString("eventType",
                        added ? kDiskAddedEventType : kDiskRemovedEventType);
  DictionaryValue* disk_info = DiskToDictionaryValue(disk);
  mount_info->Set("volumeInfo", disk_info);

  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      extensions::event_names::kOnFileBrowserDiskChanged, args.Pass(), NULL,
      GURL());
}

void FileBrowserEventRouter::DispatchMountCompletedEvent(
    DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const DiskMountManager::MountPointInfo& mount_info) {
  if (!profile_ || mount_info.mount_type == chromeos::MOUNT_TYPE_INVALID) {
    NOTREACHED();
    return;
  }

  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* mount_info_value = new DictionaryValue();
  args->Append(mount_info_value);
  mount_info_value->SetString("eventType",
      event == DiskMountManager::MOUNTING ? "mount" : "unmount");
  mount_info_value->SetString("status", MountErrorToString(error_code));
  mount_info_value->SetString(
      "mountType",
      DiskMountManager::MountTypeToString(mount_info.mount_type));

  // Add sourcePath to the event.
  mount_info_value->SetString("sourcePath", mount_info.source_path);

  FilePath relative_mount_path;
  bool relative_mount_path_set = false;

  // If there were no error or some special conditions occured, add mountPath
  // to the event.
  if (error_code == chromeos::MOUNT_ERROR_NONE ||
      mount_info.mount_condition) {
    // Convert mount point path to relative path with the external file system
    // exposed within File API.
    if (file_manager_util::ConvertFileToRelativeFileSystemPath(profile_,
            FilePath(mount_info.mount_path),
            &relative_mount_path)) {
      mount_info_value->SetString("mountPath",
                                  "/" + relative_mount_path.value());
      relative_mount_path_set = true;
    } else {
      LOG(ERROR) << "Mount path is not accessible: " << mount_info.mount_path;
      mount_info_value->SetString("status",
          MountErrorToString(chromeos::MOUNT_ERROR_PATH_UNMOUNTED));
    }
  }

  profile_->GetExtensionEventRouter()->DispatchEventToRenderers(
      extensions::event_names::kOnFileBrowserMountCompleted, args.Pass(), NULL,
      GURL());

  // Do not attempt to open File Manager while the login is in progress or
  // the screen is locked.
  if (chromeos::BaseLoginDisplayHost::default_host() ||
      chromeos::ScreenLocker::default_screen_locker())
    return;

  if (relative_mount_path_set &&
      mount_info.mount_type == chromeos::MOUNT_TYPE_DEVICE &&
      !mount_info.mount_condition &&
      event == DiskMountManager::MOUNTING) {
    file_manager_util::ViewRemovableDrive(FilePath(mount_info.mount_path));
  }
}

void FileBrowserEventRouter::OnDiskAdded(
    const DiskMountManager::Disk* disk) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(1) << "Disk added: " << disk->device_path();
  if (disk->device_path().empty()) {
    VLOG(1) << "Empty system path for " << disk->device_path();
    return;
  }

  // If disk is not mounted yet and it has media and there is no policy
  // forbidding external storage, give it a try.
  if (disk->mount_path().empty() && disk->has_media() &&
      !profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled)) {
    // Initiate disk mount operation. MountPath auto-detects the filesystem
    // format if the second argument is empty. The third argument (mount label)
    // is not used in a disk mount operation.
    DiskMountManager::GetInstance()->MountPath(
        disk->device_path(), std::string(), std::string(),
        chromeos::MOUNT_TYPE_DEVICE);
  } else {
    // Either the disk was mounted or it has no media. In both cases we don't
    // want the Scanning notification to persist.
    notifications_->HideNotification(FileBrowserNotifications::DEVICE,
                                     disk->system_path_prefix());
  }
  DispatchDiskEvent(disk, true);
}

void FileBrowserEventRouter::OnDiskRemoved(
    const DiskMountManager::Disk* disk) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(1) << "Disk removed: " << disk->device_path();

  if (!disk->mount_path().empty()) {
    DiskMountManager::GetInstance()->UnmountPath(disk->mount_path());
  }
  DispatchDiskEvent(disk, false);
}

void FileBrowserEventRouter::OnDeviceAdded(
    const std::string& device_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(1) << "Device added : " << device_path;

  // If the policy is set instead of showing the new device notification we show
  // a notification that the operation is not permitted.
  if (profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled)) {
    notifications_->ShowNotificationWithMessage(
        FileBrowserNotifications::DEVICE_FAIL,
        device_path,
        l10n_util::GetStringUTF16(IDS_EXTERNAL_STORAGE_DISABLED_MESSAGE));
    return;
  }

  notifications_->RegisterDevice(device_path);
  notifications_->ShowNotificationDelayed(FileBrowserNotifications::DEVICE,
                                          device_path,
                                          base::TimeDelta::FromSeconds(5));
}

void FileBrowserEventRouter::OnDeviceRemoved(
    const std::string& device_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(1) << "Device removed : " << device_path;
  notifications_->HideNotification(FileBrowserNotifications::DEVICE,
                                   device_path);
  notifications_->HideNotification(FileBrowserNotifications::DEVICE_FAIL,
                                   device_path);
  notifications_->UnregisterDevice(device_path);
}

void FileBrowserEventRouter::OnDeviceScanned(
    const std::string& device_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "Device scanned : " << device_path;
}

void FileBrowserEventRouter::OnFormattingStarted(
    const std::string& device_path, bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (success) {
    notifications_->ShowNotification(FileBrowserNotifications::FORMAT_START,
                                     device_path);
  } else {
    notifications_->ShowNotification(
        FileBrowserNotifications::FORMAT_START_FAIL, device_path);
  }
}

void FileBrowserEventRouter::OnFormattingFinished(
    const std::string& device_path, bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (success) {
    notifications_->HideNotification(FileBrowserNotifications::FORMAT_START,
                                     device_path);
    notifications_->ShowNotification(FileBrowserNotifications::FORMAT_SUCCESS,
                                     device_path);
    // Hide it after a couple of seconds.
    notifications_->HideNotificationDelayed(
        FileBrowserNotifications::FORMAT_SUCCESS,
        device_path,
        base::TimeDelta::FromSeconds(4));
    // MountPath auto-detects filesystem format if second argument is empty.
    // The third argument (mount label) is not used in a disk mount operation.
    DiskMountManager::GetInstance()->MountPath(device_path, std::string(),
                                               std::string(),
                                               chromeos::MOUNT_TYPE_DEVICE);
  } else {
    notifications_->HideNotification(FileBrowserNotifications::FORMAT_START,
                                     device_path);
    notifications_->ShowNotification(FileBrowserNotifications::FORMAT_FAIL,
                                     device_path);
  }
}

// FileBrowserEventRouter::WatcherDelegate methods.
FileBrowserEventRouter::FileWatcherDelegate::FileWatcherDelegate(
    FileBrowserEventRouter* router) : router_(router) {
}

void FileBrowserEventRouter::FileWatcherDelegate::OnFilePathChanged(
    const FilePath& local_path) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&FileWatcherDelegate::HandleFileWatchOnUIThread,
                 this, local_path, false));
}

void FileBrowserEventRouter::FileWatcherDelegate::OnFilePathError(
    const FilePath& local_path) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
          base::Bind(&FileWatcherDelegate::HandleFileWatchOnUIThread,
                     this, local_path, true));
}

void
FileBrowserEventRouter::FileWatcherDelegate::HandleFileWatchOnUIThread(
     const FilePath& local_path, bool got_error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  router_->HandleFileWatchNotification(local_path, got_error);
}


FileBrowserEventRouter::FileWatcherExtensions::FileWatcherExtensions(
    const FilePath& path, const std::string& extension_id,
    bool is_remote_file_system)
    : ref_count_(0),
      is_remote_file_system_(is_remote_file_system) {
  if (!is_remote_file_system_)
    file_watcher_.reset(new base::files::FilePathWatcher());

  virtual_path_ = path;
  AddExtension(extension_id);
}

FileBrowserEventRouter::FileWatcherExtensions::~FileWatcherExtensions() {}

void FileBrowserEventRouter::FileWatcherExtensions::AddExtension(
    const std::string& extension_id) {
  ExtensionUsageRegistry::iterator it = extensions_.find(extension_id);
  if (it != extensions_.end()) {
    it->second++;
  } else {
    extensions_.insert(ExtensionUsageRegistry::value_type(extension_id, 1));
  }

  ref_count_++;
}

void FileBrowserEventRouter::FileWatcherExtensions::RemoveExtension(
    const std::string& extension_id) {
  ExtensionUsageRegistry::iterator it = extensions_.find(extension_id);

  if (it != extensions_.end()) {
    // If entry found - decrease it's count and remove if necessary
    if (0 == it->second--) {
      extensions_.erase(it);
    }

    ref_count_--;
  } else {
    // Might be reference counting problem - e.g. if some component of
    // extension subscribes/unsubscribes correctly, but other component
    // only unsubscribes, developer of first one might receive this message
    LOG(FATAL) << " Extension [" << extension_id
        << "] tries to unsubscribe from folder [" << local_path_.value()
        << "] it isn't subscribed";
  }
}

const FileBrowserEventRouter::ExtensionUsageRegistry&
FileBrowserEventRouter::FileWatcherExtensions::GetExtensions() const {
  return extensions_;
}

unsigned int
FileBrowserEventRouter::FileWatcherExtensions::GetRefCount() const {
  return ref_count_;
}

const FilePath&
FileBrowserEventRouter::FileWatcherExtensions::GetVirtualPath() const {
  return virtual_path_;
}

gdata::DriveFileSystemInterface*
FileBrowserEventRouter::GetRemoteFileSystem() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfile(profile_);
  return (system_service ? system_service->file_system() : NULL);
}

bool FileBrowserEventRouter::FileWatcherExtensions::Watch
    (const FilePath& path, FileWatcherDelegate* delegate) {
  if (is_remote_file_system_)
    return true;

  return file_watcher_->Watch(path, delegate);
}

// static
scoped_refptr<FileBrowserEventRouter>
FileBrowserEventRouterFactory::GetForProfile(Profile* profile) {
  return static_cast<FileBrowserEventRouter*>(
      GetInstance()->GetServiceForProfile(profile, true).get());
}

// static
FileBrowserEventRouterFactory*
FileBrowserEventRouterFactory::GetInstance() {
  return Singleton<FileBrowserEventRouterFactory>::get();
}

FileBrowserEventRouterFactory::FileBrowserEventRouterFactory()
    : RefcountedProfileKeyedServiceFactory("FileBrowserEventRouter",
          ProfileDependencyManager::GetInstance()) {
  DependsOn(DriveSystemServiceFactory::GetInstance());
}

FileBrowserEventRouterFactory::~FileBrowserEventRouterFactory() {
}

scoped_refptr<RefcountedProfileKeyedService>
FileBrowserEventRouterFactory::BuildServiceInstanceFor(Profile* profile) const {
  return scoped_refptr<RefcountedProfileKeyedService>(
      new FileBrowserEventRouter(profile));
}

bool FileBrowserEventRouterFactory::ServiceHasOwnInstanceInIncognito() const {
  // Explicitly and always allow this router in guest login mode.   see
  // chrome/browser/profiles/profile_keyed_base_factory.h comment
  // for the details.
  return true;
}
