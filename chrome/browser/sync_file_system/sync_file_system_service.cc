// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_service.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/api/sync_file_system/extension_sync_event_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync_file_system/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_event_observer.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/syncable/sync_direction.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

using content::BrowserThread;
using fileapi::FileSystemURL;
using fileapi::FileSystemURLSet;

namespace sync_file_system {

namespace {

// Run the given join_callback when all the callbacks created by this runner
// are run. If any of the callbacks return non-OK state the given join_callback
// will be dispatched with the non-OK state that comes first.
class SharedCallbackRunner
    : public base::RefCountedThreadSafe<SharedCallbackRunner> {
 public:
  explicit SharedCallbackRunner(const SyncStatusCallback& join_callback)
      : join_callback_(join_callback),
        num_shared_callbacks_(0),
        status_(SYNC_STATUS_OK) {}

  SyncStatusCallback CreateCallback() {
    ++num_shared_callbacks_;
    return base::Bind(&SharedCallbackRunner::Done, this);
  }

  template <typename R>
  base::Callback<void(SyncStatusCode, const R& in)>
  CreateAssignAndRunCallback(R* out) {
    ++num_shared_callbacks_;
    return base::Bind(&SharedCallbackRunner::AssignAndRun<R>, this, out);
  }

 private:
  virtual ~SharedCallbackRunner() {}
  friend class base::RefCountedThreadSafe<SharedCallbackRunner>;

  template <typename R>
  void AssignAndRun(R* out, SyncStatusCode status, const R& in) {
    DCHECK(out);
    DCHECK_GT(num_shared_callbacks_, 0);
    if (join_callback_.is_null())
      return;
    *out = in;
    Done(status);
  }

  void Done(SyncStatusCode status) {
    if (status != SYNC_STATUS_OK && status_ == SYNC_STATUS_OK) {
      status_ = status;
    }
    if (--num_shared_callbacks_ > 0)
      return;
    join_callback_.Run(status_);
    join_callback_.Reset();
  }

  SyncStatusCallback join_callback_;
  int num_shared_callbacks_;
  SyncStatusCode status_;
};

void VerifyFileSystemURLSetCallback(
    base::WeakPtr<SyncFileSystemService> service,
    const GURL& app_origin,
    const std::string& service_name,
    const SyncFileSetCallback& callback,
    SyncStatusCode status,
    const FileSystemURLSet& urls) {
  if (!service.get())
    return;

#ifndef NDEBUG
  if (status == SYNC_STATUS_OK) {
    for (FileSystemURLSet::const_iterator iter = urls.begin();
         iter != urls.end(); ++iter) {
      DCHECK(iter->origin() == app_origin);
      DCHECK(iter->filesystem_id() == service_name);
    }
  }
#endif

  callback.Run(status, urls);
}

SyncEventObserver::SyncServiceState RemoteStateToSyncServiceState(
    RemoteServiceState state) {
  switch (state) {
    case REMOTE_SERVICE_OK:
      return SyncEventObserver::SYNC_SERVICE_RUNNING;
    case REMOTE_SERVICE_TEMPORARY_UNAVAILABLE:
      return SyncEventObserver::SYNC_SERVICE_TEMPORARY_UNAVAILABLE;
    case REMOTE_SERVICE_AUTHENTICATION_REQUIRED:
      return SyncEventObserver::SYNC_SERVICE_AUTHENTICATION_REQUIRED;
    case REMOTE_SERVICE_DISABLED:
      return SyncEventObserver::SYNC_SERVICE_DISABLED;
  }
  NOTREACHED();
  return SyncEventObserver::SYNC_SERVICE_DISABLED;
}

void DidHandleOriginForExtensionEvent(
    int type,
    const GURL& origin,
    SyncStatusCode code) {
  if (code != SYNC_STATUS_OK) {
    DCHECK(chrome::NOTIFICATION_EXTENSION_UNLOADED == type ||
           chrome::NOTIFICATION_EXTENSION_LOADED == type);
    const char* event =
        (chrome::NOTIFICATION_EXTENSION_UNLOADED == type) ? "UNLOAD" : "LOAD";
    LOG(WARNING) << "Register/Unregistering origin for " << event << " failed:"
                 << origin.spec();
  }
}

}  // namespace

void SyncFileSystemService::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
    const std::string& service_name,
    const GURL& app_origin,
    const SyncStatusCallback& callback) {
  DCHECK(local_file_service_);
  DCHECK(remote_file_service_);
  DCHECK(app_origin == app_origin.GetOrigin());

  DVLOG(1) << "InitializeForApp: " << app_origin.spec();

  local_file_service_->MaybeInitializeFileSystemContext(
      app_origin, service_name, file_system_context,
      base::Bind(&SyncFileSystemService::DidInitializeFileSystem,
                 AsWeakPtr(), app_origin, callback));
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

SyncFileSystemService::SyncFileSystemService(Profile* profile)
    : profile_(profile),
      pending_local_changes_(0),
      pending_remote_changes_(0),
      local_sync_running_(false),
      remote_sync_running_(false),
      is_waiting_remote_sync_enabled_(false),
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

  remote_file_service_->AddServiceObserver(this);
  remote_file_service_->AddFileStatusObserver(this);

  ProfileSyncServiceBase* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (profile_sync_service) {
    UpdateSyncEnabledStatus(profile_sync_service);
    profile_sync_service->AddObserver(this);
  }

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
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
  remote_file_service_->RegisterOriginForTrackingChanges(
      app_origin,
      base::Bind(&SyncFileSystemService::DidRegisterOrigin,
                 AsWeakPtr(), app_origin, callback));
}

