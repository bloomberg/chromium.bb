// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_CANNED_SYNCABLE_FILE_SYSTEM_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_CANNED_SYNCABLE_FILE_SYSTEM_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/observer_list_threadsafe.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_status.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "webkit/browser/blob/blob_data_handle.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/quota/quota_callbacks.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"
#include "webkit/common/quota/quota_types.h"

namespace base {
class SingleThreadTaskRunner;
class Thread;
}

namespace storage {
class FileSystemContext;
class FileSystemOperationRunner;
class FileSystemURL;
}

namespace leveldb {
class Env;
}

namespace net {
class URLRequestContext;
}

namespace storage {
class QuotaManager;
}

namespace sync_file_system {

class FileChangeList;
class LocalFileSyncContext;
class SyncFileSystemBackend;

// A canned syncable filesystem for testing.
// This internally creates its own QuotaManager and FileSystemContext
// (as we do so for each isolated application).
class CannedSyncableFileSystem
    : public LocalFileSyncStatus::Observer {
 public:
  typedef base::Callback<void(const GURL& root,
                              const std::string& name,
                              base::File::Error result)>
      OpenFileSystemCallback;
  typedef base::Callback<void(base::File::Error)> StatusCallback;
  typedef base::Callback<void(int64)> WriteCallback;
  typedef storage::FileSystemOperation::FileEntryList FileEntryList;

  enum QuotaMode {
    QUOTA_ENABLED,
    QUOTA_DISABLED,
  };

  CannedSyncableFileSystem(const GURL& origin,
                           leveldb::Env* env_override,
                           base::SingleThreadTaskRunner* io_task_runner,
                           base::SingleThreadTaskRunner* file_task_runner);
  virtual ~CannedSyncableFileSystem();

  // SetUp must be called before using this instance.
  void SetUp(QuotaMode quota_mode);

  // TearDown must be called before destructing this instance.
  void TearDown();

  // Creates a FileSystemURL for the given (utf8) path string.
  storage::FileSystemURL URL(const std::string& path) const;

  // Initialize this with given |sync_context| if it hasn't
  // been initialized.
  sync_file_system::SyncStatusCode MaybeInitializeFileSystemContext(
      LocalFileSyncContext* sync_context);

  // Opens a new syncable file system.
  base::File::Error OpenFileSystem();

  // Register sync status observers. Unlike original
  // LocalFileSyncStatus::Observer implementation the observer methods
  // are called on the same thread where AddSyncStatusObserver were called.
  void AddSyncStatusObserver(LocalFileSyncStatus::Observer* observer);
  void RemoveSyncStatusObserver(LocalFileSyncStatus::Observer* observer);

  // Accessors.
  storage::FileSystemContext* file_system_context() {
    return file_system_context_.get();
  }
  storage::QuotaManager* quota_manager() { return quota_manager_.get(); }
  GURL origin() const { return origin_; }
  storage::FileSystemType type() const { return type_; }
  storage::StorageType storage_type() const {
    return FileSystemTypeToQuotaStorageType(type_);
  }

  // Helper routines to perform file system operations.
  // OpenFileSystem() must have been called before calling any of them.
  // They create an operation and run it on IO task runner, and the operation
  // posts a task on file runner.
  base::File::Error CreateDirectory(const storage::FileSystemURL& url);
  base::File::Error CreateFile(const storage::FileSystemURL& url);
  base::File::Error Copy(const storage::FileSystemURL& src_url,
                         const storage::FileSystemURL& dest_url);
  base::File::Error Move(const storage::FileSystemURL& src_url,
                         const storage::FileSystemURL& dest_url);
  base::File::Error TruncateFile(const storage::FileSystemURL& url, int64 size);
  base::File::Error TouchFile(const storage::FileSystemURL& url,
                              const base::Time& last_access_time,
                              const base::Time& last_modified_time);
  base::File::Error Remove(const storage::FileSystemURL& url, bool recursive);
  base::File::Error FileExists(const storage::FileSystemURL& url);
  base::File::Error DirectoryExists(const storage::FileSystemURL& url);
  base::File::Error VerifyFile(const storage::FileSystemURL& url,
                               const std::string& expected_data);
  base::File::Error GetMetadataAndPlatformPath(
      const storage::FileSystemURL& url,
      base::File::Info* info,
      base::FilePath* platform_path);
  base::File::Error ReadDirectory(const storage::FileSystemURL& url,
                                  FileEntryList* entries);

  // Returns the # of bytes written (>=0) or an error code (<0).
  int64 Write(net::URLRequestContext* url_request_context,
              const storage::FileSystemURL& url,
              scoped_ptr<storage::BlobDataHandle> blob_data_handle);
  int64 WriteString(const storage::FileSystemURL& url, const std::string& data);

  // Purges the file system local storage.
  base::File::Error DeleteFileSystem();

  // Retrieves the quota and usage.
  storage::QuotaStatusCode GetUsageAndQuota(int64* usage, int64* quota);

  // ChangeTracker related methods. They run on file task runner.
  void GetChangedURLsInTracker(storage::FileSystemURLSet* urls);
  void ClearChangeForURLInTracker(const storage::FileSystemURL& url);
  void GetChangesForURLInTracker(const storage::FileSystemURL& url,
                                 FileChangeList* changes);

  SyncFileSystemBackend* backend();
  storage::FileSystemOperationRunner* operation_runner();

  // LocalFileSyncStatus::Observer overrides.
  virtual void OnSyncEnabled(const storage::FileSystemURL& url) OVERRIDE;
  virtual void OnWriteEnabled(const storage::FileSystemURL& url) OVERRIDE;

  // Operation methods body.
  // They can be also called directly if the caller is already on IO thread.
  void DoOpenFileSystem(const OpenFileSystemCallback& callback);
  void DoCreateDirectory(const storage::FileSystemURL& url,
                         const StatusCallback& callback);
  void DoCreateFile(const storage::FileSystemURL& url,
                    const StatusCallback& callback);
  void DoCopy(const storage::FileSystemURL& src_url,
              const storage::FileSystemURL& dest_url,
              const StatusCallback& callback);
  void DoMove(const storage::FileSystemURL& src_url,
              const storage::FileSystemURL& dest_url,
              const StatusCallback& callback);
  void DoTruncateFile(const storage::FileSystemURL& url,
                      int64 size,
                      const StatusCallback& callback);
  void DoTouchFile(const storage::FileSystemURL& url,
                   const base::Time& last_access_time,
                   const base::Time& last_modified_time,
                   const StatusCallback& callback);
  void DoRemove(const storage::FileSystemURL& url,
                bool recursive,
                const StatusCallback& callback);
  void DoFileExists(const storage::FileSystemURL& url,
                    const StatusCallback& callback);
  void DoDirectoryExists(const storage::FileSystemURL& url,
                         const StatusCallback& callback);
  void DoVerifyFile(const storage::FileSystemURL& url,
                    const std::string& expected_data,
                    const StatusCallback& callback);
  void DoGetMetadataAndPlatformPath(const storage::FileSystemURL& url,
                                    base::File::Info* info,
                                    base::FilePath* platform_path,
                                    const StatusCallback& callback);
  void DoReadDirectory(const storage::FileSystemURL& url,
                       FileEntryList* entries,
                       const StatusCallback& callback);
  void DoWrite(net::URLRequestContext* url_request_context,
               const storage::FileSystemURL& url,
               scoped_ptr<storage::BlobDataHandle> blob_data_handle,
               const WriteCallback& callback);
  void DoWriteString(const storage::FileSystemURL& url,
                     const std::string& data,
                     const WriteCallback& callback);
  void DoGetUsageAndQuota(int64* usage,
                          int64* quota,
                          const storage::StatusCallback& callback);

 private:
  typedef ObserverListThreadSafe<LocalFileSyncStatus::Observer> ObserverList;

  // Callbacks.
  void DidOpenFileSystem(base::SingleThreadTaskRunner* original_task_runner,
                         const base::Closure& quit_closure,
                         const GURL& root,
                         const std::string& name,
                         base::File::Error result);
  void DidInitializeFileSystemContext(const base::Closure& quit_closure,
                                      sync_file_system::SyncStatusCode status);

  void InitializeSyncStatusObserver();

  base::ScopedTempDir data_dir_;
  const std::string service_name_;

  scoped_refptr<storage::QuotaManager> quota_manager_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  const GURL origin_;
  const storage::FileSystemType type_;
  GURL root_url_;
  base::File::Error result_;
  sync_file_system::SyncStatusCode sync_status_;

  leveldb::Env* env_override_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  // Boolean flags mainly for helping debug.
  bool is_filesystem_set_up_;
  bool is_filesystem_opened_;  // Should be accessed only on the IO thread.

  scoped_refptr<ObserverList> sync_status_observers_;

  DISALLOW_COPY_AND_ASSIGN(CannedSyncableFileSystem);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_CANNED_SYNCABLE_FILE_SYSTEM_H_
