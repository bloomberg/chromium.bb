// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_service.h"

#include <string>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/sync_file_system/extension_sync_event_observer.h"
#include "chrome/browser/extensions/api/sync_file_system/sync_file_system_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/sync_direction.h"
#include "chrome/browser/sync_file_system/sync_event_observer.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "chrome/common/extensions/extension.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"

using content::BrowserThread;
using fileapi::FileSystemURL;
using fileapi::FileSystemURLSet;

namespace sync_file_system {

namespace {

const int64 kSyncDelayInSeconds = 1;
const int64 kSyncDelaySlowInSeconds = 30;  // Start with 30 sec + exp backoff.
const int64 kSyncDelayMaxInSeconds = 30 * 60;  // 30 min.

SyncServiceState RemoteStateToSyncServiceState(
    RemoteServiceState state) {
  switch (state) {
    case REMOTE_SERVICE_OK:
      return SYNC_SERVICE_RUNNING;
    case REMOTE_SERVICE_TEMPORARY_UNAVAILABLE:
      return SYNC_SERVICE_TEMPORARY_UNAVAILABLE;
    case REMOTE_SERVICE_AUTHENTICATION_REQUIRED:
      return SYNC_SERVICE_AUTHENTICATION_REQUIRED;
    case REMOTE_SERVICE_DISABLED:
      return SYNC_SERVICE_DISABLED;
  }
  NOTREACHED() << "Unknown remote service state: " << state;
  return SYNC_SERVICE_DISABLED;
}

void DidHandleOriginForExtensionUnloadedEvent(
    int type,
    const GURL& origin,
    SyncStatusCode code) {
  DCHECK(chrome::NOTIFICATION_EXTENSION_UNLOADED == type ||
         chrome::NOTIFICATION_EXTENSION_UNINSTALLED == type);
  if (code != SYNC_STATUS_OK &&
      code != SYNC_STATUS_UNKNOWN_ORIGIN) {
    switch (type) {
      case chrome::NOTIFICATION_EXTENSION_UNLOADED:
        util::Log(logging::LOG_WARNING,
                  FROM_HERE,
                  "Disabling origin for UNLOADED(DISABLE) failed: %s",
                  origin.spec().c_str());
        break;
      case chrome::NOTIFICATION_EXTENSION_UNINSTALLED:
        util::Log(logging::LOG_WARNING,
                  FROM_HERE,
                  "Uninstall origin for UNINSTALLED failed: %s",
                  origin.spec().c_str());
        break;
      default:
        break;
    }
  }
}

void DidHandleOriginForExtensionEnabledEvent(
    int type,
    const GURL& origin,
    SyncStatusCode code) {
  DCHECK(chrome::NOTIFICATION_EXTENSION_ENABLED == type);
  if (code != SYNC_STATUS_OK)
    util::Log(logging::LOG_WARNING,
              FROM_HERE,
              "Enabling origin for ENABLED failed: %s",
              origin.spec().c_str());
}

std::string SyncFileStatusToString(SyncFileStatus sync_file_status) {
  return extensions::api::sync_file_system::ToString(
      extensions::SyncFileStatusToExtensionEnum(sync_file_status));
}

// Gets called repeatedly until every SyncFileStatus has been mapped.
void DidGetFileSyncStatusForDump(
    base::ListValue* files,
    size_t* num_results,
    const SyncFileSystemService::DumpFilesCallback& callback,
    base::DictionaryValue* file,
    SyncStatusCode sync_status_code,
    SyncFileStatus sync_file_status) {
  DCHECK(files);
  DCHECK(num_results);

  if (file)
    file->SetString("status", SyncFileStatusToString(sync_file_status));

  // Once all results have been received, run the callback to signal end.
  DCHECK_LE(*num_results, files->GetSize());
  if (++*num_results < files->GetSize())
    return;

  callback.Run(files);
}

}  // namespace

class SyncFileSystemService::SyncRunner {
 public:
  typedef base::Callback<void(const SyncStatusCallback&)> Task;
  SyncRunner(const std::string& task_name,
             const Task& sync_task,
             RemoteFileSyncService* remote_service)
    : task_name_(task_name),
      sync_task_(sync_task),
      remote_service_(remote_service),
      current_delay_(0),
      last_delay_(0),
      pending_changes_(0),
      running_(false),
      factory_(this) {}
  ~SyncRunner() {}

