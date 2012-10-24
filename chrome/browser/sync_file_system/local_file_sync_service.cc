// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local_file_sync_service.h"

#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/local_file_sync_context.h"

using content::BrowserThread;

namespace sync_file_system {

LocalFileSyncService::LocalFileSyncService()
    : sync_context_(new fileapi::LocalFileSyncContext(
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
    const GURL& app_url,
    fileapi::FileSystemContext* file_system_context,
    const StatusCallback& callback) {
  sync_context_->MaybeInitializeFileSystemContext(
      app_url, file_system_context, callback);
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
  // TODO(kinuko): implement.
  NOTIMPLEMENTED();
}

}  // namespace sync_file_system
