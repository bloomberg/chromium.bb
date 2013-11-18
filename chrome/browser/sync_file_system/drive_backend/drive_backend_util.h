// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_UTIL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_UTIL_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "webkit/common/blob/scoped_file.h"

namespace google_apis {
class ChangeResource;
class FileResource;
}

namespace leveldb {
class WriteBatch;
}

namespace sync_file_system {
namespace drive_backend {

void PutServiceMetadataToBatch(const ServiceMetadata& service_metadata,
                               leveldb::WriteBatch* batch);
void PutFileToBatch(const FileMetadata& file, leveldb::WriteBatch* batch);
void PutTrackerToBatch(const FileTracker& tracker, leveldb::WriteBatch* batch);

void PopulateFileDetailsByFileResource(
    const google_apis::FileResource& file_resource,
    FileDetails* details);
scoped_ptr<FileMetadata> CreateFileMetadataFromFileResource(
    int64 change_id,
    const google_apis::FileResource& resource);
scoped_ptr<FileMetadata> CreateFileMetadataFromChangeResource(
    const google_apis::ChangeResource& change);

// Creates a temporary file in |dir_path|.  This must be called on an
// IO-allowed thread.
webkit_blob::ScopedFile CreateTemporaryFile();

std::string FileKindToString(FileKind file_kind);

bool HasFileAsParent(const FileDetails& details, const std::string& file_id);

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_UTIL_H_