  void Schedule() {
    int64 delay = kSyncDelayInSeconds;
    if (pending_changes_ == 0) {
      ScheduleInternal(kSyncDelayMaxInSeconds);
      return;
    }
    switch (remote_service_->GetCurrentState()) {
      case REMOTE_SERVICE_OK:
        delay = kSyncDelayInSeconds;
        break;

      case REMOTE_SERVICE_TEMPORARY_UNAVAILABLE:
        delay = kSyncDelaySlowInSeconds;
        if (last_delay_ >= kSyncDelaySlowInSeconds &&
            last_delay_ < kSyncDelayMaxInSeconds)
          delay = last_delay_ * 2;
        break;

      case REMOTE_SERVICE_AUTHENTICATION_REQUIRED:
      case REMOTE_SERVICE_DISABLED:
        delay = kSyncDelayMaxInSeconds;
        break;
    }
    ScheduleInternal(delay);
  }

  void ScheduleIfNotRunning() {
    if (!timer_.IsRunning())
      Schedule();
  }

  void OnChangesUpdated(int64 pending_changes) {
    DCHECK_GE(pending_changes, 0);
    if (pending_changes_ != pending_changes) {
      util::Log(logging::LOG_VERBOSE, FROM_HERE,
                "[%s] pending_changes updated: %" PRId64,
                task_name_.c_str(), pending_changes);
    }
    pending_changes_ = pending_changes;
    Schedule();
  }

  int64 pending_changes() const { return pending_changes_; }

 private:
  void Finished(SyncStatusCode status) {
    DCHECK(running_);
    running_ = false;
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[%s] * Finished (elapsed: %" PRId64 " sec)",
              task_name_.c_str(),
              (base::Time::Now() - last_scheduled_).InSeconds());
    if (status == SYNC_STATUS_NO_CHANGE_TO_SYNC ||
        status == SYNC_STATUS_FILE_BUSY)
      ScheduleInternal(kSyncDelayMaxInSeconds);
    else
      Schedule();
  }

  void Run() {
    if (running_)
      return;
    running_ = true;
    last_scheduled_ = base::Time::Now();
    last_delay_ = current_delay_;

    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[%s] * Started", task_name_.c_str());

    sync_task_.Run(base::Bind(&SyncRunner::Finished, factory_.GetWeakPtr()));
  }

  void ScheduleInternal(int64 delay) {
    base::TimeDelta time_to_next = base::TimeDelta::FromSeconds(delay);

    if (timer_.IsRunning()) {
      if (current_delay_ == delay)
        return;

      base::TimeDelta elapsed = base::Time::Now() - last_scheduled_;
      if (elapsed < time_to_next)
        time_to_next = time_to_next - elapsed;
      else
        time_to_next = base::TimeDelta::FromSeconds(kSyncDelayInSeconds);
      timer_.Stop();
    }

    if (current_delay_ != delay) {
      util::Log(logging::LOG_VERBOSE, FROM_HERE,
                "[%s] Scheduling task in %" PRId64 " secs",
                task_name_.c_str(), time_to_next.InSeconds());
    }
    current_delay_ = delay;

    timer_.Start(FROM_HERE, time_to_next, this, &SyncRunner::Run);
  }

  std::string task_name_;
  Task sync_task_;
  RemoteFileSyncService* remote_service_;
  base::OneShotTimer<SyncRunner> timer_;
  base::Time last_scheduled_;
  int64 current_delay_;
  int64 last_delay_;
  int64 pending_changes_;
  bool running_;
  base::WeakPtrFactory<SyncRunner> factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncRunner);
};

//-----------------------------------------------------------------------------

void SyncFileSystemService::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  local_sync_.reset();
  remote_sync_.reset();

  local_file_service_->Shutdown();
  local_file_service_.reset();

  remote_file_service_.reset();

  ProfileSyncServiceBase* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (profile_sync_service)
    profile_sync_service->RemoveObserver(this);

  profile_ = NULL;
}

SyncFileSystemService::~SyncFileSystemService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!profile_);
}

void SyncFileSystemService::InitializeForApp(
    fileapi::FileSystemContext* file_system_context,
    const GURL& app_origin,
    const SyncStatusCallback& callback) {
  DCHECK(local_file_service_);
  DCHECK(remote_file_service_);
  DCHECK(app_origin == app_origin.GetOrigin());

  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "Initializing for App: %s", app_origin.spec().c_str());

  local_file_service_->MaybeInitializeFileSystemContext(
      app_origin, file_system_context,
      base::Bind(&SyncFileSystemService::DidInitializeFileSystem,
                 AsWeakPtr(), app_origin, callback));
}

SyncServiceState SyncFileSystemService::GetSyncServiceState() {
  return RemoteStateToSyncServiceState(remote_file_service_->GetCurrentState());
}

