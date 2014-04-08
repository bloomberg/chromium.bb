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
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/open_util.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/login/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/screen_locker.h"
#include "chrome/browser/drive/drive_service_interface.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

using chromeos::disks::DiskMountManager;
using chromeos::NetworkHandler;
using content::BrowserThread;
using drive::DriveIntegrationService;
using drive::DriveIntegrationServiceFactory;
using file_manager::util::EntryDefinition;
using file_manager::util::FileDefinition;

namespace file_browser_private = extensions::api::file_browser_private;

namespace file_manager {
namespace {

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
const int64 kProgressEventFrequencyInMilliseconds = 1000;

// Utility function to check if |job_info| is a file uploading job.
bool IsUploadJob(drive::JobType type) {
  return (type == drive::TYPE_UPLOAD_NEW_FILE ||
          type == drive::TYPE_UPLOAD_EXISTING_FILE);
}

// Converts the job info to a IDL generated type.
void JobInfoToTransferStatus(
    Profile* profile,
    const std::string& extension_id,
    const std::string& job_status,
    const drive::JobInfo& job_info,
    file_browser_private::FileTransferStatus* status) {
  DCHECK(IsActiveFileTransferJobInfo(job_info));

  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  GURL url = util::ConvertDrivePathToFileSystemUrl(
      profile, job_info.file_path, extension_id);
  status->file_url = url.spec();
  status->transfer_state = file_browser_private::ParseTransferState(job_status);
  status->transfer_type =
      IsUploadJob(job_info.job_type) ?
      file_browser_private::TRANSFER_TYPE_UPLOAD :
      file_browser_private::TRANSFER_TYPE_DOWNLOAD;
  // JavaScript does not have 64-bit integers. Instead we use double, which
  // is in IEEE 754 formant and accurate up to 52-bits in JS, and in practice
  // in C++. Larger values are rounded.
  status->processed.reset(
      new double(static_cast<double>(job_info.num_completed_bytes)));
  status->total.reset(
      new double(static_cast<double>(job_info.num_total_bytes)));
}

// Checks for availability of the Google+ Photos app.
// TODO(mtomasz): Replace with crbug.com/341902 solution.
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

// Checks if the Recovery Tool is running. This is a temporary solution.
// TODO(mtomasz): Replace with crbug.com/341902 solution.
bool IsRecoveryToolRunning(Profile* profile) {
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(profile);
  if (!extension_prefs)
    return false;

  const std::string kRecoveryToolIds[] = {
      "kkebgepbbgbcmghedmmdfcbdcodlkngh",  // Recovert tool staging
      "jndclpdbaamdhonoechobihbbiimdgai"   // Recovery tool prod
  };

  for (size_t i = 0; i < arraysize(kRecoveryToolIds); ++i) {
    const std::string extension_id = kRecoveryToolIds[i];
    if (extension_prefs->IsExtensionRunning(extension_id))
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

file_browser_private::MountCompletedStatus
MountErrorToMountCompletedStatus(chromeos::MountError error) {
  switch (error) {
    case chromeos::MOUNT_ERROR_NONE:
      return file_browser_private::MOUNT_COMPLETED_STATUS_SUCCESS;
    case chromeos::MOUNT_ERROR_UNKNOWN:
      return file_browser_private::MOUNT_COMPLETED_STATUS_ERROR_UNKNOWN;
    case chromeos::MOUNT_ERROR_INTERNAL:
      return file_browser_private::MOUNT_COMPLETED_STATUS_ERROR_INTERNAL;
    case chromeos::MOUNT_ERROR_INVALID_ARGUMENT:
      return file_browser_private::
          MOUNT_COMPLETED_STATUS_ERROR_INVALID_ARGUMENT;
    case chromeos::MOUNT_ERROR_INVALID_PATH:
      return file_browser_private::MOUNT_COMPLETED_STATUS_ERROR_INVALID_PATH;
    case chromeos::MOUNT_ERROR_PATH_ALREADY_MOUNTED:
      return file_browser_private::
          MOUNT_COMPLETED_STATUS_ERROR_PATH_ALREADY_MOUNTED;
    case chromeos::MOUNT_ERROR_PATH_NOT_MOUNTED:
      return file_browser_private::
          MOUNT_COMPLETED_STATUS_ERROR_PATH_NOT_MOUNTED;
    case chromeos::MOUNT_ERROR_DIRECTORY_CREATION_FAILED:
      return file_browser_private
          ::MOUNT_COMPLETED_STATUS_ERROR_DIRECTORY_CREATION_FAILED;
    case chromeos::MOUNT_ERROR_INVALID_MOUNT_OPTIONS:
      return file_browser_private
          ::MOUNT_COMPLETED_STATUS_ERROR_INVALID_MOUNT_OPTIONS;
    case chromeos::MOUNT_ERROR_INVALID_UNMOUNT_OPTIONS:
      return file_browser_private::
          MOUNT_COMPLETED_STATUS_ERROR_INVALID_UNMOUNT_OPTIONS;
    case chromeos::MOUNT_ERROR_INSUFFICIENT_PERMISSIONS:
      return file_browser_private::
          MOUNT_COMPLETED_STATUS_ERROR_INSUFFICIENT_PERMISSIONS;
    case chromeos::MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND:
      return file_browser_private::
          MOUNT_COMPLETED_STATUS_ERROR_MOUNT_PROGRAM_NOT_FOUND;
    case chromeos::MOUNT_ERROR_MOUNT_PROGRAM_FAILED:
      return file_browser_private::
          MOUNT_COMPLETED_STATUS_ERROR_MOUNT_PROGRAM_FAILED;
    case chromeos::MOUNT_ERROR_INVALID_DEVICE_PATH:
      return file_browser_private::
          MOUNT_COMPLETED_STATUS_ERROR_INVALID_DEVICE_PATH;
    case chromeos::MOUNT_ERROR_UNKNOWN_FILESYSTEM:
      return file_browser_private::
          MOUNT_COMPLETED_STATUS_ERROR_UNKNOWN_FILESYSTEM;
    case chromeos::MOUNT_ERROR_UNSUPPORTED_FILESYSTEM:
      return file_browser_private::
          MOUNT_COMPLETED_STATUS_ERROR_UNSUPORTED_FILESYSTEM;
    case chromeos::MOUNT_ERROR_INVALID_ARCHIVE:
      return file_browser_private::MOUNT_COMPLETED_STATUS_ERROR_INVALID_ARCHIVE;
    case chromeos::MOUNT_ERROR_NOT_AUTHENTICATED:
      return file_browser_private::MOUNT_COMPLETED_STATUS_ERROR_AUTHENTICATION;
    case chromeos::MOUNT_ERROR_PATH_UNMOUNTED:
      return file_browser_private::MOUNT_COMPLETED_STATUS_ERROR_PATH_UNMOUNTED;
  }
  NOTREACHED();
  return file_browser_private::MOUNT_COMPLETED_STATUS_NONE;
}

void BroadcastMountCompletedEvent(
    Profile* profile,
    file_browser_private::MountCompletedEventType event_type,
    chromeos::MountError error,
    const VolumeInfo& volume_info,
    bool is_remounting) {
  file_browser_private::MountCompletedEvent event;
  event.event_type = event_type;
  event.status = MountErrorToMountCompletedStatus(error);
  util::VolumeInfoToVolumeMetadata(
      profile, volume_info, &event.volume_metadata);
  event.is_remounting = is_remounting;

  BroadcastEvent(
      profile,
      file_browser_private::OnMountCompleted::kEventName,
      file_browser_private::OnMountCompleted::Create(event));
}

file_browser_private::CopyProgressStatusType
CopyProgressTypeToCopyProgressStatusType(
    fileapi::FileSystemOperation::CopyProgressType type) {
  switch (type) {
    case fileapi::FileSystemOperation::BEGIN_COPY_ENTRY:
      return file_browser_private::COPY_PROGRESS_STATUS_TYPE_BEGIN_COPY_ENTRY;
    case fileapi::FileSystemOperation::END_COPY_ENTRY:
      return file_browser_private::COPY_PROGRESS_STATUS_TYPE_END_COPY_ENTRY;
    case fileapi::FileSystemOperation::PROGRESS:
      return file_browser_private::COPY_PROGRESS_STATUS_TYPE_PROGRESS;
  }
  NOTREACHED();
  return file_browser_private::COPY_PROGRESS_STATUS_TYPE_NONE;
}

std::string FileErrorToErrorName(base::File::Error error_code) {
  namespace js = extensions::api::file_browser_private;
  switch (error_code) {
    case base::File::FILE_ERROR_NOT_FOUND:
      return "NotFoundError";
    case base::File::FILE_ERROR_INVALID_OPERATION:
    case base::File::FILE_ERROR_EXISTS:
    case base::File::FILE_ERROR_NOT_EMPTY:
      return "InvalidModificationError";
    case base::File::FILE_ERROR_NOT_A_DIRECTORY:
    case base::File::FILE_ERROR_NOT_A_FILE:
      return "TypeMismatchError";
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return "NoModificationAllowedError";
    case base::File::FILE_ERROR_FAILED:
      return "InvalidStateError";
    case base::File::FILE_ERROR_ABORT:
      return "AbortError";
    case base::File::FILE_ERROR_SECURITY:
      return "SecurityError";
    case base::File::FILE_ERROR_NO_SPACE:
      return "QuotaExceededError";
    case base::File::FILE_ERROR_INVALID_URL:
      return "EncodingError";
    default:
      return "InvalidModificationError";
  }
}

void GrantAccessForAddedProfileToRunningInstance(Profile* added_profile,
                                                 Profile* running_profile) {
  extensions::ProcessManager* const process_manager =
      extensions::ExtensionSystem::Get(running_profile)->process_manager();
  if (!process_manager)
    return;

  extensions::ExtensionHost* const extension_host =
      process_manager->GetBackgroundHostForExtension(kFileManagerAppId);
  if (!extension_host || !extension_host->render_process_host())
    return;

  const int id = extension_host->render_process_host()->GetID();
  file_manager::util::SetupProfileFileAccessPermissions(id, added_profile);
}

// Checks if we should send a progress event or not according to the
// |last_time| of sending an event. If |always| is true, the function always
// returns true. If the function returns true, the function also updates
// |last_time|.
bool ShouldSendProgressEvent(bool always, base::Time* last_time) {
  const base::Time now = base::Time::Now();
  const int64 delta = (now - *last_time).InMilliseconds();
  // delta < 0 may rarely happen if system clock is synced and rewinded.
  // To be conservative, we don't skip in that case.
  if (!always && 0 <= delta && delta < kProgressEventFrequencyInMilliseconds) {
    return false;
  } else {
    *last_time = now;
    return true;
  }
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
    : pref_change_registrar_(new PrefChangeRegistrar),
      profile_(profile),
      multi_user_window_manager_observer_registered_(false),
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

  chrome::MultiUserWindowManager* const multi_user_window_manager =
      chrome::MultiUserWindowManager::GetInstance();
  if (multi_user_window_manager &&
      multi_user_window_manager_observer_registered_) {
    multi_user_window_manager_observer_registered_ = false;
    multi_user_window_manager->RemoveObserver(this);
  }

  profile_ = NULL;
}

void EventRouter::ObserveEvents() {
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

  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_ADDED,
                              content::NotificationService::AllSources());
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
                                  base::File::Error error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  file_browser_private::CopyProgressStatus status;
  if (error == base::File::FILE_OK) {
    // Send success event.
    status.type = file_browser_private::COPY_PROGRESS_STATUS_TYPE_SUCCESS;
    status.source_url.reset(new std::string(source_url.spec()));
    status.destination_url.reset(new std::string(destination_url.spec()));
  } else {
    // Send error event.
    status.type = file_browser_private::COPY_PROGRESS_STATUS_TYPE_ERROR;
    status.error.reset(new std::string(FileErrorToErrorName(error)));
  }

  BroadcastEvent(
      profile_,
      file_browser_private::OnCopyProgress::kEventName,
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

  // Should not skip events other than TYPE_PROGRESS.
  const bool always =
      status.type != file_browser_private::COPY_PROGRESS_STATUS_TYPE_PROGRESS;
  if (!ShouldSendProgressEvent(always, &last_copy_progress_event_))
    return;

  BroadcastEvent(
      profile_,
      file_browser_private::OnCopyProgress::kEventName,
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
      file_browser_private::OnDriveConnectionStatusChanged::kEventName,
      file_browser_private::OnDriveConnectionStatusChanged::Create());
}

void EventRouter::OnFileManagerPrefsChanged() {
  if (!profile_ ||
      !extensions::ExtensionSystem::Get(profile_)->event_router()) {
    NOTREACHED();
    return;
  }

  BroadcastEvent(
      profile_,
      file_browser_private::OnPreferencesChanged::kEventName,
      file_browser_private::OnPreferencesChanged::Create());
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

  // When |always| flag is not set, we don't send the event until certain
  // amount of time passes after the previous one. This is to avoid
  // flooding the IPC between extensions by many onFileTransferUpdated events.
  if (!ShouldSendProgressEvent(always, &last_file_transfer_event_))
    return;

  // Convert the current |drive_jobs_| to IDL type.
  std::vector<linked_ptr<file_browser_private::FileTransferStatus> >
      status_list;
  for (std::map<drive::JobID, DriveJobInfoWithStatus>::iterator
           iter = drive_jobs_.begin(); iter != drive_jobs_.end(); ++iter) {
    linked_ptr<file_browser_private::FileTransferStatus> status(
        new file_browser_private::FileTransferStatus());
    JobInfoToTransferStatus(profile_,
                            kFileManagerAppId,
                            iter->second.status,
                            iter->second.job_info,
                            status.get());
    status_list.push_back(status);
  }
  BroadcastEvent(
      profile_,
      file_browser_private::OnFileTransfersUpdated::kEventName,
      file_browser_private::OnFileTransfersUpdated::Create(status_list));
}

void EventRouter::OnDirectoryChanged(const base::FilePath& drive_path) {
  HandleFileWatchNotification(drive_path, false);
}

void EventRouter::OnDriveSyncError(drive::file_system::DriveSyncErrorType type,
                                   const base::FilePath& drive_path) {
  file_browser_private::DriveSyncErrorEvent event;
  switch (type) {
    case drive::file_system::DRIVE_SYNC_ERROR_DELETE_WITHOUT_PERMISSION:
      event.type =
          file_browser_private::DRIVE_SYNC_ERROR_TYPE_DELETE_WITHOUT_PERMISSION;
      break;
    case drive::file_system::DRIVE_SYNC_ERROR_SERVICE_UNAVAILABLE:
      event.type =
          file_browser_private::DRIVE_SYNC_ERROR_TYPE_SERVICE_UNAVAILABLE;
      break;
    case drive::file_system::DRIVE_SYNC_ERROR_MISC:
      event.type =
          file_browser_private::DRIVE_SYNC_ERROR_TYPE_MISC;
      break;
  }
  event.file_url = util::ConvertDrivePathToFileSystemUrl(
      profile_, drive_path, kFileManagerAppId).spec();
  BroadcastEvent(
      profile_,
      file_browser_private::OnDriveSyncError::kEventName,
      file_browser_private::OnDriveSyncError::Create(event));
}

void EventRouter::OnRefreshTokenInvalid() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Raise a DriveConnectionStatusChanged event to notify the status offline.
  BroadcastEvent(
      profile_,
      file_browser_private::OnDriveConnectionStatusChanged::kEventName,
      file_browser_private::OnDriveConnectionStatusChanged::Create());
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

    FileDefinition file_definition;
    file_definition.virtual_path = virtual_path;
    file_definition.is_directory = true;

    file_manager::util::ConvertFileDefinitionToEntryDefinition(
        profile_,
        extension_id,
        file_definition,
        base::Bind(
            &EventRouter::DispatchDirectoryChangeEventWithEntryDefinition,
            weak_factory_.GetWeakPtr(),
            got_error));
  }
}

void EventRouter::DispatchDirectoryChangeEventWithEntryDefinition(
    bool watcher_error,
    const EntryDefinition& entry_definition) {
  if (entry_definition.error != base::File::FILE_OK) {
    DVLOG(1) << "Unable to dispatch event because resolving the entry "
             << "definition failed.";
    return;
  }

  file_browser_private::FileWatchEvent event;
  event.event_type = watcher_error
      ? file_browser_private::FILE_WATCH_EVENT_TYPE_ERROR
      : file_browser_private::FILE_WATCH_EVENT_TYPE_CHANGED;

  event.entry.additional_properties.SetString(
      "fileSystemName", entry_definition.file_system_name);
  event.entry.additional_properties.SetString(
      "fileSystemRoot", entry_definition.file_system_root_url);
  event.entry.additional_properties.SetString(
      "fileFullPath", "/" + entry_definition.full_path.value());
  event.entry.additional_properties.SetBoolean("fileIsDirectory",
                                               entry_definition.is_directory);

  BroadcastEvent(profile_,
                 file_browser_private::OnDirectoryChanged::kEventName,
                 file_browser_private::OnDirectoryChanged::Create(event));
}

void EventRouter::ShowRemovableDeviceInFileManager(
    const base::FilePath& mount_path) {
  // Do not attempt to open File Manager while the login is in progress or
  // the screen is locked or running in kiosk app mode and make sure the file
  // manager is opened only for the active user.
  if (chromeos::LoginDisplayHostImpl::default_host() ||
      chromeos::ScreenLocker::default_screen_locker() ||
      chrome::IsRunningInForcedAppMode() ||
      profile_ != ProfileManager::GetActiveUserProfile())
    return;

  // Do not pop-up the File Manager, if the recovery tool is running.
  if (IsRecoveryToolRunning(profile_))
    return;

  // According to DCF (Design rule of Camera File system) by JEITA / CP-3461
  // cameras should have pictures located in the DCIM root directory.
  const base::FilePath dcim_path = mount_path.Append(
      FILE_PATH_LITERAL("DCIM"));

  // If there is a DCIM folder and Google+ Photos is installed, then do not
  // launch Files.app.
  DirectoryExistsOnUIThread(
      dcim_path,
      IsGooglePhotosInstalled(profile_)
          ? base::Bind(&base::DoNothing)
          : base::Bind(&util::OpenRemovableDrive, profile_, mount_path),
      base::Bind(&util::OpenRemovableDrive, profile_, mount_path));
}

void EventRouter::DispatchDeviceEvent(
    file_browser_private::DeviceEventType type,
    const std::string& device_path) {
  file_browser_private::DeviceEvent event;
  event.type = type;
  event.device_path = device_path;
  BroadcastEvent(profile_,
                 file_browser_private::OnDeviceChanged::kEventName,
                 file_browser_private::OnDeviceChanged::Create(event));
}

void EventRouter::OnDiskAdded(
    const DiskMountManager::Disk& disk, bool mounting) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!mounting) {
    // If the disk is not being mounted, we don't want the Scanning
    // notification to persist.
    DispatchDeviceEvent(
        file_browser_private::DEVICE_EVENT_TYPE_SCAN_CANCELED,
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
    DispatchDeviceEvent(
        file_browser_private::DEVICE_EVENT_TYPE_DISABLED,
        device_path);
    return;
  }

  DispatchDeviceEvent(
      file_browser_private::DEVICE_EVENT_TYPE_ADDED,
      device_path);
}

void EventRouter::OnDeviceRemoved(const std::string& device_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DispatchDeviceEvent(
      file_browser_private::DEVICE_EVENT_TYPE_REMOVED,
      device_path);
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
      file_browser_private::MOUNT_COMPLETED_EVENT_TYPE_MOUNT,
      error_code,
      volume_info,
      is_remounting);

