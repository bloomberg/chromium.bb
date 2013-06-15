// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_FILE_METADATA_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_FILE_METADATA_H_

#include <string>

#include "base/basictypes.h"
#include "webkit/browser/fileapi/syncable/sync_file_status.h"

namespace sync_file_system {

enum FileType {
  TYPE_FILE = 0,
  TYPE_FOLDER = 1
};

// Abstraction of FileMetadata to structure common elements that all service
// specific metadata should be able to map to. e.g. DriveMetadata.
struct FileMetadata {
  FileMetadata() {}

  // sync_status is not known until SyncFileSystemService because
  // local_file_sync_service has to be checked for pending changes.
  FileMetadata(const std::string& title,
               FileType type,
               const std::string& service_specific_metadata)
     : title(title),
       type(type),
       sync_status(SYNC_FILE_STATUS_UNKNOWN),
       service_specific_metadata(service_specific_metadata) {}


  // Common information.
  std::string title;
  FileType type;
  SyncFileStatus sync_status;

  // Sync service specific information.
  std::string service_specific_metadata;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_FILE_METADATA_H_
