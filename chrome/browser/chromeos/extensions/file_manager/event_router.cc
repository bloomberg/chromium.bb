// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/desktop_notifications.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/mounted_disk_monitor.h"
#include "chrome/browser/chromeos/file_manager/open_util.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

using chromeos::disks::DiskMountManager;
using chromeos::NetworkHandler;
using content::BrowserThread;
using drive::DriveIntegrationService;
using drive::DriveIntegrationServiceFactory;

namespace file_manager {
namespace {

const char kPathChanged[] = "changed";
const char kPathWatchError[] = "error";

// Used as a callback for FileSystem::MarkCacheFileAsUnmounted().
void OnMarkAsUnmounted(drive::FileError error) {
  // Do nothing.
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

void DirectoryExistsOnBlockingPool(const base::FilePath& directory_path,
                                   const base::Closure& success_callback,
                                   const base::Closure& failure_callback) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  if (base::DirectoryExists(directory_path))
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

// Creates a base::FilePathWatcher and starts watching at |watch_path| with
// |callback|. Returns NULL on failure.
base::FilePathWatcher* CreateAndStartFilePathWatcher(
    const base::FilePath& watch_path,
    const base::FilePathWatcher::Callback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!callback.is_null());

  base::FilePathWatcher* watcher(new base::FilePathWatcher);
  if (!watcher->Watch(watch_path, false /* recursive */, callback)) {
    delete watcher;
    return NULL;
  }

  return watcher;
}

// Constants for the "transferState" field of onFileTransferUpdated event.
const char kFileTransferStateStarted[] = "started";
const char kFileTransferStateInProgress[] = "in_progress";
const char kFileTransferStateCompleted[] = "completed";
const char kFileTransferStateFailed[] = "failed";

// Frequency of sending onFileTransferUpdated.
const int64 kFileTransferEventFrequencyInMilliseconds = 1000;

// Utility function to check if |job_info| is a file uploading job.
bool IsUploadJob(drive::JobType type) {
  return (type == drive::TYPE_UPLOAD_NEW_FILE ||
          type == drive::TYPE_UPLOAD_EXISTING_FILE);
}

// Utility function to check if |job_info| is a file downloading job.
bool IsDownloadJob(drive::JobType type) {
  return type == drive::TYPE_DOWNLOAD_FILE;
}

// Converts the job info to its JSON (Value) form.
scoped_ptr<base::DictionaryValue> JobInfoToDictionaryValue(
    const std::string& extension_id,
    const std::string& job_status,
    const drive::JobInfo& job_info) {
  DCHECK(IsActiveFileTransferJobInfo(job_info));

  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  GURL url = util::ConvertRelativeFilePathToFileSystemUrl(
      job_info.file_path, extension_id);
  result->SetString("fileUrl", url.spec());
  result->SetString("transferState", job_status);
  result->SetString("transferType",
                    IsUploadJob(job_info.job_type) ? "upload" : "download");
  // JavaScript does not have 64-bit integers. Instead we use double, which
  // is in IEEE 754 formant and accurate up to 52-bits in JS, and in practice
  // in C++. Larger values are rounded.
  result->SetDouble("processed",
                    static_cast<double>(job_info.num_completed_bytes));
  result->SetDouble("total", static_cast<double>(job_info.num_total_bytes));
  return result.Pass();
}

// Checks for availability of the Google+ Photos app.
bool IsGooglePhotosInstalled(Profile *profile) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service)
    return false;

  // Google+ Photos uses several ids for different channels. Therefore, all of
  // them should be checked.
  const std::string kGooglePlusPhotosIds[] = {
    "ebpbnabdhheoknfklmpddcdijjkmklkp",  // G+ Photos staging
    "efjnaogkjbogokcnohkmnjdojkikgobo",  // G+ Photos prod
    "ejegoaikibpmikoejfephaneibodccma"   // G+ Photos dev
  };

  for (size_t i = 0; i < arraysize(kGooglePlusPhotosIds); ++i) {
    if (service->GetExtensionById(kGooglePlusPhotosIds[i],
                                  false /* include_disable */) != NULL)
      return true;
  }

  return false;
}

}  // namespace

// Pass dummy value to JobInfo's constructor for make it default constructible.
EventRouter::DriveJobInfoWithStatus::DriveJobInfoWithStatus()
    : job_info(drive::TYPE_DOWNLOAD_FILE) {
}

EventRouter::DriveJobInfoWithStatus::DriveJobInfoWithStatus(
    const drive::JobInfo& info, const std::string& status)
    : job_info(info), status(status) {
}

