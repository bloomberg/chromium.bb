// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_H_

#include <map>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend/tracker_set.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace leveldb {
class DB;
class WriteBatch;
}

namespace google_apis {
class ChangeResource;
class FileResource;
class ResourceEntry;
}

namespace sync_file_system {
namespace drive_backend {

class FileMetadata;
class FileTracker;
class ServiceMetadata;
struct DatabaseContents;

// MetadataDatabase instance holds and maintains database and its indexes.  The
// database holds metadata of the server-side files/folders as
// DriveFileMetadata instances.
// The term "file" includes files, folders and other resources on Drive.
//
// Files have following state:
//   - Unknown file
//     - Is initial state of the files, only file_id and parent_folder_id field
//       are known.
//     - Has empty synced_details, must be active and dirty.
//   - Folder
//     - Is either one of sync-root folder, app-root folder or a regular folder.
//     - Sync-root folder holds app-root folders as its direct children, and
//       holds entire SyncFileSystem files as its descentants.  Its file_id is
//       stored in ServiceMetadata.
//     - App-root folder holds all files for an application as its descendants.
//   - File
//   - Unsupported file
//     - Represents unsupported files such as hosted documents. Must be
//       inactive.
//
// Invariants:
//   - Any file in the database must either:
//     - be sync-root,
//     - have an app-root as its parent folder, or
//     - have an active folder as its parent.
//   That is, all files must be reachable from sync-root via app-root folders
//   and active folders.
//
//   - Any active folder must either:
//     - have needs_folder_listing flag and dirty flag, or
//     - have all children at the stored largest change id.
//
class MetadataDatabase {
 public:
  typedef std::map<std::string, FileMetadata*> FileByID;
  typedef std::map<int64, FileTracker*> TrackerByID;
  typedef std::map<std::string, TrackerSet> TrackersByFileID;
  typedef std::map<std::string, TrackerSet> TrackersByTitle;
  typedef std::map<int64, TrackersByTitle> TrackersByParentAndTitle;
  typedef std::map<std::string, FileTracker*> TrackerByAppID;

  typedef base::Callback<
      void(SyncStatusCode status, scoped_ptr<MetadataDatabase> instance)>
      CreateCallback;

  static void Create(base::SequencedTaskRunner* task_runner,
                     const base::FilePath& database_path,
                     const CreateCallback& callback);
  ~MetadataDatabase();

  int64 GetLargestChangeID() const;

  // Registers existing folder as the app-root for |app_id|.  The folder
  // must be an inactive folder that does not yet associated to any App.
  // This method associates the folder with |app_id| and activates it.
  void RegisterApp(const std::string& app_id,
                   const std::string& folder_id,
                   const SyncStatusCallback& callback);

  // Inactivates the folder associated to the app to disable |app_id|.
  // Does nothing if |app_id| is already disabled.
  void DisableApp(const std::string& app_id,
                  const SyncStatusCallback& callback);

  // Activates the folder associated to |app_id| to enable |app_id|.
  // Does nothing if |app_id| is already enabled.
  void EnableApp(const std::string& app_id,
                 const SyncStatusCallback& callback);

  // Unregisters the folder as the app-root for |app_id|.  If |app_id| does not
  // exist, does nothing.  The folder is left as an inactive normal folder.
  void UnregisterApp(const std::string& app_id,
                     const SyncStatusCallback& callback);

  // Updates database by |changes|.
  // Marks dirty for each changed file if the file has the metadata in the
  // database.  Adds new metadata to track the file if the file doesn't have
  // the metadata and its parent folder has the active metadata.
  void UpdateByChangeList(ScopedVector<google_apis::ChangeResource> changes,
                          const SyncStatusCallback& callback);

 private:
  struct DirtyTrackerComparator {
    bool operator()(const FileTracker* left,
                    const FileTracker* right) const;
  };

  typedef std::set<FileTracker*, DirtyTrackerComparator> DirtyTrackers;

  friend class MetadataDatabaseTest;

  explicit MetadataDatabase(base::SequencedTaskRunner* task_runner);
  static void CreateOnTaskRunner(base::SingleThreadTaskRunner* callback_runner,
                                 base::SequencedTaskRunner* task_runner,
                                 const base::FilePath& database_path,
                                 const CreateCallback& callback);
  static SyncStatusCode CreateForTesting(
      scoped_ptr<leveldb::DB> db,
      scoped_ptr<MetadataDatabase>* metadata_database_out);
  SyncStatusCode InitializeOnTaskRunner(const base::FilePath& database_path);
  void BuildIndexes(DatabaseContents* contents);

  void WriteToDatabase(scoped_ptr<leveldb::WriteBatch> batch,
                       const SyncStatusCallback& callback);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_ptr<leveldb::DB> db_;

  scoped_ptr<ServiceMetadata> service_metadata_;

  FileByID file_by_id_;  // Owned.
  TrackerByID tracker_by_id_;  // Owned.

  // Maps FileID to trackers.  The active tracker must be unique per FileID.
  // This must be updated when updating |active| field of a tracker.
  TrackersByFileID trackers_by_file_id_;  // Not owned.

  // Maps AppID to the app-root tracker.
  // This must be updated when a tracker is registered/unregistered as an
  // app-root.
  TrackerByAppID app_root_by_app_id_;  // Not owned.

  // Maps |tracker_id| to its children grouped by their |title|.
  // If the title is unknown for a tracker, treats its title as empty. Empty
  // titled file must not be active.
  // The active tracker must be unique per its parent_tracker and its title.
  // This must be updated when updating |title|, |active| or
  // |parent_tracker_id|.
  TrackersByParentAndTitle trackers_by_parent_and_title_;

  // Holds all trackers which marked as dirty.
  // This must be updated when updating |dirty| field of a tracker.
  DirtyTrackers dirty_trackers_;  // Not owned.

  base::WeakPtrFactory<MetadataDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MetadataDatabase);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_H_
