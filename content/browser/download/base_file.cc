// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/base_file.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_net_log_parameters.h"
#include "content/browser/download/download_stats.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "crypto/secure_hash.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

namespace content {

// This will initialize the entire array to zero.
const unsigned char BaseFile::kEmptySha256Hash[] = { 0 };

BaseFile::BaseFile(const FilePath& full_path,
                   const GURL& source_url,
                   const GURL& referrer_url,
                   int64 received_bytes,
                   bool calculate_hash,
                   const std::string& hash_state_bytes,
                   scoped_ptr<net::FileStream> file_stream,
                   const net::BoundNetLog& bound_net_log)
    : full_path_(full_path),
      source_url_(source_url),
      referrer_url_(referrer_url),
      file_stream_(file_stream.Pass()),
      bytes_so_far_(received_bytes),
      start_tick_(base::TimeTicks::Now()),
      calculate_hash_(calculate_hash),
      detached_(false),
      bound_net_log_(bound_net_log) {
  memcpy(sha256_hash_, kEmptySha256Hash, kSha256HashLen);
  if (calculate_hash_) {
    secure_hash_.reset(crypto::SecureHash::Create(crypto::SecureHash::SHA256));
    if ((bytes_so_far_ > 0) &&  // Not starting at the beginning.
        (!IsEmptyHash(hash_state_bytes))) {
      Pickle hash_state(hash_state_bytes.c_str(), hash_state_bytes.size());
      PickleIterator data_iterator(hash_state);
      secure_hash_->Deserialize(&data_iterator);
    }
  }
}

BaseFile::~BaseFile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (detached_)
    Close();
  else
    Cancel();  // Will delete the file.
}

DownloadInterruptReason BaseFile::Initialize(
    const FilePath& default_directory) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!detached_);

  if (file_stream_.get()) {
    file_stream_->SetBoundNetLogSource(bound_net_log_);
    file_stream_->EnableErrorStatistics();
  }

  if (full_path_.empty()) {
    FilePath initial_directory(default_directory);
    FilePath temp_file;
    if (initial_directory.empty()) {
      initial_directory =
          GetContentClient()->browser()->GetDefaultDownloadDirectory();
    }
    // |initial_directory| can still be empty if ContentBrowserClient returned
    // an empty path for the downloads directory.
    if ((initial_directory.empty() ||
         !file_util::CreateTemporaryFileInDir(initial_directory, &temp_file)) &&
        !file_util::CreateTemporaryFile(&temp_file)) {
      return LogInterruptReason("Unable to create", 0,
                                DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
    }
    full_path_ = temp_file;
  }

  return Open();
}

DownloadInterruptReason BaseFile::AppendDataToFile(const char* data,
                                                   size_t data_len) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!detached_);

  // NOTE(benwells): The above DCHECK won't be present in release builds,
  // so we log any occurences to see how common this error is in the wild.
  if (detached_)
    RecordDownloadCount(APPEND_TO_DETACHED_FILE_COUNT);

  if (!file_stream_.get())
    return LogInterruptReason("No file stream on append", 0,
                              DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);

  // TODO(phajdan.jr): get rid of this check.
  if (data_len == 0)
    return DOWNLOAD_INTERRUPT_REASON_NONE;

  // The Write call below is not guaranteed to write all the data.
  size_t write_count = 0;
  size_t len = data_len;
  const char* current_data = data;
  while (len > 0) {
    write_count++;
    int write_result =
        file_stream_->WriteSync(current_data, len);
    DCHECK_NE(0, write_result);

    // Check for errors.
    if (static_cast<size_t>(write_result) != data_len) {
      // We should never get ERR_IO_PENDING, as the Write above is synchronous.
      DCHECK_NE(net::ERR_IO_PENDING, write_result);

      // Report errors on file writes.
      if (write_result < 0)
        return LogNetError("Write", static_cast<net::Error>(write_result));
    }

    // Update status.
    size_t write_size = static_cast<size_t>(write_result);
    DCHECK_LE(write_size, len);
    len -= write_size;
    current_data += write_size;
    bytes_so_far_ += write_size;
  }

  RecordDownloadWriteSize(data_len);
  RecordDownloadWriteLoopCount(write_count);

  if (calculate_hash_)
    secure_hash_->Update(data, data_len);

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

