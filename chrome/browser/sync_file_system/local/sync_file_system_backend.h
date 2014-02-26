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

class SyncFileSystemBackend
    : public fileapi::FileSystemBackend {
 public:
  explicit SyncFileSystemBackend(Profile* profile);
  virtual ~SyncFileSystemBackend();

  static SyncFileSystemBackend* CreateForTesting();

  // FileSystemBackend overrides.
  virtual bool CanHandleType(fileapi::FileSystemType type) const OVERRIDE;
  virtual void Initialize(fileapi::FileSystemContext* context) OVERRIDE;
  virtual void ResolveURL(const fileapi::FileSystemURL& url,
                          fileapi::OpenFileSystemMode mode,
                          const OpenFileSystemCallback& callback) OVERRIDE;
  virtual fileapi::AsyncFileUtil* GetAsyncFileUtil(
      fileapi::FileSystemType type) OVERRIDE;
  virtual fileapi::CopyOrMoveFileValidatorFactory*
      GetCopyOrMoveFileValidatorFactory(
          fileapi::FileSystemType type,
          base::File::Error* error_code) OVERRIDE;
  virtual fileapi::FileSystemOperation* CreateFileSystemOperation(
      const fileapi::FileSystemURL& url,
      fileapi::FileSystemContext* context,
      base::File::Error* error_code) const OVERRIDE;
  virtual bool SupportsStreaming(
      const fileapi::FileSystemURL& url) const OVERRIDE;
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

  static SyncFileSystemBackend* GetBackend(
      const fileapi::FileSystemContext* context);

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
  fileapi::FileSystemContext* context_;

  scoped_ptr<LocalFileChangeTracker> change_tracker_;
  scoped_refptr<LocalFileSyncContext> sync_context_;

  // Should be accessed on the UI thread.
  scoped_ptr<ProfileHolder> profile_holder_;

  // A flag to skip the initialization sequence of SyncFileSystemService for
  // testing.
  bool skip_initialize_syncfs_service_for_testing_;

  fileapi::SandboxFileSystemBackendDelegate* GetDelegate() const;

  void InitializeSyncFileSystemService(
      const GURL& origin_url,
      const SyncStatusCallback& callback);
  void DidInitializeSyncFileSystemService(
      fileapi::FileSystemContext* context,
      const GURL& origin_url,
      fileapi::FileSystemType type,
      fileapi::OpenFileSystemMode mode,
      const OpenFileSystemCallback& callback,
      SyncStatusCode status);

  DISALLOW_COPY_AND_ASSIGN(SyncFileSystemBackend);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNC_FILE_SYSTEM_BACKEND_H_
