// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_file_stream_reader.h"

#include <algorithm>
#include <cstring>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_reader.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/google_apis/task_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace drive {
namespace {

// Converts FileError code to net::Error code.
int FileErrorToNetError(FileError error) {
  return net::PlatformFileErrorToNetError(FileErrorToPlatformError(error));
}

}  // namespace

namespace internal {
namespace {

// Copies the content in |pending_data| into |buffer| at most
// |buffer_length| bytes, and erases the copied data from
// |pending_data|. Returns the number of copied bytes.
int ReadInternal(ScopedVector<std::string>* pending_data,
                 net::IOBuffer* buffer, int buffer_length) {
  size_t index = 0;
  int offset = 0;
  for (; index < pending_data->size() && offset < buffer_length; ++index) {
    const std::string& chunk = *(*pending_data)[index];
    DCHECK(!chunk.empty());

    size_t bytes_to_read = std::min(
        chunk.size(), static_cast<size_t>(buffer_length - offset));
    std::memmove(buffer->data() + offset, chunk.data(), bytes_to_read);
    offset += bytes_to_read;
    if (bytes_to_read < chunk.size()) {
      // The chunk still has some remaining data.
      // So remove leading (copied) bytes, and quit the loop so that
      // the remaining data won't be deleted in the following erase().
      (*pending_data)[index]->erase(0, bytes_to_read);
      break;
    }
  }

  // Consume the copied data.
  pending_data->erase(pending_data->begin(), pending_data->begin() + index);

  return offset;
}

}  // namespace

LocalReaderProxy::LocalReaderProxy(scoped_ptr<util::FileReader> file_reader,
                                   int64 length)
    : file_reader_(file_reader.Pass()),
      remaining_length_(length),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(file_reader_);
}

LocalReaderProxy::~LocalReaderProxy() {
}

int LocalReaderProxy::Read(net::IOBuffer* buffer, int buffer_length,
                           const net::CompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(file_reader_);

  if (buffer_length > remaining_length_) {
    // Here, narrowing is safe.
    buffer_length = static_cast<int>(remaining_length_);
  }

  file_reader_->Read(buffer, buffer_length,
                     base::Bind(&LocalReaderProxy::OnReadCompleted,
                                weak_ptr_factory_.GetWeakPtr(), callback));
  return net::ERR_IO_PENDING;
}

void LocalReaderProxy::OnGetContent(scoped_ptr<std::string> data) {
  // This method should never be called, because no data should be received
  // from the network during the reading of local-cache file.
  NOTREACHED();
}

void LocalReaderProxy::OnCompleted(FileError error) {
  // If this method is called, no network error should be happened.
  DCHECK_EQ(FILE_ERROR_OK, error);
}

void LocalReaderProxy::OnReadCompleted(const net::CompletionCallback& callback,
                                       int read_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(file_reader_);

  if (read_result >= 0) {
    // |read_result| bytes data is read.
    DCHECK_LE(read_result, remaining_length_);
    remaining_length_ -= read_result;
  } else {
    // An error occurs. Close the FileReader.
    file_reader_.reset();
  }
  callback.Run(read_result);
}

NetworkReaderProxy::NetworkReaderProxy(
    int64 offset,
    int64 content_length,
    const base::Closure& job_canceller)
    : remaining_offset_(offset),
      remaining_content_length_(content_length),
      error_code_(net::OK),
      buffer_length_(0),
      job_canceller_(job_canceller) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

NetworkReaderProxy::~NetworkReaderProxy() {
  if (!job_canceller_.is_null()) {
    job_canceller_.Run();
  }
}

