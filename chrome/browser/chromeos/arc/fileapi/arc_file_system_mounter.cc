// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_mounter.h"

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/fileapi/external_mount_points.h"

using content::BrowserThread;

namespace arc {

// static
const char ArcFileSystemMounter::kArcServiceName[] =
    "arc::ArcFileSystemMounter";

ArcFileSystemMounter::ArcFileSystemMounter(ArcBridgeService* bridge_service)
    : ArcService(bridge_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  mount_points->RegisterFileSystem(
      kContentFileSystemMountPointName, storage::kFileSystemTypeArcContent,
      storage::FileSystemMountOption(),
      base::FilePath(kContentFileSystemMountPointPath));
  mount_points->RegisterFileSystem(
      kDocumentsProviderMountPointName,
      storage::kFileSystemTypeArcDocumentsProvider,
      storage::FileSystemMountOption(),
      base::FilePath(kDocumentsProviderMountPointPath));
}

ArcFileSystemMounter::~ArcFileSystemMounter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();

  mount_points->RevokeFileSystem(kContentFileSystemMountPointName);
  mount_points->RevokeFileSystem(kDocumentsProviderMountPointPath);
}

}  // namespace arc
