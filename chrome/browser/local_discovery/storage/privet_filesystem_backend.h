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

class PrivetFileSystemBackend : public fileapi::FileSystemBackend {
 public:
  PrivetFileSystemBackend(fileapi::ExternalMountPoints* mount_points,
                          content::BrowserContext* browser_context);
  virtual ~PrivetFileSystemBackend();

  // FileSystemBackend implementation.
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

 private:
  // User mount points.
  scoped_refptr<fileapi::ExternalMountPoints> mount_points_;
  scoped_ptr<PrivetFileSystemAsyncUtil> async_util_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_BACKEND_H_
