// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_UTIL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_UTIL_H_

namespace leveldb {
class WriteBatch;
}

namespace sync_file_system {
namespace drive_backend {

class ServiceMetadata;
class FileMetadata;
class FileTracker;

void PutServiceMetadataToBatch(const ServiceMetadata& service_metadata,
                               leveldb::WriteBatch* batch);
void PutFileToBatch(const FileMetadata& file, leveldb::WriteBatch* batch);
void PutTrackerToBatch(const FileTracker& tracker, leveldb::WriteBatch* batch);

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_UTIL_H_
