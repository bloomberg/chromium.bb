// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_io_context.h"

#include "base/base64.h"
#include "base/containers/queue.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/lazy_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/common/blob_storage/blob_storage_constants.h"

#include <queue>

namespace content {

namespace {

base::SequencedTaskRunner* impl_task_runner() {
  constexpr base::TaskTraits kBlockingTraits = {base::MayBlock(),
                                                base::TaskPriority::BACKGROUND};
  static base::LazySequencedTaskRunner s_sequenced_task_unner =
      LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(kBlockingTraits);
  return s_sequenced_task_unner.Get().get();
}

using storage::BlobReader;

unsigned s_last_stream_handle = 0;

class TempFileStream : public DevToolsIOContext::RWStream {
 public:
  TempFileStream();

  void Read(off_t position, size_t max_size, ReadCallback callback) override;
  void Close(bool invoke_pending_callbacks) override {}
  void Append(std::unique_ptr<std::string> data) override;
  const std::string& handle() override { return handle_; }

 private:
  ~TempFileStream() override;

  void ReadOnFileSequence(off_t pos, size_t max_size, ReadCallback callback);
  void AppendOnFileSequence(std::unique_ptr<std::string> data);
  bool InitOnFileSequenceIfNeeded();

  const std::string handle_;
  base::File file_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  bool had_errors_;
  off_t last_read_pos_;

  DISALLOW_COPY_AND_ASSIGN(TempFileStream);
};

TempFileStream::TempFileStream()
    : DevToolsIOContext::RWStream(impl_task_runner()),
      handle_(base::UintToString(++s_last_stream_handle)),
      task_runner_(impl_task_runner()),
      had_errors_(false),
      last_read_pos_(0) {}

TempFileStream::~TempFileStream() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

bool TempFileStream::InitOnFileSequenceIfNeeded() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::AssertBlockingAllowed();
  if (had_errors_)
    return false;
  if (file_.IsValid())
    return true;
  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path)) {
    LOG(ERROR) << "Failed to create temporary file";
    had_errors_ = true;
    return false;
  }
  const unsigned flags = base::File::FLAG_OPEN_TRUNCATED |
                         base::File::FLAG_WRITE | base::File::FLAG_READ |
                         base::File::FLAG_DELETE_ON_CLOSE;
  file_.Initialize(temp_path, flags);
  if (!file_.IsValid()) {
    LOG(ERROR) << "Failed to open temporary file: " << temp_path.value()
        << ", " << base::File::ErrorToString(file_.error_details());
    had_errors_ = true;
    DeleteFile(temp_path, false);
    return false;
  }
  return true;
}

void TempFileStream::Read(off_t position,
                          size_t max_size,
                          ReadCallback callback) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TempFileStream::ReadOnFileSequence, this,
                                position, max_size, std::move(callback)));
}

void TempFileStream::Append(std::unique_ptr<std::string> data) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TempFileStream::AppendOnFileSequence, this,
                                std::move(data)));
}

void TempFileStream::ReadOnFileSequence(off_t position,
                                        size_t max_size,
                                        ReadCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  Status status = StatusFailure;
  std::unique_ptr<std::string> data;

  if (file_.IsValid()) {
    std::string buffer;
    buffer.resize(max_size);
    if (position < 0)
      position = last_read_pos_;
    int size_got = file_.ReadNoBestEffort(position, &*buffer.begin(), max_size);
    if (size_got < 0) {
      LOG(ERROR) << "Failed to read temporary file";
      had_errors_ = true;
      file_.Close();
    } else {
      // Provided client has requested sufficient large block, make their
      // life easier by not truncating in the middle of a UTF-8 character.
      if (size_got > 6 && !CBU8_IS_SINGLE(buffer[size_got - 1])) {
        base::TruncateUTF8ToByteSize(buffer, size_got, &buffer);
        size_got = buffer.size();
      } else {
        buffer.resize(size_got);
      }
      data.reset(new std::string(std::move(buffer)));
      status = size_got ? StatusSuccess : StatusEOF;
      last_read_pos_ = position + size_got;
    }
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(std::move(callback), std::move(data), false, status));
}

