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
class ServiceMetadata;
struct DatabaseContents;
// TODO(peria): Migrate implementation of ParentIDAndTitle structure from
//     metadata_database_index.{cc,h} to here, on removing the files.
struct ParentIDAndTitle;

// Maintains indexes of MetadataDatabase on disk.
class MetadataDatabaseIndexOnDisk : public MetadataDatabaseIndexInterface {
 public:
  static scoped_ptr<MetadataDatabaseIndexOnDisk>
      Create(leveldb::DB* db, leveldb::WriteBatch* batch);

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
      int64 parent_tracker_id, const std::string& title) const OVERRIDE;
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
  virtual void SetSyncRootTrackerID(int64 sync_root_id,
                                    leveldb::WriteBatch* batch) const OVERRIDE;
  virtual void SetLargestChangeID(int64 largest_change_id,
                                  leveldb::WriteBatch* batch) const OVERRIDE;
  virtual void SetNextTrackerID(int64 next_tracker_id,
                                leveldb::WriteBatch* batch) const OVERRIDE;
  virtual int64 GetSyncRootTrackerID() const OVERRIDE;
  virtual int64 GetLargestChangeID() const OVERRIDE;
  virtual int64 GetNextTrackerID() const OVERRIDE;
  virtual std::vector<std::string> GetRegisteredAppIDs() const OVERRIDE;
  virtual std::vector<int64> GetAllTrackerIDs() const OVERRIDE;
  virtual std::vector<std::string> GetAllMetadataIDs() const OVERRIDE;

  // Builds on-disk indexes from FileTracker entries on disk.
  void BuildTrackerIndexes(leveldb::WriteBatch* batch);

 private:
  enum NumEntries {
    NONE,      // No entries are found.
    SINGLE,    // One entry is found.
    MULTIPLE,  // Two or more entires are found.
  };

  explicit MetadataDatabaseIndexOnDisk(leveldb::DB* db);

  // Maintain indexes from AppIDs to tracker IDs.
  void AddToAppIDIndex(const FileTracker& new_tracker,
                       leveldb::WriteBatch* batch);
  void UpdateInAppIDIndex(const FileTracker& old_tracker,
                          const FileTracker& new_tracker,
                          leveldb::WriteBatch* batch);
  void RemoveFromAppIDIndex(const FileTracker& tracker,
                            leveldb::WriteBatch* batch);

  // Maintain indexes from remote file IDs to tracker ID sets.
  void AddToFileIDIndexes(const FileTracker& new_tracker,
                          leveldb::WriteBatch* batch);
  void UpdateInFileIDIndexes(const FileTracker& old_tracker,
                             const FileTracker& new_tracker,
                             leveldb::WriteBatch* batch);
  void RemoveFromFileIDIndexes(const FileTracker& tracker,
                               leveldb::WriteBatch* batch);

  // Maintain indexes from path indexes to tracker ID sets
  void AddToPathIndexes(const FileTracker& new_tracker,
                        leveldb::WriteBatch* batch);
  void UpdateInPathIndexes(const FileTracker& old_tracker,
                           const FileTracker& new_tracker,
                           leveldb::WriteBatch* batch);
  void RemoveFromPathIndexes(const FileTracker& tracker,
                             leveldb::WriteBatch* batch);

  // Maintain dirty tracker IDs.
  void AddToDirtyTrackerIndexes(const FileTracker& new_tracker,
                                leveldb::WriteBatch* batch);
  void UpdateInDirtyTrackerIndexes(const FileTracker& old_tracker,
                                   const FileTracker& new_tracker,
                                   leveldb::WriteBatch* batch);
  void RemoveFromDirtyTrackerIndexes(const FileTracker& tracker,
                                     leveldb::WriteBatch* batch);

  // Returns a TrackerIDSet built from IDs which are found with given key
  // and key prefix.
  TrackerIDSet GetTrackerIDSetByPrefix(
      const std::string& active_tracker_key,
      const std::string& key_prefix) const;


  // Simulate behavior of TrackerIDSet class.

  // Adds an entry with |key_prefix| and tracker ID of |tracker|.  If |tracker|
  // is active, an entry for |active_key| is added.
  void AddToTrackerIDSetWithPrefix(
      const std::string& active_tracker_key,
      const std::string& key_prefix,
      const FileTracker& tracker,
      leveldb::WriteBatch* batch);

  // Returns true if |tracker_id| is removed successfully.  If no other entries
  // are stored with |key_prefix|, the entry for |active_key| is also removed.
  bool EraseInTrackerIDSetWithPrefix(
      const std::string& active_tracker_key,
      const std::string& key_prefix,
      int64 tracker_id,
      leveldb::WriteBatch* batch);

  // Adds an entry for |active_key| on DB, if |tracker_id| has an entry with
  // |key_prefix|.
  void ActivateInTrackerIDSetWithPrefix(
      const std::string& active_tracker_key,
      const std::string& key_prefix,
      int64 tracker_id,
      leveldb::WriteBatch* batch);

  // Removes an entry for |active_key| on DB, if the value of |active_key| key
  // is |tracker_id|.
  void DeactivateInTrackerIDSetWithPrefix(
      const std::string& active_tracker_key,
      const std::string& key_prefix,
      int64 tracker_id,
      leveldb::WriteBatch* batch);

  // Checks if |db_| has an entry whose key is |key|.
  bool DBHasKey(const std::string& key);

  // Returns the number of entries starting with |prefix| in NumEntries format.
  // Entries for |ignored_id| are not counted in.
  NumEntries CountWithPrefix(const std::string& prefix, int64 ignored_id);

  leveldb::DB* db_;  // Not owned.
  scoped_ptr<ServiceMetadata> service_metadata_;

  DISALLOW_COPY_AND_ASSIGN(MetadataDatabaseIndexOnDisk);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_ON_DISK_H_
