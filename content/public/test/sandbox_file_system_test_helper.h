// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SANDBOX_FILE_SYSTEM_TEST_HELPER_H_
#define CONTENT_PUBLIC_TEST_SANDBOX_FILE_SYSTEM_TEST_HELPER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/file_system_usage_cache.h"
#include "webkit/browser/fileapi/task_runner_bound_observer_list.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"
#include "webkit/common/quota/quota_types.h"

namespace base {
class FilePath;
}

namespace quota {
class QuotaManagerProxy;
}

namespace fileapi {
class FileSystemContext;
class FileSystemFileUtil;
class FileSystemOperationContext;
class FileSystemOperationRunner;
}

namespace content {

// Filesystem test helper class that encapsulates test environment for
// a given {origin, type} pair.  This helper only works for sandboxed
// file systems (Temporary or Persistent).
class SandboxFileSystemTestHelper {
 public:
  SandboxFileSystemTestHelper(const GURL& origin, fileapi::FileSystemType type);
  SandboxFileSystemTestHelper();
  ~SandboxFileSystemTestHelper();

  void SetUp(const base::FilePath& base_dir);
  // If you want to use more than one SandboxFileSystemTestHelper in
  // a single base directory, they have to share a context, so that they don't
  // have multiple databases fighting over the lock to the origin directory
  // [deep down inside ObfuscatedFileUtil].
  void SetUp(fileapi::FileSystemContext* file_system_context);
  void SetUp(const base::FilePath& base_dir,
             quota::QuotaManagerProxy* quota_manager_proxy);
  void TearDown();

  base::FilePath GetOriginRootPath();
  base::FilePath GetLocalPath(const base::FilePath& path);
  base::FilePath GetLocalPathFromASCII(const std::string& path);

  // Returns empty path if filesystem type is neither temporary nor persistent.
  base::FilePath GetUsageCachePath() const;

  fileapi::FileSystemURL CreateURL(const base::FilePath& path) const;
  fileapi::FileSystemURL CreateURLFromUTF8(const std::string& utf8) const {
    return CreateURL(base::FilePath::FromUTF8Unsafe(utf8));
  }

  // This returns cached usage size returned by QuotaUtil.
  int64 GetCachedOriginUsage() const;

  // This doesn't work with OFSFU.
  int64 ComputeCurrentOriginUsage();

  int64 ComputeCurrentDirectoryDatabaseUsage();

  fileapi::FileSystemOperationRunner* operation_runner();
  fileapi::FileSystemOperationContext* NewOperationContext();

  void AddFileChangeObserver(fileapi::FileChangeObserver* observer);

  fileapi::FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

  const GURL& origin() const { return origin_; }
  fileapi::FileSystemType type() const { return type_; }
  quota::StorageType storage_type() const {
    return fileapi::FileSystemTypeToQuotaStorageType(type_);
  }
  fileapi::FileSystemFileUtil* file_util() const { return file_util_; }
  fileapi::FileSystemUsageCache* usage_cache();

 private:
  void SetUpFileSystem();

  scoped_refptr<fileapi::FileSystemContext> file_system_context_;

  const GURL origin_;
  const fileapi::FileSystemType type_;
  fileapi::FileSystemFileUtil* file_util_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SANDBOX_FILE_SYSTEM_TEST_HELPER_H_
