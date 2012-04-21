// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_protocol_handler.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/chromeos/gdata/gdata_files.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

using content::BrowserThread;

namespace gdata {

namespace {

const net::UnescapeRule::Type kUrlPathUnescapeMask =
    net::UnescapeRule::SPACES |
    net::UnescapeRule::URL_SPECIAL_CHARS |
    net::UnescapeRule::CONTROL_CHARS;

const int kHTTPOk = 200;
const int kHTTPNotAllowed = 403;
const int kHTTPNotFound = 404;
const int kHTTPInternalError = 500;

const char kHTTPOkText[] = "OK";
const char kHTTPNotAllowedText[] = "Not Allowed";
const char kHTTPNotFoundText[] = "Not Found";
const char kHTTPInternalErrorText[] = "Internal Error";

// Empty callback for net::FileStream::Close().
void EmptyCompletionCallback(int) {
}

// Helper function that reads file size.
void GetFileSizeOnIOThreadPool(const FilePath& file_path,
                               int64* file_size) {
  if (!file_util::GetFileSize(file_path, file_size))
    *file_size = 0;
}

// Helper function that parsers gdata://viewfile/<resource_id>/<file_name> into
// its components.
bool ParseGDataUrlPath(const std::string& path,
                       std::string* resource_id,
                       std::string* file_name) {
  std::vector<std::string> components;
  FilePath url_path= FilePath::FromUTF8Unsafe(path);
  url_path.GetComponents(&components);
  if (components.size() != 4 ||
      components[1] != gdata::util::kGDataViewFileHostnameUrl) {
    LOG(WARNING) << "Invalid path: " << url_path.value();
    return false;
  }

  *resource_id = net::UnescapeURLComponent(components[2], kUrlPathUnescapeMask);
  *file_name = net::UnescapeURLComponent(components[3], kUrlPathUnescapeMask);
  return true;
}

}  // namespace

// Helper function to get GDataFileSystem from Profile on UI thread.
void GetFileSystemOnUIThread(GDataFileSystem** file_system) {
  GDataSystemService* system_service = GDataSystemServiceFactory::GetForProfile(
      ProfileManager::GetDefaultProfile());
  *file_system = system_service ? system_service->file_system() : NULL;
}

// Helper function to cancel GData download operation on UI thread.
void CancelGDataDownloadOnUIThread(const FilePath& gdata_file_path) {
  GDataFileSystem* file_system = NULL;
  GetFileSystemOnUIThread(&file_system);
  if (file_system)
    file_system->CancelOperation(gdata_file_path);
}

// Class delegate to find file by resource id and extract relevant file info.
class GetFileInfoDelegate : public gdata::FindEntryDelegate {
 public:
  GetFileInfoDelegate() {}
  virtual ~GetFileInfoDelegate() {}

  const std::string& mime_type() const { return mime_type_; }
  const std::string& file_name() const { return file_name_; }
  const FilePath& gdata_file_path() const { return gdata_file_path_; }

 private:
  // GDataFileSystem::FindEntryDelegate overrides.
  virtual void OnDone(base::PlatformFileError error,
                      const FilePath& directory_path,
                      gdata::GDataEntry* entry) OVERRIDE {
    if (error == base::PLATFORM_FILE_OK && entry && entry->AsGDataFile()) {
      mime_type_ = entry->AsGDataFile()->content_mime_type();
      file_name_ = entry->AsGDataFile()->file_name();
      gdata_file_path_= entry->GetFilePath();
    }
  }

  std::string mime_type_;
  std::string file_name_;
  FilePath gdata_file_path_;
};

// GDataURLRequesetJob is the gateway between network-level gdata://viewfile/
// requests for gdata resources and GDataFileSytem.  It exposes content URLs
// formatted as gdata://viewfile/<resource-id>/<file_name>.
class GDataURLRequestJob : public net::URLRequestJob {
 public:
  explicit GDataURLRequestJob(net::URLRequest* request);
  virtual ~GDataURLRequestJob();

  // net::URLRequestJob overrides:
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int* bytes_read) OVERRIDE;

 private:
  // Helper for Start() to let us start asynchronously.
  void StartAsync(GDataFileSystem** file_system);

  // Helper callback for handling async responses from
  // GDataFileSystem::GetFileByResourceId().
  void OnGetFileByResourceId(base::PlatformFileError error,
                             const FilePath& local_file_path,
                             const std::string& mime_type,
                             GDataFileType file_type);

