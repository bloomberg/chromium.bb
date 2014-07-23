// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"

#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/local/local_file_change_tracker.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/blob/scoped_file.h"

using content::BrowserThread;
using fileapi::FileSystemURL;

namespace sync_file_system {

namespace {

void PrepareForProcessRemoteChangeCallbackAdapter(
    const RemoteChangeProcessor::PrepareChangeCallback& callback,
    SyncStatusCode status,
    const LocalFileSyncInfo& sync_file_info,
    webkit_blob::ScopedFile snapshot) {
  callback.Run(status, sync_file_info.metadata, sync_file_info.changes);
}

void InvokeCallbackOnNthInvocation(int* count, const base::Closure& callback) {
  --*count;
  if (*count <= 0)
    callback.Run();
}

}  // namespace

LocalFileSyncService::OriginChangeMap::OriginChangeMap()
    : next_(change_count_map_.end()) {}
LocalFileSyncService::OriginChangeMap::~OriginChangeMap() {}

bool LocalFileSyncService::OriginChangeMap::NextOriginToProcess(GURL* origin) {
  DCHECK(origin);
  if (change_count_map_.empty())
    return false;
  Map::iterator begin = next_;
  do {
    if (next_ == change_count_map_.end())
      next_ = change_count_map_.begin();
    DCHECK_NE(0, next_->second);
    *origin = next_++->first;
    if (!ContainsKey(disabled_origins_, *origin))
      return true;
  } while (next_ != begin);
  return false;
}

int64 LocalFileSyncService::OriginChangeMap::GetTotalChangeCount() const {
  int64 num_changes = 0;
  for (Map::const_iterator iter = change_count_map_.begin();
       iter != change_count_map_.end(); ++iter) {
    if (ContainsKey(disabled_origins_, iter->first))
      continue;
    num_changes += iter->second;
  }
  return num_changes;
}

void LocalFileSyncService::OriginChangeMap::SetOriginChangeCount(
    const GURL& origin, int64 changes) {
  if (changes != 0) {
    change_count_map_[origin] = changes;
    return;
  }
  Map::iterator found = change_count_map_.find(origin);
  if (found != change_count_map_.end()) {
    if (next_ == found)
      ++next_;
    change_count_map_.erase(found);
  }
}

void LocalFileSyncService::OriginChangeMap::SetOriginEnabled(
    const GURL& origin, bool enabled) {
  if (enabled)
    disabled_origins_.erase(origin);
  else
    disabled_origins_.insert(origin);
}

// LocalFileSyncService -------------------------------------------------------

scoped_ptr<LocalFileSyncService> LocalFileSyncService::Create(
    Profile* profile) {
  return make_scoped_ptr(new LocalFileSyncService(profile, NULL));
}

scoped_ptr<LocalFileSyncService> LocalFileSyncService::CreateForTesting(
    Profile* profile,
    leveldb::Env* env) {
  scoped_ptr<LocalFileSyncService> sync_service(
      new LocalFileSyncService(profile, env));
  sync_service->sync_context_->set_mock_notify_changes_duration_in_sec(0);
  return sync_service.Pass();
}

LocalFileSyncService::~LocalFileSyncService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void LocalFileSyncService::Shutdown() {
  sync_context_->RemoveOriginChangeObserver(this);
  sync_context_->ShutdownOnUIThread();
  profile_ = NULL;
}

void LocalFileSyncService::MaybeInitializeFileSystemContext(
    const GURL& app_origin,
    fileapi::FileSystemContext* file_system_context,
    const SyncStatusCallback& callback) {
  sync_context_->MaybeInitializeFileSystemContext(
      app_origin, file_system_context,
      base::Bind(&LocalFileSyncService::DidInitializeFileSystemContext,
                 AsWeakPtr(), app_origin,
                 make_scoped_refptr(file_system_context), callback));
}

void LocalFileSyncService::AddChangeObserver(Observer* observer) {
  change_observers_.AddObserver(observer);
}

void LocalFileSyncService::RegisterURLForWaitingSync(
    const FileSystemURL& url,
    const base::Closure& on_syncable_callback) {
  sync_context_->RegisterURLForWaitingSync(url, on_syncable_callback);
}

void LocalFileSyncService::ProcessLocalChange(
    const SyncFileCallback& callback) {
  // Pick an origin to process next.
  GURL origin;
  if (!origin_change_map_.NextOriginToProcess(&origin)) {
    callback.Run(SYNC_STATUS_NO_CHANGE_TO_SYNC, FileSystemURL());
    return;
  }
  DCHECK(local_sync_callback_.is_null());
  DCHECK(!origin.is_empty());
  DCHECK(ContainsKey(origin_to_contexts_, origin));

  DVLOG(1) << "Starting ProcessLocalChange";

  local_sync_callback_ = callback;

  sync_context_->GetFileForLocalSync(
      origin_to_contexts_[origin],
      base::Bind(&LocalFileSyncService::DidGetFileForLocalSync,
                 AsWeakPtr()));
}

void LocalFileSyncService::SetLocalChangeProcessor(
    LocalChangeProcessor* local_change_processor) {
  local_change_processor_ = local_change_processor;
}

void LocalFileSyncService::SetLocalChangeProcessorCallback(
    const GetLocalChangeProcessorCallback& get_local_change_processor) {
  get_local_change_processor_ = get_local_change_processor;
}

void LocalFileSyncService::HasPendingLocalChanges(
    const FileSystemURL& url,
    const HasPendingLocalChangeCallback& callback) {
  if (!ContainsKey(origin_to_contexts_, url.origin())) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, SYNC_FILE_ERROR_INVALID_URL, false));
    return;
  }
  sync_context_->HasPendingLocalChanges(
      origin_to_contexts_[url.origin()], url, callback);
}

