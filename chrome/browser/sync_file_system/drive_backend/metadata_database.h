// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
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

class ServiceMetadata;
class DriveFileMetadata;
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
 private:
  struct FileIDComparator {
    bool operator()(DriveFileMetadata* left, DriveFileMetadata* right) const;
  };

 public:
  typedef std::set<DriveFileMetadata*, FileIDComparator> FileSet;
  typedef std::map<std::string, DriveFileMetadata*> FileByFileID;
  typedef std::map<std::string, FileSet> FilesByParent;
  typedef std::map<std::pair<std::string, std::string>, DriveFileMetadata*>
      FileByParentAndTitle;
  typedef std::map<std::string, DriveFileMetadata*> FileByAppID;

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

  // Finds the app-root folder for |app_id|.  Returns true if exists.
  // Copies the result to |folder| if it is non-NULL.
  bool FindAppRootFolder(const std::string& app_id,
                         DriveFileMetadata* folder) const;

  // Finds file by |file_id|.  Returns true if the file was found.
  // Copies the DriveFileMetadata instance identified by |file_id| into
  // |file| if exists and |file| is non-NULL.
  bool FindFileByFileID(const std::string& file_id,
                        DriveFileMetadata* file) const;

  // Finds files by |title| under the folder identified by |folder_id|, and
  // returns the number of the files.
  // Copies the DriveFileMetadata instances to |files| if it is non-NULL.
  size_t FindFilesByParentAndTitle(
      const std::string& folder_id,
      const std::string& title,
      ScopedVector<DriveFileMetadata>* files) const;

  // Finds active file by |title| under the folder identified by |folder_id|.
  // Returns true if the file exists.
  bool FindActiveFileByParentAndTitle(
      const std::string& folder_id,
      const std::string& title,
      DriveFileMetadata* file) const;

  // Finds the active file identified by |app_id| and |path|, which must be
  // unique.  Returns true if the file was found.
  // Copies the DriveFileMetadata instance into |file| if the file is found and
  // |file| is non-NULL.
  // |path| must be an absolute path in |app_id|. (i.e. relative to the app-root
  // folder.)
  bool FindActiveFileByPath(const std::string& app_id,
                            const base::FilePath& path,
                            DriveFileMetadata* file) const;

  // Looks up FilePath from FileID.  Returns true on success.
  // |path| must be non-NULL.
  bool BuildPathForFile(const std::string& file_id,
                        base::FilePath* path) const;

  // Updates database by |changes|.
  // Marks dirty for each changed file if the file has the metadata in the
  // database.  Adds new metadata to track the file if the file doesn't have
  // the metadata and its parent folder has the active metadata.
  void UpdateByChangeList(ScopedVector<google_apis::ChangeResource> changes,
                          const SyncStatusCallback& callback);

  // Populates |folder| with |children|.  Each |children| initially has empty
  // |synced_details| and |remote_details|.
  void PopulateFolder(const std::string& folder_id,
                      ScopedVector<google_apis::ResourceEntry> children,
                      const SyncStatusCallback& callback);

 private:
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

  // Database manipulation methods.
  void RegisterFolderAsAppRoot(const std::string& app_id,
                               const std::string& folder,
                               leveldb::WriteBatch* batch);
  void MakeFileActive(const std::string& file_id,
                      leveldb::WriteBatch* batch);
  void MakeFileInactive(const std::string& file_id,
                        leveldb::WriteBatch* batch);
  void UnregisterFolderAsAppRoot(const std::string& app_id,
                                 leveldb::WriteBatch* batch);
  void RemoveFile(const std::string& file_id,
                  leveldb::WriteBatch* batch);
  void UpdateRemoteDetails(int64 change_id,
                           const std::string& file_id,
                           const google_apis::FileResource* file,
                           leveldb::WriteBatch* batch);
  void RegisterNewFile(int64 change_id,
                       const DriveFileMetadata& parent_folder,
                       const google_apis::FileResource& new_file,
                       leveldb::WriteBatch* batch);

  void WriteToDatabase(scoped_ptr<leveldb::WriteBatch> batch,
                       const SyncStatusCallback& callback);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_ptr<leveldb::DB> db_;

  scoped_ptr<ServiceMetadata> service_metadata_;
  FileByFileID file_by_file_id_;  // Owned.

  FilesByParent files_by_parent_;  // Not owned.
  FileByAppID app_root_by_app_id_;  // Not owned.
  FileByParentAndTitle active_file_by_parent_and_title_;  // Not owned.
  FileSet dirty_files_;  // Not owned.

  base::WeakPtrFactory<MetadataDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MetadataDatabase);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_H_
