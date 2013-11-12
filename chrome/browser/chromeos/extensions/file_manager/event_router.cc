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
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/desktop_notifications.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/open_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_browser_private.h"
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

namespace file_browser_private = extensions::api::file_browser_private;

namespace file_manager {
namespace {

const char kPathChanged[] = "changed";
const char kPathWatchError[] = "error";

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

// Sends an event named |event_name| with arguments |event_args| to extensions.
void BroadcastEvent(Profile* profile,
                    const std::string& event_name,
                    scoped_ptr<base::ListValue> event_args) {
  extensions::ExtensionSystem::Get(profile)->event_router()->
      BroadcastEvent(make_scoped_ptr(
          new extensions::Event(event_name, event_args.Pass())));
}

file_browser_private::MountCompletedEvent::Status
MountErrorToMountCompletedStatus(chromeos::MountError error) {
  using file_browser_private::MountCompletedEvent;

  switch (error) {
    case chromeos::MOUNT_ERROR_NONE:
      return MountCompletedEvent::STATUS_SUCCESS;
    case chromeos::MOUNT_ERROR_UNKNOWN:
      return MountCompletedEvent::STATUS_ERROR_UNKNOWN;
    case chromeos::MOUNT_ERROR_INTERNAL:
      return MountCompletedEvent::STATUS_ERROR_INTERNAL;
    case chromeos::MOUNT_ERROR_INVALID_ARGUMENT:
      return MountCompletedEvent::STATUS_ERROR_INVALID_ARGUMENT;
    case chromeos::MOUNT_ERROR_INVALID_PATH:
      return MountCompletedEvent::STATUS_ERROR_INVALID_PATH;
    case chromeos::MOUNT_ERROR_PATH_ALREADY_MOUNTED:
      return MountCompletedEvent::STATUS_ERROR_PATH_ALREADY_MOUNTED;
    case chromeos::MOUNT_ERROR_PATH_NOT_MOUNTED:
      return MountCompletedEvent::STATUS_ERROR_PATH_NOT_MOUNTED;
    case chromeos::MOUNT_ERROR_DIRECTORY_CREATION_FAILED:
      return MountCompletedEvent::STATUS_ERROR_DIRECTORY_CREATION_FAILED;
    case chromeos::MOUNT_ERROR_INVALID_MOUNT_OPTIONS:
      return MountCompletedEvent::STATUS_ERROR_INVALID_MOUNT_OPTIONS;
    case chromeos::MOUNT_ERROR_INVALID_UNMOUNT_OPTIONS:
      return MountCompletedEvent::STATUS_ERROR_INVALID_UNMOUNT_OPTIONS;
    case chromeos::MOUNT_ERROR_INSUFFICIENT_PERMISSIONS:
      return MountCompletedEvent::STATUS_ERROR_INSUFFICIENT_PERMISSIONS;
    case chromeos::MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND:
      return MountCompletedEvent::STATUS_ERROR_MOUNT_PROGRAM_NOT_FOUND;
    case chromeos::MOUNT_ERROR_MOUNT_PROGRAM_FAILED:
      return MountCompletedEvent::STATUS_ERROR_MOUNT_PROGRAM_FAILED;
    case chromeos::MOUNT_ERROR_INVALID_DEVICE_PATH:
      return MountCompletedEvent::STATUS_ERROR_INVALID_DEVICE_PATH;
    case chromeos::MOUNT_ERROR_UNKNOWN_FILESYSTEM:
      return MountCompletedEvent::STATUS_ERROR_UNKNOWN_FILESYSTEM;
    case chromeos::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM:
      return MountCompletedEvent::STATUS_ERROR_UNSUPORTED_FILESYSTEM;
    case chromeos::MOUNT_ERROR_INVALID_ARCHIVE:
      return MountCompletedEvent::STATUS_ERROR_INVALID_ARCHIVE;
    case chromeos::MOUNT_ERROR_NOT_AUTHENTICATED:
      return MountCompletedEvent::STATUS_ERROR_AUTHENTICATION;
    case chromeos::MOUNT_ERROR_PATH_UNMOUNTED:
      return MountCompletedEvent::STATUS_ERROR_PATH_UNMOUNTED;
  }
  NOTREACHED();
  return MountCompletedEvent::STATUS_NONE;
}

void BroadcastMountCompletedEvent(
    Profile* profile,
    file_browser_private::MountCompletedEvent::EventType event_type,
    chromeos::MountError error,
    const VolumeInfo& volume_info) {
  file_browser_private::MountCompletedEvent event;
  event.event_type = event_type;
  event.status = MountErrorToMountCompletedStatus(error);
  util::VolumeInfoToVolumeMetadata(
      profile, volume_info, &event.volume_metadata);

  if (!volume_info.mount_path.empty() &&
      event.volume_metadata.mount_path.empty()) {
    event.status =
        file_browser_private::MountCompletedEvent::STATUS_ERROR_PATH_UNMOUNTED;
  }

  BroadcastEvent(
      profile,
      extensions::event_names::kOnFileBrowserMountCompleted,
      file_browser_private::OnMountCompleted::Create(event));
}

file_browser_private::CopyProgressStatus::Type
CopyProgressTypeToCopyProgressStatusType(
    fileapi::FileSystemOperation::CopyProgressType type) {
  using file_browser_private::CopyProgressStatus;

  switch (type) {
    case fileapi::FileSystemOperation::BEGIN_COPY_ENTRY:
      return CopyProgressStatus::TYPE_BEGIN_COPY_ENTRY;
    case fileapi::FileSystemOperation::END_COPY_ENTRY:
      return CopyProgressStatus::TYPE_END_COPY_ENTRY;
    case fileapi::FileSystemOperation::PROGRESS:
      return CopyProgressStatus::TYPE_PROGRESS;
  }
  NOTREACHED();
  return CopyProgressStatus::TYPE_NONE;
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

EventRouter::EventRouter(Profile* profile)
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

  pref_change_registrar_->RemoveAll();

  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }

