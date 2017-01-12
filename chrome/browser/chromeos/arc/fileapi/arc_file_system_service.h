// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_SERVICE_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/file_system.mojom.h"
#include "components/arc/instance_holder.h"

namespace arc {

class ArcBridgeService;
class ArcFileSystemObserver;

// ArcFileSystemService registers ARC file systems to the system.
class ArcFileSystemService
    : public ArcService,
      public InstanceHolder<mojom::FileSystemInstance>::Observer {
 public:
  explicit ArcFileSystemService(ArcBridgeService* bridge_service);
  ~ArcFileSystemService() override;

  void AddObserver(ArcFileSystemObserver* observer);
  void RemoveObserver(ArcFileSystemObserver* observer);

  // InstanceHolder<mojom::FileSystemInstance>::Observer overrides:
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

  // For supporting ArcServiceManager::GetService<T>().
  static const char kArcServiceName[];

 private:
  base::ObserverList<ArcFileSystemObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ArcFileSystemService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_SERVICE_H_
