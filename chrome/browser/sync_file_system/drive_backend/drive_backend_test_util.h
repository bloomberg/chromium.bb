// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_TEST_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "google_apis/drive/gdata_errorcode.h"

namespace google_apis {
class FileResource;
class ResourceEntry;
}

namespace sync_file_system {
namespace drive_backend {

class FileDetails;
class FileMetadata;
class FileTracker;
class MetadataDatabase;
class ServiceMetadata;

namespace test_util {

void ExpectEquivalentDetails(const FileDetails& left, const FileDetails& right);
void ExpectEquivalentMetadata(const FileMetadata& left,
                              const FileMetadata& right);
void ExpectEquivalentTrackers(const FileTracker& left,
                              const FileTracker& right);

scoped_ptr<FileMetadata> CreateFolderMetadata(const std::string& file_id,
                                              const std::string& title);
scoped_ptr<FileMetadata> CreateFileMetadata(const std::string& file_id,
                                            const std::string& title,
                                            const std::string& md5);
scoped_ptr<FileTracker> CreateTracker(const FileMetadata& metadata,
                                      int64 tracker_id,
                                      const FileTracker* parent_tracker);
scoped_ptr<FileTracker> CreatePlaceholderTracker(
    const std::string& file_id,
    int64 tracker_id,
    const FileTracker* parent_tracker);

}  // namespace test_util
}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_TEST_UTIL_H_
