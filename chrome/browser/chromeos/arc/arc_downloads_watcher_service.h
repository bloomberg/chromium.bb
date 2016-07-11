// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_DOWNLOADS_WATCHER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_DOWNLOADS_WATCHER_SERVICE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"

namespace base {
class FilePath;
}

namespace arc {

// Watches Downloads directory and registers newly created media files to
// Android MediaProvider.
class ArcDownloadsWatcherService : public ArcService,
                                   public ArcBridgeService::Observer {
 public:
  explicit ArcDownloadsWatcherService(ArcBridgeService* bridge_service);
  ~ArcDownloadsWatcherService() override;

  // ArcBridgeService::Observer
  void OnFileSystemInstanceReady() override;
  void OnFileSystemInstanceClosed() override;

 private:
  class DownloadsWatcher;

  void StartWatchingDownloads();
  void StopWatchingDownloads();

  void OnDownloadsChanged(const std::vector<base::FilePath>& paths);

  std::unique_ptr<DownloadsWatcher> watcher_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ArcDownloadsWatcherService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcDownloadsWatcherService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_DOWNLOADS_WATCHER_SERVICE_H_
