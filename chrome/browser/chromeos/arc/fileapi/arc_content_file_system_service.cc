// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_service.h"

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "storage/browser/fileapi/external_mount_points.h"

namespace arc {

ArcContentFileSystemService::ArcContentFileSystemService(
    ArcBridgeService* arc_bridge_service)
    : ArcService(arc_bridge_service) {
  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      kMountPointName, storage::kFileSystemTypeArcContent,
      storage::FileSystemMountOption(), base::FilePath(kMountPointPath));
}

ArcContentFileSystemService::~ArcContentFileSystemService() {
  storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
      kMountPointName);
}

}  // namespace arc