DownloadInterruptReason BaseFile::Rename(const FilePath& new_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DownloadInterruptReason rename_result = DOWNLOAD_INTERRUPT_REASON_NONE;

  // If the new path is same as the old one, there is no need to perform the
  // following renaming logic.
  if (new_path == full_path_)
    return DOWNLOAD_INTERRUPT_REASON_NONE;

  // Save the information whether the download is in progress because
  // it will be overwritten by closing the file.
  bool was_in_progress = in_progress();

  bound_net_log_.BeginEvent(
      net::NetLog::TYPE_DOWNLOAD_FILE_RENAMED,
      base::Bind(&FileRenamedNetLogCallback, &full_path_, &new_path));
  Close();
  file_util::CreateDirectory(new_path.DirName());

  // A simple rename wouldn't work here since we want the file to have
  // permissions / security descriptors that makes sense in the new directory.
  rename_result = MoveFileAndAdjustPermissions(new_path);

  if (rename_result == DOWNLOAD_INTERRUPT_REASON_NONE) {
    full_path_ = new_path;
    // Re-open the file if we were still using it.
    if (was_in_progress)
      rename_result = Open();
  }

  bound_net_log_.EndEvent(net::NetLog::TYPE_DOWNLOAD_FILE_RENAMED);
  return rename_result;
}

void BaseFile::Detach() {
  detached_ = true;
  bound_net_log_.AddEvent(net::NetLog::TYPE_DOWNLOAD_FILE_DETACHED);
}

void BaseFile::Cancel() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!detached_);

  bound_net_log_.AddEvent(net::NetLog::TYPE_CANCELLED);

  Close();

  if (!full_path_.empty()) {
    bound_net_log_.AddEvent(net::NetLog::TYPE_DOWNLOAD_FILE_DELETED);

    file_util::Delete(full_path_, false);
  }
}

void BaseFile::Finish() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (calculate_hash_)
    secure_hash_->Finish(sha256_hash_, kSha256HashLen);

  Close();
}

// OS_WIN, OS_MACOSX and OS_LINUX have specialized implementations.
#if !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(OS_LINUX)
DownloadInterruptReason BaseFile::AnnotateWithSourceInformation() {
  return DOWNLOAD_INTERRUPT_REASON_NONE;
}
#endif

int64 BaseFile::CurrentSpeed() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return CurrentSpeedAtTime(base::TimeTicks::Now());
}

bool BaseFile::GetHash(std::string* hash) {
  DCHECK(!detached_);
  hash->assign(reinterpret_cast<const char*>(sha256_hash_),
               sizeof(sha256_hash_));
  return (calculate_hash_ && !in_progress());
}

std::string BaseFile::GetHashState() {
  if (!calculate_hash_)
    return "";

  Pickle hash_state;
  if (!secure_hash_->Serialize(&hash_state))
    return "";

  return std::string(reinterpret_cast<const char*>(hash_state.data()),
                     hash_state.size());
}

// static
bool BaseFile::IsEmptyHash(const std::string& hash) {
  return (hash.size() == kSha256HashLen &&
          0 == memcmp(hash.data(), kEmptySha256Hash, sizeof(kSha256HashLen)));
}

std::string BaseFile::DebugString() const {
  return base::StringPrintf("{ source_url_ = \"%s\""
                            " full_path_ = \"%" PRFilePath "\""
                            " bytes_so_far_ = %" PRId64
                            " detached_ = %c }",
                            source_url_.spec().c_str(),
                            full_path_.value().c_str(),
                            bytes_so_far_,
                            detached_ ? 'T' : 'F');
}

void BaseFile::CreateFileStream() {
  file_stream_.reset(new net::FileStream(bound_net_log_.net_log()));
  file_stream_->SetBoundNetLogSource(bound_net_log_);
}

