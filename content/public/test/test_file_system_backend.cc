// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_file_system_backend.h"

#include <set>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/sequenced_task_runner.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/file_observers.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_quota_util.h"
#include "webkit/browser/fileapi/local_file_util.h"
#include "webkit/browser/fileapi/native_file_util.h"
#include "webkit/browser/fileapi/quota/quota_reservation.h"
#include "webkit/browser/fileapi/sandbox_file_stream_writer.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/common/fileapi/file_system_util.h"

using fileapi::FileSystemContext;
using fileapi::FileSystemOperation;
using fileapi::FileSystemOperationContext;
using fileapi::FileSystemURL;

namespace content {

namespace {

class TestFileUtil : public fileapi::LocalFileUtil {
 public:
  explicit TestFileUtil(const base::FilePath& base_path)
      : base_path_(base_path) {}
  virtual ~TestFileUtil() {}

  // LocalFileUtil overrides.
  virtual base::File::Error GetLocalFilePath(
      FileSystemOperationContext* context,
      const FileSystemURL& file_system_url,
      base::FilePath* local_file_path) OVERRIDE {
    *local_file_path = base_path_.Append(file_system_url.path());
    return base::File::FILE_OK;
  }

 private:
  base::FilePath base_path_;
};

}  // namespace

// This only supports single origin.
class TestFileSystemBackend::QuotaUtil
    : public fileapi::FileSystemQuotaUtil,
      public fileapi::FileUpdateObserver {
 public:
  explicit QuotaUtil(base::SequencedTaskRunner* task_runner)
      : usage_(0),
        task_runner_(task_runner) {
    update_observers_ = update_observers_.AddObserver(this, task_runner_.get());
  }
  virtual ~QuotaUtil() {}

  // FileSystemQuotaUtil overrides.
  virtual base::File::Error DeleteOriginDataOnFileTaskRunner(
      FileSystemContext* context,
      quota::QuotaManagerProxy* proxy,
      const GURL& origin_url,
      fileapi::FileSystemType type) OVERRIDE {
    NOTREACHED();
    return base::File::FILE_OK;
  }

  virtual scoped_refptr<fileapi::QuotaReservation>
  CreateQuotaReservationOnFileTaskRunner(
      const GURL& origin_url,
      fileapi::FileSystemType type) OVERRIDE {
    NOTREACHED();
    return scoped_refptr<fileapi::QuotaReservation>();
  }

  virtual void GetOriginsForTypeOnFileTaskRunner(
      fileapi::FileSystemType type,
      std::set<GURL>* origins) OVERRIDE {
    NOTREACHED();
  }

  virtual void GetOriginsForHostOnFileTaskRunner(
      fileapi::FileSystemType type,
      const std::string& host,
      std::set<GURL>* origins) OVERRIDE {
    NOTREACHED();
  }

  virtual int64 GetOriginUsageOnFileTaskRunner(
      FileSystemContext* context,
      const GURL& origin_url,
      fileapi::FileSystemType type) OVERRIDE {
    return usage_;
  }

  virtual void AddFileUpdateObserver(
      fileapi::FileSystemType type,
      FileUpdateObserver* observer,
      base::SequencedTaskRunner* task_runner) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void AddFileChangeObserver(
      fileapi::FileSystemType type,
      fileapi::FileChangeObserver* observer,
      base::SequencedTaskRunner* task_runner) OVERRIDE {
    change_observers_ = change_observers_.AddObserver(observer, task_runner);
  }

  virtual void AddFileAccessObserver(
      fileapi::FileSystemType type,
      fileapi::FileAccessObserver* observer,
      base::SequencedTaskRunner* task_runner) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual const fileapi::UpdateObserverList* GetUpdateObservers(
      fileapi::FileSystemType type) const OVERRIDE {
    return &update_observers_;
  }

  virtual const fileapi::ChangeObserverList* GetChangeObservers(
      fileapi::FileSystemType type) const OVERRIDE {
    return &change_observers_;
  }

  virtual const fileapi::AccessObserverList* GetAccessObservers(
      fileapi::FileSystemType type) const OVERRIDE {
    return NULL;
  }

  // FileUpdateObserver overrides.
  virtual void OnStartUpdate(const FileSystemURL& url) OVERRIDE {}
  virtual void OnUpdate(const FileSystemURL& url, int64 delta) OVERRIDE {
    usage_ += delta;
  }
  virtual void OnEndUpdate(const FileSystemURL& url) OVERRIDE {}

  base::SequencedTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  int64 usage_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  fileapi::UpdateObserverList update_observers_;
  fileapi::ChangeObserverList change_observers_;
};

TestFileSystemBackend::TestFileSystemBackend(
    base::SequencedTaskRunner* task_runner,
    const base::FilePath& base_path)
    : base_path_(base_path),
      file_util_(
          new fileapi::AsyncFileUtilAdapter(new TestFileUtil(base_path))),
      quota_util_(new QuotaUtil(task_runner)),
      require_copy_or_move_validator_(false) {
}

TestFileSystemBackend::~TestFileSystemBackend() {
}

bool TestFileSystemBackend::CanHandleType(fileapi::FileSystemType type) const {
  return (type == fileapi::kFileSystemTypeTest);
}

void TestFileSystemBackend::Initialize(FileSystemContext* context) {
}

void TestFileSystemBackend::ResolveURL(const FileSystemURL& url,
                                       fileapi::OpenFileSystemMode mode,
                                       const OpenFileSystemCallback& callback) {
  callback.Run(GetFileSystemRootURI(url.origin(), url.type()),
               GetFileSystemName(url.origin(), url.type()),
               base::File::FILE_OK);
}

fileapi::AsyncFileUtil* TestFileSystemBackend::GetAsyncFileUtil(
    fileapi::FileSystemType type) {
  return file_util_.get();
}

fileapi::CopyOrMoveFileValidatorFactory*
TestFileSystemBackend::GetCopyOrMoveFileValidatorFactory(
    fileapi::FileSystemType type,
    base::File::Error* error_code) {
  DCHECK(error_code);
  *error_code = base::File::FILE_OK;
  if (require_copy_or_move_validator_) {
    if (!copy_or_move_file_validator_factory_)
      *error_code = base::File::FILE_ERROR_SECURITY;
    return copy_or_move_file_validator_factory_.get();
  }
  return NULL;
}

void TestFileSystemBackend::InitializeCopyOrMoveFileValidatorFactory(
    scoped_ptr<fileapi::CopyOrMoveFileValidatorFactory> factory) {
  if (!copy_or_move_file_validator_factory_)
    copy_or_move_file_validator_factory_ = factory.Pass();
}

FileSystemOperation* TestFileSystemBackend::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::File::Error* error_code) const {
  scoped_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(context));
  operation_context->set_update_observers(*GetUpdateObservers(url.type()));
  operation_context->set_change_observers(
      *quota_util_->GetChangeObservers(url.type()));
  return FileSystemOperation::Create(url, context, operation_context.Pass());
}

bool TestFileSystemBackend::SupportsStreaming(
    const fileapi::FileSystemURL& url) const {
  return false;
}

scoped_ptr<webkit_blob::FileStreamReader>
TestFileSystemBackend::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  return scoped_ptr<webkit_blob::FileStreamReader>(
      webkit_blob::FileStreamReader::CreateForFileSystemFile(
          context, url, offset, expected_modification_time));
}

scoped_ptr<fileapi::FileStreamWriter>
TestFileSystemBackend::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return scoped_ptr<fileapi::FileStreamWriter>(
      new fileapi::SandboxFileStreamWriter(context, url, offset,
                                           *GetUpdateObservers(url.type())));
}

fileapi::FileSystemQuotaUtil* TestFileSystemBackend::GetQuotaUtil() {
  return quota_util_.get();
}

const fileapi::UpdateObserverList* TestFileSystemBackend::GetUpdateObservers(
    fileapi::FileSystemType type) const {
  return quota_util_->GetUpdateObservers(type);
}

void TestFileSystemBackend::AddFileChangeObserver(
    fileapi::FileChangeObserver* observer) {
  quota_util_->AddFileChangeObserver(
      fileapi::kFileSystemTypeTest, observer, quota_util_->task_runner());
}

}  // namespace content
