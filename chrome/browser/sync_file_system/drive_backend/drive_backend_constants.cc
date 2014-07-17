// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"

namespace sync_file_system {
namespace drive_backend {

const char kSyncRootFolderTitle[] = "Chrome Syncable FileSystem";
const char kSyncRootFolderTitleDev[] = "Chrome Syncable FileSystem Dev";
const char kMimeTypeOctetStream[] = "application/octet-stream";

const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("DriveMetadata_v2");

const char kDatabaseVersionKey[] = "VERSION";
const int64 kCurrentDatabaseVersion = 3;
const int64 kDatabaseOnDiskVersion = 4;
const char kServiceMetadataKey[] = "SERVICE";
const char kFileMetadataKeyPrefix[] = "FILE: ";
const char kFileTrackerKeyPrefix[] = "TRACKER: ";

const char kAppRootIDByAppIDKeyPrefix[] = "APP_ROOT: ";
const char kActiveTrackerIDByFileIDKeyPrefix[] = "ACTIVE_FILE: ";
const char kTrackerIDByFileIDKeyPrefix[] = "TRACKER_FILE: ";
const char kMultiTrackerByFileIDKeyPrefix[] = "MULTI_FILE: ";
const char kActiveTrackerIDByParentAndTitleKeyPrefix[] = "ACTIVE_PATH: ";
const char kTrackerIDByParentAndTitleKeyPrefix[] = "TRACKER_PATH: ";
const char kMultiBackingParentAndTitleKeyPrefix[] = "MULTI_PATH: ";
const char kDirtyIDKeyPrefix[] = "DIRTY: ";
const char kDemotedDirtyIDKeyPrefix[] = "DEMOTED_DIRTY: ";

const int kMaxRetry = 5;
const int64 kListChangesRetryDelaySeconds = 60 * 60;

const int64 kInvalidTrackerID = 0;

}  // namespace drive_backend
}  // namespace sync_file_system
