// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_WATCHER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_WATCHER_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/fileapi/watcher_manager.h"

namespace arc {

class ArcDocumentsProviderRootMap;

// The implementation of storage::WatcherManager for ARC documents provider
// filesystem.
//
// Note that this WatcherManager is not always correct. See comments at
// ArcDocumentsProviderRoot::AddWatcher().
class ArcDocumentsProviderWatcherManager : public storage::WatcherManager {
 public:
  explicit ArcDocumentsProviderWatcherManager(
      ArcDocumentsProviderRootMap* roots);
  ~ArcDocumentsProviderWatcherManager() override;

  // storage::WatcherManager overrides.
  void AddWatcher(const storage::FileSystemURL& url,
                  bool recursive,
                  const StatusCallback& callback,
                  const NotificationCallback& notification_callback) override;
  void RemoveWatcher(const storage::FileSystemURL& url,
                     bool recursive,
                     const StatusCallback& callback) override;

 private:
  // Owned by ArcDocumentsProviderBackendDelegate.
  ArcDocumentsProviderRootMap* const roots_;

  base::WeakPtrFactory<ArcDocumentsProviderWatcherManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcDocumentsProviderWatcherManager);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_DOCUMENTS_PROVIDER_WATCHER_MANAGER_H_
