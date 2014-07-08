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
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

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
//
//   # Dirty tracker IDs
//   key: "DIRTY: " + <int64 'dirty_tracker_id'>
//   value: <empty>
//
//   # Demoted dirty tracker IDs
//   key: "DEMOTED_DIRTY: " + <int64 'demoted_dirty_tracker_id'>
//   value: <empty>

namespace sync_file_system {
namespace drive_backend {

namespace {

std::string GenerateAppRootIDByAppIDKey(const std::string& app_id) {
  return kAppRootIDByAppIDKeyPrefix + app_id;
}

std::string GenerateDirtyIDKey(int64 tracker_id) {
  return kDirtyIDKeyPrefix + base::Int64ToString(tracker_id);
}

std::string GenerateDemotedDirtyIDKey(int64 tracker_id) {
  return kDemotedDirtyIDKeyPrefix + base::Int64ToString(tracker_id);
}

}  // namespace

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

  if (status.IsNotFound())
    return false;

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

  if (status.IsNotFound())
    return false;

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

  int64 tracker_id = tracker->tracker_id();
  FileTracker old_tracker;
  if (!GetFileTracker(tracker_id, &old_tracker)) {
    DVLOG(3) << "Adding new tracker: " << tracker->tracker_id()
             << " " << GetTrackerTitle(*tracker);
    AddToAppIDIndex(*tracker, batch);
    AddToDirtyTrackerIndexes(*tracker, batch);
    // TODO(peria): Add other indexes.
  } else {
    DVLOG(3) << "Updating tracker: " << tracker->tracker_id()
             << " " << GetTrackerTitle(*tracker);
    UpdateInAppIDIndex(old_tracker, *tracker, batch);
    UpdateInDirtyTrackerIndexes(old_tracker, *tracker, batch);
    // TODO(peria): Update other indexes.
  }
}

void MetadataDatabaseIndexOnDisk::RemoveFileMetadata(
    const std::string& file_id, leveldb::WriteBatch* batch) {
  PutFileMetadataDeletionToBatch(file_id, batch);
}

void MetadataDatabaseIndexOnDisk::RemoveFileTracker(
    int64 tracker_id, leveldb::WriteBatch* batch) {
  PutFileTrackerDeletionToBatch(tracker_id, batch);

  FileTracker tracker;
  if (!GetFileTracker(tracker_id, &tracker)) {
    NOTREACHED();
    return;
  }

  DVLOG(3) << "Removing tracker: "
           << tracker.tracker_id() << " " << GetTrackerTitle(tracker);
  RemoveFromAppIDIndex(tracker, batch);
  RemoveFromDirtyTrackerIndexes(tracker, batch);
  // TODO(peria): Remove from other indexes.
}

TrackerIDSet MetadataDatabaseIndexOnDisk::GetFileTrackerIDsByFileID(
    const std::string& file_id) const {
  // TODO(peria): Implement here
  NOTIMPLEMENTED();
  return TrackerIDSet();
}

int64 MetadataDatabaseIndexOnDisk::GetAppRootTracker(
    const std::string& app_id) const {
  const std::string& key(GenerateAppRootIDByAppIDKey(app_id));
  std::string value;
  leveldb::Status status = db_->Get(leveldb::ReadOptions(), key, &value);

  if (status.IsNotFound())
    return kInvalidTrackerID;

  if (!status.ok()) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "LevelDB error (%s) in getting AppRoot for AppID: %s",
              status.ToString().c_str(),
              app_id.c_str());
    return kInvalidTrackerID;
  }

  int64 root_id;
  if (!base::StringToInt64(value, &root_id)) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "Failed to parse a root ID (%s) for an App ID: %s",
              value.c_str(),
              app_id.c_str());
    return kInvalidTrackerID;
  }

  return root_id;
}

TrackerIDSet MetadataDatabaseIndexOnDisk::GetFileTrackerIDsByParentAndTitle(
    int64 parent_tracker_id, const std::string& title) const {
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
  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  itr->Seek(kDirtyIDKeyPrefix);
  if (!itr->Valid())
    return kInvalidTrackerID;

  std::string id_str;
  if (!RemovePrefix(itr->key().ToString(), kDirtyIDKeyPrefix, &id_str))
    return kInvalidTrackerID;

  int64 tracker_id;
  if (!base::StringToInt64(id_str, &tracker_id))
    return kInvalidTrackerID;

  return tracker_id;
}