void TempFileStream::AppendOnFileSequence(std::unique_ptr<std::string> data) {
  if (!InitOnFileSequenceIfNeeded())
    return;
  int size_written = file_.WriteAtCurrentPos(&*data->begin(), data->length());
  if (size_written != static_cast<int>(data->length())) {
    LOG(ERROR) << "Failed to write temporary file";
    had_errors_ = true;
    file_.Close();
  }
}

class BlobStream : public DevToolsIOContext::ROStream {
 public:
  using OpenCallback = base::OnceCallback<void(bool)>;

  BlobStream()
      : DevToolsIOContext::ROStream(
            BrowserThread::GetTaskRunnerForThread(BrowserThread::IO)),
        last_read_pos_(0),
        failed_(false),
        is_binary_(false) {}

  void Open(scoped_refptr<ChromeBlobStorageContext> context,
            StoragePartition* partition,
            const std::string& handle,
            OpenCallback callback);

  void Read(off_t position, size_t max_size, ReadCallback callback) override;
  void Close(bool invoke_pending_callbacks) override;

 private:
  struct ReadRequest {
    off_t position;
    size_t max_size;
    ReadCallback callback;

    void Fail();

    ReadRequest(off_t position, size_t max_size, ReadCallback callback)
        : position(position),
          max_size(max_size),
          callback(std::move(callback)) {}
  };

  ~BlobStream() override = default;

  void OpenOnIO(scoped_refptr<ChromeBlobStorageContext> blob_context,
                scoped_refptr<storage::FileSystemContext> fs_context,
                const std::string& uuid,
                OpenCallback callback);
  void ReadOnIO(std::unique_ptr<ReadRequest> request);
  void CloseOnIO(bool invoke_pending_callbacks);

  void FailOnIO();
  void FailOnIO(OpenCallback callback) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::BindOnce(std::move(callback), false));
    FailOnIO();
  }

  void StartReadRequest();
  void CreateReader();
  void BeginRead();

  void OnReadComplete(int bytes_read);
  void OnBlobConstructionComplete(storage::BlobStatus status);
  void OnCalculateSizeComplete(int net_error);

  static bool IsTextMimeType(const std::string& mime_type);

  std::unique_ptr<storage::BlobDataHandle> blob_handle_;
  OpenCallback open_callback_;
  scoped_refptr<storage::FileSystemContext> fs_context_;
  std::unique_ptr<BlobReader> blob_reader_;
  base::queue<std::unique_ptr<ReadRequest>> pending_reads_;
  scoped_refptr<net::IOBufferWithSize> io_buf_;
  off_t last_read_pos_;
  bool failed_;
  bool is_binary_;

  DISALLOW_COPY_AND_ASSIGN(BlobStream);
};

void BlobStream::ReadRequest::Fail() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(std::move(callback), nullptr, false,
                                         ROStream::StatusFailure));
}

// static
bool BlobStream::IsTextMimeType(const std::string& mime_type) {
  static const char* kTextMIMETypePrefixes[] = {
      "text/", "application/x-javascript", "application/json",
      "application/xml"};
  for (size_t i = 0; i < arraysize(kTextMIMETypePrefixes); ++i) {
    if (base::StartsWith(mime_type, kTextMIMETypePrefixes[i],
                         base::CompareCase::INSENSITIVE_ASCII))
      return true;
  }
  return false;
}

void BlobStream::Open(scoped_refptr<ChromeBlobStorageContext> context,
                      StoragePartition* partition,
                      const std::string& handle,
                      OpenCallback callback) {
  scoped_refptr<storage::FileSystemContext> fs_context =
      partition->GetFileSystemContext();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&BlobStream::OpenOnIO, this, context, fs_context, handle,
                     std::move(callback)));
}

void BlobStream::Read(off_t position, size_t max_size, ReadCallback callback) {
  std::unique_ptr<ReadRequest> request(
      new ReadRequest(position, max_size, std::move(callback)));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&BlobStream::ReadOnIO, this, std::move(request)));
}

void BlobStream::Close(bool invoke_pending_callbacks) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&BlobStream::CloseOnIO, this, invoke_pending_callbacks));
}

