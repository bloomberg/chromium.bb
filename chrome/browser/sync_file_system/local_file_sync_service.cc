// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local_file_sync_service.h"

#include "base/stl_util.h"
#include "chrome/browser/sync_file_system/change_observer_interface.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/local_file_change_tracker.h"
#include "webkit/fileapi/syncable/local_file_sync_context.h"

using content::BrowserThread;
using fileapi::LocalFileSyncContext;
using fileapi::SyncStatusCallback;
using fileapi::SyncCompletionCallback;

namespace sync_file_system {

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

void LocalFileSyncService::AddChangeObserver(LocalChangeObserver* observer) {
  change_observers_.AddObserver(observer);
}

void LocalFileSyncService::ProcessLocalChange(
    LocalChangeProcessor* processor,
    const SyncCompletionCallback& callback) {
  // TODO(kinuko): implement.
  NOTIMPLEMENTED();
}

void LocalFileSyncService::PrepareForProcessRemoteChange(
    const fileapi::FileSystemURL& url,
    const PrepareChangeCallback& callback) {
  DCHECK(ContainsKey(origin_to_contexts_, url.origin()));
  sync_context_->PrepareForSync(
      origin_to_contexts_[url.origin()],
      url, callback);
}

void LocalFileSyncService::ApplyRemoteChange(
    const fileapi::FileChange& change,
    const FilePath& local_path,
    const fileapi::FileSystemURL& url,
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
    per_origin_changes_[origin] = context->change_tracker()->num_changes();
  }
  int64 num_changes = 0;
  for (std::map<GURL, int64>::iterator iter = per_origin_changes_.begin();
       iter != per_origin_changes_.end(); ++iter) {
    num_changes += iter->second;
  }
  FOR_EACH_OBSERVER(LocalChangeObserver, change_observers_,
                    OnLocalChangeAvailable(num_changes));
}

void LocalFileSyncService::DidInitializeFileSystemContext(
    const GURL& app_origin,
    fileapi::FileSystemContext* file_system_context,
    const SyncStatusCallback& callback,
    fileapi::SyncStatusCode status) {
  if (status == fileapi::SYNC_STATUS_OK)
    origin_to_contexts_[app_origin] = file_system_context;
  callback.Run(status);
}

}  // namespace sync_file_system