void LocalFileSyncService::PromoteDemotedChanges(
    const base::Closure& callback) {
  if (origin_to_contexts_.empty()) {
    callback.Run();
    return;
  }

  base::Closure completion_callback =
      base::Bind(&InvokeCallbackOnNthInvocation,
                 base::Owned(new int(origin_to_contexts_.size())), callback);
  for (OriginToContext::iterator iter = origin_to_contexts_.begin();
       iter != origin_to_contexts_.end(); ++iter)
    sync_context_->PromoteDemotedChanges(iter->first, iter->second,
                                         completion_callback);
}

void LocalFileSyncService::GetLocalFileMetadata(
    const FileSystemURL& url, const SyncFileMetadataCallback& callback) {
  DCHECK(ContainsKey(origin_to_contexts_, url.origin()));
  sync_context_->GetFileMetadata(origin_to_contexts_[url.origin()],
                                 url, callback);
}

void LocalFileSyncService::PrepareForProcessRemoteChange(
    const FileSystemURL& url,
    const PrepareChangeCallback& callback) {
  DVLOG(1) << "PrepareForProcessRemoteChange: " << url.DebugString();

  if (!ContainsKey(origin_to_contexts_, url.origin())) {
    // This could happen if a remote sync is triggered for the app that hasn't
    // been initialized in this service.
    DCHECK(profile_);
    // The given url.origin() must be for valid installed app.
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile_)
            ->enabled_extensions().GetAppByURL(url.origin());
    if (!extension) {
      util::Log(
          logging::LOG_WARNING,
          FROM_HERE,
          "PrepareForProcessRemoteChange called for non-existing origin: %s",
          url.origin().spec().c_str());

      // The extension has been uninstalled and this method is called
      // before the remote changes for the origin are removed.
      callback.Run(SYNC_STATUS_NO_CHANGE_TO_SYNC,
                   SyncFileMetadata(), FileChangeList());
      return;
    }
    GURL site_url =
        extensions::util::GetSiteForExtensionId(extension->id(), profile_);
    DCHECK(!site_url.is_empty());
    scoped_refptr<fileapi::FileSystemContext> file_system_context =
        content::BrowserContext::GetStoragePartitionForSite(
            profile_, site_url)->GetFileSystemContext();
    MaybeInitializeFileSystemContext(
        url.origin(),
        file_system_context.get(),
        base::Bind(&LocalFileSyncService::DidInitializeForRemoteSync,
                   AsWeakPtr(),
                   url,
                   file_system_context,
                   callback));
    return;
  }

  DCHECK(ContainsKey(origin_to_contexts_, url.origin()));
  sync_context_->PrepareForSync(
      origin_to_contexts_[url.origin()], url,
      LocalFileSyncContext::SYNC_EXCLUSIVE,
      base::Bind(&PrepareForProcessRemoteChangeCallbackAdapter, callback));
}