void SyncFileSystemService::DidRegisterOrigin(
    const GURL& app_origin,
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  DVLOG(1) << "DidRegisterOrigin: " << app_origin.spec() << " " << status;

  callback.Run(status);
}

void SyncFileSystemService::SetSyncEnabledForTesting(bool enabled) {
  sync_enabled_ = enabled;
  remote_file_service_->SetSyncEnabled(sync_enabled_);
}

void SyncFileSystemService::MaybeStartSync() {
  if (!profile_ || !sync_enabled_)
    return;

  DCHECK(local_file_service_);
  DCHECK(remote_file_service_);

  MaybeStartRemoteSync();
  MaybeStartLocalSync();
}

void SyncFileSystemService::MaybeStartRemoteSync() {
  if (remote_file_service_->GetCurrentState() == REMOTE_SERVICE_DISABLED)
    return;
  // See if we cannot / should not start a new remote sync.
  if (remote_sync_running_ || pending_remote_changes_ == 0)
    return;
  // If we have registered a URL for waiting until sync is enabled on a
  // file (and the registerred URL seems to be still valid) it won't be
  // worth trying to start another remote sync.
  if (is_waiting_remote_sync_enabled_)
    return;
  DCHECK(sync_enabled_);
  DVLOG(1) << "Calling ProcessRemoteChange";
  remote_sync_running_ = true;
  remote_file_service_->ProcessRemoteChange(
      local_file_service_.get(),
      base::Bind(&SyncFileSystemService::DidProcessRemoteChange,
                 AsWeakPtr()));
}

void SyncFileSystemService::MaybeStartLocalSync() {
  // If the remote service is not ready probably we should not start a
  // local sync yet.
  // (We should be still trying a remote sync so the state should become OK
  // if the remote-side attempt succeeds.)
  if (remote_file_service_->GetCurrentState() != REMOTE_SERVICE_OK)
    return;
  // See if we cannot / should not start a new local sync.
  if (local_sync_running_ || pending_local_changes_ == 0)
    return;
  DVLOG(1) << "Calling ProcessLocalChange";
  local_sync_running_ = true;
  local_file_service_->ProcessLocalChange(
      remote_file_service_->GetLocalChangeProcessor(),
      base::Bind(&SyncFileSystemService::DidProcessLocalChange,
                 AsWeakPtr()));
}

void SyncFileSystemService::DidProcessRemoteChange(
    SyncStatusCode status,
    const FileSystemURL& url) {
  DVLOG(1) << "DidProcessRemoteChange: "
           << " status=" << status
           << " (" << SyncStatusCodeToString(status) << ")"
           << " url=" << url.DebugString();
  DCHECK(remote_sync_running_);
  remote_sync_running_ = false;

  if (status != SYNC_STATUS_NO_CHANGE_TO_SYNC &&
      remote_file_service_->GetCurrentState() != REMOTE_SERVICE_DISABLED) {
    DCHECK(url.is_valid());
    local_file_service_->ClearSyncFlagForURL(url);
  }

  if (status == SYNC_STATUS_NO_CHANGE_TO_SYNC) {
    // We seem to have no changes to work on for now.
    // TODO(kinuko): Might be better setting a timer to call MaybeStartSync.
    return;
  }
  if (status == SYNC_STATUS_FILE_BUSY) {
    is_waiting_remote_sync_enabled_ = true;
    local_file_service_->RegisterURLForWaitingSync(
        url, base::Bind(&SyncFileSystemService::OnSyncEnabledForRemoteSync,
                        AsWeakPtr()));
    return;
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(&SyncFileSystemService::MaybeStartSync,
                            AsWeakPtr()));
}