void MetadataDatabaseIndexOnDisk::DemoteDirtyTracker(
    int64 tracker_id, leveldb::WriteBatch* batch) {
  const std::string& key(GenerateDirtyIDKey(tracker_id));

  std::string value;
  leveldb::Status status = db_->Get(leveldb::ReadOptions(), key, &value);
  if (status.IsNotFound())
    return;
  if (!status.ok()) {
    util::Log(logging::LOG_WARNING, FROM_HERE,
              "LevelDB error (%s) in getting a dirty tracker for ID: %" PRId64,
              status.ToString().c_str(),
              tracker_id);
    return;
  }

  batch->Delete(key);
  batch->Put(GenerateDemotedDirtyIDKey(tracker_id), "");
}

bool MetadataDatabaseIndexOnDisk::HasDemotedDirtyTracker() const {
  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  itr->Seek(kDemotedDirtyIDKeyPrefix);
  if (!itr->Valid())
    return false;
  const std::string& key(itr->key().ToString());
  return StartsWithASCII(key, kDemotedDirtyIDKeyPrefix, true);
}

void MetadataDatabaseIndexOnDisk::PromoteDemotedDirtyTrackers(
    leveldb::WriteBatch* batch) {
  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(kDirtyIDKeyPrefix); itr->Valid(); itr->Next()) {
    std::string id_str;
    if (!RemovePrefix(itr->key().ToString(), kDirtyIDKeyPrefix, &id_str))
      break;

    int64 tracker_id;
    if (!base::StringToInt64(id_str, &tracker_id))
      continue;

    batch->Delete(itr->key());
    batch->Put(GenerateDemotedDirtyIDKey(tracker_id), "");
  }
}

size_t MetadataDatabaseIndexOnDisk::CountDirtyTracker() const {
  size_t num_dirty_trackers = 0;

  // TODO(peria): Store the number of dirty trackers, and do not iterate
  // everytime.
  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(kDirtyIDKeyPrefix); itr->Valid(); itr->Next()) {
    const std::string& key(itr->key().ToString());
    if (!StartsWithASCII(key, kDirtyIDKeyPrefix, true))
      break;
    ++num_dirty_trackers;
  }

  for (itr->Seek(kDemotedDirtyIDKeyPrefix); itr->Valid(); itr->Next()) {
    const std::string& key(itr->key().ToString());
    if (!StartsWithASCII(key, kDemotedDirtyIDKeyPrefix, true))
      break;
    ++num_dirty_trackers;
  }

  return num_dirty_trackers;
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
  std::vector<std::string> result;
  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  for (itr->Seek(kAppRootIDByAppIDKeyPrefix); itr->Valid(); itr->Next()) {
    std::string id;
    if (!RemovePrefix(itr->key().ToString(), kAppRootIDByAppIDKeyPrefix, &id))
      break;
    result.push_back(id);
  }
  return result;
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

void MetadataDatabaseIndexOnDisk::AddToAppIDIndex(
    const FileTracker& tracker, leveldb::WriteBatch* batch) {
  if (!IsAppRoot(tracker)) {
    DVLOG(3) << "  Tracker for " << tracker.file_id() << " is not an App root.";
    return;
  }

  DVLOG(1) << "  Add to app_root_by_app_id: " << tracker.app_id();

  const std::string& db_key(GenerateAppRootIDByAppIDKey(tracker.app_id()));
  DCHECK(tracker.active());
  DCHECK(!DBHasKey(db_key));
  batch->Put(db_key, base::Int64ToString(tracker.tracker_id()));
}