void LocalFileSyncService::ApplyRemoteChange(
    const FileChange& change,
    const base::FilePath& local_path,
    const FileSystemURL& url,
    const SyncStatusCallback& callback) {
  DCHECK(ContainsKey(origin_to_contexts_, url.origin()));
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "[Remote -> Local] ApplyRemoteChange: %s on %s",
            change.DebugString().c_str(),
            url.DebugString().c_str());

  sync_context_->ApplyRemoteChange(
      origin_to_contexts_[url.origin()],
      change, local_path, url,
      base::Bind(&LocalFileSyncService::DidApplyRemoteChange, AsWeakPtr(),
                 callback));
}

void LocalFileSyncService::FinalizeRemoteSync(
    const FileSystemURL& url,
    bool clear_local_changes,
    const base::Closure& completion_callback) {
  DCHECK(ContainsKey(origin_to_contexts_, url.origin()));
  sync_context_->FinalizeExclusiveSync(
      origin_to_contexts_[url.origin()],
      url, clear_local_changes, completion_callback);
}

void LocalFileSyncService::RecordFakeLocalChange(
    const FileSystemURL& url,
    const FileChange& change,
    const SyncStatusCallback& callback) {
  DCHECK(ContainsKey(origin_to_contexts_, url.origin()));
  sync_context_->RecordFakeLocalChange(origin_to_contexts_[url.origin()],
                                       url, change, callback);
}

void LocalFileSyncService::OnChangesAvailableInOrigins(
    const std::set<GURL>& origins) {
  bool need_notification = false;
  for (std::set<GURL>::const_iterator iter = origins.begin();
       iter != origins.end(); ++iter) {
    const GURL& origin = *iter;
    if (!ContainsKey(origin_to_contexts_, origin)) {
      // This could happen if this is called for apps/origins that haven't
      // been initialized yet, or for apps/origins that are disabled.
      // (Local change tracker could call this for uninitialized origins
      // while it's reading dirty files from the database in the
      // initialization phase.)
      pending_origins_with_changes_.insert(origin);
      continue;
    }
    need_notification = true;
    SyncFileSystemBackend* backend =
        SyncFileSystemBackend::GetBackend(origin_to_contexts_[origin]);
    DCHECK(backend);
    DCHECK(backend->change_tracker());
    origin_change_map_.SetOriginChangeCount(
        origin, backend->change_tracker()->num_changes());
  }
  if (!need_notification)
    return;
  int64 num_changes = origin_change_map_.GetTotalChangeCount();
  FOR_EACH_OBSERVER(Observer, change_observers_,
                    OnLocalChangeAvailable(num_changes));
}

void LocalFileSyncService::SetOriginEnabled(const GURL& origin, bool enabled) {
  if (!ContainsKey(origin_to_contexts_, origin))
    return;
  origin_change_map_.SetOriginEnabled(origin, enabled);
}

LocalFileSyncService::LocalFileSyncService(Profile* profile,
                                           leveldb::Env* env_override)
    : profile_(profile),
      sync_context_(new LocalFileSyncContext(
          profile_->GetPath(),
          env_override,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI).get(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)
              .get())),
      local_change_processor_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sync_context_->AddOriginChangeObserver(this);
}

void LocalFileSyncService::DidInitializeFileSystemContext(
    const GURL& app_origin,
    fileapi::FileSystemContext* file_system_context,
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }
  DCHECK(file_system_context);
  origin_to_contexts_[app_origin] = file_system_context;

  if (pending_origins_with_changes_.find(app_origin) !=
      pending_origins_with_changes_.end()) {
    // We have remaining changes for the origin.
    pending_origins_with_changes_.erase(app_origin);
    SyncFileSystemBackend* backend =
        SyncFileSystemBackend::GetBackend(file_system_context);
    DCHECK(backend);
    DCHECK(backend->change_tracker());
    origin_change_map_.SetOriginChangeCount(
        app_origin, backend->change_tracker()->num_changes());
    int64 num_changes = origin_change_map_.GetTotalChangeCount();
    FOR_EACH_OBSERVER(Observer, change_observers_,
                      OnLocalChangeAvailable(num_changes));
  }
  callback.Run(status);
}

void LocalFileSyncService::DidInitializeForRemoteSync(
    const FileSystemURL& url,
    fileapi::FileSystemContext* file_system_context,
    const PrepareChangeCallback& callback,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    DVLOG(1) << "FileSystemContext initialization failed for remote sync:"
             << url.DebugString() << " status=" << status
             << " (" << SyncStatusCodeToString(status) << ")";
    callback.Run(status, SyncFileMetadata(), FileChangeList());
    return;
  }
  origin_to_contexts_[url.origin()] = file_system_context;
  PrepareForProcessRemoteChange(url, callback);
}

