// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/file_stream_reader.h"

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/chromeos/file_system_provider/abort_callback.h"
#include "chrome/browser/chromeos/file_system_provider/fileapi/provider_async_file_util.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/scoped_file_opener.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace chromeos {
namespace file_system_provider {

// Converts net::CompletionCallback to net::Int64CompletionCallback.
void Int64ToIntCompletionCallback(net::CompletionCallback callback,
                                  int64_t result) {
  callback.Run(static_cast<int>(result));
}

class FileStreamReader::OperationRunner
    : public base::RefCountedThreadSafe<
          FileStreamReader::OperationRunner,
          content::BrowserThread::DeleteOnUIThread> {
 public:
  OperationRunner() : file_handle_(0) {}

  // Opens a file for reading and calls the completion callback. Must be called
  // on UI thread.
  void OpenFileOnUIThread(
      const storage::FileSystemURL& url,
      const storage::AsyncFileUtil::StatusCallback& callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(abort_callback_.is_null());

    util::FileSystemURLParser parser(url);
    if (!parser.Parse()) {
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(callback, base::File::FILE_ERROR_SECURITY));
      return;
    }

    file_system_ = parser.file_system()->GetWeakPtr();
    file_path_ = parser.file_path();
    file_opener_.reset(new ScopedFileOpener(
        parser.file_system(), parser.file_path(), OPEN_FILE_MODE_READ,
        base::Bind(&OperationRunner::OnOpenFileCompletedOnUIThread, this,
                   callback)));
  }

  // Requests reading contents of a file. |callback| will always run eventually.
  // It can be called many times, until |has_more| is set to false. This
  // function guarantees that it will succeed only if the file has not been
  // changed while reading. Must be called on UI thread.
  void ReadFileOnUIThread(
      scoped_refptr<net::IOBuffer> buffer,
      int64_t offset,
      int length,
      const ProvidedFileSystemInterface::ReadChunkReceivedCallback& callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(abort_callback_.is_null());

    // If the file system got unmounted, then abort the reading operation.
    if (!file_system_.get()) {
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(
              callback, 0, false /* has_more */, base::File::FILE_ERROR_ABORT));
      return;
    }

    abort_callback_ = file_system_->ReadFile(
        file_handle_,
        buffer.get(),
        offset,
        length,
        base::Bind(
            &OperationRunner::OnReadFileCompletedOnUIThread, this, callback));
  }

  // Requests metadata of a file. |callback| will always run eventually.
  // Must be called on UI thread.
  void GetMetadataOnUIThread(
      const ProvidedFileSystemInterface::GetMetadataCallback& callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(abort_callback_.is_null());

    // If the file system got unmounted, then abort the get length operation.
    if (!file_system_.get()) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(callback,
                     base::Passed(base::WrapUnique<EntryMetadata>(NULL)),
                     base::File::FILE_ERROR_ABORT));
      return;
    }

    abort_callback_ = file_system_->GetMetadata(
        file_path_,
        ProvidedFileSystemInterface::METADATA_FIELD_SIZE |
            ProvidedFileSystemInterface::METADATA_FIELD_MODIFICATION_TIME,
        base::Bind(&OperationRunner::OnGetMetadataCompletedOnUIThread, this,
                   callback));
  }

  // Aborts the most recent operation (if exists) and closes a file if opened.
  // The runner must not be used anymore after calling this method.
  void CloseRunnerOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (!abort_callback_.is_null()) {
      const AbortCallback last_abort_callback = abort_callback_;
      abort_callback_ = AbortCallback();
      last_abort_callback.Run();
    }

    // Close the file (if opened).
    file_opener_.reset();
  }

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<OperationRunner>;

  virtual ~OperationRunner() {}

  // Remembers a file handle for further operations and forwards the result to
  // the IO thread.
  void OnOpenFileCompletedOnUIThread(
      const storage::AsyncFileUtil::StatusCallback& callback,
      int file_handle,
      base::File::Error result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    abort_callback_ = AbortCallback();

    if (result == base::File::FILE_OK)
      file_handle_ = file_handle;

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::Bind(callback, result));
  }

  // Forwards a metadata to the IO thread.
  void OnGetMetadataCompletedOnUIThread(
      const ProvidedFileSystemInterface::GetMetadataCallback& callback,
      std::unique_ptr<EntryMetadata> metadata,
      base::File::Error result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    abort_callback_ = AbortCallback();

    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(callback, base::Passed(&metadata), result));
  }

  // Forwards a response of reading from a file to the IO thread.
  void OnReadFileCompletedOnUIThread(
      const ProvidedFileSystemInterface::ReadChunkReceivedCallback&
          chunk_received_callback,
      int chunk_length,
      bool has_more,
      base::File::Error result) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!has_more)
      abort_callback_ = AbortCallback();

    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(chunk_received_callback, chunk_length, has_more, result));
  }

  AbortCallback abort_callback_;
  base::WeakPtr<ProvidedFileSystemInterface> file_system_;
  base::FilePath file_path_;
  std::unique_ptr<ScopedFileOpener> file_opener_;
  int file_handle_;

  DISALLOW_COPY_AND_ASSIGN(OperationRunner);
};