  DriveIntegrationService* integration_service =
      DriveIntegrationServiceFactory::FindForProfileRegardlessOfStates(
          profile_);
  if (integration_service) {
    integration_service->file_system()->RemoveObserver(this);
    integration_service->drive_service()->RemoveObserver(this);
    integration_service->job_list()->RemoveObserver(this);
  }

  VolumeManager* volume_manager = VolumeManager::Get(profile_);
  if (volume_manager)
    volume_manager->RemoveObserver(this);

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

  // VolumeManager's construction triggers DriveIntegrationService's
  // construction, so it is necessary to call VolumeManager's Get before
  // accessing DriveIntegrationService.
  VolumeManager* volume_manager = VolumeManager::Get(profile_);
  if (volume_manager)
    volume_manager->AddObserver(this);

  DriveIntegrationService* integration_service =
      DriveIntegrationServiceFactory::FindForProfileRegardlessOfStates(
          profile_);
  if (integration_service) {
    integration_service->drive_service()->AddObserver(this);
    integration_service->file_system()->AddObserver(this);
    integration_service->job_list()->AddObserver(this);
  }

  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->AddObserver(this,
                                                                FROM_HERE);
  }

  pref_change_registrar_->Init(profile_->GetPrefs());
  base::Closure callback =
      base::Bind(&EventRouter::OnFileManagerPrefsChanged,
                 weak_factory_.GetWeakPtr());
  pref_change_registrar_->Add(prefs::kDisableDriveOverCellular, callback);
  pref_change_registrar_->Add(prefs::kDisableDriveHostedFiles, callback);
  pref_change_registrar_->Add(prefs::kDisableDrive, callback);
  pref_change_registrar_->Add(prefs::kUse24HourClock, callback);
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

void EventRouter::OnCopyCompleted(int copy_id,
                                  const GURL& source_url,
                                  const GURL& destination_url,
                                  base::PlatformFileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_browser_private::CopyProgressStatus status;
  if (error == base::PLATFORM_FILE_OK) {
    // Send success event.
    status.type = file_browser_private::CopyProgressStatus::TYPE_SUCCESS;
    status.source_url.reset(new std::string(source_url.spec()));
    status.destination_url.reset(new std::string(destination_url.spec()));
  } else {
    // Send error event.
    status.type = file_browser_private::CopyProgressStatus::TYPE_ERROR;
    status.error.reset(
        new int(fileapi::PlatformFileErrorToWebFileError(error)));
  }

  BroadcastEvent(
      profile_,
      extensions::event_names::kOnFileBrowserCopyProgress,
      file_browser_private::OnCopyProgress::Create(copy_id, status));
}

void EventRouter::OnCopyProgress(
    int copy_id,
    fileapi::FileSystemOperation::CopyProgressType type,
    const GURL& source_url,
    const GURL& destination_url,
    int64 size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_browser_private::CopyProgressStatus status;
  status.type = CopyProgressTypeToCopyProgressStatusType(type);
  status.source_url.reset(new std::string(source_url.spec()));
  if (type == fileapi::FileSystemOperation::END_COPY_ENTRY)
    status.destination_url.reset(new std::string(destination_url.spec()));
  if (type == fileapi::FileSystemOperation::PROGRESS)
    status.size.reset(new double(size));

  BroadcastEvent(
      profile_,
      extensions::event_names::kOnFileBrowserCopyProgress,
      file_browser_private::OnCopyProgress::Create(copy_id, status));
}

void EventRouter::DefaultNetworkChanged(const chromeos::NetworkState* network) {
  if (!profile_ ||
      !extensions::ExtensionSystem::Get(profile_)->event_router()) {
    NOTREACHED();
    return;
  }

  BroadcastEvent(
      profile_,
      extensions::event_names::kOnFileBrowserDriveConnectionStatusChanged,
      make_scoped_ptr(new ListValue));
}

void EventRouter::OnFileManagerPrefsChanged() {
  if (!profile_ ||
      !extensions::ExtensionSystem::Get(profile_)->event_router()) {
    NOTREACHED();
    return;
  }

  BroadcastEvent(
      profile_,
      extensions::event_names::kOnFileBrowserPreferencesChanged,
      make_scoped_ptr(new ListValue));
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

void EventRouter::OnRefreshTokenInvalid() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Raise a DriveConnectionStatusChanged event to notify the status offline.
  BroadcastEvent(
      profile_,
      extensions::event_names::kOnFileBrowserDriveConnectionStatusChanged,
      make_scoped_ptr(new ListValue));
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

    scoped_ptr<extensions::Event> event(new extensions::Event(
        extensions::event_names::kOnDirectoryChanged, args.Pass()));
    extensions::ExtensionSystem::Get(profile_)->event_router()->
        DispatchEventToExtension(extension_id, event.Pass());
  }
}

