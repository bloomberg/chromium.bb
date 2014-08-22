// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNC_FILE_SYSTEM_BACKEND_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNC_FILE_SYSTEM_BACKEND_H_

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_quota_util.h"
#include "webkit/browser/fileapi/sandbox_file_system_backend_delegate.h"

namespace sync_file_system {

class LocalFileChangeTracker;
class LocalFileSyncContext;

class SyncFileSystemBackend : public storage::FileSystemBackend {
 public:
  explicit SyncFileSystemBackend(Profile* profile);
  virtual ~SyncFileSystemBackend();

  static SyncFileSystemBackend* CreateForTesting();

  // FileSystemBackend overrides.
  virtual bool CanHandleType(storage::FileSystemType type) const OVERRIDE;
  virtual void Initialize(storage::FileSystemContext* context) OVERRIDE;
  virtual void ResolveURL(const storage::FileSystemURL& url,
                          storage::OpenFileSystemMode mode,
                          const OpenFileSystemCallback& callback) OVERRIDE;
  virtual storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) OVERRIDE;
  virtual storage::CopyOrMoveFileValidatorFactory*
      GetCopyOrMoveFileValidatorFactory(storage::FileSystemType type,
                                        base::File::Error* error_code) OVERRIDE;
  virtual storage::FileSystemOperation* CreateFileSystemOperation(
      const storage::FileSystemURL& url,
      storage::FileSystemContext* context,
      base::File::Error* error_code) const OVERRIDE;
  virtual bool SupportsStreaming(
      const storage::FileSystemURL& url) const OVERRIDE;
  virtual bool HasInplaceCopyImplementation(
      storage::FileSystemType type) const OVERRIDE;
  virtual scoped_ptr<storage::FileStreamReader> CreateFileStreamReader(
      const storage::FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context) const OVERRIDE;
  virtual scoped_ptr<storage::FileStreamWriter> CreateFileStreamWriter(
      const storage::FileSystemURL& url,
      int64 offset,
      storage::FileSystemContext* context) const OVERRIDE;
  virtual storage::FileSystemQuotaUtil* GetQuotaUtil() OVERRIDE;

  static SyncFileSystemBackend* GetBackend(
      const storage::FileSystemContext* context);

  LocalFileChangeTracker* change_tracker() { return change_tracker_.get(); }
  void SetLocalFileChangeTracker(scoped_ptr<LocalFileChangeTracker> tracker);

  LocalFileSyncContext* sync_context() { return sync_context_.get(); }
  void set_sync_context(LocalFileSyncContext* sync_context);

 private:
  class ProfileHolder : public content::NotificationObserver {
   public:
    explicit ProfileHolder(Profile* profile);

    // NotificationObserver override.
    virtual void Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) OVERRIDE;

    Profile* GetProfile();

   private:
    content::NotificationRegistrar registrar_;
    Profile* profile_;
  };

  // Not owned.
  storage::FileSystemContext* context_;

  scoped_ptr<LocalFileChangeTracker> change_tracker_;
  scoped_refptr<LocalFileSyncContext> sync_context_;

  // Should be accessed on the UI thread.
  scoped_ptr<ProfileHolder> profile_holder_;

  // A flag to skip the initialization sequence of SyncFileSystemService for
  // testing.
  bool skip_initialize_syncfs_service_for_testing_;

  storage::SandboxFileSystemBackendDelegate* GetDelegate() const;

  void InitializeSyncFileSystemService(
      const GURL& origin_url,
      const SyncStatusCallback& callback);
  void DidInitializeSyncFileSystemService(
      storage::FileSystemContext* context,
      const GURL& origin_url,
      storage::FileSystemType type,
      storage::OpenFileSystemMode mode,
      const OpenFileSystemCallback& callback,
      SyncStatusCode status);

  DISALLOW_COPY_AND_ASSIGN(SyncFileSystemBackend);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNC_FILE_SYSTEM_BACKEND_H_