EventRouter::EventRouter(
    Profile* profile)
    : notifications_(new DesktopNotifications(profile)),
      pref_change_registrar_(new PrefChangeRegistrar),
      profile_(profile),
      weak_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

EventRouter::~EventRouter() {
}

void EventRouter::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DLOG_IF(WARNING, !file_watchers_.empty())
      << "Not all file watchers are "
      << "removed. This can happen when Files.app is open during shutdown.";
  STLDeleteValues(&file_watchers_);
  if (!profile_) {
    NOTREACHED();
    return;
  }

  VolumeManager* volume_manager = VolumeManager::Get(profile_);
  if (volume_manager)
    volume_manager->RemoveObserver(this);

  DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();
  if (disk_mount_manager)
    disk_mount_manager->RemoveObserver(this);

  DriveIntegrationService* integration_service =
      DriveIntegrationServiceFactory::FindForProfileRegardlessOfStates(
          profile_);
  if (integration_service) {
    integration_service->RemoveObserver(this);
    integration_service->file_system()->RemoveObserver(this);
    integration_service->drive_service()->RemoveObserver(this);
    integration_service->job_list()->RemoveObserver(this);
  }

  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
  profile_ = NULL;
}

void EventRouter::ObserveFileSystemEvents() {
  if (!profile_) {
    NOTREACHED();
    return;
  }
  if (!chromeos::LoginState::IsInitialized() ||
      !chromeos::LoginState::Get()->IsUserLoggedIn()) {
    return;
  }

  DiskMountManager* disk_mount_manager = DiskMountManager::GetInstance();
  if (disk_mount_manager) {
    disk_mount_manager->RemoveObserver(this);
    disk_mount_manager->AddObserver(this);
    disk_mount_manager->RequestMountInfoRefresh();
  }

  DriveIntegrationService* integration_service =
      DriveIntegrationServiceFactory::GetForProfileRegardlessOfStates(
          profile_);
  if (integration_service) {
    integration_service->AddObserver(this);
    integration_service->drive_service()->AddObserver(this);
    integration_service->file_system()->AddObserver(this);
    integration_service->job_list()->AddObserver(this);
  }

  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->AddObserver(this,
                                                                FROM_HERE);
  }

  mounted_disk_monitor_.reset(new MountedDiskMonitor());

  pref_change_registrar_->Init(profile_->GetPrefs());

  pref_change_registrar_->Add(
      prefs::kExternalStorageDisabled,
      base::Bind(&EventRouter::OnExternalStorageDisabledChanged,
                 weak_factory_.GetWeakPtr()));

  base::Closure callback =
      base::Bind(&EventRouter::OnFileManagerPrefsChanged,
                 weak_factory_.GetWeakPtr());
  pref_change_registrar_->Add(prefs::kDisableDriveOverCellular, callback);
  pref_change_registrar_->Add(prefs::kDisableDriveHostedFiles, callback);
  pref_change_registrar_->Add(prefs::kDisableDrive, callback);
  pref_change_registrar_->Add(prefs::kUse24HourClock, callback);

  VolumeManager* volume_manager = VolumeManager::Get(profile_);
  if (volume_manager)
    volume_manager->AddObserver(this);
}

// File watch setup routines.
void EventRouter::AddFileWatch(const base::FilePath& local_path,
                               const base::FilePath& virtual_path,
                               const std::string& extension_id,
                               const BoolCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  base::FilePath watch_path = local_path;
  bool is_on_drive = drive::util::IsUnderDriveMountPoint(watch_path);
  // Tweak watch path for remote sources - we need to drop leading /special
  // directory from there in order to be able to pair these events with
  // their change notifications.
  if (is_on_drive)
    watch_path = drive::util::ExtractDrivePath(watch_path);

  WatcherMap::iterator iter = file_watchers_.find(watch_path);
  if (iter == file_watchers_.end()) {
    scoped_ptr<FileWatcher> watcher(new FileWatcher(virtual_path));
    watcher->AddExtension(extension_id);

    if (is_on_drive) {
      // For Drive, file watching is done via OnDirectoryChanged().
      base::MessageLoopProxy::current()->PostTask(FROM_HERE,
                                                  base::Bind(callback, true));
    } else {
      // For local files, start watching using FileWatcher.
      watcher->WatchLocalFile(
          watch_path,
          base::Bind(&EventRouter::HandleFileWatchNotification,
                     weak_factory_.GetWeakPtr()),
          callback);
    }

    file_watchers_[watch_path] = watcher.release();
  } else {
    iter->second->AddExtension(extension_id);
    base::MessageLoopProxy::current()->PostTask(FROM_HERE,
                                                base::Bind(callback, true));
  }
}

