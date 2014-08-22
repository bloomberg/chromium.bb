// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_BACKEND_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_BACKEND_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_context.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_backend.h"

namespace local_discovery {

class PrivetFileSystemAsyncUtil;

class PrivetFileSystemBackend : public storage::FileSystemBackend {
 public:
  PrivetFileSystemBackend(storage::ExternalMountPoints* mount_points,
                          content::BrowserContext* browser_context);
  virtual ~PrivetFileSystemBackend();

  // FileSystemBackend implementation.
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

 private:
  // User mount points.
  scoped_refptr<storage::ExternalMountPoints> mount_points_;
  scoped_ptr<PrivetFileSystemAsyncUtil> async_util_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_BACKEND_H_
