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
#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/sync_direction.h"
#include "chrome/browser/sync_file_system/sync_event_observer.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "chrome/browser/sync_file_system/sync_process_runner.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::ExtensionPrefs;
using fileapi::FileSystemURL;
using fileapi::FileSystemURLSet;

namespace sync_file_system {

namespace {

const char kLocalSyncName[] = "Local sync";
const char kRemoteSyncName[] = "Remote sync";
const char kRemoteSyncNameV2[] = "Remote sync (v2)";

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

// We need this indirection because WeakPtr can only be bound to methods
// without a return value.
LocalChangeProcessor* GetLocalChangeProcessorAdapter(
    base::WeakPtr<SyncFileSystemService> service,
    const GURL& origin) {
  if (!service)
    return NULL;
  return service->GetLocalChangeProcessor(origin);
}

}  // namespace

//---------------------------------------------------------------------------
// SyncProcessRunner's.

// SyncProcessRunner implementation for LocalSync.
class LocalSyncRunner : public SyncProcessRunner,
                        public LocalFileSyncService::Observer {
 public:
  LocalSyncRunner(const std::string& name,
                  SyncFileSystemService* sync_service)
      : SyncProcessRunner(name, sync_service),
        factory_(this) {}

  virtual void StartSync(const SyncStatusCallback& callback) OVERRIDE {
    sync_service()->local_service_->ProcessLocalChange(
        base::Bind(&LocalSyncRunner::DidProcessLocalChange,
                   factory_.GetWeakPtr(), callback));
  }

  // LocalFileSyncService::Observer overrides.
  virtual void OnLocalChangeAvailable(int64 pending_changes) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    OnChangesUpdated(pending_changes);

    // Kick other sync runners just in case they're not running.
    sync_service()->RunForEachSyncRunners(
        &SyncProcessRunner::ScheduleIfNotRunning);
  }

 private:
  void DidProcessLocalChange(
      const SyncStatusCallback& callback,
      SyncStatusCode status,
      const FileSystemURL& url) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "ProcessLocalChange finished with status=%d (%s) for url=%s",
              status, SyncStatusCodeToString(status),
              url.DebugString().c_str());
    callback.Run(status);
  }

  base::WeakPtrFactory<LocalSyncRunner> factory_;
  DISALLOW_COPY_AND_ASSIGN(LocalSyncRunner);
};

// SyncProcessRunner implementation for RemoteSync.
class RemoteSyncRunner : public SyncProcessRunner,
                         public RemoteFileSyncService::Observer {
 public:
  RemoteSyncRunner(const std::string& name,
                   SyncFileSystemService* sync_service,
                   RemoteFileSyncService* remote_service)
      : SyncProcessRunner(name, sync_service),
        remote_service_(remote_service),
        last_state_(REMOTE_SERVICE_OK),
        factory_(this) {}

  virtual void StartSync(const SyncStatusCallback& callback) OVERRIDE {
    remote_service_->ProcessRemoteChange(
        base::Bind(&RemoteSyncRunner::DidProcessRemoteChange,
                   factory_.GetWeakPtr(), callback));
  }

  virtual SyncServiceState GetServiceState() OVERRIDE {
    return RemoteStateToSyncServiceState(last_state_);
  }

  // RemoteFileSyncService::Observer overrides.
  virtual void OnRemoteChangeQueueUpdated(int64 pending_changes) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    OnChangesUpdated(pending_changes);

    // Kick other sync runners just in case they're not running.
    sync_service()->RunForEachSyncRunners(
        &SyncProcessRunner::ScheduleIfNotRunning);
  }

  virtual void OnRemoteServiceStateUpdated(
      RemoteServiceState state,
      const std::string& description) OVERRIDE {
    // Just forward to SyncFileSystemService.
    sync_service()->OnRemoteServiceStateUpdated(state, description);
    last_state_ = state;
  }

 private:
  void DidProcessRemoteChange(
      const SyncStatusCallback& callback,
      SyncStatusCode status,
      const FileSystemURL& url) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "ProcessRemoteChange finished with status=%d (%s) for url=%s",
              status, SyncStatusCodeToString(status),
              url.DebugString().c_str());

    if (status == SYNC_STATUS_FILE_BUSY) {
      sync_service()->local_service_->RegisterURLForWaitingSync(
          url, base::Bind(&RemoteSyncRunner::Schedule,
                          factory_.GetWeakPtr()));
    }
    callback.Run(status);
  }

  RemoteFileSyncService* remote_service_;
  RemoteServiceState last_state_;
  base::WeakPtrFactory<RemoteSyncRunner> factory_;
  DISALLOW_COPY_AND_ASSIGN(RemoteSyncRunner);
};