DownloadInterruptReason BaseFile::Open() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!detached_);
  DCHECK(!full_path_.empty());

  bound_net_log_.BeginEvent(
      net::NetLog::TYPE_DOWNLOAD_FILE_OPENED,
      base::Bind(&FileOpenedNetLogCallback, &full_path_, bytes_so_far_));

  // Create a new file stream if it is not provided.
  if (!file_stream_.get()) {
    CreateFileStream();
    file_stream_->EnableErrorStatistics();
    int open_result = file_stream_->OpenSync(
        full_path_,
        base::PLATFORM_FILE_OPEN_ALWAYS | base::PLATFORM_FILE_WRITE);
    if (open_result != net::OK) {
      ClearStream();
      return LogNetError("Open", static_cast<net::Error>(open_result));
    }

    // We may be re-opening the file after rename. Always make sure we're
    // writing at the end of the file.
    int64 seek_result = file_stream_->SeekSync(net::FROM_END, 0);
    if (seek_result < 0) {
      ClearStream();
      return LogNetError("Seek", static_cast<net::Error>(seek_result));
    }
  } else {
    file_stream_->SetBoundNetLogSource(bound_net_log_);
  }

  int64 file_size = file_stream_->SeekSync(net::FROM_END, 0);
  if (file_size > bytes_so_far_) {
    // The file is larger than we expected.
    // This is OK, as long as we don't use the extra.
    // Truncate the file.
    int64 truncate_result = file_stream_->Truncate(bytes_so_far_);
    if (truncate_result < 0)
      return LogNetError("Truncate", static_cast<net::Error>(truncate_result));

    // If if wasn't an error, it should have truncated to the size
    // specified.
    DCHECK_EQ(bytes_so_far_, truncate_result);
  } else if (file_size < bytes_so_far_) {
    // The file is shorter than we expected.  Our hashes won't be valid.
    return LogInterruptReason("Unable to seek to last written point", 0,
                              DOWNLOAD_INTERRUPT_REASON_FILE_TOO_SHORT);
  }

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

void BaseFile::Close() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  bound_net_log_.AddEvent(net::NetLog::TYPE_DOWNLOAD_FILE_CLOSED);

  if (file_stream_.get()) {
#if defined(OS_CHROMEOS)
    // Currently we don't really care about the return value, since if it fails
    // theres not much we can do.  But we might in the future.
    file_stream_->FlushSync();
#endif
    ClearStream();
  }
}

void BaseFile::ClearStream() {
  // This should only be called when we have a stream.
  DCHECK(file_stream_.get() != NULL);
  file_stream_.reset();
  bound_net_log_.EndEvent(net::NetLog::TYPE_DOWNLOAD_FILE_OPENED);
}

int64 BaseFile::CurrentSpeedAtTime(base::TimeTicks current_time) const {
  base::TimeDelta diff = current_time - start_tick_;
  int64 diff_ms = diff.InMilliseconds();
  return diff_ms == 0 ? 0 : bytes_so_far() * 1000 / diff_ms;
}

DownloadInterruptReason BaseFile::LogNetError(
    const char* operation,
    net::Error error) {
  bound_net_log_.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_FILE_ERROR,
      base::Bind(&FileErrorNetLogCallback, operation, error));
  return ConvertNetErrorToInterruptReason(error, DOWNLOAD_INTERRUPT_FROM_DISK);
}

DownloadInterruptReason BaseFile::LogSystemError(
    const char* operation,
    int os_error) {
  // There's no direct conversion from a system error to an interrupt reason.
  net::Error net_error = net::MapSystemError(os_error);
  return LogInterruptReason(
      operation, os_error,
      ConvertNetErrorToInterruptReason(
          net_error, DOWNLOAD_INTERRUPT_FROM_DISK));
}

DownloadInterruptReason BaseFile::LogInterruptReason(
    const char* operation,
    int os_error,
    DownloadInterruptReason reason) {
  bound_net_log_.AddEvent(
      net::NetLog::TYPE_DOWNLOAD_FILE_ERROR,
      base::Bind(&FileInterruptedNetLogCallback, operation, os_error, reason));
  return reason;
}

}  // namespace content
