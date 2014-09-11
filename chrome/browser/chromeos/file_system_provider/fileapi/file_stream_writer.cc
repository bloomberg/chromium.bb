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

}  // namespace

class FileStreamWriter::OperationRunner
    : public base::RefCountedThreadSafe<FileStreamWriter::OperationRunner> {
 public:
  OperationRunner() : file_handle_(-1) {}

  // Opens a file for writing and calls the completion callback. Must be called
  // on UI thread.
  void OpenFileOnUIThread(
      const storage::FileSystemURL& url,
      const storage::AsyncFileUtil::StatusCallback& callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    util::FileSystemURLParser parser(url);
    if (!parser.Parse()) {
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(callback, base::File::FILE_ERROR_SECURITY));
      return;
    }

    file_system_ = parser.file_system()->GetWeakPtr();
    abort_callback_ = parser.file_system()->OpenFile(
        parser.file_path(),
        ProvidedFileSystemInterface::OPEN_FILE_MODE_WRITE,
        base::Bind(
            &OperationRunner::OnOpenFileCompletedOnUIThread, this, callback));
  }

  // Closes a file. Ignores result, since outlives the caller. Must be called on
  // UI thread.
  void CloseFileOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (file_system_.get() && file_handle_ != -1) {
      // Closing a file must not be aborted, since we could end up on files
      // which are never closed.
      file_system_->CloseFile(file_handle_, base::Bind(&EmptyStatusCallback));
    }
  }

  // Requests writing bytes to the file. In case of either success or a failure
  // |callback| is executed. Must be called on UI thread.
  void WriteFileOnUIThread(
      scoped_refptr<net::IOBuffer> buffer,
      int64 offset,
      int length,
      const storage::AsyncFileUtil::StatusCallback& callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // If the file system got unmounted, then abort the writing operation.
    if (!file_system_.get()) {
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(callback, base::File::FILE_ERROR_ABORT));
      return;
    }

    abort_callback_ = file_system_->WriteFile(
        file_handle_,
        buffer.get(),
        offset,
        length,
        base::Bind(
            &OperationRunner::OnWriteFileCompletedOnUIThread, this, callback));
  }

  // Aborts the most recent operation (if exists), and calls the callback.
  void AbortOnUIThread(const storage::AsyncFileUtil::StatusCallback& callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (abort_callback_.is_null()) {
      // No operation to be cancelled. At most a callback call, which will be
      // discarded.
      BrowserThread::PostTask(BrowserThread::IO,
                              FROM_HERE,
                              base::Bind(callback, base::File::FILE_OK));
      return;
    }

    const ProvidedFileSystemInterface::AbortCallback abort_callback =
        abort_callback_;
    abort_callback_ = ProvidedFileSystemInterface::AbortCallback();
    abort_callback.Run(base::Bind(
        &OperationRunner::OnAbortCompletedOnUIThread, this, callback));
  }

 private:
  friend class base::RefCountedThreadSafe<OperationRunner>;

  virtual ~OperationRunner() {}

  // Remembers a file handle for further operations and forwards the result to
  // the IO thread.
  void OnOpenFileCompletedOnUIThread(
      const storage::AsyncFileUtil::StatusCallback& callback,
      int file_handle,
      base::File::Error result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    file_handle_ = file_handle;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::Bind(callback, result));
  }

  // Forwards a response of writing to a file to the IO thread.
  void OnWriteFileCompletedOnUIThread(
      const storage::AsyncFileUtil::StatusCallback& callback,
      base::File::Error result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::Bind(callback, result));
  }

  // Forwards a response of aborting an operation to the IO thread.
  void OnAbortCompletedOnUIThread(
      const storage::AsyncFileUtil::StatusCallback& callback,
      base::File::Error result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::Bind(callback, result));
  }

  ProvidedFileSystemInterface::AbortCallback abort_callback_;
  base::WeakPtr<ProvidedFileSystemInterface> file_system_;
  int file_handle_;

  DISALLOW_COPY_AND_ASSIGN(OperationRunner);
};

FileStreamWriter::FileStreamWriter(const storage::FileSystemURL& url,
                                   int64 initial_offset)
    : url_(url),
      current_offset_(initial_offset),
      runner_(new OperationRunner),
      state_(NOT_INITIALIZED),
      weak_ptr_factory_(this) {
}

FileStreamWriter::~FileStreamWriter() {
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationRunner::CloseFileOnUIThread, runner_));
}

void FileStreamWriter::Initialize(
    const base::Closure& pending_closure,
    const net::CompletionCallback& error_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(NOT_INITIALIZED, state_);
  state_ = INITIALIZING;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationRunner::OpenFileOnUIThread,
                 runner_,
                 url_,
                 base::Bind(&FileStreamWriter::OnOpenFileCompleted,
                            weak_ptr_factory_.GetWeakPtr(),
                            pending_closure,
                            error_callback)));
}

void FileStreamWriter::OnOpenFileCompleted(
    const base::Closure& pending_closure,
    const net::CompletionCallback& error_callback,
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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationRunner::AbortOnUIThread,
                 runner_,
                 base::Bind(&FileStreamWriter::OnAbortCompleted,
                            weak_ptr_factory_.GetWeakPtr(),
                            callback)));
  return net::ERR_IO_PENDING;
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

void FileStreamWriter::OnAbortCompleted(const net::CompletionCallback& callback,
                                        base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (result != base::File::FILE_OK)
    state_ = FAILED;

  callback.Run(net::FileErrorToNetError(result));
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
      base::Bind(&OperationRunner::WriteFileOnUIThread,
                 runner_,
                 buffer,
                 current_offset_,
                 buffer_length,
                 base::Bind(&FileStreamWriter::OnWriteFileCompleted,
                            weak_ptr_factory_.GetWeakPtr(),
                            buffer_length,
                            callback)));
}

}  // namespace file_system_provider
}  // namespace chromeos
