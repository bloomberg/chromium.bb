// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_TEST_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive_resource_metadata.h"
#include "chrome/browser/chromeos/drive/search_metadata.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/test_util.h"

namespace base {
class FilePath;
class Value;
}

namespace drive {

class DriveCache;
class DriveCacheEntry;
class DriveEntryProto;
class ChangeListLoader;

typedef std::vector<DriveEntryProto> DriveEntryProtoVector;

namespace test_util {

// This is a bitmask of cache states in DriveCacheEntry. Used only in tests.
enum TestDriveCacheState {
  TEST_CACHE_STATE_NONE       = 0,
  TEST_CACHE_STATE_PINNED     = 1 << 0,
  TEST_CACHE_STATE_PRESENT    = 1 << 1,
  TEST_CACHE_STATE_DIRTY      = 1 << 2,
  TEST_CACHE_STATE_MOUNTED    = 1 << 3,
  TEST_CACHE_STATE_PERSISTENT = 1 << 4,
};

// Converts |cache_state| which is a bit mask of TestDriveCacheState, to a
// DriveCacheEntry.
DriveCacheEntry ToCacheEntry(int cache_state);

// Returns true if the cache state of the given two cache entries are equal.
bool CacheStatesEqual(const DriveCacheEntry& a, const DriveCacheEntry& b);

// Copies |error| and |entries| to |out_error| and |out_entries|
// respectively. Used to run asynchronous functions that take
// ReadDirectoryCallback from tests.
void CopyResultsFromReadDirectoryCallback(
    DriveFileError* out_error,
    scoped_ptr<DriveEntryProtoVector>* out_entries,
    DriveFileError error,
    scoped_ptr<DriveEntryProtoVector> entries);

// Copies |error| and |entries| to |out_error| and |out_entries|
// respectively. Used to run asynchronous functions that take
// ReadDirectoryCallback from tests.
void CopyResultsFromReadDirectoryByPathCallback(
    DriveFileError* out_error,
    scoped_ptr<DriveEntryProtoVector>* out_entries,
    DriveFileError error,
    bool /* hide_hosted_documents */,
    scoped_ptr<DriveEntryProtoVector> entries);

// Copies |success| to |out_success|. Used to run asynchronous functions that
// take InitializeCacheCallback from tests.
void CopyResultFromInitializeCacheCallback(bool* out_success,
                                           bool success);

// Copies results from DriveCache methods. Used to run asynchronous functions
// that take GetFileFromCacheCallback from tests.
void CopyResultsFromGetFileFromCacheCallback(
    DriveFileError* out_error,
    base::FilePath* out_cache_file_path,
    DriveFileError error,
    const base::FilePath& cache_file_path);

// Copies results from DriveCache methods. Used to run asynchronous functions
// that take GetCacheEntryCallback from tests.
void CopyResultsFromGetCacheEntryCallback(bool* out_success,
                                          DriveCacheEntry* out_cache_entry,
                                          bool success,
                                          const DriveCacheEntry& cache_entry);

// Copies results from DriveFileSystem methods. Used to run asynchronous
// functions that take GetFileCallback from tests.
void CopyResultsFromGetFileCallback(DriveFileError* out_error,
                                    base::FilePath* out_file_path,
                                    DriveFileType* out_file_type,
                                    DriveFileError error,
                                    const base::FilePath& file_path,
                                    const std::string& mime_type,
                                    DriveFileType file_type);

// Copies results from DriveFileSystem methods. Used to run asynchronous
// functions that take GetAvailableSpaceCallback from tests.
void CopyResultsFromGetAvailableSpaceCallback(DriveFileError* out_error,
                                              int64* out_bytes_total,
                                              int64* out_bytes_used,
                                              DriveFileError error,
                                              int64 bytes_total,
                                              int64 bytes_used);

// Copies results from SearchMetadataCallback.
void CopyResultsFromSearchMetadataCallback(
    DriveFileError* out_error,
    scoped_ptr<MetadataSearchResultVector>* out_result,
    DriveFileError error,
    scoped_ptr<MetadataSearchResultVector> result);

// Copies the results from DriveFileSystem methods and stops the message loop
// of the current thread. Used to run asynchronous function that take
// OpenFileCallback.
void CopyResultsFromOpenFileCallbackAndQuit(DriveFileError* out_error,
                                            base::FilePath* out_file_path,
                                            DriveFileError error,
                                            const base::FilePath& file_path);

// Copies the results from DriveFileSystem methods and stops the message loop
// of the current thread. Used to run asynchronous function that take
// CloseFileCallback.
void CopyResultsFromCloseFileCallbackAndQuit(DriveFileError* out_error,
                                             DriveFileError error);

// Loads a test json file as root ("/drive") element from a test file stored
// under chrome/test/data/chromeos. Returns true on success.
bool LoadChangeFeed(const std::string& relative_path,
                    ChangeListLoader* change_list_loader,
                    bool is_delta_feed,
                    const std::string& root_resource_id,
                    int64 root_feed_changestamp);

// Helper to destroy objects which needs Destroy() to be called on destruction.
// Note: When using this helper, you should destruct objects before
// BrowserThread.
struct DestroyHelperForTests {
  template<typename T>
  void operator()(T* object) const {
    if (object) {
      object->Destroy();
      google_apis::test_util::RunBlockingPoolTask();  // Finish destruction.
    }
  }
};

}  // namespace test_util
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_TEST_UTIL_H_