  // Helper callback for GetFileSizeOnIOThreadPool that sets |remaining_bytes_|
  // to |file_size|, and notifies result for Start().
  void OnGetFileSize(int64 *file_size);

  // Helper methods for ReadRawData to open file and read from its corresponding
  // stream in a streaming fashion.
  bool ContinueRead(int* bytes_read);
  void ReadFromFile();
  void ReadFileStream(int bytes_to_read);

  // Helper callback for handling async responses from FileStream::Open().
  void OnFileOpen(int bytes_to_read, int open_result);

    // Helper callback for handling async responses from FileStream::Read().
  void OnReadFileStream(int bytes_read);

  // Helper methods to handle |reamining_bytes_| and |read_buf_|.
  int BytesReadCompleted();
  void RecordBytesRead(int bytes_read);

  // Helper methods to fomulate and notify about response status, info and
  // headers.
  void NotifySuccess();
  void NotifyFailure(int);
  void HeadersCompleted(int status_code, const std::string& status_txt);

  // Helper method to close |stream_|.
  void CloseFileStream();

  base::WeakPtrFactory<GDataURLRequestJob> weak_factory_;
  GDataFileSystem* file_system_;

  bool error_;  // True if we've encountered an error.
  bool headers_set_;  // True if headers have been set.

  FilePath local_file_path_;
  FilePath gdata_file_path_;
  std::string mime_type_;
  int64 remaining_bytes_;
  scoped_ptr<net::FileStream> stream_;
  scoped_refptr<net::DrainableIOBuffer> read_buf_;
  scoped_ptr<net::HttpResponseInfo> response_info_;

  DISALLOW_COPY_AND_ASSIGN(GDataURLRequestJob);
};

GDataURLRequestJob::GDataURLRequestJob(net::URLRequest* request)
    : net::URLRequestJob(request),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      file_system_(NULL),
      error_(false),
      headers_set_(false),
      remaining_bytes_(0) {
}

GDataURLRequestJob::~GDataURLRequestJob() {
  CloseFileStream();
}

void GDataURLRequestJob::Start() {
  DVLOG(1) << "Starting request";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // As per pattern shared by most net::URLRequestJob implementations, we start
  // asynchronously.

  // Here is how Start and its helper methods work:
  // 1) If we haven't got file system, post task ta UI thread to do so.
  //    1.1) reply task proceeds directly to step 3.
  // 2) Otherwise, because start request should be asynchronous (so that all
  //    error reporting and data callbacks happen as they would for network
  //    requests), post task to same thread to actually start request.
  // 3) If unable to get file system or request method is not GET, report start
  //    error and bail out.
  // 4) Otherwise, parse request url to get resource id and file name.
  // 5) Find file from file system to get its mime type and gdata file path.
  // 6) Get file from file system asynchronously - this would either get it from
  //    cache or download it from gdata.
  // 7) In get-file callback, post task to get file size of local file.
  // 8) In get-file-size callback, record file size and notify success.
  // Any error arising from steps 4-8, immediately report start error and bail
  // out.

  // Request job is created and runs on IO thread but getting file system via
  // profile needs to happen on UI thread, so post GetFileSystemOnUIThread to
  // UI thread; StartAsync reply task will proceed with actually starting the
  // request.
  GDataFileSystem** file_system = new GDataFileSystem*(NULL);
  BrowserThread::PostTaskAndReply(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GetFileSystemOnUIThread,
                 file_system),
      base::Bind(&GDataURLRequestJob::StartAsync,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(file_system)));
}

void GDataURLRequestJob::Kill() {
  DVLOG(1) << "Killing request";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  CloseFileStream();

  // If download operation for gdata file (via
  // GDataFileSystem::GetFileByResourceId) is still in progress (i.e.
  // |local_file_path_| is still empty), cancel it by posting a task on the UI
  // thread.
  if (file_system_ && !gdata_file_path_.empty() && local_file_path_.empty()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&CancelGDataDownloadOnUIThread,
                   gdata_file_path_));
  }

  net::URLRequestJob::Kill();
  weak_factory_.InvalidateWeakPtrs();
}

bool GDataURLRequestJob::GetMimeType(std::string* mime_type) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!file_system_)
    return false;

  std::string resource_id;
  std::string unused_file_name;
  if (!ParseGDataUrlPath(request_->url().spec(), &resource_id,
                         &unused_file_name)) {
    return false;
  }

  GetFileInfoDelegate delegate;
  file_system_->FindEntryByResourceIdSync(resource_id, &delegate);
  mime_type->assign(delegate.mime_type());
  return !mime_type->empty();
}

void GDataURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (response_info_.get())
    *info = *response_info_;
}

int GDataURLRequestJob::GetResponseCode() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!response_info_.get())
    return -1;

  return response_info_->headers->response_code();
}

bool GDataURLRequestJob::ReadRawData(net::IOBuffer* dest,
                                     int dest_size,
                                     int* bytes_read) {
  // ReadRawData here is similar to the reading from file in
  // webkit_blob::BlobURLRequsetJob.
  // Here is how it and its helper methods work:
  // First time ReadRawData is called:
  // 1)  Open file stream asynchronously.
  // 2)  If there's an immediate error, report failure.
  // 3)  Otherwise, set request status to pending.
  // 4)  Return false for ReadRawData to indicate no data and either error or
  //     open pending
  // 5)  In file-open callback, continue with step 6.
  // Subsequent times and reading part of first time:
  // 6)  Determine number of bytes to read, which is the lesser of remaining
  //     bytes in read buffer or file.
  // 7)  Read file stream asynchronously.
  // 8)  If there's an immediate error, report failure.
  // 9)  Otherwise, set request status to pending.
  // 10) Return false for ReadRawData to indicate no data and either error or
  //     read pending.
  // 11) In file-read callback:
  //     11.1) Clear pending request status.
  //     11.2) If read buffer is all filled up, notify read complete.
  //     11.3) Otherwise, repeat from step 6.
  // After step 11.2, ReadRawData will be called to read the next chunk.
  // This repeats until we return true and 0 for bytes_read for ReadRawData.

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_NE(dest_size, 0);
  DCHECK(bytes_read);
  DCHECK_GE(remaining_bytes_, 0);

  DVLOG(1) << "ReadRawData: bytes_requested=" << dest_size;

  // Bail out immediately if we encounter an error.
  if (error_) {
    DVLOG(1) << "ReadRawData: has previous error";
    *bytes_read = 0;
    return true;
  }

  if (remaining_bytes_ < dest_size)
    dest_size = static_cast<int>(remaining_bytes_);

  // If we should copy zero bytes because |remaining_bytes_| is zero, short
  // circuit here.
  if (!dest_size) {
    DVLOG(1) << "ReadRawData: done reading "
             << local_file_path_.BaseName().RemoveExtension().value();
    *bytes_read = 0;
    return true;
  }

  // Keep track of the buffer.
  DCHECK(!read_buf_);
  read_buf_ = new net::DrainableIOBuffer(dest, dest_size);

  bool rc = ContinueRead(bytes_read);
  DVLOG(1) << "ReadRawData: out with "
           << (rc ? "has" : "no")
           << "_data, bytes_read=" << *bytes_read
           << ", buf_remaining="
           << (read_buf_ ? read_buf_->BytesRemaining() : 0)
           << ", file_remaining=" << remaining_bytes_;
  return rc;
}

//======================= GDataURLRequestJob private methods ===================

void GDataURLRequestJob::StartAsync(GDataFileSystem** file_system) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  file_system_ = *file_system;

  if (!request_ || !file_system_) {
    LOG(WARNING) << "Failed to start request: null "
                 << (!request_ ? "request" : "file system");
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_FAILED));
    return;
  }

  // We only support GET request.
  if (request()->method() != "GET") {
    LOG(WARNING) << "Failed to start request: GET method is not supported";
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_METHOD_NOT_SUPPORTED));
    return;
  }

  std::string resource_id;
  std::string file_name;
  if (!ParseGDataUrlPath(request_->url().spec(), &resource_id, &file_name)) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_INVALID_URL));
    return;
  }

  // First, check if file metadata is matching our expectations.
  GetFileInfoDelegate delegate;
  file_system_->FindEntryByResourceIdSync(resource_id, &delegate);
  if (delegate.file_name() != file_name) {
    LOG(WARNING) << "Failed to start request: "
                 << "filename in request url and file system are different";
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_FILE_NOT_FOUND));
    return;
  }

  mime_type_ = delegate.mime_type();
  gdata_file_path_ = delegate.gdata_file_path();

  file_system_->GetFileByResourceId(
      resource_id,
      base::Bind(&GDataURLRequestJob::OnGetFileByResourceId,
                 weak_factory_.GetWeakPtr()));
}