void MetadataDatabaseIndexOnDisk::UpdateInAppIDIndex(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker,
    leveldb::WriteBatch* batch) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());

  if (IsAppRoot(old_tracker) && !IsAppRoot(new_tracker)) {
    DCHECK(old_tracker.active());
    DCHECK(!new_tracker.active());
    const std::string& key(GenerateAppRootIDByAppIDKey(old_tracker.app_id()));
    DCHECK(DBHasKey(key));

    DVLOG(1) << "  Remove from app_root_by_app_id: " << old_tracker.app_id();
    batch->Delete(key);
  } else if (!IsAppRoot(old_tracker) && IsAppRoot(new_tracker)) {
    DCHECK(!old_tracker.active());
    DCHECK(new_tracker.active());
    const std::string& key(GenerateAppRootIDByAppIDKey(new_tracker.app_id()));
    DCHECK(!DBHasKey(key));

    DVLOG(1) << "  Add to app_root_by_app_id: " << new_tracker.app_id();
    batch->Put(key, base::Int64ToString(new_tracker.tracker_id()));
  }
}

void MetadataDatabaseIndexOnDisk::RemoveFromAppIDIndex(
    const FileTracker& tracker, leveldb::WriteBatch* batch) {
  if (!IsAppRoot(tracker)) {
    DVLOG(3) << "  Tracker for " << tracker.file_id() << " is not an App root.";
    return;
  }

  DCHECK(tracker.active());
  const std::string& key(GenerateAppRootIDByAppIDKey(tracker.app_id()));
  DCHECK(DBHasKey(key));

  DVLOG(1) << "  Remove from app_root_by_app_id: " << tracker.app_id();
  batch->Delete(key);
}

void MetadataDatabaseIndexOnDisk::AddToDirtyTrackerIndexes(
    const FileTracker& new_tracker,
    leveldb::WriteBatch* batch) {
  const std::string& dirty_key(GenerateDirtyIDKey(new_tracker.tracker_id()));
  DCHECK(!DBHasKey(dirty_key));
  DCHECK(!DBHasKey(GenerateDemotedDirtyIDKey(new_tracker.tracker_id())));

  if (new_tracker.dirty()) {
    DVLOG(1) << "  Add to dirty tracker IDs: " << new_tracker.tracker_id();
    batch->Put(dirty_key, "");
  }
}

void MetadataDatabaseIndexOnDisk::UpdateInDirtyTrackerIndexes(
    const FileTracker& old_tracker,
    const FileTracker& new_tracker,
    leveldb::WriteBatch* batch) {
  DCHECK_EQ(old_tracker.tracker_id(), new_tracker.tracker_id());

  int64 tracker_id = new_tracker.tracker_id();
  const std::string& dirty_key(GenerateDirtyIDKey(tracker_id));
  const std::string& demoted_key(GenerateDemotedDirtyIDKey(tracker_id));
  if (old_tracker.dirty() && !new_tracker.dirty()) {
    DCHECK(DBHasKey(dirty_key) || DBHasKey(demoted_key));

    DVLOG(1) << "  Remove from dirty trackers IDs: " << tracker_id;

    batch->Delete(dirty_key);
    batch->Delete(demoted_key);
  } else if (!old_tracker.dirty() && new_tracker.dirty()) {
    DCHECK(!DBHasKey(dirty_key));
    DCHECK(!DBHasKey(demoted_key));

    DVLOG(1) << "  Add to dirty tracker IDs: " << tracker_id;

    batch->Put(dirty_key, "");
  }
}

void MetadataDatabaseIndexOnDisk::RemoveFromDirtyTrackerIndexes(
    const FileTracker& tracker,
    leveldb::WriteBatch* batch) {
  if (tracker.dirty()) {
    int64 tracker_id = tracker.tracker_id();
    const std::string& dirty_key(GenerateDirtyIDKey(tracker_id));
    const std::string& demoted_key(GenerateDemotedDirtyIDKey(tracker_id));
    DCHECK(DBHasKey(dirty_key) || DBHasKey(demoted_key));

    DVLOG(1) << "  Remove from dirty tracker IDs: " << tracker_id;
    batch->Delete(dirty_key);
    batch->Delete(demoted_key);
  }
}

bool MetadataDatabaseIndexOnDisk::DBHasKey(const std::string& key) {
  scoped_ptr<leveldb::Iterator> itr(db_->NewIterator(leveldb::ReadOptions()));
  itr->Seek(key);
  return itr->Valid() && (itr->key() == key);
}

}  // namespace drive_backend
}  // namespace sync_file_system