void SyncFileSystemService::GetExtensionStatusMap(
    std::map<GURL, std::string>* status_map) {
  DCHECK(status_map);
  remote_file_service_->GetOriginStatusMap(status_map);
}

void SyncFileSystemService::DumpFiles(const GURL& origin,
                                      const DumpFilesCallback& callback) {
  DCHECK(!origin.is_empty());

  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartitionForSite(profile_, origin);
  fileapi::FileSystemContext* file_system_context =
      storage_partition->GetFileSystemContext();
  local_file_service_->MaybeInitializeFileSystemContext(
      origin, file_system_context,
      base::Bind(&SyncFileSystemService::DidInitializeFileSystemForDump,
                 AsWeakPtr(), origin, callback));
}

void SyncFileSystemService::GetFileSyncStatus(
    const FileSystemURL& url, const SyncFileStatusCallback& callback) {
  DCHECK(local_file_service_);
  DCHECK(remote_file_service_);

  // It's possible to get an invalid FileEntry.
  if (!url.is_valid()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   SYNC_FILE_ERROR_INVALID_URL,
                   SYNC_FILE_STATUS_UNKNOWN));
    return;
  }

  if (remote_file_service_->IsConflicting(url)) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   SYNC_STATUS_OK,
                   SYNC_FILE_STATUS_CONFLICTING));
    return;
  }

  local_file_service_->HasPendingLocalChanges(
      url,
      base::Bind(&SyncFileSystemService::DidGetLocalChangeStatus,
                 AsWeakPtr(), callback));
}

void SyncFileSystemService::AddSyncEventObserver(SyncEventObserver* observer) {
  observers_.AddObserver(observer);
}

void SyncFileSystemService::RemoveSyncEventObserver(
    SyncEventObserver* observer) {
  observers_.RemoveObserver(observer);
}

ConflictResolutionPolicy
SyncFileSystemService::GetConflictResolutionPolicy() const {
  return remote_file_service_->GetConflictResolutionPolicy();
}

SyncStatusCode SyncFileSystemService::SetConflictResolutionPolicy(
    ConflictResolutionPolicy policy) {
  return remote_file_service_->SetConflictResolutionPolicy(policy);
}

SyncFileSystemService::SyncFileSystemService(Profile* profile)
    : profile_(profile),
      sync_enabled_(true) {
}

void SyncFileSystemService::Initialize(
    scoped_ptr<LocalFileSyncService> local_file_service,
    scoped_ptr<RemoteFileSyncService> remote_file_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(local_file_service);
  DCHECK(remote_file_service);
  DCHECK(profile_);

  local_file_service_ = local_file_service.Pass();
  remote_file_service_ = remote_file_service.Pass();

  local_file_service_->AddChangeObserver(this);
  local_file_service_->SetLocalChangeProcessor(
      remote_file_service_->GetLocalChangeProcessor());

  remote_file_service_->AddServiceObserver(this);
  remote_file_service_->AddFileStatusObserver(this);
  remote_file_service_->SetRemoteChangeProcessor(local_file_service_.get());

  local_sync_.reset(new SyncRunner(
      "Local sync",
      base::Bind(&SyncFileSystemService::StartLocalSync, AsWeakPtr()),
      remote_file_service_.get()));
  remote_sync_.reset(new SyncRunner(
      "Remote sync",
      base::Bind(&SyncFileSystemService::StartRemoteSync, AsWeakPtr()),
      remote_file_service_.get()));

  ProfileSyncServiceBase* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (profile_sync_service) {
    UpdateSyncEnabledStatus(profile_sync_service);
    profile_sync_service->AddObserver(this);
  }

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_ENABLED,
                 content::Source<Profile>(profile_));
}

void SyncFileSystemService::DidInitializeFileSystem(
    const GURL& app_origin,
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  DVLOG(1) << "DidInitializeFileSystem: "
           << app_origin.spec() << " " << status;

  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }

  // Local side of initialization for the app is done.
  // Continue on initializing the remote side.
  remote_file_service_->RegisterOrigin(
      app_origin,
      base::Bind(&SyncFileSystemService::DidRegisterOrigin,
                 AsWeakPtr(), app_origin, callback));
}

void SyncFileSystemService::DidRegisterOrigin(
    const GURL& app_origin,
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "DidInitializeForApp (registered the origin): %s: %s",
            app_origin.spec().c_str(),
            SyncStatusCodeToString(status));

  callback.Run(status);
}

