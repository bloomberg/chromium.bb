// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_TEST_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/gdata/drive_resource_metadata.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

class FilePath;

namespace base {
class Value;
}

namespace gdata {

class DriveCacheEntry;
class DriveEntryProto;
class DriveFileSystem;

typedef std::vector<DriveEntryProto> DriveEntryProtoVector;

namespace test_util {

// Runs a task posted to the blocking pool, including subquent tasks posted
// to the UI message loop and the blocking pool.
//
// A task is often posted to the blocking pool with PostTaskAndReply(). In
// that case, a task is posted back to the UI message loop, which can again
// post a task to the blocking pool. This function processes these tasks
// repeatedly.
void RunBlockingPoolTask();

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

// Copies |error| to |output|. Used to run asynchronous functions that take
// FileOperationCallback from tests.
void CopyErrorCodeFromFileOperationCallback(DriveFileError* output,
                                            DriveFileError error);

// Copies |error| and |moved_file_path| to |out_error| and |out_file_path|.
// Used to run asynchronous functions that take FileMoveCallback from tests.
void CopyResultsFromFileMoveCallback(DriveFileError* out_error,
                                     FilePath* out_file_path,
                                     DriveFileError error,
                                     const FilePath& moved_file_path);

// Copies |error| and |entry_proto| to |out_error| and |out_entry_proto|
// respectively. Used to run asynchronous functions that take
// GetEntryInfoCallback from tests.
void CopyResultsFromGetEntryInfoCallback(
    DriveFileError* out_error,
    scoped_ptr<DriveEntryProto>* out_entry_proto,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto);

// Copies |error| and |entries| to |out_error| and |out_entries|
// respectively. Used to run asynchronous functions that take
// GetEntryInfoCallback from tests.
void CopyResultsFromReadDirectoryCallback(
    DriveFileError* out_error,
    scoped_ptr<DriveEntryProtoVector>* out_entries,
    DriveFileError error,
    scoped_ptr<DriveEntryProtoVector> entries);

// Copies |error|, |drive_file_path|, and |entry_proto| to |out_error|,
// |out_drive_file_path|, and |out_entry_proto| respectively. Used to run
// asynchronous functions that take GetEntryInfoWithFilePathCallback from
// tests.
void CopyResultsFromGetEntryInfoWithFilePathCallback(
    DriveFileError* out_error,
    FilePath* out_drive_file_path,
    scoped_ptr<DriveEntryProto>* out_entry_proto,
    DriveFileError error,
    const FilePath& drive_file_path,
    scoped_ptr<DriveEntryProto> entry_proto);

// Copies |result| to |out_result|. Used to run asynchronous functions
// that take GetEntryInfoPairCallback from tests.
void CopyResultsFromGetEntryInfoPairCallback(
    scoped_ptr<EntryInfoPairResult>* out_result,
    scoped_ptr<EntryInfoPairResult> result);

// Returns the absolute path for a test file stored under
// chrome/test/data/chromeos.
FilePath GetTestFilePath(const std::string& relative_path);

// Loads a test JSON file as a base::Value, from a test file stored under
// chrome/test/data/chromeos.
//
// The caller should delete the returned object.
base::Value* LoadJSONFile(const std::string& relative_path);

// Loads a test json file as root ("/drive") element from a test file stored
// under chrome/test/data/chromeos.
void LoadChangeFeed(const std::string& relative_path,
                    DriveFileSystem* file_system,
                    int64 start_changestamp,
                    int64 root_feed_changestamp);

}  // namespace test_util
}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_TEST_UTIL_H_