void BlobStream::OpenOnIO(scoped_refptr<ChromeBlobStorageContext> blob_context,
                          scoped_refptr<storage::FileSystemContext> fs_context,
                          const std::string& uuid,
                          OpenCallback callback) {
  DCHECK(!blob_handle_);

  storage::BlobStorageContext* bsc = blob_context->context();
  blob_handle_ = bsc->GetBlobDataFromUUID(uuid);
  if (!blob_handle_) {
    LOG(ERROR) << "No blob with uuid: " << uuid;
    FailOnIO(std::move(callback));
    return;
  }
  is_binary_ = !IsTextMimeType(blob_handle_->content_type());
  open_callback_ = std::move(callback);
  fs_context_ = std::move(fs_context);
  blob_handle_->RunOnConstructionComplete(
      base::Bind(&BlobStream::OnBlobConstructionComplete, this));
}

void BlobStream::OnBlobConstructionComplete(storage::BlobStatus status) {
  DCHECK(!BlobStatusIsPending(status));
  if (BlobStatusIsError(status)) {
    LOG(ERROR) << "Blob building failed: " << static_cast<int>(status);
    FailOnIO(std::move(open_callback_));
    return;
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::BindOnce(std::move(open_callback_), true));
  if (!pending_reads_.empty())
    StartReadRequest();
}

void BlobStream::ReadOnIO(std::unique_ptr<ReadRequest> request) {
  if (failed_) {
    request->Fail();
    return;
  }
  pending_reads_.push(std::move(request));
  if (pending_reads_.size() > 1 || open_callback_)
    return;
  StartReadRequest();
}

void BlobStream::FailOnIO() {
  failed_ = true;
  while (!pending_reads_.empty()) {
    pending_reads_.front()->Fail();
    pending_reads_.pop();
  }
}

void BlobStream::CloseOnIO(bool invoke_pending_callbacks) {
  if (blob_reader_) {
    blob_reader_->Kill();
    blob_reader_.reset();
  }
  if (blob_handle_)
    blob_handle_.reset();
  if (invoke_pending_callbacks) {
    FailOnIO();
    return;
  }
  failed_ = true;
  pending_reads_ = base::queue<std::unique_ptr<ReadRequest>>();
  open_callback_ = OpenCallback();
}

void BlobStream::StartReadRequest() {
  DCHECK_GE(pending_reads_.size(), 1UL);
  DCHECK(blob_handle_);
  DCHECK(!failed_);

  ReadRequest& request = *pending_reads_.front();
  if (request.position < 0)
    request.position = last_read_pos_;
  if (request.position != last_read_pos_)
    blob_reader_.reset();
  if (!blob_reader_)
    CreateReader();
  else
    BeginRead();
}

void BlobStream::BeginRead() {
  DCHECK_GE(pending_reads_.size(), 1UL);
  ReadRequest& request = *pending_reads_.front();
  if (!io_buf_ || static_cast<size_t>(io_buf_->size()) < request.max_size)
    io_buf_ = new net::IOBufferWithSize(request.max_size);
  int bytes_read;
  BlobReader::Status status =
      blob_reader_->Read(io_buf_.get(), request.max_size, &bytes_read,
                         base::Bind(&BlobStream::OnReadComplete, this));
  if (status == BlobReader::Status::IO_PENDING)
    return;
  // This is for uniformity with the asynchronous case.
  if (status == BlobReader::Status::NET_ERROR) {
    bytes_read = blob_reader_->net_error();
    DCHECK_LT(0, bytes_read);
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&BlobStream::OnReadComplete, this, bytes_read));
}