  if (volume_info.type == VOLUME_TYPE_REMOVABLE_DISK_PARTITION &&
      !is_remounting) {
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
      file_browser_private::MOUNT_COMPLETED_EVENT_TYPE_UNMOUNT,
      error_code,
      volume_info,
      false);
}

void EventRouter::OnFormatStarted(const std::string& device_path,
                                  bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (success) {
    DispatchDeviceEvent(file_browser_private::DEVICE_EVENT_TYPE_FORMAT_START,
                        device_path);
  } else {
    DispatchDeviceEvent(file_browser_private::DEVICE_EVENT_TYPE_FORMAT_FAIL,
                        device_path);
  }
}

void EventRouter::OnFormatCompleted(const std::string& device_path,
                                    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DispatchDeviceEvent(success ?
                      file_browser_private::DEVICE_EVENT_TYPE_FORMAT_SUCCESS :
                      file_browser_private::DEVICE_EVENT_TYPE_FORMAT_FAIL,
                      device_path);
}

void EventRouter::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_PROFILE_ADDED) {
    Profile* const added_profile = content::Source<Profile>(source).ptr();
    if (!added_profile->IsOffTheRecord())
      GrantAccessForAddedProfileToRunningInstance(added_profile, profile_);

    BroadcastEvent(profile_,
                   file_browser_private::OnProfileAdded::kEventName,
                   file_browser_private::OnProfileAdded::Create());
  }
}

void EventRouter::RegisterMultiUserWindowManagerObserver() {
  if (multi_user_window_manager_observer_registered_)
    return;
  chrome::MultiUserWindowManager* const multi_user_window_manager =
      chrome::MultiUserWindowManager::GetInstance();
  if (multi_user_window_manager) {
    multi_user_window_manager->AddObserver(this);
    multi_user_window_manager_observer_registered_ = true;
  }
}

void EventRouter::OnOwnerEntryChanged(aura::Window* window) {
  BroadcastEvent(profile_,
                 file_browser_private::OnDesktopChanged::kEventName,
                 file_browser_private::OnDesktopChanged::Create());
}

}  // namespace file_manager
