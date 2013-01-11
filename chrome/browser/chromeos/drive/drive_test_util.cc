// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_test_util.h"

#include <string>

#include "base/bind_helpers.h"
#include "base/json/json_file_value_serializer.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_feed_loader.h"
#include "chrome/browser/chromeos/drive/drive_file_system.h"
#include "chrome/browser/google_apis/drive_api_parser.h"

namespace drive {

namespace test_util {

DriveCacheEntry ToCacheEntry(int cache_state) {
  DriveCacheEntry cache_entry;
  cache_entry.set_is_present(cache_state & TEST_CACHE_STATE_PRESENT);
  cache_entry.set_is_pinned(cache_state & TEST_CACHE_STATE_PINNED);
  cache_entry.set_is_dirty(cache_state & TEST_CACHE_STATE_DIRTY);
  cache_entry.set_is_mounted(cache_state & TEST_CACHE_STATE_MOUNTED);
  cache_entry.set_is_persistent(cache_state & TEST_CACHE_STATE_PERSISTENT);
  return cache_entry;
}

bool CacheStatesEqual(const DriveCacheEntry& a, const DriveCacheEntry& b) {
  return (a.is_present() == b.is_present() &&
          a.is_pinned() == b.is_pinned() &&
          a.is_dirty() == b.is_dirty() &&
          a.is_mounted() == b.is_mounted() &&
          a.is_persistent() == b.is_persistent());
}

void CopyErrorCodeFromFileOperationCallback(DriveFileError* output,
                                            DriveFileError error) {
  DCHECK(output);
  *output = error;
}

void CopyResultsFromFileMoveCallback(
    DriveFileError* out_error,
    FilePath* out_file_path,
    DriveFileError error,
    const FilePath& moved_file_path) {
  DCHECK(out_error);
  DCHECK(out_file_path);

  *out_error = error;
  *out_file_path = moved_file_path;
}

void CopyResultsFromGetEntryInfoCallback(
    DriveFileError* out_error,
    scoped_ptr<DriveEntryProto>* out_entry_proto,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(out_error);
  DCHECK(out_entry_proto);

  *out_error = error;
  *out_entry_proto = entry_proto.Pass();
}

void CopyResultsFromReadDirectoryCallback(
    DriveFileError* out_error,
    scoped_ptr<DriveEntryProtoVector>* out_entries,
    DriveFileError error,
    scoped_ptr<DriveEntryProtoVector> entries) {
  DCHECK(out_error);
  DCHECK(out_entries);

  *out_error = error;
  *out_entries = entries.Pass();
}

void CopyResultsFromReadDirectoryByPathCallback(
    DriveFileError* out_error,
    scoped_ptr<DriveEntryProtoVector>* out_entries,
    DriveFileError error,
    bool /* hide_hosted_documents */,
    scoped_ptr<DriveEntryProtoVector> entries) {
  DCHECK(out_error);
  DCHECK(out_entries);

  *out_error = error;
  *out_entries = entries.Pass();
}

void CopyResultsFromGetEntryInfoWithFilePathCallback(
    DriveFileError* out_error,
    FilePath* out_drive_file_path,
    scoped_ptr<DriveEntryProto>* out_entry_proto,
    DriveFileError error,
    const FilePath& drive_file_path,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(out_error);
  DCHECK(out_drive_file_path);
  DCHECK(out_entry_proto);

  *out_error = error;
  *out_drive_file_path = drive_file_path;
  *out_entry_proto = entry_proto.Pass();
}

void CopyResultsFromGetEntryInfoPairCallback(
    scoped_ptr<EntryInfoPairResult>* out_result,
    scoped_ptr<EntryInfoPairResult> result) {
  DCHECK(out_result);

  *out_result = result.Pass();
}

void CopyResultFromInitializeCacheCallback(bool* out_success,
                                           bool success) {
  DCHECK(out_success);
  *out_success = success;
}

void CopyResultsFromGetFileFromCacheCallback(DriveFileError* out_error,
                                             FilePath* out_cache_file_path,
                                             DriveFileError error,
                                             const FilePath& cache_file_path) {
  DCHECK(out_error);
  DCHECK(out_cache_file_path);
  *out_error = error;
  *out_cache_file_path = cache_file_path;
}

void CopyResultsFromGetCacheEntryCallback(bool* out_success,
                                          DriveCacheEntry* out_cache_entry,
                                          bool success,
                                          const DriveCacheEntry& cache_entry) {
  DCHECK(out_success);
  DCHECK(out_cache_entry);
  *out_success = success;
  *out_cache_entry = cache_entry;
}

void CopyResultsFromGetFileCallback(DriveFileError* out_error,
                                    FilePath* out_file_path,
                                    DriveFileType* out_file_type,
                                    DriveFileError error,
                                    const FilePath& file_path,
                                    const std::string& /*mime_type*/,
                                    DriveFileType file_type) {
  DCHECK(out_error);
  DCHECK(out_file_path);
  DCHECK(out_file_type);
  *out_error = error;
  *out_file_path = file_path;
  *out_file_type = file_type;
}

void CopyResultsFromGetAvailableSpaceCallback(DriveFileError* out_error,
                                              int64* out_bytes_total,
                                              int64* out_bytes_used,
                                              DriveFileError error,
                                              int64 bytes_total,
                                              int64 bytes_used) {
  DCHECK(out_error);
  DCHECK(out_bytes_total);
  DCHECK(out_bytes_used);
  *out_error = error;
  *out_bytes_total = bytes_total;
  *out_bytes_used = bytes_used;
}

bool LoadChangeFeed(const std::string& relative_path,
                    DriveFileSystem* file_system,
                    bool is_delta_feed,
                    int64 root_feed_changestamp) {
  scoped_ptr<Value> document =
      google_apis::test_util::LoadJSONFile(relative_path);
  if (!document.get())
    return false;
  if (document->GetType() != Value::TYPE_DICTIONARY)
    return false;

  scoped_ptr<google_apis::ResourceList> document_feed(
      google_apis::ResourceList::ExtractAndParse(*document));
  if (!document_feed.get())
    return false;

  ScopedVector<google_apis::ResourceList> feed_list;
  feed_list.push_back(document_feed.release());

  file_system->feed_loader()->UpdateFromFeed(
      feed_list,
      is_delta_feed,
      root_feed_changestamp,
      base::Bind(&base::DoNothing));
  // DriveFeedLoader::UpdateFromFeed is asynchronous, so wait for it to finish.
  google_apis::test_util::RunBlockingPoolTask();

  return true;
}

}  // namespace test_util
}  // namespace drive
