// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index_on_disk.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

// LevelDB database schema
// =======================
//
// NOTE
// - Entries are sorted by keys.
// - int64 value is serialized as a string by base::Int64ToString().
// - ServiceMetadata, FileMetadata, and FileTracker values are serialized
//   as a string by SerializeToString() of protocol buffers.
//
// Version 4:
//   # Version of this schema
//   key: "VERSION"
//   value: "4"
//
//   # Metadata of the SyncFS service (compatible with version 3)
//   key: "SERVICE"
//   value: <ServiceMetadata 'service_metadata'>
//
//   # Metadata of remote files (compatible with version 3)
//   key: "FILE: " + <string 'file_id'>
//   value: <FileMetadata 'metadata'>
//
//   # Trackers of remote file updates (compatible with version 3)
//   key: "TRACKER: " + <int64 'tracker_id'>
//   value: <FileTracker 'tracker'>
//
//   # Index from App ID to the tracker ID
//   key: "APP_ROOT: " + <string 'app_id'>
//   value: <int64 'app_root_tracker_id'>
//
//   # Index from file ID to the active tracker ID
//   key: "ACTIVE_BY_FILE: " + <string 'file_id'>
//   value: <int64 'active_tracker_id'>
//
//   # Index from file ID to a tracker ID
//   key: "IDS_BY_FILE: " + <string 'file_id'> + '\x00' + <int64 'tracker_id'>
//   value: <empty>
//
//   # Index from the parent tracker ID and the title to the active tracker ID
//   key: "ACTIVE_BY_PATH_INDEX: " + <int64 'parent_tracker_id'> +
//        '\x00' + <string 'title'>
//   value: <int64 'active_tracker_id'>
//
//   # Index from the parent tracker ID and the title to a tracker ID
//   key: "IDS_BY_PATH_INDEX: " + <int64 'parent_tracker_id'> +
//        '\x00' + <string 'title'> + '\x00' + <int64 'tracker_id'>
//   value: <empty>

namespace sync_file_system {
namespace drive_backend {

MetadataDatabaseIndexOnDisk::MetadataDatabaseIndexOnDisk(leveldb::DB* db)
    : db_(db) {
  // TODO(peria): Add UMA to measure the number of FileMetadata, FileTracker,
  //    and AppRootId.
  // TODO(peria): If the DB version is 3, build up index lists.
}

MetadataDatabaseIndexOnDisk::~MetadataDatabaseIndexOnDisk() {}

bool MetadataDatabaseIndexOnDisk::GetFileMetadata(
    const std::string& file_id, FileMetadata* metadata) const {
  const std::string key(kFileMetadataKeyPrefix + file_id);
  std::string value;
  leveldb::Status status = db_->Get(leveldb::ReadOptions(), key, &value);
  if (!status.ok()) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "LevelDB error (%s) in getting FileMetadata for ID: %s",
              status.ToString().c_str(),
              file_id.c_str());
    return false;
  }

  FileMetadata tmp_metadata;
  if (!tmp_metadata.ParseFromString(value)) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "Failed to parse a FileMetadata for ID: %s",
              file_id.c_str());
    return false;
  }
  if (metadata)
    metadata->CopyFrom(tmp_metadata);

  return true;
}

bool MetadataDatabaseIndexOnDisk::GetFileTracker(
    int64 tracker_id, FileTracker* tracker) const {
  const std::string key(kFileTrackerKeyPrefix +
                        base::Int64ToString(tracker_id));
  std::string value;
  leveldb::Status status = db_->Get(leveldb::ReadOptions(), key, &value);
  if (!status.ok()) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "LevelDB error (%s) in getting FileTracker for ID: %" PRId64,
              status.ToString().c_str(),
              tracker_id);
    return false;
  }

  FileTracker tmp_tracker;
  if (!tmp_tracker.ParseFromString(value)) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "Failed to parse a Tracker for ID: %" PRId64,
              tracker_id);
    return false;
  }
  if (tracker)
    tracker->CopyFrom(tmp_tracker);

  return true;
}

void MetadataDatabaseIndexOnDisk::StoreFileMetadata(
    scoped_ptr<FileMetadata> metadata, leveldb::WriteBatch* batch) {
  DCHECK(metadata);
  PutFileMetadataToBatch(*metadata, batch);
}

void MetadataDatabaseIndexOnDisk::StoreFileTracker(
    scoped_ptr<FileTracker> tracker, leveldb::WriteBatch* batch) {
  DCHECK(tracker);
  PutFileTrackerToBatch(*tracker, batch);
  // TODO(peria): Update indexes.
}

void MetadataDatabaseIndexOnDisk::RemoveFileMetadata(
    const std::string& file_id, leveldb::WriteBatch* batch) {
  PutFileMetadataDeletionToBatch(file_id, batch);
}

void MetadataDatabaseIndexOnDisk::RemoveFileTracker(
    int64 tracker_id, leveldb::WriteBatch* batch) {
  PutFileTrackerDeletionToBatch(tracker_id, batch);
  // TODO(peria): Update indexes.
}

TrackerIDSet MetadataDatabaseIndexOnDisk::GetFileTrackerIDsByFileID(
    const std::string& file_id) const {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
  return TrackerIDSet();
}

int64 MetadataDatabaseIndexOnDisk::GetAppRootTracker(
    const std::string& app_id) const {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
  return 0;
}

TrackerIDSet MetadataDatabaseIndexOnDisk::GetFileTrackerIDsByParentAndTitle(
    int64 parent_tracker_id,
    const std::string& title) const {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
  return TrackerIDSet();
}

std::vector<int64> MetadataDatabaseIndexOnDisk::GetFileTrackerIDsByParent(
    int64 parent_tracker_id) const {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
  return std::vector<int64>();
}

std::string MetadataDatabaseIndexOnDisk::PickMultiTrackerFileID() const {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
  return std::string();
}

ParentIDAndTitle MetadataDatabaseIndexOnDisk::PickMultiBackingFilePath() const {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
  return ParentIDAndTitle();
}

int64 MetadataDatabaseIndexOnDisk::PickDirtyTracker() const {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
  return 0;
}

void MetadataDatabaseIndexOnDisk::DemoteDirtyTracker(int64 tracker_id) {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
}

bool MetadataDatabaseIndexOnDisk::HasDemotedDirtyTracker() const {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
  return true;
}

void MetadataDatabaseIndexOnDisk::PromoteDemotedDirtyTrackers() {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
}

size_t MetadataDatabaseIndexOnDisk::CountDirtyTracker() const {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
  return 0;
}

size_t MetadataDatabaseIndexOnDisk::CountFileMetadata() const {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
  return 0;
}

size_t MetadataDatabaseIndexOnDisk::CountFileTracker() const {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
  return 0;
}

std::vector<std::string>
MetadataDatabaseIndexOnDisk::GetRegisteredAppIDs() const {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
  return std::vector<std::string>();
}

std::vector<int64> MetadataDatabaseIndexOnDisk::GetAllTrackerIDs() const {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
  return std::vector<int64>();
}

std::vector<std::string>
MetadataDatabaseIndexOnDisk::GetAllMetadataIDs() const {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
  return std::vector<std::string>();
}

}  // namespace drive_backend
}  // namespace sync_file_system