FileStreamReader::FileStreamReader(storage::FileSystemContext* context,
                                   const storage::FileSystemURL& url,
                                   int64_t initial_offset,
                                   const base::Time& expected_modification_time)
    : url_(url),
      current_offset_(initial_offset),
      current_length_(0),
      expected_modification_time_(expected_modification_time),
      runner_(new OperationRunner),
      state_(NOT_INITIALIZED),
      weak_ptr_factory_(this) {}

FileStreamReader::~FileStreamReader() {
  // FileStreamReader doesn't have a Cancel() method like in FileStreamWriter.
  // Therefore, aborting and/or closing an opened file is done from the
  // destructor.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OperationRunner::CloseRunnerOnUIThread, runner_));

  // If a read is in progress, mark it as completed.
  TRACE_EVENT_ASYNC_END0("file_system_provider", "FileStreamReader::Read",
                         this);
}

void FileStreamReader::Initialize(
    const base::Closure& pending_closure,
    const net::Int64CompletionCallback& error_callback) {
  DCHECK_EQ(NOT_INITIALIZED, state_);
  state_ = INITIALIZING;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationRunner::OpenFileOnUIThread,
                 runner_,
                 url_,
                 base::Bind(&FileStreamReader::OnOpenFileCompleted,
                            weak_ptr_factory_.GetWeakPtr(),
                            pending_closure,
                            error_callback)));
}

void FileStreamReader::OnOpenFileCompleted(
    const base::Closure& pending_closure,
    const net::Int64CompletionCallback& error_callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(INITIALIZING, state_);

  // In case of an error, return immediately using the |error_callback| of the
  // Read() or GetLength() pending request.
  if (result != base::File::FILE_OK) {
    state_ = FAILED;
    error_callback.Run(net::FileErrorToNetError(result));
    return;
  }

  DCHECK_EQ(base::File::FILE_OK, result);

  // Verify the last modification time.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationRunner::GetMetadataOnUIThread,
                 runner_,
                 base::Bind(&FileStreamReader::OnInitializeCompleted,
                            weak_ptr_factory_.GetWeakPtr(),
                            pending_closure,
                            error_callback)));
}

void FileStreamReader::OnInitializeCompleted(
    const base::Closure& pending_closure,
    const net::Int64CompletionCallback& error_callback,
    std::unique_ptr<EntryMetadata> metadata,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(INITIALIZING, state_);

  // In case of an error, abort.
  if (result != base::File::FILE_OK) {
    state_ = FAILED;
    error_callback.Run(net::FileErrorToNetError(result));
    return;
  }

  // If the file modification time has changed, then abort. Note, that the file
  // may be changed without affecting the modification time.
  DCHECK(metadata.get());
  if (!expected_modification_time_.is_null() &&
      *metadata->modification_time != expected_modification_time_) {
    state_ = FAILED;
    error_callback.Run(net::ERR_UPLOAD_FILE_CHANGED);
    return;
  }

  DCHECK_EQ(base::File::FILE_OK, result);
  state_ = INITIALIZED;

  // Run the task waiting for the initialization to be completed.
  pending_closure.Run();
}

