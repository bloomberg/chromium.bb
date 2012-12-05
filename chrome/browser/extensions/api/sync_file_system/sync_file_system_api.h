// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_API_H_

#include "base/platform_file.h"
#include "chrome/browser/extensions/extension_function.h"
#include "webkit/fileapi/syncable/sync_file_status.h"
#include "webkit/fileapi/syncable/sync_status_code.h"
#include "webkit/quota/quota_types.h"

namespace fileapi {
class FileSystemContext;
}

namespace extensions {

class SyncFileSystemDeleteFileSystemFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("syncFileSystem.deleteFileSystem");

 protected:
  virtual ~SyncFileSystemDeleteFileSystemFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  void DidDeleteFileSystem(base::PlatformFileError error);
};


class SyncFileSystemGetFileSyncStatusFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("syncFileSystem.getFileSyncStatus");

 protected:
  virtual ~SyncFileSystemGetFileSyncStatusFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  void DidGetFileSyncStatus(const fileapi::SyncStatusCode sync_service_status,
                            const fileapi::SyncFileStatus sync_file_status);
};

class SyncFileSystemGetUsageAndQuotaFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("syncFileSystem.getUsageAndQuota");

 protected:
  virtual ~SyncFileSystemGetUsageAndQuotaFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  void DidGetUsageAndQuota(quota::QuotaStatusCode status,
                           int64 usage,
                           int64 quota);
};

class SyncFileSystemRequestFileSystemFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("syncFileSystem.requestFileSystem");

 protected:
  virtual ~SyncFileSystemRequestFileSystemFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  typedef SyncFileSystemRequestFileSystemFunction self;

  // Returns the file system context for this extension.
  fileapi::FileSystemContext* GetFileSystemContext();

  void DidInitializeFileSystemContext(const std::string& service_name,
                                      fileapi::SyncStatusCode status);
  void DidOpenFileSystem(base::PlatformFileError error,
                         const std::string& file_system_name,
                         const GURL& root_url);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_API_H_