void SyncFileSystemService::DidInitializeFileSystemForDump(
    const GURL& origin,
    const DumpFilesCallback& callback,
    SyncStatusCode status) {
  DCHECK(!origin.is_empty());

  if (status != SYNC_STATUS_OK) {
    base::ListValue empty_result;
    callback.Run(&empty_result);
    return;
  }

  base::ListValue* files = remote_file_service_->DumpFiles(origin).release();
  if (!files->GetSize()) {
    callback.Run(files);
    return;
  }

  base::Callback<void(base::DictionaryValue* file,
                      SyncStatusCode sync_status,
                      SyncFileStatus sync_file_status)> completion_callback =
      base::Bind(&DidGetFileSyncStatusForDump, base::Owned(files),
                 base::Owned(new size_t(0)), callback);

  // After all metadata loaded, sync status can be added to each entry.
  for (size_t i = 0; i < files->GetSize(); ++i) {
    base::DictionaryValue* file = NULL;
    std::string path_string;
    if (!files->GetDictionary(i, &file) ||
        !file->GetString("path", &path_string)) {
      NOTREACHED();
      completion_callback.Run(
          NULL, SYNC_FILE_ERROR_FAILED, SYNC_FILE_STATUS_UNKNOWN);
      continue;
    }

    base::FilePath file_path = base::FilePath::FromUTF8Unsafe(path_string);
    FileSystemURL url = CreateSyncableFileSystemURL(origin, file_path);
    GetFileSyncStatus(url, base::Bind(completion_callback, file));
  }
}

void SyncFileSystemService::SetSyncEnabledForTesting(bool enabled) {
  sync_enabled_ = enabled;
  remote_file_service_->SetSyncEnabled(sync_enabled_);
}

void SyncFileSystemService::StartLocalSync(
    const SyncStatusCallback& callback) {
  local_file_service_->ProcessLocalChange(
      base::Bind(&SyncFileSystemService::DidProcessLocalChange, AsWeakPtr(),
                 callback));
}

void SyncFileSystemService::StartRemoteSync(
    const SyncStatusCallback& callback) {
  remote_file_service_->ProcessRemoteChange(
      base::Bind(&SyncFileSystemService::DidProcessRemoteChange, AsWeakPtr(),
                 callback));
}

void SyncFileSystemService::DidProcessRemoteChange(
    const SyncStatusCallback& callback,
    SyncStatusCode status,
    const FileSystemURL& url) {
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "ProcessRemoteChange finished with status=%d (%s) for url=%s",
            status, SyncStatusCodeToString(status), url.DebugString().c_str());

  if (url.is_valid())
    local_file_service_->ClearSyncFlagForURL(url);

  if (status == SYNC_STATUS_FILE_BUSY) {
    local_file_service_->RegisterURLForWaitingSync(
        url, base::Bind(&SyncFileSystemService::OnSyncEnabledForRemoteSync,
                        AsWeakPtr()));
  }

  callback.Run(status);
}

void SyncFileSystemService::DidProcessLocalChange(
    const SyncStatusCallback& callback,
    SyncStatusCode status,
    const FileSystemURL& url) {
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "ProcessLocalChange finished with status=%d (%s) for url=%s",
            status, SyncStatusCodeToString(status), url.DebugString().c_str());

  if (url.is_valid())
    local_file_service_->ClearSyncFlagForURL(url);

  callback.Run(status);
}

void SyncFileSystemService::DidGetLocalChangeStatus(
    const SyncFileStatusCallback& callback,
    SyncStatusCode status,
    bool has_pending_local_changes) {
  callback.Run(
      status,
      has_pending_local_changes ?
          SYNC_FILE_STATUS_HAS_PENDING_CHANGES : SYNC_FILE_STATUS_SYNCED);
}

void SyncFileSystemService::OnSyncEnabledForRemoteSync() {
  remote_sync_->Schedule();
}

void SyncFileSystemService::OnLocalChangeAvailable(int64 pending_changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  local_sync_->OnChangesUpdated(pending_changes);
  remote_sync_->ScheduleIfNotRunning();
}

void SyncFileSystemService::OnRemoteChangeQueueUpdated(int64 pending_changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  remote_sync_->OnChangesUpdated(pending_changes);
  local_sync_->ScheduleIfNotRunning();
}

void SyncFileSystemService::OnRemoteServiceStateUpdated(
    RemoteServiceState state,
    const std::string& description) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  util::Log(logging::LOG_INFO, FROM_HERE,
            "OnRemoteServiceStateChanged: %d %s", state, description.c_str());

  remote_sync_->Schedule();
  local_sync_->Schedule();

  FOR_EACH_OBSERVER(
      SyncEventObserver, observers_,
      OnSyncStateUpdated(GURL(),
                         RemoteStateToSyncServiceState(state),
                         description));
}

