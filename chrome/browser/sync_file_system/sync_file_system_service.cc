// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_service.h"

#include <string>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
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
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
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
    case REMOTE_SERVICE_STATE_MAX:
      NOTREACHED();
  }
  NOTREACHED() << "Unknown remote service state: " << state;
  return SYNC_SERVICE_DISABLED;
}

void DidHandleUninstalledEvent(const GURL& origin, SyncStatusCode code) {
  if (code != SYNC_STATUS_OK && code != SYNC_STATUS_UNKNOWN_ORIGIN) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "Failed to uninstall origin for uninstall event: %s",
              origin.spec().c_str());
  }
}

void DidHandleUnloadedEvent(const GURL& origin, SyncStatusCode code) {
  if (code != SYNC_STATUS_OK && code != SYNC_STATUS_UNKNOWN_ORIGIN) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "Failed to disable origin for unload event: %s",
              origin.spec().c_str());
  }
}

void DidHandleLoadEvent(
    const GURL& origin,
    SyncStatusCode code) {
  if (code != SYNC_STATUS_OK) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "Failed to enable origin for load event: %s",
              origin.spec().c_str());
  }
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

  callback.Run(*files);
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
      : SyncProcessRunner(name, sync_service,
                          scoped_ptr<SyncProcessRunner::TimerHelper>(), 1),
        factory_(this) {}

  virtual void StartSync(const SyncStatusCallback& callback) OVERRIDE {
    GetSyncService()->local_service_->ProcessLocalChange(
        base::Bind(&LocalSyncRunner::DidProcessLocalChange,
                   factory_.GetWeakPtr(), callback));
  }

  // LocalFileSyncService::Observer overrides.
  virtual void OnLocalChangeAvailable(int64 pending_changes) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    OnChangesUpdated(pending_changes);

    // Kick other sync runners just in case they're not running.
    GetSyncService()->RunForEachSyncRunners(&SyncProcessRunner::Schedule);
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
      : SyncProcessRunner(name, sync_service,
                          scoped_ptr<SyncProcessRunner::TimerHelper>(), 1),
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
    GetSyncService()->RunForEachSyncRunners(&SyncProcessRunner::Schedule);
  }

  virtual void OnRemoteServiceStateUpdated(
      RemoteServiceState state,
      const std::string& description) OVERRIDE {
    // Just forward to SyncFileSystemService.
    GetSyncService()->OnRemoteServiceStateUpdated(state, description);
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
      GetSyncService()->local_service_->RegisterURLForWaitingSync(
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

  local_sync_runners_.clear();
  remote_sync_runners_.clear();

  local_service_->Shutdown();
  local_service_.reset();

  remote_service_.reset();
  v2_remote_service_.reset();

  ProfileSyncServiceBase* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (profile_sync_service)
    profile_sync_service->RemoveObserver(this);

  ExtensionRegistry::Get(profile_)->RemoveObserver(this);

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

void SyncFileSystemService::GetExtensionStatusMap(
    const ExtensionStatusMapCallback& callback) {
  remote_service_->GetOriginStatusMap(
      base::Bind(&SyncFileSystemService::DidGetExtensionStatusMap,
                 AsWeakPtr(), callback));
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

void SyncFileSystemService::DumpDatabase(const DumpFilesCallback& callback) {
  remote_service_->DumpDatabase(
      base::Bind(&SyncFileSystemService::DidDumpDatabase,
                 AsWeakPtr(), callback));
}

void SyncFileSystemService::GetFileSyncStatus(
    const FileSystemURL& url, const SyncFileStatusCallback& callback) {
  DCHECK(local_service_);
  DCHECK(GetRemoteService(url.origin()));

  // It's possible to get an invalid FileEntry.
  if (!url.is_valid()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   SYNC_FILE_ERROR_INVALID_URL,
                   SYNC_FILE_STATUS_UNKNOWN));
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

LocalChangeProcessor* SyncFileSystemService::GetLocalChangeProcessor(
    const GURL& origin) {
  return GetRemoteService(origin)->GetLocalChangeProcessor();
}

void SyncFileSystemService::OnSyncIdle() {
  int64 remote_changes = 0;
  for (ScopedVector<SyncProcessRunner>::iterator iter =
           remote_sync_runners_.begin();
       iter != remote_sync_runners_.end(); ++iter)
    remote_changes += (*iter)->pending_changes();
  if (remote_changes == 0)
    local_service_->PromoteDemotedChanges(NoopClosure());

  int64 local_changes = 0;
  for (ScopedVector<SyncProcessRunner>::iterator iter =
           local_sync_runners_.begin();
       iter != local_sync_runners_.end(); ++iter)
    local_changes += (*iter)->pending_changes();
  if (local_changes == 0 && v2_remote_service_)
    v2_remote_service_->PromoteDemotedChanges(NoopClosure());
}

SyncServiceState SyncFileSystemService::GetSyncServiceState() {
  // For now we always query the state from the main RemoteFileSyncService.
  return RemoteStateToSyncServiceState(remote_service_->GetCurrentState());
}

SyncFileSystemService* SyncFileSystemService::GetSyncService() {
  return this;
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

  ExtensionRegistry::Get(profile_)->AddObserver(this);
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

  UMA_HISTOGRAM_ENUMERATION("SyncFileSystem.RegisterOriginResult",
                            GetRemoteService(app_origin)->GetCurrentState(),
                            REMOTE_SERVICE_STATE_MAX);

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
    callback.Run(base::ListValue());
    return;
  }

  GetRemoteService(origin)->DumpFiles(
      origin,
      base::Bind(
          &SyncFileSystemService::DidDumpFiles,
          AsWeakPtr(),
          origin,
          callback));
}

void SyncFileSystemService::DidDumpFiles(
    const GURL& origin,
    const DumpFilesCallback& callback,
    scoped_ptr<base::ListValue> dump_files) {
  if (!dump_files || !dump_files->GetSize()) {
    callback.Run(base::ListValue());
    return;
  }

  base::ListValue* files = dump_files.get();
  base::Callback<void(base::DictionaryValue*,
                      SyncStatusCode,
                      SyncFileStatus)> completion_callback =
      base::Bind(&DidGetFileSyncStatusForDump,
                 base::Owned(dump_files.release()),
                 base::Owned(new size_t(0)),
                 callback);

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

void SyncFileSystemService::DidDumpDatabase(
    const DumpFilesCallback& callback, scoped_ptr<base::ListValue> list) {
  if (!list)
    list = make_scoped_ptr(new base::ListValue);

  if (!v2_remote_service_) {
    callback.Run(*list);
    return;
  }

  v2_remote_service_->DumpDatabase(
      base::Bind(&SyncFileSystemService::DidDumpV2Database,
                 AsWeakPtr(), callback, base::Passed(&list)));
}

void SyncFileSystemService::DidDumpV2Database(
    const DumpFilesCallback& callback,
    scoped_ptr<base::ListValue> v1list,
    scoped_ptr<base::ListValue> v2list) {
  DCHECK(v1list);

  if (v2list) {
    for (base::ListValue::iterator itr = v2list->begin();
         itr != v2list->end();) {
      scoped_ptr<base::Value> item;
      itr = v2list->Erase(itr, &item);
      v1list->Append(item.release());
    }
  }

  callback.Run(*v1list);
}

void SyncFileSystemService::DidGetExtensionStatusMap(
    const ExtensionStatusMapCallback& callback,
    scoped_ptr<RemoteFileSyncService::OriginStatusMap> status_map) {
  if (!v2_remote_service_) {
    callback.Run(*status_map);
    return;
  }

  v2_remote_service_->GetOriginStatusMap(
      base::Bind(&SyncFileSystemService::DidGetV2ExtensionStatusMap,
                 AsWeakPtr(),
                 callback,
                 base::Passed(&status_map)));
}

void SyncFileSystemService::DidGetV2ExtensionStatusMap(
    const ExtensionStatusMapCallback& callback,
    scoped_ptr<RemoteFileSyncService::OriginStatusMap> status_map_v1,
    scoped_ptr<RemoteFileSyncService::OriginStatusMap> status_map_v2) {
  // Merge |status_map_v2| into |status_map_v1|.
  status_map_v1->insert(status_map_v2->begin(), status_map_v2->end());

  callback.Run(*status_map_v1);
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

void SyncFileSystemService::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  GURL app_origin = Extension::GetBaseURLFromExtensionId(extension->id());
  DVLOG(1) << "Handle extension notification for INSTALLED: " << app_origin;
  // NOTE: When an app is uninstalled and re-installed in a sequence,
  // |local_service_| may still keeps |app_origin| as disabled origin.
  local_service_->SetOriginEnabled(app_origin, true);
}

void SyncFileSystemService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  if (reason != extensions::UnloadedExtensionInfo::REASON_DISABLE)
    return;

  GURL app_origin = Extension::GetBaseURLFromExtensionId(extension->id());
  int disable_reasons =
      ExtensionPrefs::Get(profile_)->GetDisableReasons(extension->id());
  if (disable_reasons & Extension::DISABLE_RELOAD) {
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
      base::Bind(&DidHandleUnloadedEvent, app_origin));
  local_service_->SetOriginEnabled(app_origin, false);
}

void SyncFileSystemService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
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
      base::Bind(&DidHandleUninstalledEvent, app_origin));
  local_service_->SetOriginEnabled(app_origin, false);
}

void SyncFileSystemService::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  GURL app_origin = Extension::GetBaseURLFromExtensionId(extension->id());
  DVLOG(1) << "Handle extension notification for LOADED: " << app_origin;
  GetRemoteService(app_origin)->EnableOrigin(
      app_origin,
      base::Bind(&DidHandleLoadEvent, app_origin));
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
        RemoteFileSyncService::V2, profile_, &task_logger_);
    scoped_ptr<RemoteSyncRunner> v2_remote_syncer(
        new RemoteSyncRunner(kRemoteSyncNameV2, this,
                             v2_remote_service_.get()));
    v2_remote_service_->AddServiceObserver(v2_remote_syncer.get());
    v2_remote_service_->AddFileStatusObserver(this);
    v2_remote_service_->SetRemoteChangeProcessor(local_service_.get());
    v2_remote_service_->SetSyncEnabled(sync_enabled_);
    remote_sync_runners_.push_back(v2_remote_syncer.release());
  }
  return v2_remote_service_.get();
}

}  // namespace sync_file_system
