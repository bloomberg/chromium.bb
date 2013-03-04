// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_event_router.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/chromeos/extensions/file_browser_notifications.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/cros_disks_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

using chromeos::DBusThreadManager;
using chromeos::disks::DiskMountManager;
using content::BrowserThread;
using drive::DriveSystemService;
using drive::DriveSystemServiceFactory;

namespace {

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

// Used as a callback for DriveCache::MarkAsUnmounted().
void OnMarkAsUnmounted(drive::DriveFileError error) {
  LOG_IF(ERROR, error != drive::DRIVE_FILE_OK)
      << "Failed to unmount: " << error;
}

const char* MountErrorToString(chromeos::MountError error) {
  switch (error) {
    case chromeos::MOUNT_ERROR_NONE:
      return "success";
    case chromeos::MOUNT_ERROR_UNKNOWN:
      return "error_unknown";
    case chromeos::MOUNT_ERROR_INTERNAL:
      return "error_internal";
    case chromeos::MOUNT_ERROR_INVALID_ARGUMENT:
      return "error_invalid_argument";
    case chromeos::MOUNT_ERROR_INVALID_PATH:
      return "error_invalid_path";
    case chromeos::MOUNT_ERROR_PATH_ALREADY_MOUNTED:
      return "error_path_already_mounted";
    case chromeos::MOUNT_ERROR_PATH_NOT_MOUNTED:
      return "error_path_not_mounted";
    case chromeos::MOUNT_ERROR_DIRECTORY_CREATION_FAILED:
      return "error_directory_creation_failed";
    case chromeos::MOUNT_ERROR_INVALID_MOUNT_OPTIONS:
      return "error_invalid_mount_options";
    case chromeos::MOUNT_ERROR_INVALID_UNMOUNT_OPTIONS:
      return "error_invalid_unmount_options";
    case chromeos::MOUNT_ERROR_INSUFFICIENT_PERMISSIONS:
      return "error_insufficient_permissions";
    case chromeos::MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND:
      return "error_mount_program_not_found";
    case chromeos::MOUNT_ERROR_MOUNT_PROGRAM_FAILED:
      return "error_mount_program_failed";
    case chromeos::MOUNT_ERROR_INVALID_DEVICE_PATH:
      return "error_invalid_device_path";
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
  }
  NOTREACHED();
  return "";
}

void RelayFileWatcherCallbackToUIThread(
    const base::FilePathWatcher::Callback& callback,
    const base::FilePath& local_path,
    bool got_error) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, local_path, got_error));
}

// Observes PowerManager and updates its state when the system suspends and
// resumes. After the system resumes it will stay in "is_resuming" state for 5
// seconds. This is to give DiskManager time to process device removed/added
// events (events for the devices that were present before suspend should not
// trigger any new notifications or file manager windows).
class SuspendStateDelegateImpl
    : public chromeos::PowerManagerClient::Observer,
      public FileBrowserEventRouter::SuspendStateDelegate {
 public:
  SuspendStateDelegateImpl()
      : is_resuming_(false),
        weak_factory_(this) {
    DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  }

  virtual ~SuspendStateDelegateImpl() {
    DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  }

  // chromeos::PowerManagerClient::Observer implementation.
  virtual void SuspendImminent() OVERRIDE {
    is_resuming_ = false;
    weak_factory_.InvalidateWeakPtrs();
  }

  // chromeos::PowerManagerClient::Observer implementation.
  virtual void SystemResumed(const base::TimeDelta& sleep_duration) OVERRIDE {
    is_resuming_ = true;
    base::MessageLoopProxy::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&SuspendStateDelegateImpl::Reset,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(5));
  }

  // FileBrowserEventRouter::SuspendStateDelegate implementation.
  virtual bool SystemIsResuming() const OVERRIDE {
    return is_resuming_;
  }

  // FileBrowserEventRouter::SuspendStateDelegate implementation.
  virtual bool DiskWasPresentBeforeSuspend(
      const DiskMountManager::Disk& disk) const OVERRIDE {
    // TODO(tbarzic): Implement this. Blocked on http://crbug.com/153338.
    return false;
  }

 private:
  void Reset() {
    is_resuming_ = false;
  }

  bool is_resuming_;

  base::WeakPtrFactory<SuspendStateDelegateImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SuspendStateDelegateImpl);
};