void EventRouter::RemoveFileWatch(const base::FilePath& local_path,
                                  const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::FilePath watch_path = local_path;
  // Tweak watch path for remote sources - we need to drop leading /special
  // directory from there in order to be able to pair these events with
  // their change notifications.
  if (drive::util::IsUnderDriveMountPoint(watch_path)) {
    watch_path = drive::util::ExtractDrivePath(watch_path);
  }
  WatcherMap::iterator iter = file_watchers_.find(watch_path);
  if (iter == file_watchers_.end())
    return;
  // Remove the watcher if |watch_path| is no longer watched by any extensions.
  iter->second->RemoveExtension(extension_id);
  if (iter->second->GetExtensionIds().empty()) {
    delete iter->second;
    file_watchers_.erase(iter);
  }
}

void EventRouter::OnDiskEvent(DiskMountManager::DiskEvent event,
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

void EventRouter::OnDeviceEvent(DiskMountManager::DeviceEvent event,
                                const std::string& device_path) {
  // Device event is dispatched by VolumeManager now. Do nothing.
}

void EventRouter::OnMountEvent(
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
    if (!disk || mounted_disk_monitor_->DiskIsRemounting(*disk))
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
    // Clear the "mounted" state for archive files in drive cache
    // when mounting failed or unmounting succeeded.
    if ((event == DiskMountManager::MOUNTING) !=
        (error_code == chromeos::MOUNT_ERROR_NONE)) {
      DriveIntegrationService* integration_service =
          DriveIntegrationServiceFactory::GetForProfile(profile_);
      drive::FileSystemInterface* file_system =
          integration_service ? integration_service->file_system() : NULL;
      if (file_system) {
        file_system->MarkCacheFileAsUnmounted(
            base::FilePath(mount_info.source_path),
            base::Bind(&OnMarkAsUnmounted));
      }
    }
  }
}

void EventRouter::OnFormatEvent(DiskMountManager::FormatEvent event,
                                chromeos::FormatError error_code,
                                const std::string& device_path) {
  if (event == DiskMountManager::FORMAT_STARTED) {
    OnFormatStarted(device_path, error_code == chromeos::FORMAT_ERROR_NONE);
  } else if (event == DiskMountManager::FORMAT_COMPLETED) {
    OnFormatCompleted(device_path, error_code == chromeos::FORMAT_ERROR_NONE);
  }
}

void EventRouter::NetworkManagerChanged() {
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

void EventRouter::DefaultNetworkChanged(const chromeos::NetworkState* network) {
  NetworkManagerChanged();
}

void EventRouter::OnExternalStorageDisabledChanged() {
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
                           chromeos::UNMOUNT_OPTIONS_NONE,
                           DiskMountManager::UnmountPathCallback());
    }
  }
}

void EventRouter::OnFileManagerPrefsChanged() {
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

void EventRouter::OnJobAdded(const drive::JobInfo& job_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  OnJobUpdated(job_info);
}

void EventRouter::OnJobUpdated(const drive::JobInfo& job_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!drive::IsActiveFileTransferJobInfo(job_info))
    return;

  bool is_new_job = (drive_jobs_.find(job_info.job_id) == drive_jobs_.end());

  // Replace with the latest job info.
  drive_jobs_[job_info.job_id] = DriveJobInfoWithStatus(
      job_info,
      is_new_job ? kFileTransferStateStarted : kFileTransferStateInProgress);

  // Fire event if needed.
  bool always = is_new_job;
  SendDriveFileTransferEvent(always);
}

void EventRouter::OnJobDone(const drive::JobInfo& job_info,
                            drive::FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!drive::IsActiveFileTransferJobInfo(job_info))
    return;

  // Replace with the latest job info.
  drive_jobs_[job_info.job_id] = DriveJobInfoWithStatus(
      job_info,
      error == drive::FILE_ERROR_OK ? kFileTransferStateCompleted
      : kFileTransferStateFailed);

  // Fire event if needed.
  bool always = true;
  SendDriveFileTransferEvent(always);

  // Forget about the job.
  drive_jobs_.erase(job_info.job_id);
}

