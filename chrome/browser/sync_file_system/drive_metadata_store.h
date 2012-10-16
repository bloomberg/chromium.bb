// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_METADATA_STORE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_METADATA_STORE_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"

namespace base {
class SequencedTaskRunner;
}

namespace fileapi {
class FileSystemURL;
}

namespace gdata {
class DocumentFeed;
}

class FilePath;
class GURL;

namespace sync_file_system {

class DriveMetadataDB;

// This class holds snapshot of server side metadata..
class DriveMetadataStore
    : public base::NonThreadSafe,
      public base::SupportsWeakPtr<DriveMetadataStore> {
 public:
  typedef base::Callback<void(bool success, bool has_cache)>
      InitializationCallback;

  DriveMetadataStore(const FilePath& base_dir,
                      base::SequencedTaskRunner* file_task_runner);
  ~DriveMetadataStore();

  void InitializeFromDiskCache(const InitializationCallback& callback);
  void InitializeWithDocumentFeed(scoped_ptr<gdata::DocumentFeed> feed,
                                  const InitializationCallback& callback);

  void SetLargestChangeStamp(int64 largest_changestamp);
  int64 GetLargestChangeStamp() const;

  void ApplyChangeFeed(scoped_ptr<gdata::DocumentFeed> feed);

  std::string GetResourceId(const fileapi::FileSystemURL& url) const;
  std::string GetETag(const fileapi::FileSystemURL& url) const;
  GURL GetDownloadURL(const fileapi::FileSystemURL& url) const;
  GURL GetUploadURL(const fileapi::FileSystemURL& url) const;

 private:
  scoped_ptr<DriveMetadataDB> db_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  bool db_disabled_;
  int64 largest_changestamp_;

  DISALLOW_COPY_AND_ASSIGN(DriveMetadataStore);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_METADATA_STORE_H_
