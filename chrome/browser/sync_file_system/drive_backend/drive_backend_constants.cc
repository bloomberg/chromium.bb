// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"

namespace sync_file_system {
namespace drive_backend {

const char kSyncRootFolderTitle[] = "Chrome Syncable FileSystem";
const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("DriveMetadata");

}  // namespace drive_backend
}  // namespace sync_file_system
