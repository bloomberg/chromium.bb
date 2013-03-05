// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_METADATA_STORE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_METADATA_STORE_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/sync_callbacks.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

namespace base {
class SequencedTaskRunner;
}

class GURL;

namespace sync_file_system {

class DriveMetadata;
class DriveMetadataDB;
struct DriveMetadataDBContents;

// This class holds a snapshot of the server side metadata.
class DriveMetadataStore
    : public base::NonThreadSafe,
      public base::SupportsWeakPtr<DriveMetadataStore> {
 public:
  typedef std::map<GURL, std::string> ResourceIDMap;
  typedef std::map<base::FilePath, DriveMetadata> PathToMetadata;
  typedef std::map<GURL, PathToMetadata> MetadataMap;
  typedef std::vector<std::pair<fileapi::FileSystemURL, std::string> >
      URLAndResourceIdList;
  typedef base::Callback<void(SyncStatusCode status, bool created)>
      InitializationCallback;

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

  // Returns true if |origin| is a batch sync origin, i.e. the origin's entire
  // file list hasn't been fully fetched and processed yet.
  bool IsBatchSyncOrigin(const GURL& origin) const;

  // Returns true if |origin| is an incremental sync origin, i.e. the origin's
  // entire file list has been cached and is ready to apply changes
  // incrementally.
  bool IsIncrementalSyncOrigin(const GURL& origin) const;

  // Marks |origin| as a batch sync origin and associates it with the directory
  // identified by |resource_id|.
  // |origin| must not be a batch sync origin nor an incremental sync origin.
  void AddBatchSyncOrigin(const GURL& origin, const std::string& resource_id);

  // Marks |origin| as an incremental sync origin.
  // |origin| must be a batch sync origin.
  void MoveBatchSyncOriginToIncremental(const GURL& origin);

  void RemoveOrigin(const GURL& origin,
                    const SyncStatusCallback& callback);

  // Sets the directory identified by |resource_id| as the sync data directory.
  // All data for the Sync FileSystem should be store into the directory.
  // It is invalid to overwrite the directory.
  void SetSyncRootDirectory(const std::string& resource_id);

  // Returns a set of URLs for files in conflict.
  SyncStatusCode GetConflictURLs(
      fileapi::FileSystemURLSet* urls) const;

  // Returns a set of URLs and Resource IDs for files to be fetched.
  SyncStatusCode GetToBeFetchedFiles(URLAndResourceIdList* list) const;

  // Returns resource id for |origin|. |origin| must be a batch sync origin or
  // an incremental sync origin.
  std::string GetResourceIdForOrigin(const GURL& origin) const;

  const std::string& sync_root_directory() const {
    DCHECK(CalledOnValidThread());
    return sync_root_directory_resource_id_;
  }

  const ResourceIDMap& batch_sync_origins() const {
    DCHECK(CalledOnValidThread());
    return batch_sync_origins_;
  }

  const ResourceIDMap& incremental_sync_origins() const {
    DCHECK(CalledOnValidThread());
    return incremental_sync_origins_;
  }

  // Returns all origins that are tracked. i.e. Union of batch_sync_origins_ and
  // incremental_sync_origins_.
  void GetAllOrigins(std::vector<GURL>* origins);

 private:
  friend class DriveMetadataStoreTest;

  void UpdateDBStatus(SyncStatusCode status);
  void UpdateDBStatusAndInvokeCallback(const SyncStatusCallback& callback,
                                       SyncStatusCode status);
  void DidInitialize(const InitializationCallback& callback,
                     DriveMetadataDBContents* contents,
                     SyncStatusCode error);
  void DidRemoveOrigin(const SyncStatusCallback& callback,
                       SyncStatusCode status);

  // These are only for testing.
  void RestoreSyncRootDirectory(const SyncStatusCallback& callback);
  void DidRestoreSyncRootDirectory(const SyncStatusCallback& callback,
                                   std::string* sync_root_directory_resource_id,
                                   SyncStatusCode status);
  void RestoreSyncOrigins(const SyncStatusCallback& callback);
  void DidRestoreSyncOrigins(const SyncStatusCallback& callback,
                             ResourceIDMap* batch_sync_origins,
                             ResourceIDMap* incremental_sync_origins,
                             SyncStatusCode status);

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  scoped_ptr<DriveMetadataDB> db_;
  SyncStatusCode db_status_;

  int64 largest_changestamp_;
  MetadataMap metadata_map_;

  std::string sync_root_directory_resource_id_;
  ResourceIDMap batch_sync_origins_;
  ResourceIDMap incremental_sync_origins_;

  DISALLOW_COPY_AND_ASSIGN(DriveMetadataStore);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_METADATA_STORE_H_
