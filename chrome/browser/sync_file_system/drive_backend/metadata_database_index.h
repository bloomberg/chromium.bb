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
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"

namespace sync_file_system {
namespace drive_backend {

struct ParentIDAndTitle {
  int64 parent_id;
  std::string title;

  ParentIDAndTitle();
  ParentIDAndTitle(int64 parent_id, const std::string& title);
};

bool operator==(const ParentIDAndTitle& left, const ParentIDAndTitle& right);
bool operator<(const ParentIDAndTitle& left, const ParentIDAndTitle& right);

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

class FileMetadata;
class FileTracker;
struct DatabaseContents;

// Maintains indexes of MetadataDatabase.  This class doesn't modify database
// entries on memory nor on disk.
class MetadataDatabaseIndex {
 public:
  explicit MetadataDatabaseIndex(DatabaseContents* content);
  ~MetadataDatabaseIndex();

  // Returns FileMetadata identified by |file_id| if exists, otherwise returns
  // NULL.
  const FileMetadata* GetFileMetadata(const std::string& file_id) const;

  // Returns FileTracker identified by |tracker_id| if exists, otherwise returns
  // NULL.
  const FileTracker* GetFileTracker(int64 tracker_id) const;

  // Stores |metadata| and updates indexes.
  // This overwrites existing FileMetadata for the same |file_id|.
  void StoreFileMetadata(scoped_ptr<FileMetadata> metadata);

  // Stores |tracker| and updates indexes.
  // This overwrites existing FileTracker for the same |tracker_id|.
  void StoreFileTracker(scoped_ptr<FileTracker> tracker);

  // Removes FileMetadata identified by |file_id| from indexes.
  void RemoveFileMetadata(const std::string& file_id);

  // Removes FileTracker identified by |tracker_id| from indexes.
  void RemoveFileTracker(int64 tracker_id);

  // Returns a set of FileTracker that have |file_id| as its own.
  TrackerIDSet GetFileTrackerIDsByFileID(const std::string& file_id) const;

  // Returns an app-root tracker identified by |app_id|.  Returns 0 if not
  // found.
  int64 GetAppRootTracker(const std::string& app_id) const;

  // Returns a set of FileTracker that have |parent_tracker_id| and |title|.
  TrackerIDSet GetFileTrackerIDsByParentAndTitle(
      int64 parent_tracker_id,
      const std::string& title) const;

  std::vector<int64> GetFileTrackerIDsByParent(int64 parent_tracker_id) const;

  // Returns the |file_id| of a file that has multiple trackers.
  std::string PickMultiTrackerFileID() const;

  // Returns a pair of |parent_tracker_id| and |title| that has multiple file
  // at the path.
  ParentIDAndTitle PickMultiBackingFilePath() const;

  // Returns a FileTracker whose |dirty| is set and which isn't demoted.
  // Returns 0 if not found.
  int64 PickDirtyTracker() const;

  // Demotes a dirty tracker.
  void DemoteDirtyTracker(int64 tracker_id);

  bool HasDemotedDirtyTracker() const;

  // Promotes all demoted dirty trackers to normal dirty trackers.
  void PromoteDemotedDirtyTrackers();

  std::vector<std::string> GetRegisteredAppIDs() const;

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

  friend class MetadataDatabase;
  friend class MetadataDatabaseTest;

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