int NetworkReaderProxy::Read(net::IOBuffer* buffer, int buffer_length,
                             const net::CompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Check if there is no pending Read operation.
  DCHECK(!buffer_);
  DCHECK_EQ(buffer_length_, 0);
  DCHECK(callback_.is_null());
  // Validate the arguments.
  DCHECK(buffer);
  DCHECK_GT(buffer_length, 0);
  DCHECK(!callback.is_null());

  if (error_code_ != net::OK) {
    // An error is already found. Return it immediately.
    return error_code_;
  }

  if (remaining_content_length_ == 0) {
    // If no more data, return immediately.
    return 0;
  }

  if (buffer_length > remaining_content_length_) {
    // Here, narrowing cast should be safe.
    buffer_length = static_cast<int>(remaining_content_length_);
  }

  if (pending_data_.empty()) {
    // No data is available. Keep the arguments, and return pending status.
    buffer_ = buffer;
    buffer_length_ = buffer_length;
    callback_ = callback;
    return net::ERR_IO_PENDING;
  }

  int result = ReadInternal(&pending_data_, buffer, buffer_length);
  remaining_content_length_ -= result;
  DCHECK_GE(remaining_content_length_, 0);
  return result;
}

void NetworkReaderProxy::OnGetContent(scoped_ptr<std::string> data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(data && !data->empty());

  if (remaining_offset_ >= static_cast<int64>(data->length())) {
    // Skip unneeded leading data.
    remaining_offset_ -= data->length();
    return;
  }

  if (remaining_offset_ > 0) {
    // Erase unnecessary leading bytes.
    data->erase(0, static_cast<size_t>(remaining_offset_));
    remaining_offset_ = 0;
  }

  pending_data_.push_back(data.release());
  if (!buffer_) {
    // No pending Read operation.
    return;
  }

  int result = ReadInternal(&pending_data_, buffer_.get(), buffer_length_);
  remaining_content_length_ -= result;
  DCHECK_GE(remaining_content_length_, 0);

  buffer_ = NULL;
  buffer_length_ = 0;
  DCHECK(!callback_.is_null());
  base::ResetAndReturn(&callback_).Run(result);
}

void NetworkReaderProxy::OnCompleted(FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // The downloading is completed, so we do not need to cancel the job
  // in the destructor.
  job_canceller_.Reset();

  if (error == FILE_ERROR_OK) {
    return;
  }

  error_code_ = FileErrorToNetError(error);
  pending_data_.clear();

  if (callback_.is_null()) {
    // No pending Read operation.
    return;
  }

  buffer_ = NULL;
  buffer_length_ = 0;
  base::ResetAndReturn(&callback_).Run(error_code_);
}

}  // namespace internal

namespace {

// Calls FileSystemInterface::GetFileContentByPath if the file system
// is available. If not, the |completion_callback| is invoked with
// FILE_ERROR_FAILED.
void GetFileContentByPathOnUIThread(
    const DriveFileStreamReader::FileSystemGetter& file_system_getter,
    const base::FilePath& drive_file_path,
    const GetFileContentInitializedCallback& initialized_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const FileOperationCallback& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FileSystemInterface* file_system = file_system_getter.Run();
  if (!file_system) {
    completion_callback.Run(FILE_ERROR_FAILED);
    return;
  }

  file_system->GetFileContentByPath(drive_file_path,
                                    initialized_callback,
                                    get_content_callback,
                                    completion_callback);
}

// Helper to run FileSystemInterface::GetFileContentByPath on UI thread.
void GetFileContentByPath(
    const DriveFileStreamReader::FileSystemGetter& file_system_getter,
    const base::FilePath& drive_file_path,
    const GetFileContentInitializedCallback& initialized_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const FileOperationCallback& completion_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GetFileContentByPathOnUIThread,
                 file_system_getter,
                 drive_file_path,
                 google_apis::CreateRelayCallback(initialized_callback),
                 google_apis::CreateRelayCallback(get_content_callback),
                 google_apis::CreateRelayCallback(completion_callback)));
}

}  // namespace

struct DriveFileStreamReader::Range {
  Range(uint64 offset, uint64 length) : offset(offset), length(length) {}

  uint64 offset;
  uint64 length;
};

DriveFileStreamReader::DriveFileStreamReader(
    const FileSystemGetter& file_system_getter,
    base::SequencedTaskRunner* file_task_runner)
    : file_system_getter_(file_system_getter),
      file_task_runner_(file_task_runner),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

DriveFileStreamReader::~DriveFileStreamReader() {
}

bool DriveFileStreamReader::IsInitialized() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return reader_proxy_.get() != NULL;
}