void LocalFileSyncService::RunLocalSyncCallback(
    SyncStatusCode status,
    const FileSystemURL& url) {
  DVLOG(1) << "Local sync is finished with: " << status
           << " on " << url.DebugString();
  DCHECK(!local_sync_callback_.is_null());
  SyncFileCallback callback = local_sync_callback_;
  local_sync_callback_.Reset();
  callback.Run(status, url);
}

void LocalFileSyncService::DidApplyRemoteChange(
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "[Remote -> Local] ApplyRemoteChange finished --> %s",
            SyncStatusCodeToString(status));
  callback.Run(status);
}

void LocalFileSyncService::DidGetFileForLocalSync(
    SyncStatusCode status,
    const LocalFileSyncInfo& sync_file_info,
    webkit_blob::ScopedFile snapshot) {
  DCHECK(!local_sync_callback_.is_null());
  if (status != SYNC_STATUS_OK) {
    RunLocalSyncCallback(status, sync_file_info.url);
    return;
  }
  if (sync_file_info.changes.empty()) {
    // There's a slight chance this could happen.
    SyncFileCallback callback = local_sync_callback_;
    local_sync_callback_.Reset();
    ProcessLocalChange(callback);
    return;
  }

  FileChange next_change = sync_file_info.changes.front();
  DVLOG(1) << "ProcessLocalChange: " << sync_file_info.url.DebugString()
           << " change:" << next_change.DebugString();

  GetLocalChangeProcessor(sync_file_info.url)->ApplyLocalChange(
      next_change,
      sync_file_info.local_file_path,
      sync_file_info.metadata,
      sync_file_info.url,
      base::Bind(&LocalFileSyncService::ProcessNextChangeForURL,
                 AsWeakPtr(), base::Passed(&snapshot), sync_file_info,
                 next_change, sync_file_info.changes.PopAndGetNewList()));
}

void LocalFileSyncService::ProcessNextChangeForURL(
    webkit_blob::ScopedFile snapshot,
    const LocalFileSyncInfo& sync_file_info,
    const FileChange& processed_change,
    const FileChangeList& changes,
    SyncStatusCode status) {
  DVLOG(1) << "Processed one local change: "
           << sync_file_info.url.DebugString()
           << " change:" << processed_change.DebugString()
           << " status:" << status;

  if (status == SYNC_STATUS_RETRY) {
    GetLocalChangeProcessor(sync_file_info.url)->ApplyLocalChange(
        processed_change,
        sync_file_info.local_file_path,
        sync_file_info.metadata,
        sync_file_info.url,
        base::Bind(&LocalFileSyncService::ProcessNextChangeForURL,
                   AsWeakPtr(), base::Passed(&snapshot),
                   sync_file_info, processed_change, changes));
    return;
  }

  if (status == SYNC_FILE_ERROR_NOT_FOUND &&
      processed_change.change() == FileChange::FILE_CHANGE_DELETE) {
    // This must be ok (and could happen).
    status = SYNC_STATUS_OK;
  }

  const FileSystemURL& url = sync_file_info.url;
  if (status != SYNC_STATUS_OK || changes.empty()) {
    DCHECK(ContainsKey(origin_to_contexts_, url.origin()));
    sync_context_->FinalizeSnapshotSync(
        origin_to_contexts_[url.origin()], url, status,
        base::Bind(&LocalFileSyncService::RunLocalSyncCallback,
                   AsWeakPtr(), status, url));
    return;
  }

  FileChange next_change = changes.front();
  GetLocalChangeProcessor(url)->ApplyLocalChange(
      changes.front(),
      sync_file_info.local_file_path,
      sync_file_info.metadata,
      url,
      base::Bind(&LocalFileSyncService::ProcessNextChangeForURL,
                 AsWeakPtr(), base::Passed(&snapshot), sync_file_info,
                 next_change, changes.PopAndGetNewList()));
}

LocalChangeProcessor* LocalFileSyncService::GetLocalChangeProcessor(
    const FileSystemURL& url) {
  if (!get_local_change_processor_.is_null())
    return get_local_change_processor_.Run(url.origin());
  DCHECK(local_change_processor_);
  return local_change_processor_;
}

}  // namespace sync_file_system