//-----------------------------------------------------------------------------
// SyncFileSystemService

void SyncFileSystemService::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  local_service_->Shutdown();
  local_service_.reset();

  remote_service_.reset();
  v2_remote_service_.reset();

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
  DCHECK(local_service_);
  DCHECK(remote_service_);
  DCHECK(app_origin == app_origin.GetOrigin());

  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "Initializing for App: %s", app_origin.spec().c_str());

  local_service_->MaybeInitializeFileSystemContext(
      app_origin, file_system_context,
      base::Bind(&SyncFileSystemService::DidInitializeFileSystem,
                 AsWeakPtr(), app_origin, callback));
}

SyncServiceState SyncFileSystemService::GetSyncServiceState() {
  // For now we always query the state from the main RemoteFileSyncService.
  return RemoteStateToSyncServiceState(remote_service_->GetCurrentState());
}

void SyncFileSystemService::GetExtensionStatusMap(
    std::map<GURL, std::string>* status_map) {
  DCHECK(status_map);
  status_map->clear();
  remote_service_->GetOriginStatusMap(status_map);
  if (v2_remote_service_)
    v2_remote_service_->GetOriginStatusMap(status_map);
}

void SyncFileSystemService::DumpFiles(const GURL& origin,
                                      const DumpFilesCallback& callback) {
  DCHECK(!origin.is_empty());

  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartitionForSite(profile_, origin);
  fileapi::FileSystemContext* file_system_context =
      storage_partition->GetFileSystemContext();
  local_service_->MaybeInitializeFileSystemContext(
      origin, file_system_context,
      base::Bind(&SyncFileSystemService::DidInitializeFileSystemForDump,
                 AsWeakPtr(), origin, callback));
}

scoped_ptr<base::ListValue> SyncFileSystemService::DumpDatabase() {
  scoped_ptr<base::ListValue> list = remote_service_->DumpDatabase();
  if (!list)
    list.reset(new base::ListValue);
  if (v2_remote_service_) {
    scoped_ptr<base::ListValue> v2list = v2_remote_service_->DumpDatabase();
    if (!v2list)
      return list.Pass();
    for (base::ListValue::iterator itr = v2list->begin();
         itr != v2list->end(); ) {
      scoped_ptr<base::Value> item;
      itr = v2list->Erase(itr, &item);
      list->Append(item.release());
    }
  }
  return list.Pass();
}

