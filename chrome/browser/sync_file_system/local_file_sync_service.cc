// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local_file_sync_service.h"

#include "base/stl_util.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/local_file_change_tracker.h"
#include "webkit/fileapi/syncable/local_file_sync_context.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"

using content::BrowserThread;
using fileapi::FileChange;
using fileapi::FileChangeList;
using fileapi::FileSystemURL;
using fileapi::LocalFileSyncContext;
using fileapi::LocalFileSyncInfo;
using fileapi::SyncFileCallback;
using fileapi::SyncFileMetadataCallback;
using fileapi::SyncStatusCallback;
using fileapi::SyncStatusCallback;
using fileapi::SyncStatusCode;

namespace sync_file_system {

namespace {

void PrepareForProcessRemoteChangeCallbackAdapter(
    const RemoteChangeProcessor::PrepareChangeCallback& callback,
    SyncStatusCode status,
    const LocalFileSyncInfo& sync_file_info) {
  callback.Run(status, sync_file_info.metadata, sync_file_info.changes);
}

}  // namespace

LocalFileSyncService::OriginChangeMap::OriginChangeMap()
    : next_(change_count_map_.end()) {}
LocalFileSyncService::OriginChangeMap::~OriginChangeMap() {}

bool LocalFileSyncService::OriginChangeMap::NextOriginToProcess(GURL* origin) {
  DCHECK(origin);
  if (change_count_map_.empty())
    return false;
  if (next_ == change_count_map_.end())
    next_ = change_count_map_.begin();
  DCHECK_NE(0, next_->second);
  *origin = next_++->first;
  return true;
}

int64 LocalFileSyncService::OriginChangeMap::GetTotalChangeCount() const {
  int64 num_changes = 0;
  for (Map::const_iterator iter = change_count_map_.begin();
       iter != change_count_map_.end(); ++iter) {
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

// LocalFileSyncService -------------------------------------------------------

LocalFileSyncService::LocalFileSyncService()
    : sync_context_(new LocalFileSyncContext(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO))) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sync_context_->AddOriginChangeObserver(this);
}

LocalFileSyncService::~LocalFileSyncService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void LocalFileSyncService::Shutdown() {
  sync_context_->RemoveOriginChangeObserver(this);
  sync_context_->ShutdownOnUIThread();
}

void LocalFileSyncService::MaybeInitializeFileSystemContext(
    const GURL& app_origin,
    const std::string& service_name,
    fileapi::FileSystemContext* file_system_context,
    const SyncStatusCallback& callback) {
  sync_context_->MaybeInitializeFileSystemContext(
      app_origin, service_name, file_system_context,
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
    LocalChangeProcessor* processor,
    const SyncFileCallback& callback) {
  DCHECK(local_sync_callback_.is_null());
  local_sync_callback_ = callback;

  // Pick an origin to process next.
  GURL origin;
  if (!origin_change_map_.NextOriginToProcess(&origin)) {
    callback.Run(fileapi::SYNC_STATUS_NO_CHANGE_TO_SYNC, FileSystemURL());
    return;
  }
  DCHECK(!origin.is_empty());
  DCHECK(ContainsKey(origin_to_contexts_, origin));

  sync_context_->GetFileForLocalSync(
      origin_to_contexts_[origin],
      base::Bind(&LocalFileSyncService::DidGetFileForLocalSync,
                 AsWeakPtr(), processor));
}

void LocalFileSyncService::GetLocalFileMetadata(
    const fileapi::FileSystemURL& url,
    const SyncFileMetadataCallback& callback) {
  DCHECK(ContainsKey(origin_to_contexts_, url.origin()));
  sync_context_->GetFileMetadata(origin_to_contexts_[url.origin()],
                                 url, callback);
}

void LocalFileSyncService::PrepareForProcessRemoteChange(
    const FileSystemURL& url,
    const PrepareChangeCallback& callback) {
  DCHECK(ContainsKey(origin_to_contexts_, url.origin()));
  sync_context_->PrepareForSync(
      origin_to_contexts_[url.origin()], url,
      base::Bind(&PrepareForProcessRemoteChangeCallbackAdapter, callback));
}

void LocalFileSyncService::ApplyRemoteChange(
    const FileChange& change,
    const FilePath& local_path,
    const FileSystemURL& url,
    const SyncStatusCallback& callback) {
  DCHECK(ContainsKey(origin_to_contexts_, url.origin()));
  sync_context_->ApplyRemoteChange(
      origin_to_contexts_[url.origin()],
      change, local_path, url, callback);
}

void LocalFileSyncService::OnChangesAvailableInOrigins(
    const std::set<GURL>& origins) {
  for (std::set<GURL>::const_iterator iter = origins.begin();
       iter != origins.end(); ++iter) {
    const GURL& origin = *iter;
    DCHECK(ContainsKey(origin_to_contexts_, origin));
    fileapi::FileSystemContext* context = origin_to_contexts_[origin];
    DCHECK(context->change_tracker());
    origin_change_map_.SetOriginChangeCount(
        origin, context->change_tracker()->num_changes());
  }
  int64 num_changes = origin_change_map_.GetTotalChangeCount();
  FOR_EACH_OBSERVER(Observer, change_observers_,
                    OnLocalChangeAvailable(num_changes));
}

void LocalFileSyncService::DidInitializeFileSystemContext(
    const GURL& app_origin,
    fileapi::FileSystemContext* file_system_context,
    const SyncStatusCallback& callback,
    SyncStatusCode status) {
  if (status == fileapi::SYNC_STATUS_OK)
    origin_to_contexts_[app_origin] = file_system_context;
  callback.Run(status);
}

void LocalFileSyncService::RunLocalSyncCallback(
    fileapi::SyncStatusCode status,
    const fileapi::FileSystemURL& url) {
  DCHECK(!local_sync_callback_.is_null());
  fileapi::SyncFileCallback callback = local_sync_callback_;
  local_sync_callback_.Reset();
  callback.Run(status, url);
}

void LocalFileSyncService::DidGetFileForLocalSync(
    LocalChangeProcessor* processor,
    SyncStatusCode status,
    const LocalFileSyncInfo& sync_file_info) {
  DCHECK(!local_sync_callback_.is_null());
  if (status != fileapi::SYNC_STATUS_OK) {
    RunLocalSyncCallback(status, FileSystemURL());
    return;
  }
  if (sync_file_info.changes.empty()) {
    // There's a slight chance this could happen.
    fileapi::SyncFileCallback callback = local_sync_callback_;
    local_sync_callback_.Reset();
    ProcessLocalChange(processor, callback);
    return;
  }

  DVLOG(1) << "ProcessLocalChange: " << sync_file_info.url.DebugString()
           << " change:" << sync_file_info.changes.front().DebugString();

  DCHECK(processor);
  processor->ApplyLocalChange(
      sync_file_info.changes.front(),
      sync_file_info.local_file_path,
      sync_file_info.metadata,
      sync_file_info.url,
      base::Bind(&LocalFileSyncService::ProcessNextChangeForURL,
                 AsWeakPtr(), processor,
                 sync_file_info,
                 sync_file_info.changes.front(),
                 sync_file_info.changes.PopAndGetNewList()));
}

void LocalFileSyncService::ProcessNextChangeForURL(
    LocalChangeProcessor* processor,
    const LocalFileSyncInfo& sync_file_info,
    const FileChange& last_change,
    const FileChangeList& changes,
    SyncStatusCode status) {
  DVLOG(1) << "Processed one local change: "
           << sync_file_info.url.DebugString()
           << " change:" << last_change.DebugString()
           << " status:" << status;

  if (status == fileapi::SYNC_FILE_ERROR_NOT_FOUND &&
      last_change.change() == FileChange::FILE_CHANGE_DELETE) {
    // This must be ok (and could happen).
    status = fileapi::SYNC_STATUS_OK;
  }

  // TODO(kinuko,tzik): Handle other errors that should not be considered
  // a sync error.

  const FileSystemURL& url = sync_file_info.url;
  if (status != fileapi::SYNC_STATUS_OK || changes.empty()) {
    if (status == fileapi::SYNC_STATUS_OK ||
        status == fileapi::SYNC_STATUS_HAS_CONFLICT) {
      // Clear the recorded changes for the URL if the sync was successfull
      // OR has failed due to conflict (so that we won't stick to the same
      // conflicting file again and again).
      DCHECK(ContainsKey(origin_to_contexts_, url.origin()));
      sync_context_->FinalizeSyncForURL(origin_to_contexts_[url.origin()], url);
    }
    RunLocalSyncCallback(status, url);
    return;
  }

  DCHECK(processor);
  processor->ApplyLocalChange(
      changes.front(),
      sync_file_info.local_file_path,
      sync_file_info.metadata,
      url,
      base::Bind(&LocalFileSyncService::ProcessNextChangeForURL,
                 AsWeakPtr(), processor, sync_file_info,
                 changes.front(), changes.PopAndGetNewList()));
}

}  // namespace sync_file_system
