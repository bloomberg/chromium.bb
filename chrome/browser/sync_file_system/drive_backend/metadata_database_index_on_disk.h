// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_ON_DISK_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_ON_DISK_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index_interface.h"
#include "chrome/browser/sync_file_system/drive_backend/tracker_id_set.h"

namespace leveldb {
class DB;
}

namespace sync_file_system {
namespace drive_backend {

class FileMetadata;
class FileTracker;
struct DatabaseContents;
// TODO(peria): Migrate implementation of ParentIDAndTitle structure from
//     metadata_database_index.{cc,h} to here, on removing the files.
struct ParentIDAndTitle;

// Maintains indexes of MetadataDatabase on disk.
class MetadataDatabaseIndexOnDisk : public MetadataDatabaseIndexInterface {
 public:
  explicit MetadataDatabaseIndexOnDisk(leveldb::DB* db);
  virtual ~MetadataDatabaseIndexOnDisk();

  // MetadataDatabaseIndexInterface overrides.
  virtual bool GetFileMetadata(
      const std::string& file_id, FileMetadata* metadata) const OVERRIDE;
  virtual bool GetFileTracker(
      int64 tracker_id, FileTracker* tracker) const OVERRIDE;
  virtual void StoreFileMetadata(
      scoped_ptr<FileMetadata> metadata, leveldb::WriteBatch* batch) OVERRIDE;
  virtual void StoreFileTracker(
      scoped_ptr<FileTracker> tracker, leveldb::WriteBatch* batch) OVERRIDE;
  virtual void RemoveFileMetadata(
      const std::string& file_id, leveldb::WriteBatch* batch) OVERRIDE;
  virtual void RemoveFileTracker(
      int64 tracker_id, leveldb::WriteBatch* batch) OVERRIDE;
  virtual TrackerIDSet GetFileTrackerIDsByFileID(
      const std::string& file_id) const OVERRIDE;
  virtual int64 GetAppRootTracker(const std::string& app_id) const OVERRIDE;
  virtual TrackerIDSet GetFileTrackerIDsByParentAndTitle(
      int64 parent_tracker_id,
      const std::string& title) const OVERRIDE;
  virtual std::vector<int64> GetFileTrackerIDsByParent(
      int64 parent_tracker_id) const OVERRIDE;
  virtual std::string PickMultiTrackerFileID() const OVERRIDE;
  virtual ParentIDAndTitle PickMultiBackingFilePath() const OVERRIDE;
  virtual int64 PickDirtyTracker() const OVERRIDE;
  virtual void DemoteDirtyTracker(
      int64 tracker_id, leveldb::WriteBatch* batch) OVERRIDE;
  virtual bool HasDemotedDirtyTracker() const OVERRIDE;
  virtual void PromoteDemotedDirtyTrackers(leveldb::WriteBatch* batch) OVERRIDE;
  virtual size_t CountDirtyTracker() const OVERRIDE;
  virtual size_t CountFileMetadata() const OVERRIDE;
  virtual size_t CountFileTracker() const OVERRIDE;
  virtual std::vector<std::string> GetRegisteredAppIDs() const OVERRIDE;
  virtual std::vector<int64> GetAllTrackerIDs() const OVERRIDE;
  virtual std::vector<std::string> GetAllMetadataIDs() const OVERRIDE;

 private:
  // Maintain indexes from AppIDs to tracker IDs.
  void AddToAppIDIndex(const FileTracker& new_tracker,
                       leveldb::WriteBatch* batch);
  void UpdateInAppIDIndex(const FileTracker& old_tracker,
                          const FileTracker& new_tracker,
                          leveldb::WriteBatch* batch);
  void RemoveFromAppIDIndex(const FileTracker& tracker,
                            leveldb::WriteBatch* batch);

  // Maintain dirty tracker IDs.
  void AddToDirtyTrackerIndexes(const FileTracker& new_tracker,
                                leveldb::WriteBatch* batch);
  void UpdateInDirtyTrackerIndexes(const FileTracker& old_tracker,
                                   const FileTracker& new_tracker,
                                   leveldb::WriteBatch* batch);
  void RemoveFromDirtyTrackerIndexes(const FileTracker& tracker,
                                     leveldb::WriteBatch* batch);

  // Checks if |db_| has an entry whose key is |key|.
  bool DBHasKey(const std::string& key);

  leveldb::DB* db_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(MetadataDatabaseIndexOnDisk);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_ON_DISK_H_