void DirectoryExistsOnBlockingPool(const base::FilePath& directory_path,
                                   const base::Closure& success_callback,
                                   const base::Closure& failure_callback) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  if (file_util::DirectoryExists(directory_path))
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, success_callback);
  else
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, failure_callback);
};

void DirectoryExistsOnUIThread(const base::FilePath& directory_path,
                               const base::Closure& success_callback,
                               const base::Closure& failure_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(&DirectoryExistsOnBlockingPool,
                 directory_path,
                 success_callback,
                 failure_callback));
};

}  // namespace

FileBrowserEventRouter::FileBrowserEventRouter(
    Profile* profile)
    : weak_factory_(this),
      notifications_(new FileBrowserNotifications(profile)),
      pref_change_registrar_(new PrefChangeRegistrar),
      profile_(profile),
      num_remote_update_requests_(0) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Bind a weak reference back to |this| to avoid memory errors in case we
  // shut down while a callback is in flight.
  file_watcher_callback_ =
      base::Bind(&RelayFileWatcherCallbackToUIThread,
          base::Bind(&FileBrowserEventRouter::HandleFileWatchNotification,
                     weak_factory_.GetWeakPtr()));
}

FileBrowserEventRouter::~FileBrowserEventRouter() {
}

void FileBrowserEventRouter::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DLOG_IF(WARNING, !file_watchers_.empty()) << "Not all file watchers are "
      << "removed. This can happen when Files.app is open during shutdown.";
  STLDeleteValues(&file_watchers_);
  if (!profile_) {
    NOTREACHED();
    return;
  }

  DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();
  if (disk_mount_manager)
    disk_mount_manager->RemoveObserver(this);

  DriveSystemService* system_service =
      DriveSystemServiceFactory::FindForProfileRegardlessOfStates(profile_);
  if (system_service) {
    system_service->file_system()->RemoveObserver(this);
    system_service->drive_service()->RemoveObserver(this);
  }

  chromeos::CrosLibrary* cros_library = chromeos::CrosLibrary::Get();
  if (cros_library) {
    chromeos::NetworkLibrary* network_library =
        cros_library->GetNetworkLibrary();
    if (network_library)
      network_library->RemoveNetworkManagerObserver(this);
  }

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
  if (disk_mount_manager) {
    disk_mount_manager->RemoveObserver(this);
    disk_mount_manager->AddObserver(this);
    disk_mount_manager->RequestMountInfoRefresh();
  }

  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfileRegardlessOfStates(profile_);
  if (system_service) {
    system_service->drive_service()->AddObserver(this);
    system_service->file_system()->AddObserver(this);
  }

  chromeos::CrosLibrary* cros_library = chromeos::CrosLibrary::Get();
  if (cros_library) {
    chromeos::NetworkLibrary* network_library =
        cros_library->GetNetworkLibrary();
    if (network_library)
     network_library->AddNetworkManagerObserver(this);
  }

  suspend_state_delegate_.reset(new SuspendStateDelegateImpl());

  pref_change_registrar_->Init(profile_->GetPrefs());

  pref_change_registrar_->Add(
      prefs::kExternalStorageDisabled,
      base::Bind(&FileBrowserEventRouter::OnExternalStorageDisabledChanged,
                 base::Unretained(this)));

  base::Closure callback =
      base::Bind(&FileBrowserEventRouter::OnFileBrowserPrefsChanged,
                 base::Unretained(this));
  pref_change_registrar_->Add(prefs::kDisableDriveOverCellular, callback);
  pref_change_registrar_->Add(prefs::kDisableDriveHostedFiles, callback);
  pref_change_registrar_->Add(prefs::kDisableDrive, callback);
  pref_change_registrar_->Add(prefs::kUse24HourClock, callback);
}

