// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_watcher_manager.h"

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root_map.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/fileapi/file_system_url.h"

using content::BrowserThread;
using storage::FileSystemURL;

namespace arc {

ArcDocumentsProviderWatcherManager::ArcDocumentsProviderWatcherManager(
    ArcDocumentsProviderRootMap* roots)
    : roots_(roots), weak_ptr_factory_(this) {}

ArcDocumentsProviderWatcherManager::~ArcDocumentsProviderWatcherManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void ArcDocumentsProviderWatcherManager::AddWatcher(
    const FileSystemURL& url,
    bool recursive,
    const StatusCallback& callback,
    const NotificationCallback& notification_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (recursive) {
    // Recursive watching is not supported.
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots_->ParseAndLookup(url, &path);
  if (!root) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  root->AddWatcher(path, notification_callback, callback);
}

void ArcDocumentsProviderWatcherManager::RemoveWatcher(
    const FileSystemURL& url,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (recursive) {
    // Recursive watching is not supported.
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  base::FilePath path;
  ArcDocumentsProviderRoot* root = roots_->ParseAndLookup(url, &path);
  if (!root) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  root->RemoveWatcher(path, callback);
}

}  // namespace arc