void DriveFileStreamReader::Initialize(
    const base::FilePath& drive_file_path,
    uint64 range_offset,
    uint64 range_length,
    const InitializeCompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!callback.is_null());

  GetFileContentByPath(
      file_system_getter_,
      drive_file_path,
      base::Bind(&DriveFileStreamReader
                     ::InitializeAfterGetFileContentByPathInitialized,
                 weak_ptr_factory_.GetWeakPtr(),
                 Range(range_offset, range_length),
                 callback),
      base::Bind(&DriveFileStreamReader::OnGetContent,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DriveFileStreamReader::OnGetFileContentByPathCompletion,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

int DriveFileStreamReader::Read(net::IOBuffer* buffer, int buffer_length,
                                const net::CompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(reader_proxy_);
  DCHECK(buffer);
  DCHECK(!callback.is_null());
  return reader_proxy_->Read(buffer, buffer_length, callback);
}

void DriveFileStreamReader::InitializeAfterGetFileContentByPathInitialized(
    const Range& range,
    const InitializeCompletionCallback& callback,
    FileError error,
    scoped_ptr<ResourceEntry> entry,
    const base::FilePath& local_cache_file_path,
    const base::Closure& ui_cancel_download_closure) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error != FILE_ERROR_OK) {
    callback.Run(FileErrorToNetError(error), scoped_ptr<ResourceEntry>());
    return;
  }
  DCHECK(entry);

  // Clamp the reading range.
  uint64 file_size = entry->file_info().size();
  uint64 range_offset = std::min(range.offset, file_size);
  uint64 range_length = std::min(range.length, file_size - range_offset);

  if (local_cache_file_path.empty()) {
    // The file is not cached, and being downloaded.
    DCHECK(!ui_cancel_download_closure.is_null());
    reader_proxy_.reset(
        new internal::NetworkReaderProxy(
            range_offset, range_length,
            base::Bind(&google_apis::RunTaskOnUIThread,
                       ui_cancel_download_closure)));
    callback.Run(net::OK, entry.Pass());
    return;
  }

  // Otherwise, open the stream for file.
  scoped_ptr<util::FileReader> file_reader(
      new util::FileReader(file_task_runner_));
  util::FileReader* file_reader_ptr = file_reader.get();
  file_reader_ptr->Open(
      local_cache_file_path,
      range_offset,
      base::Bind(
          &DriveFileStreamReader::InitializeAfterLocalFileOpen,
          weak_ptr_factory_.GetWeakPtr(),
          range_length,
          callback,
          base::Passed(&entry),
          base::Passed(&file_reader)));
}

void DriveFileStreamReader::InitializeAfterLocalFileOpen(
    uint64 length,
    const InitializeCompletionCallback& callback,
    scoped_ptr<ResourceEntry> entry,
    scoped_ptr<util::FileReader> file_reader,
    int open_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (open_result != net::OK) {
    callback.Run(net::ERR_FAILED, scoped_ptr<ResourceEntry>());
    return;
  }

  reader_proxy_.reset(
      new internal::LocalReaderProxy(file_reader.Pass(), length));
  callback.Run(net::OK, entry.Pass());
}

void DriveFileStreamReader::OnGetContent(google_apis::GDataErrorCode error_code,
                                         scoped_ptr<std::string> data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(reader_proxy_);
  reader_proxy_->OnGetContent(data.Pass());
}

void DriveFileStreamReader::OnGetFileContentByPathCompletion(
    const InitializeCompletionCallback& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (reader_proxy_) {
    // If the proxy object available, send the error to it.
    reader_proxy_->OnCompleted(error);
  } else {
    // Here the proxy object is not yet available.
    // There are two cases. 1) Some error happens during the initialization.
    // 2) the cache file is found, but the proxy object is not *yet*
    // initialized because the file is being opened.
    // We are interested in 1) only. The callback for 2) will be called
    // after opening the file is completed.
    // Note: due to the same reason, LocalReaderProxy::OnCompleted may
    // or may not be called. This is timing issue, and it is difficult to avoid
    // unfortunately.
    if (error != FILE_ERROR_OK) {
      callback.Run(FileErrorToNetError(error), scoped_ptr<ResourceEntry>());
    }
  }
}

}  // namespace drive