// File watch setup routines.
bool FileBrowserEventRouter::AddFileWatch(
    const base::FilePath& local_path,
    const base::FilePath& virtual_path,
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::AutoLock lock(lock_);
  base::FilePath watch_path = local_path;
  bool is_remote_watch = false;
  // Tweak watch path for remote sources - we need to drop leading /special
  // directory from there in order to be able to pair these events with
  // their change notifications.
  if (drive::util::GetSpecialRemoteRootPath().IsParent(watch_path)) {
    watch_path = drive::util::ExtractDrivePath(watch_path);
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

    if (watch->Watch(watch_path, file_watcher_callback_))
      file_watchers_[watch_path] = watch.release();
    else
      return false;
  } else {
    iter->second->AddExtension(extension_id);
  }
  return true;
}

void FileBrowserEventRouter::RemoveFileWatch(
    const base::FilePath& local_path,
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  base::AutoLock lock(lock_);
  base::FilePath watch_path = local_path;
  // Tweak watch path for remote sources - we need to drop leading /special
  // directory from there in order to be able to pair these events with
  // their change notifications.
  if (drive::util::GetSpecialRemoteRootPath().IsParent(watch_path)) {
    watch_path = drive::util::ExtractDrivePath(watch_path);
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

  // Pass back the gdata mount point path as source path.
  const std::string& gdata_path = drive::util::GetDriveMountPointPathAsString();
  DiskMountManager::MountPointInfo mount_info(
      gdata_path,
      gdata_path,
      chromeos::MOUNT_TYPE_GOOGLE_DRIVE,
      chromeos::disks::MOUNT_CONDITION_NONE);

  // Raise mount event.
  // We can pass chromeos::MOUNT_ERROR_NONE even when authentication is failed
  // or network is unreachable. These two errors will be handled later.
  OnMountEvent(DiskMountManager::MOUNTING,
               chromeos::MOUNT_ERROR_NONE,
               mount_info);

  if (!callback.is_null())
    callback.Run();
}

void FileBrowserEventRouter::HandleRemoteUpdateRequestOnUIThread(bool start) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  drive::DriveFileSystemInterface* file_system = GetRemoteFileSystem();
  // |file_system| is NULL if Drive is disabled.
  if (!file_system)
    return;

  if (start) {
    file_system->CheckForUpdates();
    if (num_remote_update_requests_ == 0)
      file_system->StartPolling();
    ++num_remote_update_requests_;
  } else {
    DCHECK_LE(1, num_remote_update_requests_);
    --num_remote_update_requests_;
    if (num_remote_update_requests_ == 0)
      file_system->StopPolling();
  }
}

void FileBrowserEventRouter::OnDiskEvent(
    DiskMountManager::DiskEvent event,
    const DiskMountManager::Disk* disk) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Disregard hidden devices.
  if (disk->is_hidden())
    return;
  if (event == DiskMountManager::DISK_ADDED) {
    OnDiskAdded(disk);
  } else if (event == DiskMountManager::DISK_REMOVED) {
    OnDiskRemoved(disk);
  }
}

void FileBrowserEventRouter::OnDeviceEvent(
    DiskMountManager::DeviceEvent event,
    const std::string& device_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (event == DiskMountManager::DEVICE_ADDED) {
    OnDeviceAdded(device_path);
  } else if (event == DiskMountManager::DEVICE_REMOVED) {
    OnDeviceRemoved(device_path);
  } else if (event == DiskMountManager::DEVICE_SCANNED) {
    OnDeviceScanned(device_path);
  }
}