void GDataURLRequestJob::OnGetFileByResourceId(
    base::PlatformFileError error,
    const FilePath& local_file_path,
    const std::string& mime_type,
    GDataFileType file_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error != base::PLATFORM_FILE_OK || file_type != REGULAR_FILE) {
    LOG(WARNING) << "Failed to start request: can't get file for resource id";
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_FILE_NOT_FOUND));
    return;
  }

  local_file_path_ = local_file_path;

  // Even though we're already on BrowserThread::IO thread,
  // file_util::GetFileSize can only be called IO thread pool, so post a task
  // to blocking pool instead.
  int64* file_size = new int64(0);
  BrowserThread::GetBlockingPool()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GetFileSizeOnIOThreadPool,
                 local_file_path_,
                 base::Unretained(file_size)),
      base::Bind(&GDataURLRequestJob::OnGetFileSize,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(file_size)));
}

void GDataURLRequestJob::OnGetFileSize(int64 *file_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (*file_size == 0) {
    LOG(WARNING) << "Failed to open " << local_file_path_.value();
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_FILE_NOT_FOUND));
    return;
  }

  remaining_bytes_ = *file_size;
  DVLOG(1) << "Request started successfully: file_size=" << remaining_bytes_;

  NotifySuccess();
}

bool GDataURLRequestJob::ContinueRead(int* bytes_read) {
  // Continue to read if there's more to read from file or read buffer is not
  // filled up.
  if (remaining_bytes_ > 0 && read_buf_->BytesRemaining() > 0) {
    ReadFromFile();
    // Either async IO is pending or there's an error, return false.
    return false;
  }

  // Otherwise, we have data in read buffer, return true with number of bytes
  // read.
  *bytes_read = BytesReadCompleted();
  DVLOG(1) << "Has data: bytes_read=" << *bytes_read
           << ", buf_remaining=0, file_remaining=" << remaining_bytes_;
  return true;
}

void GDataURLRequestJob::ReadFromFile() {
  int bytes_to_read = std::min(read_buf_->BytesRemaining(),
                               static_cast<int>(remaining_bytes_));

  // If the stream already exists, keep reading from it.
  if (stream_.get()) {
    ReadFileStream(bytes_to_read);
    return;
  }

  // Otherwise, open the stream for file.
  stream_.reset(new net::FileStream(NULL));
  int result = stream_->Open(
      local_file_path_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ |
      base::PLATFORM_FILE_ASYNC,
      base::Bind(&GDataURLRequestJob::OnFileOpen,
                 weak_factory_.GetWeakPtr(),
                 bytes_to_read));

  if (result == net::ERR_IO_PENDING) {
    DVLOG(1) << "IO is pending for opening "
             << local_file_path_.BaseName().RemoveExtension().value();
    SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
  } else {
    DCHECK_NE(result, net::OK);
    LOG(WARNING) << "Failed to open " << local_file_path_.value();
    NotifyFailure(net::ERR_FILE_NOT_FOUND);
  }
}

void GDataURLRequestJob::OnFileOpen(int bytes_to_read, int open_result) {
  if (open_result != net::OK) {
    LOG(WARNING) << "Failed to open " << local_file_path_.value();
    NotifyFailure(net::ERR_FILE_NOT_FOUND);
    return;
  }

  DVLOG(1) << "Successfully opened " << local_file_path_.value();

  // Read from opened file stream.
  DCHECK(stream_.get());
  ReadFileStream(bytes_to_read);
}

void GDataURLRequestJob::ReadFileStream(int bytes_to_read) {
  DCHECK(stream_.get());
  DCHECK(stream_->IsOpen());
  DCHECK_GE(read_buf_->BytesRemaining(), bytes_to_read);

  int result = stream_->Read(read_buf_, bytes_to_read,
                             base::Bind(&GDataURLRequestJob::OnReadFileStream,
                                        weak_factory_.GetWeakPtr()));

  // If IO is pending, we just need to wait.
  if (result == net::ERR_IO_PENDING) {
    DVLOG(1) << "IO pending: bytes_to_read=" << bytes_to_read
             << ", buf_remaining=" << read_buf_->BytesRemaining()
             << ", file_remaining=" << remaining_bytes_;
    SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
  } else {  // For all other errors, bail out.
    // Asynchronous read should not return result >= 0;
    // refer to net/base/file_stream_posix.cc.
    DCHECK(result < 0);
    LOG(WARNING) << "Failed to read " << local_file_path_.value();
    NotifyFailure(net::ERR_FAILED);
  }
}

