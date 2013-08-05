// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNC_FILE_SYSTEM_BACKEND_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNC_FILE_SYSTEM_BACKEND_H_

#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_quota_util.h"
#include "webkit/browser/fileapi/sandbox_context.h"

namespace sync_file_system {

class LocalFileChangeTracker;
class LocalFileSyncContext;

class SyncFileSystemBackend
    : public fileapi::FileSystemBackend,
      public fileapi::FileSystemQuotaUtil {
 public:
  SyncFileSystemBackend();
  virtual ~SyncFileSystemBackend();

  // FileSystemBackend overrides.
  virtual bool CanHandleType(fileapi::FileSystemType type) const OVERRIDE;
  virtual void Initialize(fileapi::FileSystemContext* context) OVERRIDE;
  virtual void OpenFileSystem(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      fileapi::OpenFileSystemMode mode,
      const OpenFileSystemCallback& callback) OVERRIDE;
  virtual fileapi::FileSystemFileUtil* GetFileUtil(
      fileapi::FileSystemType type) OVERRIDE;
  virtual fileapi::AsyncFileUtil* GetAsyncFileUtil(
      fileapi::FileSystemType type) OVERRIDE;
  virtual fileapi::CopyOrMoveFileValidatorFactory*
      GetCopyOrMoveFileValidatorFactory(
          fileapi::FileSystemType type,
          base::PlatformFileError* error_code) OVERRIDE;
  virtual fileapi::FileSystemOperation* CreateFileSystemOperation(
      const fileapi::FileSystemURL& url,
      fileapi::FileSystemContext* context,
      base::PlatformFileError* error_code) const OVERRIDE;
  virtual scoped_ptr<webkit_blob::FileStreamReader> CreateFileStreamReader(
      const fileapi::FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time,
      fileapi::FileSystemContext* context) const OVERRIDE;
  virtual scoped_ptr<fileapi::FileStreamWriter> CreateFileStreamWriter(
      const fileapi::FileSystemURL& url,
      int64 offset,
      fileapi::FileSystemContext* context) const OVERRIDE;
  virtual fileapi::FileSystemQuotaUtil* GetQuotaUtil() OVERRIDE;

  // FileSystemQuotaUtil overrides.
  virtual base::PlatformFileError DeleteOriginDataOnFileThread(
      fileapi::FileSystemContext* context,
      quota::QuotaManagerProxy* proxy,
      const GURL& origin_url,
      fileapi::FileSystemType type) OVERRIDE;
  virtual void GetOriginsForTypeOnFileThread(
      fileapi::FileSystemType type,
      std::set<GURL>* origins) OVERRIDE;
  virtual void GetOriginsForHostOnFileThread(
      fileapi::FileSystemType type,
      const std::string& host,
      std::set<GURL>* origins) OVERRIDE;
  virtual int64 GetOriginUsageOnFileThread(
      fileapi::FileSystemContext* context,
      const GURL& origin_url,
      fileapi::FileSystemType type) OVERRIDE;
  virtual void InvalidateUsageCache(
      const GURL& origin_url,
      fileapi::FileSystemType type) OVERRIDE;
  virtual void StickyInvalidateUsageCache(
      const GURL& origin_url,
      fileapi::FileSystemType type) OVERRIDE;
  virtual void AddFileUpdateObserver(
      fileapi::FileSystemType type,
      fileapi::FileUpdateObserver* observer,
      base::SequencedTaskRunner* task_runner) OVERRIDE;
  virtual void AddFileChangeObserver(
      fileapi::FileSystemType type,
      fileapi::FileChangeObserver* observer,
      base::SequencedTaskRunner* task_runner) OVERRIDE;
  virtual void AddFileAccessObserver(
      fileapi::FileSystemType type,
      fileapi::FileAccessObserver* observer,
      base::SequencedTaskRunner* task_runner) OVERRIDE;
  virtual const fileapi::UpdateObserverList* GetUpdateObservers(
      fileapi::FileSystemType type) const OVERRIDE;
  virtual const fileapi::ChangeObserverList* GetChangeObservers(
      fileapi::FileSystemType type) const OVERRIDE;
  virtual const fileapi::AccessObserverList* GetAccessObservers(
      fileapi::FileSystemType type) const OVERRIDE;

  static SyncFileSystemBackend* GetBackend(
      const fileapi::FileSystemContext* context);

  sync_file_system::LocalFileChangeTracker* change_tracker() {
    return change_tracker_.get();
  }
  void SetLocalFileChangeTracker(
      scoped_ptr<sync_file_system::LocalFileChangeTracker> tracker);

  sync_file_system::LocalFileSyncContext* sync_context() {
    return sync_context_.get();
  }
  void set_sync_context(sync_file_system::LocalFileSyncContext* sync_context);

 private:
  // Observers for internal sync.
  fileapi::UpdateObserverList update_observers_;
  fileapi::ChangeObserverList change_observers_;

  fileapi::UpdateObserverList syncable_update_observers_;
  fileapi::ChangeObserverList syncable_change_observers_;

  // Owned by FileSystemContext.
  fileapi::SandboxContext* sandbox_context_;

  scoped_ptr<sync_file_system::LocalFileChangeTracker> change_tracker_;
  scoped_refptr<sync_file_system::LocalFileSyncContext> sync_context_;

  DISALLOW_COPY_AND_ASSIGN(SyncFileSystemBackend);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNC_FILE_SYSTEM_BACKEND_H_