void FileBrowserEventRouter::OnMountEvent(
    DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const DiskMountManager::MountPointInfo& mount_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // profile_ is NULL if ShutdownOnUIThread() is called earlier. This can
  // happen at shutdown.
  if (!profile_)
    return;

  DCHECK(mount_info.mount_type != chromeos::MOUNT_TYPE_INVALID);

  DispatchMountEvent(event, error_code, mount_info);

  if (mount_info.mount_type == chromeos::MOUNT_TYPE_DEVICE &&
      event == DiskMountManager::MOUNTING) {
    DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();
    const DiskMountManager::Disk* disk =
        disk_mount_manager->FindDiskBySourcePath(mount_info.source_path);
    // TODO(tbarzic): DiskWasPresentBeforeSuspend is not yet functional. It
    // always returns false.
    if (!disk || suspend_state_delegate_->DiskWasPresentBeforeSuspend(*disk))
      return;

    notifications_->ManageNotificationsOnMountCompleted(
        disk->system_path_prefix(), disk->drive_label(), disk->is_parent(),
        error_code == chromeos::MOUNT_ERROR_NONE,
        error_code == chromeos::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM);

    // If a new device was mounted, a new File manager window may need to be
    // opened.
    if (error_code == chromeos::MOUNT_ERROR_NONE)
      ShowRemovableDeviceInFileManager(
          *disk,
          base::FilePath::FromUTF8Unsafe(mount_info.mount_path));
  } else if (mount_info.mount_type == chromeos::MOUNT_TYPE_ARCHIVE) {
    // Clear the "mounted" state for archive files in gdata cache
    // when mounting failed or unmounting succeeded.
    if ((event == DiskMountManager::MOUNTING) !=
        (error_code == chromeos::MOUNT_ERROR_NONE)) {
      base::FilePath source_path(mount_info.source_path);
      DriveSystemService* system_service =
          DriveSystemServiceFactory::GetForProfile(profile_);
      drive::DriveCache* cache =
          system_service ? system_service->cache() : NULL;
      if (cache && cache->IsUnderDriveCacheDirectory(source_path))
        cache->MarkAsUnmounted(source_path, base::Bind(&OnMarkAsUnmounted));
    }
  }
}

void FileBrowserEventRouter::OnFormatEvent(
      DiskMountManager::FormatEvent event,
      chromeos::FormatError error_code,
      const std::string& device_path) {
  if (event == DiskMountManager::FORMAT_STARTED) {
    OnFormatStarted(device_path, error_code == chromeos::FORMAT_ERROR_NONE);
  } else if (event == DiskMountManager::FORMAT_COMPLETED) {
    OnFormatCompleted(device_path, error_code == chromeos::FORMAT_ERROR_NONE);
  }
}

void FileBrowserEventRouter::OnNetworkManagerChanged(
    chromeos::NetworkLibrary* network_library) {
  if (!profile_ ||
      !extensions::ExtensionSystem::Get(profile_)->event_router()) {
    NOTREACHED();
    return;
  }
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::event_names::kOnFileBrowserDriveConnectionStatusChanged,
      scoped_ptr<ListValue>(new ListValue())));
  extensions::ExtensionSystem::Get(profile_)->event_router()->
      BroadcastEvent(event.Pass());
}

void FileBrowserEventRouter::OnExternalStorageDisabledChanged() {
  // If the policy just got disabled we have to unmount every device currently
  // mounted. The opposite is fine - we can let the user re-plug her device to
  // make it available.
  if (profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled)) {
    DiskMountManager* manager = DiskMountManager::GetInstance();
    DiskMountManager::MountPointMap mounts(manager->mount_points());
    for (DiskMountManager::MountPointMap::const_iterator it = mounts.begin();
         it != mounts.end(); ++it) {
      LOG(INFO) << "Unmounting " << it->second.mount_path
                << " because of policy.";
      manager->UnmountPath(it->second.mount_path,
                           chromeos::UNMOUNT_OPTIONS_NONE);
    }
  }
}

void FileBrowserEventRouter::OnFileBrowserPrefsChanged() {
  if (!profile_ ||
      !extensions::ExtensionSystem::Get(profile_)->event_router()) {
    NOTREACHED();
    return;
  }

  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::event_names::kOnFileBrowserPreferencesChanged,
      scoped_ptr<ListValue>(new ListValue())));
  extensions::ExtensionSystem::Get(profile_)->event_router()->
      BroadcastEvent(event.Pass());
}