void BlobStream::OnReadComplete(int bytes_read) {
  std::unique_ptr<ReadRequest> request = std::move(pending_reads_.front());
  pending_reads_.pop();

  Status status;
  std::unique_ptr<std::string> data(new std::string());
  bool base64_encoded = false;

  if (bytes_read < 0) {
    status = StatusFailure;
    LOG(ERROR) << "Error reading blob: " << net::ErrorToString(bytes_read);
  } else if (!bytes_read) {
    status = StatusEOF;
  } else {
    last_read_pos_ += bytes_read;
    status = blob_reader_->remaining_bytes() ? StatusSuccess : StatusEOF;
    if (is_binary_) {
      base64_encoded = true;
      Base64Encode(base::StringPiece(io_buf_->data(), bytes_read), data.get());
    } else {
      // TODO(caseq): truncate at UTF8 boundary.
      *data = std::string(io_buf_->data(), bytes_read);
    }
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(std::move(request->callback), std::move(data),
                     base64_encoded, status));
  if (!pending_reads_.empty())
    StartReadRequest();
}

void BlobStream::CreateReader() {
  DCHECK(!blob_reader_);
  blob_reader_ = blob_handle_->CreateReader(fs_context_.get());
  BlobReader::Status status = blob_reader_->CalculateSize(
      base::Bind(&BlobStream::OnCalculateSizeComplete, this));
  if (status != BlobReader::Status::IO_PENDING) {
    OnCalculateSizeComplete(status == BlobReader::Status::NET_ERROR
                                ? blob_reader_->net_error()
                                : net::OK);
  }
}

void BlobStream::OnCalculateSizeComplete(int net_error) {
  if (net_error != net::OK) {
    FailOnIO();
    return;
  }
  off_t seek_to = pending_reads_.front()->position;
  if (seek_to != 0UL) {
    if (seek_to >= static_cast<off_t>(blob_reader_->total_size())) {
      OnReadComplete(0);
      return;
    }
    BlobReader::Status status = blob_reader_->SetReadRange(
        seek_to, blob_reader_->total_size() - seek_to);
    if (status != BlobReader::Status::DONE) {
      FailOnIO();
      return;
    }
  }
  BeginRead();
}

}  // namespace

DevToolsIOContext::ROStream::ROStream(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : RefCountedDeleteOnSequence<DevToolsIOContext::ROStream>(
          std::move(task_runner)) {}

DevToolsIOContext::ROStream::~ROStream() = default;

DevToolsIOContext::RWStream::RWStream(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : DevToolsIOContext::ROStream(std::move(task_runner)) {}

DevToolsIOContext::RWStream::~RWStream() = default;

DevToolsIOContext::DevToolsIOContext() : weak_factory_(this) {}

DevToolsIOContext::~DevToolsIOContext() {
  DiscardAllStreams();
}

scoped_refptr<DevToolsIOContext::RWStream>
DevToolsIOContext::CreateTempFileBackedStream() {
  scoped_refptr<TempFileStream> result = new TempFileStream();
  bool inserted =
      streams_.insert(std::make_pair(result->handle(), result)).second;
  DCHECK(inserted);
  return result;
}

scoped_refptr<DevToolsIOContext::ROStream> DevToolsIOContext::GetByHandle(
    const std::string& handle) {
  StreamsMap::const_iterator it = streams_.find(handle);
  return it == streams_.end() ? scoped_refptr<ROStream>() : it->second;
}

scoped_refptr<DevToolsIOContext::ROStream> DevToolsIOContext::OpenBlob(
    ChromeBlobStorageContext* context,
    StoragePartition* partition,
    const std::string& handle,
    const std::string& uuid) {
  scoped_refptr<BlobStream> result = new BlobStream();
  bool inserted = streams_.insert(std::make_pair(handle, result)).second;

  result->Open(context, partition, uuid,
               base::BindOnce(&DevToolsIOContext::OnBlobOpenComplete,
                              weak_factory_.GetWeakPtr(), handle));
  DCHECK(inserted);
  return std::move(result);
}

void DevToolsIOContext::OnBlobOpenComplete(const std::string& handle,
                                           bool success) {
  if (!success)
    Close(handle);
}

bool DevToolsIOContext::Close(const std::string& handle) {
  StreamsMap::iterator it = streams_.find(handle);
  if (it == streams_.end())
    return false;
  it->second->Close(false);
  streams_.erase(it);
  return true;
}

void DevToolsIOContext::DiscardAllStreams() {
  for (auto& entry : streams_)
    entry.second->Close(true);
  return streams_.clear();
}

}  // namespace content
