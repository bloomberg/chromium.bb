// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_DRIVE_METADATA_STORE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_DRIVE_METADATA_STORE_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace base {
class ListValue;
class SequencedTaskRunner;
}

namespace leveldb {
class DB;
class WriteBatch;
}

class GURL;

namespace sync_file_system {

class DriveMetadata;
struct DBContents;
struct FileMetadata;

// This class holds a snapshot of the server side metadata.
class DriveMetadataStore
    : public base::NonThreadSafe,
      public base::SupportsWeakPtr<DriveMetadataStore> {
 public:
  typedef std::map<GURL, std::string> ResourceIdByOrigin;
  typedef std::map<std::string, GURL> OriginByResourceId;
  typedef std::map<base::FilePath, DriveMetadata> PathToMetadata;
  typedef std::map<GURL, PathToMetadata> MetadataMap;
  typedef std::vector<std::pair<fileapi::FileSystemURL, DriveMetadata> >
      URLAndDriveMetadataList;
  typedef base::Callback<void(SyncStatusCode status, bool created)>
      InitializationCallback;

  static const base::FilePath::CharType kDatabaseName[];

  DriveMetadataStore(const base::FilePath& base_dir,
                     base::SequencedTaskRunner* file_task_runner);
  ~DriveMetadataStore();

  // Initializes the internal database and loads its content to memory.
  // This function works asynchronously.
  void Initialize(const InitializationCallback& callback);

  void SetLargestChangeStamp(int64 largest_changestamp,
                             const SyncStatusCallback& callback);
  int64 GetLargestChangeStamp() const;

  // Updates database entry. Invokes |callback|, upon completion.
  void UpdateEntry(const fileapi::FileSystemURL& url,
                   const DriveMetadata& metadata,
                   const SyncStatusCallback& callback);

  // Deletes database entry for |url|. Invokes |callback|, upon completion.
  void DeleteEntry(const fileapi::FileSystemURL& url,
                   const SyncStatusCallback& callback);

  // Lookups and reads the database entry for |url|.
  SyncStatusCode ReadEntry(const fileapi::FileSystemURL& url,
                           DriveMetadata* metadata) const;

  // Marks |origin| as incremental sync and associates it with the directory
  // identified by |resource_id|.
  // |origin| must not already be an incremental sync origin.
  void AddIncrementalSyncOrigin(const GURL& origin,
                                const std::string& resource_id);

  // Returns true if |origin| is an incremental sync or disabled origin.
  bool IsKnownOrigin(const GURL& origin) const;

  // Returns true if |origin| is an incremental sync origin, i.e. the origin's
  // entire file list has been cached and is ready to apply changes
  // incrementally.
  bool IsIncrementalSyncOrigin(const GURL& origin) const;

  // Returns true if |origin| is a disabled origin.
  bool IsOriginDisabled(const GURL& origin) const;

  void EnableOrigin(const GURL& origin,
                    const SyncStatusCallback& callback);

  void DisableOrigin(const GURL& origin,
                     const SyncStatusCallback& callback);

  void RemoveOrigin(const GURL& origin,
                    const SyncStatusCallback& callback);

  // Sets the directory identified by |resource_id| as the sync data directory.
  // All data for the Sync FileSystem should be store into the directory.
  void SetSyncRootDirectory(const std::string& resource_id);
  void SetOriginRootDirectory(const GURL& origin,
                              const std::string& resource_id);

  // Returns a set of URLs for files in conflict.
  SyncStatusCode GetConflictURLs(
      fileapi::FileSystemURLSet* urls) const;

  // Returns a set of URLs and Resource IDs for files to be fetched.
  SyncStatusCode GetToBeFetchedFiles(URLAndDriveMetadataList* list) const;

  // Returns resource id for |origin|.
  // This may return an empty string if |origin| is not a incremental or
  // disabled origin.
  std::string GetResourceIdForOrigin(const GURL& origin) const;

  const std::string& sync_root_directory() const {
    DCHECK(CalledOnValidThread());
    return sync_root_directory_resource_id_;
  }

  const ResourceIdByOrigin& incremental_sync_origins() const {
    DCHECK(CalledOnValidThread());
    return incremental_sync_origins_;
  }

  const ResourceIdByOrigin& disabled_origins() const {
    DCHECK(CalledOnValidThread());
    return disabled_origins_;
  }

  // Returns all tracked origins. i.e. incremental_sync_origins_ and
  // disabled_origins_.
  void GetAllOrigins(std::vector<GURL>* origins);

  // Maps |resource_id| to corresponding |origin|.
  // Returns true if the directory indicated by |resource_id| is not an origin
  // root directory.
  bool GetOriginByOriginRootDirectoryId(const std::string& resource_id,
                                        GURL* origin);

  // Returns all file metadata for the given origin.
  scoped_ptr<base::ListValue> DumpFiles(const GURL& origin);

 private:
  friend class DriveMetadataStoreTest;

  void WriteToDB(scoped_ptr<leveldb::WriteBatch> batch,
                 const SyncStatusCallback& callback);

  void UpdateDBStatus(SyncStatusCode status);
  void UpdateDBStatusAndInvokeCallback(const SyncStatusCallback& callback,
                                       const leveldb::Status& status);
  void DidInitialize(const InitializationCallback& callback,
                     scoped_ptr<DBContents> contents);

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  base::FilePath base_dir_;
  scoped_ptr<leveldb::DB> db_;
  SyncStatusCode db_status_;

  int64 largest_changestamp_;
  MetadataMap metadata_map_;

  std::string sync_root_directory_resource_id_;
  ResourceIdByOrigin incremental_sync_origins_;
  ResourceIdByOrigin disabled_origins_;

  OriginByResourceId origin_by_resource_id_;

  DISALLOW_COPY_AND_ASSIGN(DriveMetadataStore);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_DRIVE_METADATA_STORE_H_
