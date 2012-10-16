// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_METADATA_STORE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_METADATA_STORE_H_

#include <map>

#include "base/callback_forward.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

namespace base {
class SequencedTaskRunner;
}

class GURL;

namespace sync_file_system {

class DriveMetadata;
class DriveMetadataDB;

// This class holds a snapshot of the server side metadata.
class DriveMetadataStore
    : public base::NonThreadSafe,
      public base::SupportsWeakPtr<DriveMetadataStore> {
 public:
  typedef std::map<fileapi::FileSystemURL,
                   DriveMetadata,
                   fileapi::FileSystemURL::Comparator> MetadataMap;
  typedef base::Callback<void(fileapi::SyncStatusCode status, bool created)>
      InitializationCallback;

  DriveMetadataStore(const FilePath& base_dir,
                     base::SequencedTaskRunner* file_task_runner);
  ~DriveMetadataStore();

  // Initializes the internal database and loads its content to memory.
  // This function works asynchronously.
  void Initialize(const InitializationCallback& callback);

  void SetLargestChangeStamp(int64 largest_changestamp);
  int64 GetLargestChangeStamp() const;

  // Updates database entry.
  fileapi::SyncStatusCode UpdateEntry(const fileapi::FileSystemURL& url,
                                      const DriveMetadata& metadata);

  // Deletes database entry for |url|.
  fileapi::SyncStatusCode DeleteEntry(const fileapi::FileSystemURL& url);

  // Lookups and reads the database entry for |url|.
  fileapi::SyncStatusCode ReadEntry(const fileapi::FileSystemURL& url,
                                    DriveMetadata* metadata) const;

 private:
  void UpdateDBStatus(fileapi::SyncStatusCode status);
  void DidInitialize(const InitializationCallback& callback,
                     const int64* largest_changestamp,
                     MetadataMap* metadata_map,
                     fileapi::SyncStatusCode error);

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  scoped_ptr<DriveMetadataDB> db_;
  fileapi::SyncStatusCode db_status_;

  int64 largest_changestamp_;
  MetadataMap metadata_map_;

  DISALLOW_COPY_AND_ASSIGN(DriveMetadataStore);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_METADATA_STORE_H_
