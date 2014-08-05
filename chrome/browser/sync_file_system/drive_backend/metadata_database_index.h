// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index_interface.h"
#include "chrome/browser/sync_file_system/drive_backend/tracker_id_set.h"

namespace sync_file_system {
namespace drive_backend {

class FileMetadata;
class FileTracker;
class LevelDBWrapper;
class ServiceMetadata;

}  // namespace drive_backend
}  // namespace sync_file_system

namespace BASE_HASH_NAMESPACE {

#if defined(COMPILER_GCC)
template<> struct hash<sync_file_system::drive_backend::ParentIDAndTitle> {
  std::size_t operator()(
      const sync_file_system::drive_backend::ParentIDAndTitle& v) const {
    return base::HashInts64(v.parent_id, hash<std::string>()(v.title));
  }
};
#elif defined(COMPILER_MSVC)
inline size_t hash_value(
    const sync_file_system::drive_backend::ParentIDAndTitle& v) {
  return base::HashInts64(v.parent_id, hash_value(v.title));
}
#endif  // COMPILER

}  // namespace BASE_HASH_NAMESPACE

namespace sync_file_system {
namespace drive_backend {

struct DatabaseContents {
  DatabaseContents();
  ~DatabaseContents();
  ScopedVector<FileMetadata> file_metadata;
  ScopedVector<FileTracker> file_trackers;
};

// Maintains indexes of MetadataDatabase on memory.
class MetadataDatabaseIndex : public MetadataDatabaseIndexInterface {
 public:
  virtual ~MetadataDatabaseIndex();

  static scoped_ptr<MetadataDatabaseIndex> Create(LevelDBWrapper* db);
  static scoped_ptr<MetadataDatabaseIndex> CreateForTesting(
      DatabaseContents* contents);

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
      int64 parent_tracker_id,
      const std::string& title) const OVERRIDE;
  virtual std::vector<int64> GetFileTrackerIDsByParent(
      int64 parent_tracker_id) const OVERRIDE;
  virtual std::string PickMultiTrackerFileID() const OVERRIDE;
  virtual ParentIDAndTitle PickMultiBackingFilePath() const OVERRIDE;
  virtual int64 PickDirtyTracker() const OVERRIDE;
  virtual void DemoteDirtyTracker(int64 tracker_id) OVERRIDE;
  virtual bool HasDemotedDirtyTracker() const OVERRIDE;
  virtual void PromoteDemotedDirtyTrackers() OVERRIDE;
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

 private:
  typedef base::ScopedPtrHashMap<std::string, FileMetadata> MetadataByID;
  typedef base::ScopedPtrHashMap<int64, FileTracker> TrackerByID;
  typedef base::hash_map<std::string, TrackerIDSet> TrackerIDsByFileID;
  typedef base::hash_map<std::string, TrackerIDSet> TrackerIDsByTitle;
  typedef std::map<int64, TrackerIDsByTitle> TrackerIDsByParentAndTitle;
  typedef base::hash_map<std::string, int64> TrackerIDByAppID;
  typedef base::hash_set<std::string> FileIDSet;
  typedef base::hash_set<ParentIDAndTitle> PathSet;
  typedef std::set<int64> DirtyTrackers;

  friend class MetadataDatabaseTest;

  explicit MetadataDatabaseIndex(LevelDBWrapper* db);
  void Initialize(scoped_ptr<ServiceMetadata> service_metadata,
                  DatabaseContents* contents);

  // Maintains |app_root_by_app_id_|.
  void AddToAppIDIndex(const FileTracker& new_tracker);
  void UpdateInAppIDIndex(const FileTracker& old_tracker,
                          const FileTracker& new_tracker);
  void RemoveFromAppIDIndex(const FileTracker& tracker);

  // Maintains |trackers_by_file_id_| and |multi_tracker_file_ids_|.
  void AddToFileIDIndexes(const FileTracker& new_tracker);
  void UpdateInFileIDIndexes(const FileTracker& old_tracker,
                             const FileTracker& new_tracker);
  void RemoveFromFileIDIndexes(const FileTracker& tracker);

  // Maintains |trackers_by_parent_and_title_| and |multi_backing_file_paths_|.
  void AddToPathIndexes(const FileTracker& new_tracker);
  void UpdateInPathIndexes(const FileTracker& old_tracker,
                           const FileTracker& new_tracker1);
  void RemoveFromPathIndexes(const FileTracker& tracker);

  // Maintains |dirty_trackers_| and |demoted_dirty_trackers_|.
  void AddToDirtyTrackerIndexes(const FileTracker& new_tracker);
  void UpdateInDirtyTrackerIndexes(const FileTracker& old_tracker,
                                   const FileTracker& new_tracker);
  void RemoveFromDirtyTrackerIndexes(const FileTracker& tracker);

  scoped_ptr<ServiceMetadata> service_metadata_;
  LevelDBWrapper* db_;  // Not owned

  MetadataByID metadata_by_id_;
  TrackerByID tracker_by_id_;

  TrackerIDByAppID app_root_by_app_id_;

  TrackerIDsByFileID trackers_by_file_id_;
  FileIDSet multi_tracker_file_ids_;

  TrackerIDsByParentAndTitle trackers_by_parent_and_title_;
  PathSet multi_backing_file_paths_;

  DirtyTrackers dirty_trackers_;
  DirtyTrackers demoted_dirty_trackers_;

  DISALLOW_COPY_AND_ASSIGN(MetadataDatabaseIndex);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_INDEX_H_
