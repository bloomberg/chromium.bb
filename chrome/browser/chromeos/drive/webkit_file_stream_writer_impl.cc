// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/webkit_file_stream_writer_impl.h"

#include "base/callback_helpers.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "webkit/browser/blob/local_file_stream_reader.h"
#include "webkit/browser/fileapi/local_file_stream_writer.h"
#include "webkit/browser/fileapi/remote_file_system_proxy.h"
#include "webkit/common/blob/shareable_file_reference.h"

namespace drive {
namespace internal {

WebkitFileStreamWriterImpl::WebkitFileStreamWriterImpl(
    const scoped_refptr<fileapi::RemoteFileSystemProxyInterface>&
        remote_filesystem,
    const fileapi::FileSystemURL& url,
    int64 offset,
    base::TaskRunner* local_task_runner)
    : remote_filesystem_(remote_filesystem),
      local_task_runner_(local_task_runner),
      url_(url),
      initial_offset_(offset),
      has_pending_create_snapshot_(false),
      weak_ptr_factory_(this) {
}

WebkitFileStreamWriterImpl::~WebkitFileStreamWriterImpl() {
}

int WebkitFileStreamWriterImpl::Write(net::IOBuffer* buf,
                                      int buf_len,
                                      const net::CompletionCallback& callback) {
  DCHECK(!has_pending_create_snapshot_);
  DCHECK(pending_cancel_callback_.is_null());

  // If the local file is already available, just delegate to it.
  if (local_file_writer_)
    return local_file_writer_->Write(buf, buf_len, callback);

  // In this WebkitFileStreamWriterImpl, we only create snapshot file and don't
  // have explicit close operation. This is ok, because close is automatically
  // triggered by a refcounted |file_ref_| passed to
  // WriteAfterCreateWritableSnapshotFile, from the destructor of
  // WebkitFileStreamWriterImpl.
  has_pending_create_snapshot_ = true;
  remote_filesystem_->CreateWritableSnapshotFile(
      url_,
      base::Bind(
          &WebkitFileStreamWriterImpl::WriteAfterCreateWritableSnapshotFile,
          weak_ptr_factory_.GetWeakPtr(),
          make_scoped_refptr(buf),
          buf_len,
          callback));
  return net::ERR_IO_PENDING;
}

int WebkitFileStreamWriterImpl::Cancel(
    const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(pending_cancel_callback_.is_null());

  // If LocalFileWriter is already created, just delegate the cancel to it.
  if (local_file_writer_)
    return local_file_writer_->Cancel(callback);

  // If file open operation is in-flight, wait for its completion and cancel
  // further write operation in WriteAfterCreateWritableSnapshotFile.
  if (has_pending_create_snapshot_) {
    pending_cancel_callback_ = callback;
    return net::ERR_IO_PENDING;
  }

  // Write() is not called yet.
  return net::ERR_UNEXPECTED;
}

int WebkitFileStreamWriterImpl::Flush(const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(pending_cancel_callback_.is_null());

  // If LocalFileWriter is already created, just delegate to it.
  if (local_file_writer_)
    return local_file_writer_->Flush(callback);

  // There shouldn't be in-flight Write operation.
  DCHECK(!has_pending_create_snapshot_);

  // Here is the case Flush() is called before any Write() invocation.
  // Do nothing.
  // Synchronization to the remote server is not done until the file is closed.
  return net::OK;
}

void WebkitFileStreamWriterImpl::WriteAfterCreateWritableSnapshotFile(
    net::IOBuffer* buf,
    int buf_len,
    const net::CompletionCallback& callback,
    base::PlatformFileError open_result,
    const base::FilePath& local_path,
    const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
  has_pending_create_snapshot_ = false;
  if (!pending_cancel_callback_.is_null()) {
    // Cancel() is called during the creation of the snapshot file.
    // Don't write to the file.
    base::ResetAndReturn(&pending_cancel_callback_).Run(net::OK);
    return;
  }

  if (open_result != base::PLATFORM_FILE_OK) {
    callback.Run(net::PlatformFileErrorToNetError(open_result));
    return;
  }

  // Hold the reference to the file. Releasing the reference notifies the file
  // system about to close file.
  file_ref_ = file_ref;

  DCHECK(!local_file_writer_);
  local_file_writer_.reset(new fileapi::LocalFileStreamWriter(
      local_task_runner_.get(), local_path, initial_offset_));
  int result = local_file_writer_->Write(buf, buf_len, callback);
  if (result != net::ERR_IO_PENDING)
    callback.Run(result);
}

}  // namespace internal
}  // namespace drive
