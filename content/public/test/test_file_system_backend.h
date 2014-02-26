// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_FILE_SYSTEM_BACKEND_H_
#define CONTENT_PUBLIC_TEST_TEST_FILE_SYSTEM_BACKEND_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/browser/fileapi/async_file_util_adapter.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/task_runner_bound_observer_list.h"

namespace base {
class SequencedTaskRunner;
}

namespace fileapi {
class AsyncFileUtilAdapter;
class FileSystemQuotaUtil;
}

namespace content {

// This should be only used for testing.
// This file system backend uses LocalFileUtil and stores data file
// under the given directory.
class TestFileSystemBackend : public fileapi::FileSystemBackend {
 public:
  TestFileSystemBackend(
      base::SequencedTaskRunner* task_runner,
      const base::FilePath& base_path);
  virtual ~TestFileSystemBackend();

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

  // Initialize the CopyOrMoveFileValidatorFactory. Invalid to call more than
  // once.
  void InitializeCopyOrMoveFileValidatorFactory(
      scoped_ptr<fileapi::CopyOrMoveFileValidatorFactory> factory);

  const fileapi::UpdateObserverList*
      GetUpdateObservers(fileapi::FileSystemType type) const;
  void AddFileChangeObserver(fileapi::FileChangeObserver* observer);

  // For CopyOrMoveFileValidatorFactory testing. Once it's set to true
  // GetCopyOrMoveFileValidatorFactory will start returning security
  // error if validator is not initialized.
  void set_require_copy_or_move_validator(bool flag) {
    require_copy_or_move_validator_ = flag;
  }

 private:
  class QuotaUtil;

  base::FilePath base_path_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_ptr<fileapi::AsyncFileUtilAdapter> file_util_;
  scoped_ptr<QuotaUtil> quota_util_;

  bool require_copy_or_move_validator_;
  scoped_ptr<fileapi::CopyOrMoveFileValidatorFactory>
      copy_or_move_file_validator_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestFileSystemBackend);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_FILE_SYSTEM_BACKEND_H_