void SyncFileSystemService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // Event notification sequence.
  //
  // (User action)    (Notification type)
  // Install:         INSTALLED.
  // Update:          INSTALLED.
  // Uninstall:       UNINSTALLED.
  // Launch, Close:   No notification.
  // Enable:          ENABLED.
  // Disable:         UNLOADED(DISABLE).
  // Reload, Restart: UNLOADED(DISABLE) -> INSTALLED -> ENABLED.
  //
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED:
      HandleExtensionInstalled(details);
      break;
    case chrome::NOTIFICATION_EXTENSION_UNLOADED:
      HandleExtensionUnloaded(type, details);
      break;
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED:
      HandleExtensionUninstalled(type, details);
      break;
    case chrome::NOTIFICATION_EXTENSION_ENABLED:
      HandleExtensionEnabled(type, details);
      break;
    default:
      NOTREACHED() << "Unknown notification.";
      break;
  }
}

void SyncFileSystemService::HandleExtensionInstalled(
    const content::NotificationDetails& details) {
  const extensions::Extension* extension =
      content::Details<const extensions::InstalledExtensionInfo>(details)->
          extension;
  GURL app_origin =
      extensions::Extension::GetBaseURLFromExtensionId(extension->id());
  DVLOG(1) << "Handle extension notification for INSTALLED: " << app_origin;
  // NOTE: When an app is uninstalled and re-installed in a sequence,
  // |local_file_service_| may still keeps |app_origin| as disabled origin.
  local_file_service_->SetOriginEnabled(app_origin, true);
}

void SyncFileSystemService::HandleExtensionUnloaded(
    int type,
    const content::NotificationDetails& details) {
  content::Details<const extensions::UnloadedExtensionInfo> info(details);
  std::string extension_id = info->extension->id();
  GURL app_origin =
      extensions::Extension::GetBaseURLFromExtensionId(extension_id);
  if (info->reason != extension_misc::UNLOAD_REASON_DISABLE)
    return;
  DVLOG(1) << "Handle extension notification for UNLOAD(DISABLE): "
           << app_origin;
  remote_file_service_->DisableOrigin(
      app_origin,
      base::Bind(&DidHandleOriginForExtensionUnloadedEvent,
                 type, app_origin));
  local_file_service_->SetOriginEnabled(app_origin, false);
}

void SyncFileSystemService::HandleExtensionUninstalled(
    int type,
    const content::NotificationDetails& details) {
  std::string extension_id =
      content::Details<const extensions::Extension>(details)->id();
  GURL app_origin =
      extensions::Extension::GetBaseURLFromExtensionId(extension_id);
  DVLOG(1) << "Handle extension notification for UNINSTALLED: "
           << app_origin;
  remote_file_service_->UninstallOrigin(
      app_origin,
      base::Bind(&DidHandleOriginForExtensionUnloadedEvent,
                 type, app_origin));
  local_file_service_->SetOriginEnabled(app_origin, false);
}

void SyncFileSystemService::HandleExtensionEnabled(
    int type,
    const content::NotificationDetails& details) {
  std::string extension_id =
      content::Details<const extensions::Extension>(details)->id();
  GURL app_origin =
      extensions::Extension::GetBaseURLFromExtensionId(extension_id);
  DVLOG(1) << "Handle extension notification for ENABLED: " << app_origin;
  remote_file_service_->EnableOrigin(
      app_origin,
      base::Bind(&DidHandleOriginForExtensionEnabledEvent, type, app_origin));
  local_file_service_->SetOriginEnabled(app_origin, true);
}

void SyncFileSystemService::OnStateChanged() {
  ProfileSyncServiceBase* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (profile_sync_service)
    UpdateSyncEnabledStatus(profile_sync_service);
}

void SyncFileSystemService::OnFileStatusChanged(
    const FileSystemURL& url,
    SyncFileStatus sync_status,
    SyncAction action_taken,
    SyncDirection direction) {
  FOR_EACH_OBSERVER(
      SyncEventObserver, observers_,
      OnFileSynced(url, sync_status, action_taken, direction));
}

void SyncFileSystemService::UpdateSyncEnabledStatus(
    ProfileSyncServiceBase* profile_sync_service) {
  if (!profile_sync_service->HasSyncSetupCompleted())
    return;
  bool old_sync_enabled = sync_enabled_;
  sync_enabled_ = profile_sync_service->GetActiveDataTypes().Has(
      syncer::APPS);
  remote_file_service_->SetSyncEnabled(sync_enabled_);
  if (!old_sync_enabled && sync_enabled_) {
    local_sync_->Schedule();
    remote_sync_->Schedule();
  }
}

}  // namespace sync_file_system
