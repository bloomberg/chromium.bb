// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/api/sync_file_system/extension_sync_event_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
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
#include "webkit/fileapi/syncable/sync_file_metadata.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

using content::BrowserThread;
using fileapi::ConflictFileInfoCallback;
using fileapi::FileSystemURL;
using fileapi::SyncFileMetadata;
using fileapi::SyncStatusCallback;
using fileapi::SyncStatusCode;

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
        status_(fileapi::SYNC_STATUS_OK) {}

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
    if (status != fileapi::SYNC_STATUS_OK &&
        status_ == fileapi::SYNC_STATUS_OK) {
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
    const fileapi::SyncFileSetCallback& callback,
    fileapi::SyncStatusCode status,
    const fileapi::FileSystemURLSet& urls) {
  if (!service.get())
    return;

#ifndef NDEBUG
  if (status == fileapi::SYNC_STATUS_OK) {
    for (fileapi::FileSystemURLSet::const_iterator iter = urls.begin();
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

void DidUnregisterOriginForTrackingChanges(fileapi::SyncStatusCode code) {
  DCHECK_EQ(fileapi::SYNC_STATUS_OK, code);
}

}  // namespace

void SyncFileSystemService::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  local_file_service_->Shutdown();
  local_file_service_.reset();

  remote_file_service_.reset();

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

  if (initialized_app_origins_.find(app_origin) !=
      initialized_app_origins_.end()) {
    DVLOG(1) << "The app is already initialized: " << app_origin.spec();
    callback.Run(fileapi::SYNC_STATUS_OK);
    return;
  }

  local_file_service_->MaybeInitializeFileSystemContext(
      app_origin, service_name, file_system_context,
      base::Bind(&SyncFileSystemService::DidInitializeFileSystem,
                 AsWeakPtr(), app_origin, callback));
}

void SyncFileSystemService::GetConflictFiles(
    const GURL& app_origin,
    const std::string& service_name,
    const fileapi::SyncFileSetCallback& callback) {
  DCHECK(remote_file_service_);
  DCHECK(app_origin == app_origin.GetOrigin());

  if (!ContainsKey(initialized_app_origins_, app_origin)) {
    callback.Run(fileapi::SYNC_STATUS_NOT_INITIALIZED,
                 fileapi::FileSystemURLSet());
    return;
  }

  remote_file_service_->GetConflictFiles(
      app_origin, base::Bind(&VerifyFileSystemURLSetCallback,
                             AsWeakPtr(), app_origin, service_name, callback));
}

void SyncFileSystemService::GetConflictFileInfo(
    const GURL& app_origin,
    const std::string& service_name,
    const FileSystemURL& url,
    const ConflictFileInfoCallback& callback) {
  DCHECK(local_file_service_);
  DCHECK(remote_file_service_);
  DCHECK(app_origin == app_origin.GetOrigin());

  if (!ContainsKey(initialized_app_origins_, app_origin)) {
    callback.Run(fileapi::SYNC_STATUS_NOT_INITIALIZED,
                 fileapi::ConflictFileInfo());
    return;
  }

  // Call DidGetConflictFileInfo when both remote and local service's
  // GetFileMetadata calls are done.
  SyncFileMetadata* remote_metadata = new SyncFileMetadata;
  SyncFileMetadata* local_metadata = new SyncFileMetadata;
  SyncStatusCallback completion_callback =
      base::Bind(&SyncFileSystemService::DidGetConflictFileInfo,
                 AsWeakPtr(), callback, url,
                 base::Owned(local_metadata),
                 base::Owned(remote_metadata));
  scoped_refptr<SharedCallbackRunner> callback_runner(
      new SharedCallbackRunner(completion_callback));
  local_file_service_->GetLocalFileMetadata(
      url, callback_runner->CreateAssignAndRunCallback(local_metadata));
  remote_file_service_->GetRemoteFileMetadata(
      url, callback_runner->CreateAssignAndRunCallback(remote_metadata));
}

void SyncFileSystemService::GetFileSyncStatus(
    const fileapi::FileSystemURL& url,
    const fileapi::SyncFileStatusCallback& callback) {
  DCHECK(local_file_service_);
  DCHECK(remote_file_service_);

  if (!ContainsKey(initialized_app_origins_, url.origin())) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   fileapi::SYNC_STATUS_NOT_INITIALIZED,
                   fileapi::SYNC_FILE_STATUS_UNKNOWN));
    return;
  }

  if (remote_file_service_->IsConflicting(url)) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   fileapi::SYNC_STATUS_OK,
                   fileapi::SYNC_FILE_STATUS_CONFLICTING));
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
      auto_sync_enabled_(true) {
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
  remote_file_service_->AddObserver(this);

  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
}

