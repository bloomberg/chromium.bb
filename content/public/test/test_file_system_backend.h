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

namespace storage {
class AsyncFileUtilAdapter;
class FileSystemQuotaUtil;
}

namespace content {

// This should be only used for testing.
// This file system backend uses LocalFileUtil and stores data file
// under the given directory.
class TestFileSystemBackend : public storage::FileSystemBackend {
 public:
  TestFileSystemBackend(
      base::SequencedTaskRunner* task_runner,
      const base::FilePath& base_path);
  virtual ~TestFileSystemBackend();

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

  // Initialize the CopyOrMoveFileValidatorFactory. Invalid to call more than
  // once.
  void InitializeCopyOrMoveFileValidatorFactory(
      scoped_ptr<storage::CopyOrMoveFileValidatorFactory> factory);

  const storage::UpdateObserverList* GetUpdateObservers(
      storage::FileSystemType type) const;
  void AddFileChangeObserver(storage::FileChangeObserver* observer);

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
  scoped_ptr<storage::AsyncFileUtilAdapter> file_util_;
  scoped_ptr<QuotaUtil> quota_util_;

  bool require_copy_or_move_validator_;
  scoped_ptr<storage::CopyOrMoveFileValidatorFactory>
      copy_or_move_file_validator_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestFileSystemBackend);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_FILE_SYSTEM_BACKEND_H_
