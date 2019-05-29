// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_BASE_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_BASE_H_

#include <vector>

#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace storage {
class FileSystemContext;
class FileSystemOperationRunner;
class BlobStorageContext;
}  // namespace storage

namespace content {

// Base class for File and Directory handle implementations. Holds data that is
// common to both and (will) deal with functionality that is common as well,
// such as permission requests. Instances of this class should be owned by the
// NativeFileSystemManagerImpl instance passed in to the constructor.
//
// This class is not thread safe, all methods should only be called on the IO
// thread. This is because code interacts directly with the file system backends
// (via storage::FileSystemContext and store::FileSystemOperationRunner, which
// both expect some of their methods to only be called on the IO thread).
class NativeFileSystemHandleBase {
 public:
  using BindingContext = NativeFileSystemManagerImpl::BindingContext;

  NativeFileSystemHandleBase(
      NativeFileSystemManagerImpl* manager,
      const BindingContext& context,
      const storage::FileSystemURL& url,
      storage::IsolatedContext::ScopedFSHandle file_system);
  virtual ~NativeFileSystemHandleBase();

  const storage::FileSystemURL& url() const { return url_; }
  const storage::IsolatedContext::ScopedFSHandle& file_system() const {
    return file_system_;
  }

 protected:
  NativeFileSystemManagerImpl* manager() { return manager_; }
  const BindingContext& context() { return context_; }
  storage::FileSystemOperationRunner* operation_runner() {
    return manager()->operation_runner();
  }
  storage::FileSystemContext* file_system_context() {
    return manager()->context();
  }
  storage::BlobStorageContext* blob_context() {
    return manager()->blob_context();
  }

 private:
  // The NativeFileSystemManagerImpl that owns this instance.
  NativeFileSystemManagerImpl* const manager_;
  const BindingContext context_;
  const storage::FileSystemURL url_;
  const storage::IsolatedContext::ScopedFSHandle file_system_;

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemHandleBase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_BASE_H_