void FileBrowserEventRouter::OnProgressUpdate(
    const google_apis::OperationProgressStatusList& list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<ListValue> event_list(
      file_manager_util::ProgressStatusVectorToListValue(
          profile_, kFileBrowserDomain, list));

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(event_list.release());
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::event_names::kOnFileTransfersUpdated, args.Pass()));
  extensions::ExtensionSystem::Get(profile_)->event_router()->
      DispatchEventToExtension(kFileBrowserDomain, event.Pass());
}

void FileBrowserEventRouter::OnDirectoryChanged(
    const base::FilePath& directory_path) {
  HandleFileWatchNotification(directory_path, false);
}

void FileBrowserEventRouter::OnResourceListFetched(
    int num_accumulated_entries) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(base::Value::CreateIntegerValue(num_accumulated_entries));

  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::event_names::kOnDocumentFeedFetched, args.Pass()));
  extensions::ExtensionSystem::Get(profile_)->event_router()->
      DispatchEventToExtension(kFileBrowserDomain, event.Pass());
}

void FileBrowserEventRouter::OnFileSystemMounted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  MountDrive(base::Bind(&base::DoNothing));  // Callback does nothing.
}

void FileBrowserEventRouter::OnFileSystemBeingUnmounted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Raise a mount event to notify the File Manager.
  const std::string& gdata_path = drive::util::GetDriveMountPointPathAsString();
  DiskMountManager::MountPointInfo mount_info(
      gdata_path,
      gdata_path,
      chromeos::MOUNT_TYPE_GOOGLE_DRIVE,
      chromeos::disks::MOUNT_CONDITION_NONE);
  OnMountEvent(DiskMountManager::UNMOUNTING, chromeos::MOUNT_ERROR_NONE,
               mount_info);
}

void FileBrowserEventRouter::OnRefreshTokenInvalid() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Raise a DriveConnectionStatusChanged event to notify the status offline.
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::event_names::kOnFileBrowserDriveConnectionStatusChanged,
      scoped_ptr<ListValue>(new ListValue())));
  extensions::ExtensionSystem::Get(profile_)->event_router()->
      BroadcastEvent(event.Pass());
}

void FileBrowserEventRouter::HandleFileWatchNotification(
    const base::FilePath& local_path, bool got_error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::AutoLock lock(lock_);
  WatcherMap::const_iterator iter = file_watchers_.find(local_path);
  if (iter == file_watchers_.end()) {
    return;
  }
  DispatchDirectoryChangeEvent(iter->second->GetVirtualPath(), got_error,
                               iter->second->GetExtensions());
}

void FileBrowserEventRouter::DispatchDirectoryChangeEvent(
    const base::FilePath& virtual_path,
    bool got_error,
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
    GURL target_directory_url = GURL(base_url.spec() + virtual_path.value());
    scoped_ptr<ListValue> args(new ListValue());
    DictionaryValue* watch_info = new DictionaryValue();
    args->Append(watch_info);
    watch_info->SetString("directoryUrl", target_directory_url.spec());
    watch_info->SetString("eventType",
                          got_error ? kPathWatchError : kPathChanged);

    // TODO(mtomasz): Pass set of entries. http://crbug.com/157834
    ListValue* watch_info_entries = new ListValue();
    watch_info->Set("changedEntries", watch_info_entries);

    scoped_ptr<extensions::Event> event(new extensions::Event(
        extensions::event_names::kOnDirectoryChanged, args.Pass()));
    extensions::ExtensionSystem::Get(profile_)->event_router()->
        DispatchEventToExtension(iter->first, event.Pass());
  }
}

