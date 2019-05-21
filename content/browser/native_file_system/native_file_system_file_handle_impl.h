// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_FILE_HANDLE_IMPL_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_FILE_HANDLE_IMPL_H_

#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/common/content_export.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_file_handle.mojom.h"

namespace storage {
class BlobDataHandle;
class BlobStorageContext;
class FileSystemContext;
class FileSystemOperationRunner;
}  // namespace storage

namespace content {

// This is the browser side implementation of the
// NativeFileSystemFileHandle mojom interface. Instances of this class are
// owned by the NativeFileSystemManagerImpl instance passed in to the
// constructor.
//
// This class is not thread safe, all methods should be called on the IO thread.
// The link to the IO thread is due to its dependencies on both the blob system
// (via storage::BlobStorageContext) and the file system backends (via
// storage::FileSystemContext and storage::FileSystemOperationRunner, which both
// expect some of their methods to always be called on the IO thread).
// See https://crbug.com/957249 for some thoughts about the blob system aspect
// of this.
class CONTENT_EXPORT NativeFileSystemFileHandleImpl
    : public blink::mojom::NativeFileSystemFileHandle {
 public:
  NativeFileSystemFileHandleImpl(
      NativeFileSystemManagerImpl* manager,
      const storage::FileSystemURL& url,
      storage::IsolatedContext::ScopedFSHandle file_system);
  ~NativeFileSystemFileHandleImpl() override;

  const storage::FileSystemURL& url() const { return url_; }
  const storage::IsolatedContext::ScopedFSHandle& file_system() const {
    return file_system_;
  }

  // blink::mojom::NativeFileSystemFileHandle:
  void AsBlob(AsBlobCallback callback) override;
  void Remove(RemoveCallback callback) override;
  void Write(uint64_t offset,
             blink::mojom::BlobPtr data,
             WriteCallback callback) override;
  void WriteStream(uint64_t offset,
                   mojo::ScopedDataPipeConsumerHandle stream,
                   WriteStreamCallback callback) override;
  void Truncate(uint64_t length, TruncateCallback callback) override;
  void Transfer(
      blink::mojom::NativeFileSystemTransferTokenRequest token) override;

 private:
  // State that is kept for the duration of a write operation, to keep track of
  // progress until the write completes.
  struct WriteState;

  void DidGetMetaDataForBlob(AsBlobCallback callback,
                             base::File::Error result,
                             const base::File::Info& info);

  void DoWriteBlob(WriteCallback callback,
                   uint64_t position,
                   std::unique_ptr<storage::BlobDataHandle> blob);
  void DoWriteBlobWithFileInfo(WriteCallback callback,
                               uint64_t position,
                               std::unique_ptr<storage::BlobDataHandle> blob,
                               base::File::Error result,
                               const base::File::Info& file_info);
  void DoWriteStreamWithFileInfo(WriteStreamCallback callback,
                                 uint64_t position,
                                 mojo::ScopedDataPipeConsumerHandle data_pipe,
                                 base::File::Error result,
                                 const base::File::Info& file_info);
  void DidWrite(WriteState* state,
                base::File::Error result,
                int64_t bytes,
                bool complete);

  NativeFileSystemManagerImpl* manager() { return manager_; }
  storage::FileSystemOperationRunner* operation_runner() {
    return manager()->operation_runner();
  }
  storage::FileSystemContext* file_system_context() {
    return manager()->context();
  }
  storage::BlobStorageContext* blob_context() {
    return manager()->blob_context();
  }

  // The NativeFileSystemManagerImpl that owns us.
  NativeFileSystemManagerImpl* const manager_;
  const storage::FileSystemURL url_;
  const storage::IsolatedContext::ScopedFSHandle file_system_;

  base::WeakPtrFactory<NativeFileSystemFileHandleImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemFileHandleImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_FILE_HANDLE_IMPL_H_
