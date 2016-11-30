// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_file_stream_reader.h"

#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_instance_util.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/edk/embedder/embedder.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace arc {

namespace {

void OnGetFileSize(const net::Int64CompletionCallback& callback, int64_t size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(size < 0 ? net::ERR_FAILED : size);
}

}  // namespace

ArcContentFileSystemFileStreamReader::ArcContentFileSystemFileStreamReader(
    const GURL& arc_url,
    int64_t offset)
    : arc_url_(arc_url), offset_(offset), weak_ptr_factory_(this) {}

ArcContentFileSystemFileStreamReader::~ArcContentFileSystemFileStreamReader() =
    default;

int ArcContentFileSystemFileStreamReader::Read(
    net::IOBuffer* buffer,
    int buffer_length,
    const net::CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (file_stream_)
    return file_stream_->Read(buffer, buffer_length, callback);

  if (offset_ != 0) {
    // TODO(hashimoto): Handle offset_ != 0 cases.
    NOTIMPLEMENTED() << "Non-zero offset is not supported yet: offset_ = "
                     << offset_;
    return net::ERR_FAILED;
  }
  file_system_instance_util::OpenFileToReadOnIOThread(
      arc_url_,
      base::Bind(&ArcContentFileSystemFileStreamReader::OnOpenFile,
                 weak_ptr_factory_.GetWeakPtr(), make_scoped_refptr(buffer),
                 buffer_length, callback));
  return net::ERR_IO_PENDING;
}

int64_t ArcContentFileSystemFileStreamReader::GetLength(
    const net::Int64CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  file_system_instance_util::GetFileSizeOnIOThread(
      arc_url_, base::Bind(&OnGetFileSize, callback));
  return net::ERR_IO_PENDING;
}

void ArcContentFileSystemFileStreamReader::OnOpenFile(
    scoped_refptr<net::IOBuffer> buf,
    int buffer_length,
    const net::CompletionCallback& callback,
    mojo::ScopedHandle handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  mojo::edk::ScopedPlatformHandle platform_handle;
  if (mojo::edk::PassWrappedPlatformHandle(
          handle.release().value(), &platform_handle) != MOJO_RESULT_OK) {
    LOG(ERROR) << "PassWrappedPlatformHandle failed";
    callback.Run(net::ERR_FAILED);
    return;
  }
  base::File file(platform_handle.release().handle);
  if (!file.IsValid()) {
    LOG(ERROR) << "Invalid file.";
    callback.Run(net::ERR_FAILED);
    return;
  }
  file_stream_.reset(new net::FileStream(
      std::move(file), content::BrowserThread::GetBlockingPool()));
  // Resume Read().
  int result = Read(buf.get(), buffer_length, callback);
  if (result != net::ERR_IO_PENDING)
    callback.Run(result);
}

}  // namespace arc
