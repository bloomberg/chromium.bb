// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/file_stream_reader.h"

#include "base/files/file.h"
#include "chrome/browser/chromeos/file_system_provider/fileapi/provider_async_file_util.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace chromeos {
namespace file_system_provider {
namespace {

// Dicards the callback from CloseFile().
void EmptyStatusCallback(base::File::Error /* result */) {
}

// Converts net::CompletionCallback to net::Int64CompletionCallback.
void Int64ToIntCompletionCallback(net::CompletionCallback callback,
                                  int64 result) {
  callback.Run(static_cast<int>(result));
}

// Opens a file for reading and calls the completion callback. Must be called
// on UI thread.
void OpenFileOnUIThread(
    const fileapi::FileSystemURL& url,
    const FileStreamReader::InitializeCompletedCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(mtomasz): Check if the modification time of the file is as expected.
  util::FileSystemURLParser parser(url);
  if (!parser.Parse()) {
    callback.Run(base::WeakPtr<ProvidedFileSystemInterface>(),
                 base::FilePath(),
                 0 /* file_handle */,
                 base::File::FILE_ERROR_SECURITY);
    return;
  }

  parser.file_system()->OpenFile(
      parser.file_path(),
      ProvidedFileSystemInterface::OPEN_FILE_MODE_READ,
      false /* create */,
      base::Bind(
          callback, parser.file_system()->GetWeakPtr(), parser.file_path()));
}

// Forwards results of calling OpenFileOnUIThread back to the IO thread.
void OnOpenFileCompletedOnUIThread(
    const FileStreamReader::InitializeCompletedCallback& callback,
    base::WeakPtr<ProvidedFileSystemInterface> file_system,
    const base::FilePath& file_path,
    int file_handle,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, file_system, file_path, file_handle, result));
}

// Closes a file. Ignores result, since it is called from a constructor.
// Must be called on UI thread.
void CloseFileOnUIThread(base::WeakPtr<ProvidedFileSystemInterface> file_system,
                         int file_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (file_system.get())
    file_system->CloseFile(file_handle, base::Bind(&EmptyStatusCallback));
}

// Requests reading contents of a file. In case of either success or a failure
// |callback| is executed. It can be called many times, until |has_next| is set
// to false. This function guarantees that it will succeed only if the file has
// not been changed while reading. Must be called on UI thread.
void ReadFileOnUIThread(
    base::WeakPtr<ProvidedFileSystemInterface> file_system,
    int file_handle,
    net::IOBuffer* buffer,
    int64 offset,
    int length,
    const ProvidedFileSystemInterface::ReadChunkReceivedCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the file system got unmounted, then abort the reading operation.
  if (!file_system.get()) {
    callback.Run(0, false /* has_next */, base::File::FILE_ERROR_ABORT);
    return;
  }

  file_system->ReadFile(file_handle, buffer, offset, length, callback);
}

// Forward the completion callback to IO thread.
void OnReadChunkReceivedOnUIThread(
    const ProvidedFileSystemInterface::ReadChunkReceivedCallback&
        chunk_received_callback,
    int chunk_length,
    bool has_next,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(chunk_received_callback, chunk_length, has_next, result));
}

// Requests metadata of a file. In case of either succes or a failure,
// |callback is executed. Must be called on UI thread.
void GetMetadataOnUIThread(
    base::WeakPtr<ProvidedFileSystemInterface> file_system,
    const base::FilePath& file_path,
    const fileapi::AsyncFileUtil::GetFileInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the file system got unmounted, then abort the get length operation.
  if (!file_system.get()) {
    callback.Run(base::File::FILE_ERROR_ABORT, base::File::Info());
    return;
  }

  file_system->GetMetadata(file_path, callback);
}

// Forward the completion callback to IO thread.
void OnGetMetadataReceivedOnUIThread(
    const fileapi::AsyncFileUtil::GetFileInfoCallback& callback,
    base::File::Error result,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback, result, file_info));
}

}  // namespace

FileStreamReader::FileStreamReader(fileapi::FileSystemContext* context,
                                   const fileapi::FileSystemURL& url,
                                   int64 initial_offset,
                                   const base::Time& expected_modification_time)
    : url_(url),
      current_offset_(initial_offset),
      current_length_(0),
      expected_modification_time_(expected_modification_time),
      file_handle_(0),
      weak_ptr_factory_(this) {
}

FileStreamReader::~FileStreamReader() {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CloseFileOnUIThread, file_system_, file_handle_));
}

