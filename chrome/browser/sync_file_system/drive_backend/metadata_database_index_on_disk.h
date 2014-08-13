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

namespace sync_file_system {
namespace drive_backend {

class FileMetadata;
class FileTracker;
class LevelDBWrapper;
class ServiceMetadata;
struct DatabaseContents;
// TODO(peria): Migrate implementation of ParentIDAndTitle structure from
//     metadata_database_index.{cc,h} to here, on removing the files.
struct ParentIDAndTitle;

// Maintains indexes of MetadataDatabase on disk.
class MetadataDatabaseIndexOnDisk : public MetadataDatabaseIndexInterface {
 public:
  static scoped_ptr<MetadataDatabaseIndexOnDisk>  Create(LevelDBWrapper* db);

  virtual ~MetadataDatabaseIndexOnDisk();

  // MetadataDatabaseIndexInterface overrides.
  virtual bool GetFileMetadata(
      const std::string& file_id, FileMetadata* metadata) const OVERRIDE;
  virtual bool GetFileTracker(
      int64 tracker_id, FileTracker* tracker) const OVERRIDE;
  virtual void StoreFileMetadata(scoped_ptr<FileMetadata> metadata) OVERRIDE;
  virtual void StoreFileTracker(scoped_ptr<FileTracker> tracker) OVERRIDE;
  virtual void RemoveFileMetadata(const std::string& file_id) OVERRIDE;
  virtual void RemoveFileTracker(int64 tracker_id) OVERRIDE;
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
  virtual void DemoteDirtyTracker(int64 tracker_id) OVERRIDE;
  virtual bool HasDemotedDirtyTracker() const OVERRIDE;
  virtual void PromoteDemotedDirtyTracker(int64 tracker_id) OVERRIDE;
  virtual bool PromoteDemotedDirtyTrackers() OVERRIDE;
  virtual size_t CountDirtyTracker() const OVERRIDE;
  virtual size_t CountFileMetadata() const OVERRIDE;
  virtual size_t CountFileTracker() const OVERRIDE;
  virtual void SetSyncRootTrackerID(int64 sync_root_id) const OVERRIDE;
  virtual void SetLargestChangeID(int64 largest_change_id) const OVERRIDE;
  virtual void SetNextTrackerID(int64 next_tracker_id) const OVERRIDE;
  virtual int64 GetSyncRootTrackerID() const OVERRIDE;
  virtual int64 GetLargestChangeID() const OVERRIDE;
  virtual int64 GetNextTrackerID() const OVERRIDE;
  virtual std::vector<std::string> GetRegisteredAppIDs() const OVERRIDE;
  virtual std::vector<int64> GetAllTrackerIDs() const OVERRIDE;
  virtual std::vector<std::string> GetAllMetadataIDs() const OVERRIDE;

  // Builds on-disk indexes from FileTracker entries on disk.
  void BuildTrackerIndexes();

  LevelDBWrapper* GetDBForTesting();

 private:
  enum NumEntries {
    NONE,      // No entries are found.
    SINGLE,    // One entry is found.
    MULTIPLE,  // Two or more entires are found.
  };

  explicit MetadataDatabaseIndexOnDisk(LevelDBWrapper* db);

  // Maintain indexes from AppIDs to tracker IDs.
  void AddToAppIDIndex(const FileTracker& new_tracker);
  void UpdateInAppIDIndex(const FileTracker& old_tracker,
                          const FileTracker& new_tracker);
  void RemoveFromAppIDIndex(const FileTracker& tracker);

  // Maintain indexes from remote file IDs to tracker ID sets.
  void AddToFileIDIndexes(const FileTracker& new_tracker);
  void UpdateInFileIDIndexes(const FileTracker& old_tracker,
                             const FileTracker& new_tracker);
  void RemoveFromFileIDIndexes(const FileTracker& tracker);

  // Maintain indexes from path indexes to tracker ID sets
  void AddToPathIndexes(const FileTracker& new_tracker);
  void UpdateInPathIndexes(const FileTracker& old_tracker,
                           const FileTracker& new_tracker);
  void RemoveFromPathIndexes(const FileTracker& tracker);

  // Maintain dirty tracker IDs.
  void AddToDirtyTrackerIndexes(const FileTracker& new_tracker);
  void UpdateInDirtyTrackerIndexes(const FileTracker& old_tracker,
                                   const FileTracker& new_tracker);
  void RemoveFromDirtyTrackerIndexes(const FileTracker& tracker);

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
      const FileTracker& tracker);

  // Returns true if |tracker_id| is removed successfully.  If no other entries
  // are stored with |key_prefix|, the entry for |active_key| is also removed.
  bool EraseInTrackerIDSetWithPrefix(
      const std::string& active_tracker_key,
      const std::string& key_prefix,
      int64 tracker_id);

  // Adds an entry for |active_key| on DB, if |tracker_id| has an entry with
  // |key_prefix|.
  void ActivateInTrackerIDSetWithPrefix(
      const std::string& active_tracker_key,
      const std::string& key_prefix,
      int64 tracker_id);

  // Removes an entry for |active_key| on DB, if the value of |active_key| key
  // is |tracker_id|.
  void DeactivateInTrackerIDSetWithPrefix(
      const std::string& active_tracker_key,
      const std::string& key_prefix,
      int64 tracker_id);

  // Checks if |db_| has an entry whose key is |key|.
  bool DBHasKey(const std::string& key);

  // Returns the number of entries starting with |prefix| in NumEntries format.
  // Entries for |ignored_id| are not counted in.
  NumEntries CountWithPrefix(const std::string& prefix, int64 ignored_id);

  LevelDBWrapper* db_;  // Not owned.
  scoped_ptr<ServiceMetadata> service_metadata_;

  DISALLOW_COPY_AND_ASSIGN(MetadataDatabaseIndexOnDisk);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_ON_DISK_H_