void EventRouter::SendDriveFileTransferEvent(bool always) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const base::Time now = base::Time::Now();

  // When |always| flag is not set, we don't send the event until certain
  // amount of time passes after the previous one. This is to avoid
  // flooding the IPC between extensions by many onFileTransferUpdated events.
  if (!always) {
    const int64 delta = (now - last_file_transfer_event_).InMilliseconds();
    // delta < 0 may rarely happen if system clock is synced and rewinded.
    // To be conservative, we don't skip in that case.
    if (0 <= delta && delta < kFileTransferEventFrequencyInMilliseconds)
      return;
  }

  // Convert the current |drive_jobs_| to a JSON value.
  scoped_ptr<base::ListValue> event_list(new base::ListValue);
  for (std::map<drive::JobID, DriveJobInfoWithStatus>::iterator
           iter = drive_jobs_.begin(); iter != drive_jobs_.end(); ++iter) {

    scoped_ptr<base::DictionaryValue> job_info_dict(
        JobInfoToDictionaryValue(kFileManagerAppId,
                                 iter->second.status,
                                 iter->second.job_info));
    event_list->Append(job_info_dict.release());
  }

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(event_list.release());
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::event_names::kOnFileTransfersUpdated, args.Pass()));
  extensions::ExtensionSystem::Get(profile_)->event_router()->
      DispatchEventToExtension(kFileManagerAppId, event.Pass());

  last_file_transfer_event_ = now;
}

void EventRouter::OnDirectoryChanged(const base::FilePath& directory_path) {
  HandleFileWatchNotification(directory_path, false);
}

void EventRouter::OnFileSystemMounted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const std::string& drive_path = drive::util::GetDriveMountPointPathAsString();
  DiskMountManager::MountPointInfo mount_info(
      drive_path,
      drive_path,
      chromeos::MOUNT_TYPE_GOOGLE_DRIVE,
      chromeos::disks::MOUNT_CONDITION_NONE);

  // Raise mount event.
  // We can pass chromeos::MOUNT_ERROR_NONE even when authentication is failed
  // or network is unreachable. These two errors will be handled later.
  OnMountEvent(DiskMountManager::MOUNTING, chromeos::MOUNT_ERROR_NONE,
               mount_info);
}

void EventRouter::OnFileSystemBeingUnmounted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Raise a mount event to notify the File Manager.
  const std::string& drive_path = drive::util::GetDriveMountPointPathAsString();
  DiskMountManager::MountPointInfo mount_info(
      drive_path,
      drive_path,
      chromeos::MOUNT_TYPE_GOOGLE_DRIVE,
      chromeos::disks::MOUNT_CONDITION_NONE);
  OnMountEvent(DiskMountManager::UNMOUNTING, chromeos::MOUNT_ERROR_NONE,
               mount_info);
}

void EventRouter::OnRefreshTokenInvalid() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Raise a DriveConnectionStatusChanged event to notify the status offline.
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::event_names::kOnFileBrowserDriveConnectionStatusChanged,
      scoped_ptr<ListValue>(new ListValue())));
  extensions::ExtensionSystem::Get(profile_)->event_router()->
      BroadcastEvent(event.Pass());
}

void EventRouter::HandleFileWatchNotification(const base::FilePath& local_path,
                                              bool got_error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  WatcherMap::const_iterator iter = file_watchers_.find(local_path);
  if (iter == file_watchers_.end()) {
    return;
  }
  DispatchDirectoryChangeEvent(iter->second->virtual_path(), got_error,
                               iter->second->GetExtensionIds());
}