void FileStreamReader::Initialize(
    const base::Closure& pending_closure,
    const net::Int64CompletionCallback& error_callback) {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OpenFileOnUIThread,
                 url_,
                 base::Bind(&OnOpenFileCompletedOnUIThread,
                            base::Bind(&FileStreamReader::OnInitializeCompleted,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       pending_closure,
                                       error_callback))));
}

void FileStreamReader::OnInitializeCompleted(
    const base::Closure& pending_closure,
    const net::Int64CompletionCallback& error_callback,
    base::WeakPtr<ProvidedFileSystemInterface> file_system,
    const base::FilePath& file_path,
    int file_handle,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // In case of an error, return immediately using the |error_callback| of the
  // Read() or GetLength() pending request.
  if (result != base::File::FILE_OK) {
    error_callback.Run(net::FileErrorToNetError(result));
    return;
  }

  file_system_ = file_system;
  file_path_ = file_path;
  file_handle_ = file_handle;
  DCHECK_LT(0, file_handle);

  // Run the task waiting for the initialization to be completed.
  pending_closure.Run();
}

int FileStreamReader::Read(net::IOBuffer* buffer,
                           int buffer_length,
                           const net::CompletionCallback& callback) {
  // Lazily initialize with the first call to Read().
  if (!file_handle_) {
    Initialize(base::Bind(&FileStreamReader::ReadAfterInitialized,
                          weak_ptr_factory_.GetWeakPtr(),
                          buffer,
                          buffer_length,
                          callback),
               base::Bind(&Int64ToIntCompletionCallback, callback));
    return net::ERR_IO_PENDING;
  }

  ReadAfterInitialized(buffer, buffer_length, callback);
  return net::ERR_IO_PENDING;
}

int64 FileStreamReader::GetLength(
    const net::Int64CompletionCallback& callback) {
  // Lazily initialize with the first call to GetLength().
  if (!file_handle_) {
    Initialize(base::Bind(&FileStreamReader::GetLengthAfterInitialized,
                          weak_ptr_factory_.GetWeakPtr(),
                          callback),
               callback);
    return net::ERR_IO_PENDING;
  }

  GetLengthAfterInitialized(callback);
  return net::ERR_IO_PENDING;
}

void FileStreamReader::ReadAfterInitialized(
    net::IOBuffer* buffer,
    int buffer_length,
    const net::CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // If the file system got unmounted, then abort the reading operation.
  if (!file_handle_) {
    callback.Run(net::ERR_ABORTED);
    return;
  }

  current_length_ = 0;
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ReadFileOnUIThread,
                 file_system_,
                 file_handle_,
                 buffer,
                 current_offset_,
                 buffer_length,
                 base::Bind(&OnReadChunkReceivedOnUIThread,
                            base::Bind(&FileStreamReader::OnReadChunkReceived,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       callback))));
}

void FileStreamReader::GetLengthAfterInitialized(
    const net::Int64CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // If the file system got unmounted, then abort the length fetching operation.
  if (!file_handle_) {
    callback.Run(net::ERR_ABORTED);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &GetMetadataOnUIThread,
          file_system_,
          file_path_,
          base::Bind(
              &OnGetMetadataReceivedOnUIThread,
              base::Bind(&FileStreamReader::OnGetMetadataForGetLengthReceived,
                         weak_ptr_factory_.GetWeakPtr(),
                         callback))));
}

void FileStreamReader::OnReadChunkReceived(
    const net::CompletionCallback& callback,
    int chunk_length,
    bool has_next,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  current_length_ += chunk_length;

  // If this is the last chunk with a success, then finalize.
  if (!has_next && result == base::File::FILE_OK) {
    current_offset_ += current_length_;
    callback.Run(current_length_);
    return;
  }

  // In case of an error, abort.
  if (result != base::File::FILE_OK) {
    DCHECK(!has_next);
    callback.Run(net::FileErrorToNetError(result));
    return;
  }

  // More data is about to come, so do not call the callback yet.
  DCHECK(has_next);
}

void FileStreamReader::OnGetMetadataForGetLengthReceived(
    const net::Int64CompletionCallback& callback,
    base::File::Error result,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // In case of an error, abort.
  if (result != base::File::FILE_OK) {
    callback.Run(net::FileErrorToNetError(result));
    return;
  }

  DCHECK_EQ(result, base::File::FILE_OK);
  callback.Run(file_info.size);
}

}  // namespace file_system_provider
}  // namespace chromeos
