// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_SERVICE_H_

#include "base/macros.h"
#include "components/arc/arc_service.h"

namespace arc {

class ArcBridgeService;

// ArcContentFileSystemService registers ARC content file system to the system.
class ArcContentFileSystemService : public ArcService {
 public:
  explicit ArcContentFileSystemService(ArcBridgeService* arc_bridge_service);
  ~ArcContentFileSystemService() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcContentFileSystemService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_SERVICE_H_
