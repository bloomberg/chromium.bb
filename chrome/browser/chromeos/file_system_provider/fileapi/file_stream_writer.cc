// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/file_stream_writer.h"

#include "base/debug/trace_event.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/file_system_provider/fileapi/provider_async_file_util.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace chromeos {
namespace file_system_provider {
namespace {

// Dicards the callback from CloseFile().
void EmptyStatusCallback(base::File::Error /* result */) {
}

// Opens a file for writing and calls the completion callback. Must be called
// on UI thread.
void OpenFileOnUIThread(
    const fileapi::FileSystemURL& url,
    const FileStreamWriter::OpenFileCompletedCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
      ProvidedFileSystemInterface::OPEN_FILE_MODE_WRITE,
      base::Bind(
          callback, parser.file_system()->GetWeakPtr(), parser.file_path()));
}

// Forwards results of calling OpenFileOnUIThread back to the IO thread.
void OnOpenFileCompletedOnUIThread(
    const FileStreamWriter::OpenFileCompletedCallback& callback,
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

// Requests writing bytes to the file. In case of either success or a failure
// |callback| is executed. Must be called on UI thread.
void WriteFileOnUIThread(
    base::WeakPtr<ProvidedFileSystemInterface> file_system,
    int file_handle,
    scoped_refptr<net::IOBuffer> buffer,
    int64 offset,
    int length,
    const fileapi::AsyncFileUtil::StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the file system got unmounted, then abort the writing operation.
  if (!file_system.get()) {
    callback.Run(base::File::FILE_ERROR_ABORT);
    return;
  }

  file_system->WriteFile(file_handle, buffer, offset, length, callback);
}

// Forward the completion callback to IO thread.
void OnWriteFileCompletedOnUIThread(
    const fileapi::AsyncFileUtil::StatusCallback& callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(callback, result));
}

}  // namespace

FileStreamWriter::FileStreamWriter(const fileapi::FileSystemURL& url,
                                   int64 initial_offset)
    : url_(url),
      current_offset_(initial_offset),
      state_(NOT_INITIALIZED),
      file_handle_(0),
      weak_ptr_factory_(this) {
}

FileStreamWriter::~FileStreamWriter() {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&CloseFileOnUIThread, file_system_, file_handle_));
}

void FileStreamWriter::Initialize(
    const base::Closure& pending_closure,
    const net::CompletionCallback& error_callback) {
  DCHECK_EQ(NOT_INITIALIZED, state_);
  state_ = INITIALIZING;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OpenFileOnUIThread,
                 url_,
                 base::Bind(&OnOpenFileCompletedOnUIThread,
                            base::Bind(&FileStreamWriter::OnOpenFileCompleted,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       pending_closure,
                                       error_callback))));
}

void FileStreamWriter::OnOpenFileCompleted(
    const base::Closure& pending_closure,
    const net::CompletionCallback& error_callback,
    base::WeakPtr<ProvidedFileSystemInterface> file_system,
    const base::FilePath& file_path,
    int file_handle,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(INITIALIZING, state_);

  // In case of an error, return immediately using the |error_callback| of the
  // Write() pending request.
  if (result != base::File::FILE_OK) {
    state_ = FAILED;
    error_callback.Run(net::FileErrorToNetError(result));
    return;
  }

  file_system_ = file_system;
  file_path_ = file_path;
  file_handle_ = file_handle;
  DCHECK_LT(0, file_handle);
  DCHECK_EQ(base::File::FILE_OK, result);
  state_ = INITIALIZED;

  // Run the task waiting for the initialization to be completed.
  pending_closure.Run();
}

int FileStreamWriter::Write(net::IOBuffer* buffer,
                            int buffer_length,
                            const net::CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_ASYNC_BEGIN1("file_system_provider",
                           "FileStreamWriter::Write",
                           this,
                           "buffer_length",
                           buffer_length);

  switch (state_) {
    case NOT_INITIALIZED:
      // Lazily initialize with the first call to Write().
      Initialize(base::Bind(&FileStreamWriter::WriteAfterInitialized,
                            weak_ptr_factory_.GetWeakPtr(),
                            make_scoped_refptr(buffer),
                            buffer_length,
                            base::Bind(&FileStreamWriter::OnWriteCompleted,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       callback)),
                 base::Bind(&FileStreamWriter::OnWriteCompleted,
                            weak_ptr_factory_.GetWeakPtr(),
                            callback));
      break;

    case INITIALIZING:
      NOTREACHED();
      break;

    case INITIALIZED:
      WriteAfterInitialized(buffer,
                            buffer_length,
                            base::Bind(&FileStreamWriter::OnWriteCompleted,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       callback));
      break;

    case FAILED:
      NOTREACHED();
      break;
  }

  return net::ERR_IO_PENDING;
}

int FileStreamWriter::Cancel(const net::CompletionCallback& callback) {
  NOTIMPLEMENTED();
  return net::ERR_FAILED;
}

int FileStreamWriter::Flush(const net::CompletionCallback& callback) {
  if (state_ != INITIALIZED)
    return net::ERR_FAILED;

  return net::OK;
}

void FileStreamWriter::OnWriteFileCompleted(
    int buffer_length,
    const net::CompletionCallback& callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(INITIALIZED, state_);

  if (result != base::File::FILE_OK) {
    state_ = FAILED;
    callback.Run(net::FileErrorToNetError(result));
    return;
  }

  current_offset_ += buffer_length;
  callback.Run(buffer_length);
}

void FileStreamWriter::OnWriteCompleted(net::CompletionCallback callback,
                                        int result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(result);
  TRACE_EVENT_ASYNC_END0(
      "file_system_provider", "FileStreamWriter::Write", this);
}

void FileStreamWriter::WriteAfterInitialized(
    scoped_refptr<net::IOBuffer> buffer,
    int buffer_length,
    const net::CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(INITIALIZED, state_);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WriteFileOnUIThread,
                 file_system_,
                 file_handle_,
                 buffer,
                 current_offset_,
                 buffer_length,
                 base::Bind(&OnWriteFileCompletedOnUIThread,
                            base::Bind(&FileStreamWriter::OnWriteFileCompleted,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       buffer_length,
                                       callback))));
}

}  // namespace file_system_provider
}  // namespace chromeos