void EventRouter::ShowRemovableDeviceInFileManager(
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

void EventRouter::OnDiskAdded(
    const DiskMountManager::Disk& disk, bool mounting) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!mounting) {
    // If the disk is not being mounted, we don't want the Scanning
    // notification to persist.
    notifications_->HideNotification(DesktopNotifications::DEVICE,
                                     disk.system_path_prefix());
  }
}

void EventRouter::OnDiskRemoved(const DiskMountManager::Disk& disk) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Do nothing.
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

void EventRouter::OnVolumeMounted(chromeos::MountError error_code,
                                  const VolumeInfo& volume_info,
                                  bool is_remounting) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // profile_ is NULL if ShutdownOnUIThread() is called earlier. This can
  // happen at shutdown. This should be removed after removing Drive mounting
  // code in addMount. (addMount -> OnFileSystemMounted -> OnVolumeMounted is
  // the only path to come here after Shutdown is called).
  if (!profile_)
    return;

  BroadcastMountCompletedEvent(
      profile_,
      file_browser_private::MountCompletedEvent::EVENT_TYPE_MOUNT,
      error_code, volume_info);

  if (volume_info.type == VOLUME_TYPE_REMOVABLE_DISK_PARTITION &&
      !is_remounting) {
    notifications_->ManageNotificationsOnMountCompleted(
        volume_info.system_path_prefix.AsUTF8Unsafe(),
        volume_info.drive_label,
        volume_info.is_parent,
        error_code == chromeos::MOUNT_ERROR_NONE,
        error_code == chromeos::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM);

    // If a new device was mounted, a new File manager window may need to be
    // opened.
    if (error_code == chromeos::MOUNT_ERROR_NONE)
      ShowRemovableDeviceInFileManager(volume_info.mount_path);
  }
}

void EventRouter::OnVolumeUnmounted(chromeos::MountError error_code,
                                    const VolumeInfo& volume_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BroadcastMountCompletedEvent(
      profile_,
      file_browser_private::MountCompletedEvent::EVENT_TYPE_UNMOUNT,
      error_code, volume_info);
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
  } else {
    notifications_->HideNotification(DesktopNotifications::FORMAT_START,
                                     device_path);
    notifications_->ShowNotification(DesktopNotifications::FORMAT_FAIL,
                                     device_path);
  }
}

}  // namespace file_manager
