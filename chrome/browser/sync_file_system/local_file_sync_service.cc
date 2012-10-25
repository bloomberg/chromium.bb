// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local_file_sync_service.h"

#include "base/stl_util.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/local_file_sync_context.h"

using content::BrowserThread;
using fileapi::LocalFileSyncContext;
using fileapi::StatusCallback;
using fileapi::SyncCompletionCallback;

namespace sync_file_system {

LocalFileSyncService::LocalFileSyncService()
    : sync_context_(new LocalFileSyncContext(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO))) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

LocalFileSyncService::~LocalFileSyncService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void LocalFileSyncService::Shutdown() {
  sync_context_->ShutdownOnUIThread();
}

void LocalFileSyncService::MaybeInitializeFileSystemContext(
    const GURL& app_origin,
    fileapi::FileSystemContext* file_system_context,
    const StatusCallback& callback) {
  sync_context_->MaybeInitializeFileSystemContext(
      app_origin, file_system_context,
      base::Bind(&LocalFileSyncService::DidInitializeFileSystemContext,
                 AsWeakPtr(), app_origin, file_system_context, callback));
}

void LocalFileSyncService::ProcessChange(
    LocalChangeProcessor* processor,
    const SyncCompletionCallback& callback) {
  // TODO(kinuko): implement.
  NOTIMPLEMENTED();
}

void LocalFileSyncService::PrepareForProcessRemoteChange(
    const fileapi::FileSystemURL& url,
    const PrepareChangeCallback& callback) {
  // TODO(kinuko): implement.
  NOTIMPLEMENTED();
}

void LocalFileSyncService::ApplyRemoteChange(
    const fileapi::FileChange& change,
    const FilePath& local_path,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(ContainsKey(origin_to_contexts_, url.origin()));
  sync_context_->ApplyRemoteChange(
      origin_to_contexts_[url.origin()],
      change, local_path, url, callback);
}

void LocalFileSyncService::DidInitializeFileSystemContext(
    const GURL& app_origin,
    fileapi::FileSystemContext* file_system_context,
    const StatusCallback& callback,
    fileapi::SyncStatusCode status) {
  if (status == fileapi::SYNC_STATUS_OK)
    origin_to_contexts_[app_origin] = file_system_context;
  callback.Run(status);
}

}  // namespace sync_file_system