void EventRouter::DispatchDirectoryChangeEvent(
    const base::FilePath& virtual_path,
    bool got_error,
    const std::vector<std::string>& extension_ids) {
  if (!profile_) {
    NOTREACHED();
    return;
  }

  for (size_t i = 0; i < extension_ids.size(); ++i) {
    const std::string& extension_id = extension_ids[i];

    GURL target_origin_url(extensions::Extension::GetBaseURLFromExtensionId(
        extension_id));
    GURL base_url = fileapi::GetFileSystemRootURI(
        target_origin_url,
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
        DispatchEventToExtension(extension_id, event.Pass());
  }
}

void EventRouter::DispatchMountEvent(
    DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const DiskMountManager::MountPointInfo& mount_info) {
  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* mount_info_value = new DictionaryValue();
  args->Append(mount_info_value);
  mount_info_value->SetString(
      "eventType",
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
    if (util::ConvertAbsoluteFilePathToRelativeFileSystemPath(
            profile_,
            kFileManagerAppId,
            base::FilePath(mount_info.mount_path),
            &relative_mount_path)) {
      mount_info_value->SetString("mountPath",
                                  "/" + relative_mount_path.value());
    } else {
      mount_info_value->SetString(
          "status",
          MountErrorToString(chromeos::MOUNT_ERROR_PATH_UNMOUNTED));
    }
  }

  scoped_ptr<extensions::Event> extension_event(new extensions::Event(
      extensions::event_names::kOnFileBrowserMountCompleted, args.Pass()));
  extensions::ExtensionSystem::Get(profile_)->event_router()->
      BroadcastEvent(extension_event.Pass());
}

void EventRouter::ShowRemovableDeviceInFileManager(
    const DiskMountManager::Disk& disk,
    const base::FilePath& mount_path) {
  // Do not attempt to open File Manager while the login is in progress or
  // the screen is locked.
  if (chromeos::LoginDisplayHostImpl::default_host() ||
      chromeos::ScreenLocker::default_screen_locker())
    return;

  // According to DCF (Design rule of Camera File system) by JEITA / CP-3461
  // cameras should have pictures located in the DCIM root directory.
  const base::FilePath dcim_path = mount_path.Append(
      FILE_PATH_LITERAL("DCIM"));

  // If there is no DCIM folder or an external photo importer is not available,
  // then launch Files.app.
  DirectoryExistsOnUIThread(
      dcim_path,
      IsGooglePhotosInstalled(profile_) ?
      base::Bind(&base::DoNothing) :
      base::Bind(&util::OpenRemovableDrive, mount_path),
      base::Bind(&util::OpenRemovableDrive, mount_path));
}

void EventRouter::OnDiskAdded(const DiskMountManager::Disk* disk) {
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
    notifications_->HideNotification(DesktopNotifications::DEVICE,
                                     disk->system_path_prefix());
  }
}

void EventRouter::OnDiskRemoved(const DiskMountManager::Disk* disk) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  VLOG(1) << "Disk removed: " << disk->device_path();

  if (!disk->mount_path().empty()) {
    DiskMountManager::GetInstance()->UnmountPath(
        disk->mount_path(),
        chromeos::UNMOUNT_OPTIONS_LAZY,
        DiskMountManager::UnmountPathCallback());
  }
}

void EventRouter::OnDeviceAdded(const std::string& device_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If the policy is set instead of showing the new device notification,
  // we show a notification that the operation is not permitted.
  if (profile_->GetPrefs()->GetBoolean(prefs::kExternalStorageDisabled)) {
    notifications_->ShowNotification(
        DesktopNotifications::DEVICE_EXTERNAL_STORAGE_DISABLED,
        device_path);
    return;
  }

  notifications_->RegisterDevice(device_path);
  notifications_->ShowNotificationDelayed(DesktopNotifications::DEVICE,
                                          device_path,
                                          base::TimeDelta::FromSeconds(5));
}

void EventRouter::OnDeviceRemoved(const std::string& device_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  notifications_->HideNotification(DesktopNotifications::DEVICE,
                                   device_path);
  notifications_->HideNotification(DesktopNotifications::DEVICE_FAIL,
                                   device_path);
  notifications_->UnregisterDevice(device_path);
}

void EventRouter::OnFormatStarted(const std::string& device_path,
                                  bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (success) {
    notifications_->ShowNotification(DesktopNotifications::FORMAT_START,
                                     device_path);
  } else {
    notifications_->ShowNotification(
        DesktopNotifications::FORMAT_START_FAIL, device_path);
  }
}

void EventRouter::OnFormatCompleted(const std::string& device_path,
                                    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (success) {
    notifications_->HideNotification(DesktopNotifications::FORMAT_START,
                                     device_path);
    notifications_->ShowNotification(DesktopNotifications::FORMAT_SUCCESS,
                                     device_path);
    // Hide it after a couple of seconds.
    notifications_->HideNotificationDelayed(
        DesktopNotifications::FORMAT_SUCCESS,
        device_path,
        base::TimeDelta::FromSeconds(4));
    // MountPath auto-detects filesystem format if second argument is empty.
    // The third argument (mount label) is not used in a disk mount operation.
    DiskMountManager::GetInstance()->MountPath(device_path, std::string(),
                                               std::string(),
                                               chromeos::MOUNT_TYPE_DEVICE);
  } else {
    notifications_->HideNotification(DesktopNotifications::FORMAT_START,
                                     device_path);
    notifications_->ShowNotification(DesktopNotifications::FORMAT_FAIL,
                                     device_path);
  }
}

}  // namespace file_manager