void SyncFileSystemService::DidGetConflictFileInfo(
    const ConflictFileInfoCallback& callback,
    const FileSystemURL& url,
    const SyncFileMetadata* local_metadata,
    const SyncFileMetadata* remote_metadata,
    SyncStatusCode status) {
  DCHECK(local_metadata);
  DCHECK(remote_metadata);
  fileapi::ConflictFileInfo info;
  info.url = url;
  info.local_metadata = *local_metadata;
  info.remote_metadata = *remote_metadata;
  callback.Run(status, info);
}

void SyncFileSystemService::DidInitializeFileSystem(
    const GURL& app_origin,
    const fileapi::SyncStatusCallback& callback,
    fileapi::SyncStatusCode status) {
  DVLOG(1) << "DidInitializeFileSystem: " << app_origin.spec() << " " << status;

  if (status != fileapi::SYNC_STATUS_OK) {
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
    const fileapi::SyncStatusCallback& callback,
    fileapi::SyncStatusCode status) {
  DVLOG(1) << "DidRegisterOrigin: " << app_origin.spec() << " " << status;

  if (status == fileapi::SYNC_STATUS_OK)
    initialized_app_origins_.insert(app_origin);

  callback.Run(status);
}

void SyncFileSystemService::MaybeStartSync() {
  if (!profile_ || !auto_sync_enabled_)
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
    fileapi::SyncStatusCode status,
    const FileSystemURL& url,
    fileapi::SyncOperationResult result) {
  DVLOG(1) << "DidProcessRemoteChange: "
           << " status=" << status
           << " (" << SyncStatusCodeToString(status) << ")"
           << " url=" << url.DebugString()
           << " operation_result=" << result;
  DCHECK(remote_sync_running_);
  remote_sync_running_ = false;

  if (status != fileapi::SYNC_STATUS_NO_CHANGE_TO_SYNC &&
      remote_file_service_->GetCurrentState() != REMOTE_SERVICE_DISABLED) {
    DCHECK(url.is_valid());
    local_file_service_->ClearSyncFlagForURL(url);
  }

  if (status == fileapi::SYNC_STATUS_NO_CHANGE_TO_SYNC) {
    // We seem to have no changes to work on for now.
    // TODO(kinuko): Might be better setting a timer to call MaybeStartSync.
    return;
  }
  if (status == fileapi::SYNC_STATUS_FILE_BUSY) {
    is_waiting_remote_sync_enabled_ = true;
    local_file_service_->RegisterURLForWaitingSync(
        url, base::Bind(&SyncFileSystemService::OnSyncEnabledForRemoteSync,
                        AsWeakPtr()));
    return;
  }

  if ((status == fileapi::SYNC_STATUS_OK ||
       status == fileapi::SYNC_STATUS_HAS_CONFLICT) &&
      result != fileapi::SYNC_OPERATION_NONE) {
    // Notify observers of the changes made for a remote sync.
    FOR_EACH_OBSERVER(SyncEventObserver, observers_, OnFileSynced(url, result));
  }
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(&SyncFileSystemService::MaybeStartSync,
                            AsWeakPtr()));
}

void SyncFileSystemService::DidProcessLocalChange(
    fileapi::SyncStatusCode status, const FileSystemURL& url) {
  DVLOG(1) << "DidProcessLocalChange:"
           << " status=" << status
           << " (" << SyncStatusCodeToString(status) << ")"
           << " url=" << url.DebugString();
  DCHECK(local_sync_running_);
  local_sync_running_ = false;

  if (status == fileapi::SYNC_STATUS_NO_CHANGE_TO_SYNC) {
    // We seem to have no changes to work on for now.
    return;
  }

  DCHECK(url.is_valid());
  local_file_service_->ClearSyncFlagForURL(url);

  if (status == fileapi::SYNC_STATUS_HAS_CONFLICT) {
    FOR_EACH_OBSERVER(SyncEventObserver, observers_,
                      OnFileSynced(url, fileapi::SYNC_OPERATION_CONFLICTED));
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(&SyncFileSystemService::MaybeStartSync,
                            AsWeakPtr()));
}

void SyncFileSystemService::DidGetLocalChangeStatus(
    const fileapi::SyncFileStatusCallback& callback,
    bool has_pending_local_changes) {
  callback.Run(
      fileapi::SYNC_STATUS_OK,
      has_pending_local_changes ? fileapi::SYNC_FILE_STATUS_HAS_PENDING_CHANGES
                                : fileapi::SYNC_FILE_STATUS_SYNCED);
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
  // Extension unloaded means extension disabled or uninstalled.
  DCHECK_EQ(chrome::NOTIFICATION_EXTENSION_UNLOADED, type);

  // Unregister origin for remote synchronization.
  std::string extension_id =
      content::Details<const extensions::UnloadedExtensionInfo>(
          details)->extension->id();
  remote_file_service_->UnregisterOriginForTrackingChanges(
      extensions::Extension::GetBaseURLFromExtensionId(extension_id),
      base::Bind(&DidUnregisterOriginForTrackingChanges));
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