int FileStreamReader::Read(net::IOBuffer* buffer,
                           int buffer_length,
                           const net::CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  TRACE_EVENT_ASYNC_BEGIN1("file_system_provider",
                           "FileStreamReader::Read",
                           this,
                           "buffer_length",
                           buffer_length);

  switch (state_) {
    case NOT_INITIALIZED:
      // Lazily initialize with the first call to Read().
    Initialize(base::Bind(&FileStreamReader::ReadAfterInitialized,
                          weak_ptr_factory_.GetWeakPtr(),
                          make_scoped_refptr(buffer),
                          buffer_length,
                          base::Bind(&FileStreamReader::OnReadCompleted,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     callback)),
               base::Bind(&Int64ToIntCompletionCallback,
                          base::Bind(&FileStreamReader::OnReadCompleted,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     callback)));
    break;

    case INITIALIZING:
      NOTREACHED();
      break;

    case INITIALIZED:
      ReadAfterInitialized(buffer,
                           buffer_length,
                           base::Bind(&FileStreamReader::OnReadCompleted,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      callback));
      break;

    case FAILED:
      NOTREACHED();
      break;
  }

  return net::ERR_IO_PENDING;
}

void FileStreamReader::OnReadCompleted(net::CompletionCallback callback,
                                       int result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(static_cast<int>(result));
  TRACE_EVENT_ASYNC_END0(
      "file_system_provider", "FileStreamReader::Read", this);
}

int64_t FileStreamReader::GetLength(
    const net::Int64CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  switch (state_) {
    case NOT_INITIALIZED:
      // Lazily initialize with the first call to GetLength().
    Initialize(base::Bind(&FileStreamReader::GetLengthAfterInitialized,
                          weak_ptr_factory_.GetWeakPtr(),
                          callback),
               callback);
    break;

    case INITIALIZING:
      NOTREACHED();
      break;

    case INITIALIZED:
      GetLengthAfterInitialized(callback);
      break;

    case FAILED:
      NOTREACHED();
      break;
  }

  return net::ERR_IO_PENDING;
}

void FileStreamReader::ReadAfterInitialized(
    scoped_refptr<net::IOBuffer> buffer,
    int buffer_length,
    const net::CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(INITIALIZED, state_);

  current_length_ = 0;
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationRunner::ReadFileOnUIThread,
                 runner_,
                 buffer,
                 current_offset_,
                 buffer_length,
                 base::Bind(&FileStreamReader::OnReadChunkReceived,
                            weak_ptr_factory_.GetWeakPtr(),
                            callback)));
}

void FileStreamReader::GetLengthAfterInitialized(
    const net::Int64CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(INITIALIZED, state_);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &OperationRunner::GetMetadataOnUIThread,
          runner_,
          base::Bind(&FileStreamReader::OnGetMetadataForGetLengthReceived,
                     weak_ptr_factory_.GetWeakPtr(),
                     callback)));
}

void FileStreamReader::OnReadChunkReceived(
    const net::CompletionCallback& callback,
    int chunk_length,
    bool has_more,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(INITIALIZED, state_);

  current_length_ += chunk_length;

  // If this is the last chunk with a success, then finalize.
  if (!has_more && result == base::File::FILE_OK) {
    current_offset_ += current_length_;
    callback.Run(current_length_);
    return;
  }

  // In case of an error, abort.
  if (result != base::File::FILE_OK) {
    DCHECK(!has_more);
    state_ = FAILED;
    callback.Run(net::FileErrorToNetError(result));
    return;
  }

  // More data is about to come, so do not call the callback yet.
  DCHECK(has_more);
}

void FileStreamReader::OnGetMetadataForGetLengthReceived(
    const net::Int64CompletionCallback& callback,
    std::unique_ptr<EntryMetadata> metadata,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(INITIALIZED, state_);

  // In case of an error, abort.
  if (result != base::File::FILE_OK) {
    state_ = FAILED;
    callback.Run(net::FileErrorToNetError(result));
    return;
  }

  // If the file modification time has changed, then abort. Note, that the file
  // may be changed without affecting the modification time.
  DCHECK(metadata.get());
  if (!expected_modification_time_.is_null() &&
      *metadata->modification_time != expected_modification_time_) {
    callback.Run(net::ERR_UPLOAD_FILE_CHANGED);
    return;
  }

  DCHECK_EQ(base::File::FILE_OK, result);
  callback.Run(*metadata->size);
}

}  // namespace file_system_provider
}  // namespace chromeos