void SyncFileSystemService::DidProcessLocalChange(
    SyncStatusCode status, const FileSystemURL& url) {
  DVLOG(1) << "DidProcessLocalChange:"
           << " status=" << status
           << " (" << SyncStatusCodeToString(status) << ")"
           << " url=" << url.DebugString();
  DCHECK(local_sync_running_);
  local_sync_running_ = false;

  if (status == SYNC_STATUS_NO_CHANGE_TO_SYNC) {
    // We seem to have no changes to work on for now.
    return;
  }

  DCHECK(url.is_valid());
  local_file_service_->ClearSyncFlagForURL(url);

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(&SyncFileSystemService::MaybeStartSync,
                            AsWeakPtr()));
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
  is_waiting_remote_sync_enabled_ = false;
  MaybeStartRemoteSync();
}

void SyncFileSystemService::OnLocalChangeAvailable(int64 pending_changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_GE(pending_changes, 0);
  DVLOG(1) << "OnLocalChangeAvailable: " << pending_changes;
  pending_local_changes_ = pending_changes;
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(&SyncFileSystemService::MaybeStartSync,
                            AsWeakPtr()));
}

void SyncFileSystemService::OnRemoteChangeQueueUpdated(int64 pending_changes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_GE(pending_changes, 0);
  DVLOG(1) << "OnRemoteChangeQueueUpdated: " << pending_changes;
  pending_remote_changes_ = pending_changes;
  if (pending_changes > 0) {
    // The smallest change available might have changed from the previous one.
    // Reset the is_waiting_remote_sync_enabled_ flag so that we can retry.
    is_waiting_remote_sync_enabled_ = false;
  }
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(&SyncFileSystemService::MaybeStartSync,
                            AsWeakPtr()));
}

void SyncFileSystemService::OnRemoteServiceStateUpdated(
    RemoteServiceState state,
    const std::string& description) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "OnRemoteServiceStateUpdated: " << state
           << " " << description;
  if (state == REMOTE_SERVICE_OK) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(&SyncFileSystemService::MaybeStartSync,
                              AsWeakPtr()));
  }

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
  if (chrome::NOTIFICATION_EXTENSION_UNLOADED == type) {
    // Unregister origin for remote synchronization.
    std::string extension_id =
        content::Details<const extensions::UnloadedExtensionInfo>(
            details)->extension->id();
    GURL app_origin = extensions::Extension::GetBaseURLFromExtensionId(
        extension_id);
    remote_file_service_->UnregisterOriginForTrackingChanges(
        app_origin, base::Bind(&DidHandleOriginForExtensionEvent,
                               type, app_origin));
    local_file_service_->SetOriginEnabled(app_origin, false);
  } else if (chrome::NOTIFICATION_EXTENSION_LOADED == type) {
    std::string extension_id =
        content::Details<const extensions::Extension>(
            details)->id();
    GURL app_origin = extensions::Extension::GetBaseURLFromExtensionId(
        extension_id);
    local_file_service_->SetOriginEnabled(app_origin, true);
  } else {
    NOTREACHED() << "Unknown notification.";
  }
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
  sync_enabled_ = profile_sync_service->GetPreferredDataTypes().Has(
      syncer::APPS);
  remote_file_service_->SetSyncEnabled(sync_enabled_);
  if (sync_enabled_) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(&SyncFileSystemService::MaybeStartSync,
                              AsWeakPtr()));
  }
}

// SyncFileSystemServiceFactory -----------------------------------------------

// static
SyncFileSystemService* SyncFileSystemServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SyncFileSystemService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
SyncFileSystemServiceFactory* SyncFileSystemServiceFactory::GetInstance() {
  return Singleton<SyncFileSystemServiceFactory>::get();
}

void SyncFileSystemServiceFactory::set_mock_remote_file_service(
    scoped_ptr<RemoteFileSyncService> mock_remote_service) {
  mock_remote_file_service_ = mock_remote_service.Pass();
}

SyncFileSystemServiceFactory::SyncFileSystemServiceFactory()
    : ProfileKeyedServiceFactory("SyncFileSystemService",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

SyncFileSystemServiceFactory::~SyncFileSystemServiceFactory() {}

ProfileKeyedService* SyncFileSystemServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  SyncFileSystemService* service = new SyncFileSystemService(profile);

  scoped_ptr<LocalFileSyncService> local_file_service(
      new LocalFileSyncService(profile));

  scoped_ptr<RemoteFileSyncService> remote_file_service;
  if (mock_remote_file_service_)
    remote_file_service = mock_remote_file_service_.Pass();
  else
    remote_file_service.reset(new DriveFileSyncService(profile));

  service->Initialize(local_file_service.Pass(),
                      remote_file_service.Pass());
  return service;
}

}  // namespace sync_file_system