void FileBrowserEventRouter::DispatchMountEvent(
    DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const DiskMountManager::MountPointInfo& mount_info) {
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

  base::FilePath relative_mount_path;

  // If there were no error or some special conditions occurred, add mountPath
  // to the event.
  if (event == DiskMountManager::UNMOUNTING ||
      error_code == chromeos::MOUNT_ERROR_NONE ||
      mount_info.mount_condition) {
    // Convert mount point path to relative path with the external file system
    // exposed within File API.
    if (file_manager_util::ConvertFileToRelativeFileSystemPath(
            profile_,
            kFileBrowserDomain,
            base::FilePath(mount_info.mount_path),
            &relative_mount_path)) {
      mount_info_value->SetString("mountPath",
                                  "/" + relative_mount_path.value());
    } else {
      mount_info_value->SetString("status",
          MountErrorToString(chromeos::MOUNT_ERROR_PATH_UNMOUNTED));
    }
  }

  scoped_ptr<extensions::Event> extension_event(new extensions::Event(
      extensions::event_names::kOnFileBrowserMountCompleted, args.Pass()));
  extensions::ExtensionSystem::Get(profile_)->event_router()->
      BroadcastEvent(extension_event.Pass());
}

void FileBrowserEventRouter::ShowRemovableDeviceInFileManager(
    const DiskMountManager::Disk& disk, const base::FilePath& mount_path) {
  // Do not attempt to open File Manager while the login is in progress or
  // the screen is locked.
  if (chromeos::BaseLoginDisplayHost::default_host() ||
      chromeos::ScreenLocker::default_screen_locker())
    return;

  // According to DCF (Design rule of Camera File system) by JEITA / CP-3461
  // cameras should have pictures located in the DCIM root directory.
  const base::FilePath dcim_path = mount_path.Append(
      FILE_PATH_LITERAL("DCIM"));

  // Show the action choice dialog only for sd cards with a DCIM directory.
  if (disk.device_type() == chromeos::DEVICE_TYPE_SD) {
      DirectoryExistsOnUIThread(
          dcim_path,
          base::Bind(&file_manager_util::OpenActionChoiceDialog, mount_path),
          base::Bind(&file_manager_util::ViewRemovableDrive, mount_path));
  } else {
    file_manager_util::ViewRemovableDrive(mount_path);
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
}

void FileBrowserEventRouter::OnDiskRemoved(
    const DiskMountManager::Disk* disk) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(1) << "Disk removed: " << disk->device_path();

  if (!disk->mount_path().empty()) {
    if (!suspend_state_delegate_->SystemIsResuming()) {
      notifications_->ShowNotification(
          FileBrowserNotifications::DEVICE_HARD_UNPLUG,
          disk->system_path_prefix());
    }
    DiskMountManager::GetInstance()->UnmountPath(
        disk->mount_path(), chromeos::UNMOUNT_OPTIONS_LAZY);
  }
}

void FileBrowserEventRouter::OnDeviceAdded(
    const std::string& device_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(1) << "Device added : " << device_path;

  // If the policy is set instead of showing the new device notification we show
  // a notification that the operation is not permitted.
  if (profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled)) {
    notifications_->ShowNotification(
        FileBrowserNotifications::DEVICE_EXTERNAL_STORAGE_DISABLED,
        device_path);
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

void FileBrowserEventRouter::OnFormatStarted(
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

void FileBrowserEventRouter::OnFormatCompleted(
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

FileBrowserEventRouter::FileWatcherExtensions::FileWatcherExtensions(
    const base::FilePath& path, const std::string& extension_id,
    bool is_remote_file_system)
    : ref_count_(0),
      is_remote_file_system_(is_remote_file_system) {
  if (!is_remote_file_system_)
    file_watcher_.reset(new base::FilePathWatcher());

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

const base::FilePath&
FileBrowserEventRouter::FileWatcherExtensions::GetVirtualPath() const {
  return virtual_path_;
}

drive::DriveFileSystemInterface*
FileBrowserEventRouter::GetRemoteFileSystem() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfile(profile_);
  return (system_service ? system_service->file_system() : NULL);
}

bool FileBrowserEventRouter::FileWatcherExtensions::Watch(
    const base::FilePath& path,
    const base::FilePathWatcher::Callback& callback) {
  if (is_remote_file_system_)
    return true;

  return file_watcher_->Watch(path, false, callback);
}