void SyncFileSystemService::GetFileSyncStatus(
    const FileSystemURL& url, const SyncFileStatusCallback& callback) {
  DCHECK(local_service_);
  DCHECK(GetRemoteService(url.origin()));

  // It's possible to get an invalid FileEntry.
  if (!url.is_valid()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   SYNC_FILE_ERROR_INVALID_URL,
                   SYNC_FILE_STATUS_UNKNOWN));
    return;
  }

  if (GetRemoteService(url.origin())->IsConflicting(url)) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   SYNC_STATUS_OK,
                   SYNC_FILE_STATUS_CONFLICTING));
    return;
  }

  local_service_->HasPendingLocalChanges(
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

ConflictResolutionPolicy SyncFileSystemService::GetConflictResolutionPolicy(
    const GURL& origin) {
  return GetRemoteService(origin)->GetConflictResolutionPolicy(origin);
}

SyncStatusCode SyncFileSystemService::SetConflictResolutionPolicy(
    const GURL& origin,
    ConflictResolutionPolicy policy) {
  return GetRemoteService(origin)->SetConflictResolutionPolicy(origin, policy);
}

LocalChangeProcessor* SyncFileSystemService::GetLocalChangeProcessor(
    const GURL& origin) {
  return GetRemoteService(origin)->GetLocalChangeProcessor();
}

SyncFileSystemService::SyncFileSystemService(Profile* profile)
    : profile_(profile),
      sync_enabled_(true) {
}

void SyncFileSystemService::Initialize(
    scoped_ptr<LocalFileSyncService> local_service,
    scoped_ptr<RemoteFileSyncService> remote_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(local_service);
  DCHECK(remote_service);
  DCHECK(profile_);

  local_service_ = local_service.Pass();
  remote_service_ = remote_service.Pass();

  scoped_ptr<LocalSyncRunner> local_syncer(
      new LocalSyncRunner(kLocalSyncName, this));
  scoped_ptr<RemoteSyncRunner> remote_syncer(
      new RemoteSyncRunner(kRemoteSyncName, this, remote_service_.get()));

  local_service_->AddChangeObserver(local_syncer.get());
  local_service_->SetLocalChangeProcessorCallback(
      base::Bind(&GetLocalChangeProcessorAdapter, AsWeakPtr()));

  remote_service_->AddServiceObserver(remote_syncer.get());
  remote_service_->AddFileStatusObserver(this);
  remote_service_->SetRemoteChangeProcessor(local_service_.get());

  local_sync_runners_.push_back(local_syncer.release());
  remote_sync_runners_.push_back(remote_syncer.release());

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
  GetRemoteService(app_origin)->RegisterOrigin(
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

  if (status == SYNC_STATUS_FAILED) {
    // If we got generic error return the service status information.
    switch (GetRemoteService(app_origin)->GetCurrentState()) {
      case REMOTE_SERVICE_AUTHENTICATION_REQUIRED:
        callback.Run(SYNC_STATUS_AUTHENTICATION_FAILED);
        return;
      case REMOTE_SERVICE_TEMPORARY_UNAVAILABLE:
        callback.Run(SYNC_STATUS_SERVICE_TEMPORARILY_UNAVAILABLE);
        return;
      default:
        break;
    }
  }

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

  base::ListValue* files =
      GetRemoteService(origin)->DumpFiles(origin).release();
  if (!files) {
    callback.Run(new base::ListValue);
    return;
  }

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
  remote_service_->SetSyncEnabled(sync_enabled_);
  if (v2_remote_service_)
    v2_remote_service_->SetSyncEnabled(sync_enabled_);
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

void SyncFileSystemService::OnSyncIdle() {
  int64 remote_changes = 0;
  for (ScopedVector<SyncProcessRunner>::iterator iter =
           remote_sync_runners_.begin();
       iter != remote_sync_runners_.end(); ++iter)
    remote_changes += (*iter)->pending_changes();
  if (remote_changes == 0)
    local_service_->PromoteDemotedChanges();

  int64 local_changes = 0;
  for (ScopedVector<SyncProcessRunner>::iterator iter =
           local_sync_runners_.begin();
       iter != local_sync_runners_.end(); ++iter)
    local_changes += (*iter)->pending_changes();
  if (local_changes == 0 && v2_remote_service_)
    v2_remote_service_->PromoteDemotedChanges();
}

void SyncFileSystemService::OnRemoteServiceStateUpdated(
    RemoteServiceState state,
    const std::string& description) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "OnRemoteServiceStateChanged: %d %s", state, description.c_str());

  FOR_EACH_OBSERVER(
      SyncEventObserver, observers_,
      OnSyncStateUpdated(GURL(),
                         RemoteStateToSyncServiceState(state),
                         description));

  RunForEachSyncRunners(&SyncProcessRunner::Schedule);
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
  const Extension* extension =
      content::Details<const extensions::InstalledExtensionInfo>(details)->
          extension;
  GURL app_origin = Extension::GetBaseURLFromExtensionId(extension->id());
  DVLOG(1) << "Handle extension notification for INSTALLED: " << app_origin;
  // NOTE: When an app is uninstalled and re-installed in a sequence,
  // |local_service_| may still keeps |app_origin| as disabled origin.
  local_service_->SetOriginEnabled(app_origin, true);
}

void SyncFileSystemService::HandleExtensionUnloaded(
    int type,
    const content::NotificationDetails& details) {
  content::Details<const extensions::UnloadedExtensionInfo> info(details);
  if (info->reason != extensions::UnloadedExtensionInfo::REASON_DISABLE)
    return;

  std::string extension_id = info->extension->id();
  GURL app_origin = Extension::GetBaseURLFromExtensionId(extension_id);

  int reasons = ExtensionPrefs::Get(profile_)->GetDisableReasons(extension_id);
  if (reasons & Extension::DISABLE_RELOAD) {
    // Bypass disabling the origin since the app will be re-enabled soon.
    // NOTE: If re-enabling the app fails, the app is disabled while it is
    // handled as enabled origin in the SyncFS. This should be safe and will be
    // recovered when the user re-enables the app manually or the sync service
    // restarts.
    DVLOG(1) << "Handle extension notification for UNLOAD(DISABLE_RELOAD): "
             << app_origin;
    return;
  }

  DVLOG(1) << "Handle extension notification for UNLOAD(DISABLE): "
           << app_origin;
  GetRemoteService(app_origin)->DisableOrigin(
      app_origin,
      base::Bind(&DidHandleOriginForExtensionUnloadedEvent,
                 type, app_origin));
  local_service_->SetOriginEnabled(app_origin, false);
}

void SyncFileSystemService::HandleExtensionUninstalled(
    int type,
    const content::NotificationDetails& details) {
  const Extension* extension = content::Details<const Extension>(details).ptr();
  DCHECK(extension);

  RemoteFileSyncService::UninstallFlag flag =
      RemoteFileSyncService::UNINSTALL_AND_PURGE_REMOTE;
  // If it's loaded from an unpacked package and with key: field,
  // the uninstall will not be sync'ed and the user might be using the
  // same app key in other installs, so avoid purging the remote folder.
  if (extensions::Manifest::IsUnpackedLocation(extension->location()) &&
      extension->manifest()->HasKey(extensions::manifest_keys::kKey)) {
    flag = RemoteFileSyncService::UNINSTALL_AND_KEEP_REMOTE;
  }

  GURL app_origin = Extension::GetBaseURLFromExtensionId(extension->id());
  DVLOG(1) << "Handle extension notification for UNINSTALLED: "
           << app_origin;
  GetRemoteService(app_origin)->UninstallOrigin(
      app_origin, flag,
      base::Bind(&DidHandleOriginForExtensionUnloadedEvent,
                 type, app_origin));
  local_service_->SetOriginEnabled(app_origin, false);
}

void SyncFileSystemService::HandleExtensionEnabled(
    int type,
    const content::NotificationDetails& details) {
  std::string extension_id = content::Details<const Extension>(details)->id();
  GURL app_origin = Extension::GetBaseURLFromExtensionId(extension_id);
  DVLOG(1) << "Handle extension notification for ENABLED: " << app_origin;
  GetRemoteService(app_origin)->EnableOrigin(
      app_origin,
      base::Bind(&DidHandleOriginForExtensionEnabledEvent, type, app_origin));
  local_service_->SetOriginEnabled(app_origin, true);
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
  remote_service_->SetSyncEnabled(sync_enabled_);
  if (v2_remote_service_)
    v2_remote_service_->SetSyncEnabled(sync_enabled_);
  if (!old_sync_enabled && sync_enabled_)
    RunForEachSyncRunners(&SyncProcessRunner::Schedule);
}

void SyncFileSystemService::RunForEachSyncRunners(
    void(SyncProcessRunner::*method)()) {
  for (ScopedVector<SyncProcessRunner>::iterator iter =
           local_sync_runners_.begin();
       iter != local_sync_runners_.end(); ++iter)
    ((*iter)->*method)();
  for (ScopedVector<SyncProcessRunner>::iterator iter =
           remote_sync_runners_.begin();
       iter != remote_sync_runners_.end(); ++iter)
    ((*iter)->*method)();
}

RemoteFileSyncService* SyncFileSystemService::GetRemoteService(
    const GURL& origin) {
  if (IsV2Enabled())
    return remote_service_.get();
  if (!IsV2EnabledForOrigin(origin))
    return remote_service_.get();

  if (!v2_remote_service_) {
    v2_remote_service_ = RemoteFileSyncService::CreateForBrowserContext(
        RemoteFileSyncService::V2, profile_);
    scoped_ptr<RemoteSyncRunner> v2_remote_syncer(
        new RemoteSyncRunner(kRemoteSyncNameV2, this,
                             v2_remote_service_.get()));
    v2_remote_service_->AddServiceObserver(v2_remote_syncer.get());
    v2_remote_service_->AddFileStatusObserver(this);
    v2_remote_service_->SetRemoteChangeProcessor(local_service_.get());
    v2_remote_service_->SetSyncEnabled(sync_enabled_);
    v2_remote_service_->SetDefaultConflictResolutionPolicy(
        remote_service_->GetDefaultConflictResolutionPolicy());
    remote_sync_runners_.push_back(v2_remote_syncer.release());
  }
  return v2_remote_service_.get();
}

}  // namespace sync_file_system
