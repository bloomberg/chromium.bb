// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_BASE_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_BASE_H_

#include <vector>

#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/common/content_export.h"
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
class CONTENT_EXPORT NativeFileSystemHandleBase {
 public:
  using BindingContext = NativeFileSystemManagerImpl::BindingContext;
  using PermissionStatus = blink::mojom::PermissionStatus;

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

  PermissionStatus GetReadPermissionStatus() const {
    return read_permission_status_;
  }
  PermissionStatus GetWritePermissionStatus() const;

  // Implementation for the GetPermissionStatus method in the
  // blink::mojom::NativeFileSystemFileHandle and DirectoryHandle interfaces.
  void DoGetPermissionStatus(
      bool writable,
      base::OnceCallback<void(PermissionStatus)> callback);
  // Implementation for the RequestPermission method in the
  // blink::mojom::NativeFileSystemFileHandle and DirectoryHandle interfaces.
  void DoRequestPermission(bool writable,
                           base::OnceCallback<void(PermissionStatus)> callback);

  // Invokes |callback|, possibly after first requesting write permission. If
  // permission isn't granted, |permission_denied| is invoked instead. The
  // callbacks can be invoked synchronously.
  template <typename CallbackArgType>
  void RunWithWritePermission(
      base::OnceCallback<void(CallbackArgType)> callback,
      base::OnceCallback<void(CallbackArgType)> no_permission_callback,
      CallbackArgType callback_arg);

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

  // TODO(mek): We'll likely end up with something more complicated than simple
  // fields like this, but this will do for now.
  PermissionStatus read_permission_status_ = PermissionStatus::GRANTED;
  PermissionStatus write_permission_status_ = PermissionStatus::ASK;

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemHandleBase);
};

template <typename CallbackArgType>
void NativeFileSystemHandleBase::RunWithWritePermission(
    base::OnceCallback<void(CallbackArgType)> callback,
    base::OnceCallback<void(CallbackArgType)> no_permission_callback,
    CallbackArgType callback_arg) {
  DoRequestPermission(
      /*writable=*/true,
      base::BindOnce(
          [](base::OnceCallback<void(CallbackArgType)> callback,
             base::OnceCallback<void(CallbackArgType)> no_permission_callback,
             CallbackArgType callback_arg,
             blink::mojom::PermissionStatus status) {
            if (status == blink::mojom::PermissionStatus::GRANTED) {
              std::move(callback).Run(std::move(callback_arg));
              return;
            }
            std::move(no_permission_callback).Run(std::move(callback_arg));
          },
          std::move(callback), std::move(no_permission_callback),
          std::move(callback_arg)));
}

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_BASE_H_
