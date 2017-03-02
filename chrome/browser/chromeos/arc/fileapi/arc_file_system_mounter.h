// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_MOUNTER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_MOUNTER_H_

#include "base/macros.h"
#include "components/arc/arc_service.h"

namespace arc {

class ArcBridgeService;

// Mounts/unmounts ARC file systems.
class ArcFileSystemMounter : public ArcService {
 public:
  // For supporting ArcServiceManager::GetService<T>().
  static const char kArcServiceName[];

  explicit ArcFileSystemMounter(ArcBridgeService* bridge_service);
  ~ArcFileSystemMounter() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcFileSystemMounter);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_MOUNTER_H_