void GDataURLRequestJob::OnReadFileStream(int bytes_read) {
  if (bytes_read <= 0) {
    LOG(WARNING) << "Failed to read " << local_file_path_.value();
    NotifyFailure(net::ERR_FAILED);
    return;
  }

  SetStatus(net::URLRequestStatus());  // Clear the IO_PENDING status.

  RecordBytesRead(bytes_read);

  DVLOG(1) << "Cleared IO pending: bytes_read=" << bytes_read
           << ", buf_remaining=" << read_buf_->BytesRemaining()
           << ", file_remaining=" << remaining_bytes_;

  // If the read buffer is completely filled, we're done.
  if (!read_buf_->BytesRemaining()) {
    int bytes_read = BytesReadCompleted();
    DVLOG(1) << "Completed read: bytes_read=" << bytes_read
             << ", file_remaining=" << remaining_bytes_;
    NotifyReadComplete(bytes_read);
    return;
  }

  // Otherwise, continue the reading.
  int new_bytes_read = 0;
  if (ContinueRead(&new_bytes_read)) {
    DVLOG(1) << "Completed read: bytes_read=" << bytes_read
             << ", file_remaining=" << remaining_bytes_;
    NotifyReadComplete(new_bytes_read);
  }
}

int GDataURLRequestJob::BytesReadCompleted() {
  int bytes_read = read_buf_->BytesConsumed();
  read_buf_ = NULL;
  return bytes_read;
}

void GDataURLRequestJob::RecordBytesRead(int bytes_read) {
  DCHECK_GT(bytes_read, 0);

  // Subtract the remaining bytes.
  remaining_bytes_ -= bytes_read;
  DCHECK_GE(remaining_bytes_, 0);

  // Adjust the read buffer.
  read_buf_->DidConsume(bytes_read);
  DCHECK_GE(read_buf_->BytesRemaining(), 0);
}

void GDataURLRequestJob::CloseFileStream() {
  if (!stream_.get())
    return;
  stream_->Close(base::Bind(&EmptyCompletionCallback));
  // net::FileStream::Close blocks until stream is closed, so it's safe to
  // release the pointer here.
  stream_.reset(NULL);
}

void GDataURLRequestJob::NotifySuccess() {
  HeadersCompleted(kHTTPOk, kHTTPOkText);
}

void GDataURLRequestJob::NotifyFailure(int error_code) {
  error_ = true;

  // If we already return the headers on success, we can't change the headers
  // now. Instead, we just error out.
  if (headers_set_) {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                     error_code));
    return;
  }

  int status_code = 0;
  std::string status_txt;
  switch (error_code) {
    case net::ERR_ACCESS_DENIED:
      status_code = kHTTPNotAllowed;
      status_txt = kHTTPNotAllowedText;
      break;
    case net::ERR_FILE_NOT_FOUND:
      status_code = kHTTPNotFound;
      status_txt = kHTTPNotFoundText;
      break;
    case net::ERR_FAILED:
      status_code = kHTTPInternalError;
      status_txt = kHTTPInternalErrorText;
      break;
    default:
      DCHECK(false);
      status_code = kHTTPInternalError;
      status_txt = kHTTPInternalErrorText;
      break;
  }

  HeadersCompleted(status_code, status_txt);
}

void GDataURLRequestJob::HeadersCompleted(int status_code,
                                          const std::string& status_text) {
  std::string status("HTTP/1.1 ");
  status.append(base::IntToString(status_code));
  status.append(" ");
  status.append(status_text);
  status.append("\0\0", 2);
  net::HttpResponseHeaders* headers = new net::HttpResponseHeaders(status);

  if (status_code == kHTTPOk) {
    std::string content_length_header(net::HttpRequestHeaders::kContentLength);
    content_length_header.append(": ");
    content_length_header.append(base::Int64ToString(remaining_bytes_));
    headers->AddHeader(content_length_header);

    if (!mime_type_.empty()) {
      std::string content_type_header(net::HttpRequestHeaders::kContentType);
      content_type_header.append(": ");
      content_type_header.append(mime_type_);
      headers->AddHeader(content_type_header);
    }
  }

  response_info_.reset(new net::HttpResponseInfo());
  response_info_->headers = headers;

  set_expected_content_size(remaining_bytes_);
  headers_set_ = true;

  NotifyHeadersComplete();
}

// static
net::URLRequestJob* GDataProtocolHandler::CreateJob(net::URLRequest* request,
                                                    const std::string& scheme) {
  DVLOG(1) << "Handling url: " << request->url().spec();
  return new GDataURLRequestJob(request);
}

}  // namespace gdata
