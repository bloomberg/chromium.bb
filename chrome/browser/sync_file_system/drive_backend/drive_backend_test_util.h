// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_TEST_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"

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

void ExpectEquivalentServiceMetadata(const ServiceMetadata& left,
                                     const ServiceMetadata& right);
void ExpectEquivalentDetails(const FileDetails& left, const FileDetails& right);
void ExpectEquivalentMetadata(const FileMetadata& left,
                              const FileMetadata& right);
void ExpectEquivalentTrackers(const FileTracker& left,
                              const FileTracker& right);
void ExpectEquivalentResourceAndMetadata(
    const google_apis::FileResource& resource,
    const FileMetadata& metadata);
void ExpectEquivalentMetadataAndTracker(const FileMetadata& metadata,
                                        const FileTracker& tracker);

}  // namespace test_util
}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_TEST_UTIL_H_
