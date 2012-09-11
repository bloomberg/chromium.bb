// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_protocol_handler.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_interface.h"
#include "chrome/browser/chromeos/gdata/drive_service_interface.h"
#include "chrome/browser/chromeos/gdata/drive_system_service.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
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

const int64 kInvalidFileSize = -1;

// Initial size of download buffer, same as kBufferSize used for URLFetcherCore.
const int kInitialDownloadBufferSizeInBytes = 4096;

struct MimeTypeReplacement {
  const char* original_type;
  const char* new_type;
};

const MimeTypeReplacement kMimeTypeReplacements[] = {
  {"message/rfc822", "multipart/related"}  // Fixes MHTML
};

std::string FixupMimeType(const std::string& type) {
  for (size_t i = 0; i < arraysize(kMimeTypeReplacements); i++) {
    if (type == kMimeTypeReplacements[i].original_type)
      return kMimeTypeReplacements[i].new_type;
  }
  return type;
}

// Empty callback for net::FileStream::Close().
void EmptyCompletionCallback(int) {
}

// Helper function that reads file size.
void GetFileSizeOnBlockingPool(const FilePath& file_path,
                               int64* file_size) {
  if (!file_util::GetFileSize(file_path, file_size))
    *file_size = kInvalidFileSize;
}

// Helper function that extracts and unescapes resource_id from drive urls
// (drive:<resource_id>).
bool ParseDriveUrl(const std::string& path, std::string* resource_id) {
  const std::string drive_schema(chrome::kDriveScheme + std::string(":"));
  if (!StartsWithASCII(path, drive_schema, false) ||
      path.size() <= drive_schema.size()) {
    return false;
  }

  std::string id = path.substr(drive_schema.size());
  *resource_id = net::UnescapeURLComponent(id, kUrlPathUnescapeMask);
  return resource_id->size();
}

// Helper function to get DriveSystemService from Profile.
DriveSystemService* GetSystemService() {
  return DriveSystemServiceFactory::GetForProfile(
      ProfileManager::GetDefaultProfile());
}

// Helper function to get DriveFileSystem from Profile on UI thread.
void GetFileSystemOnUIThread(DriveFileSystemInterface** file_system) {
  DriveSystemService* system_service = GetSystemService();
  *file_system = system_service ? system_service->file_system() : NULL;
}

// Helper function to cancel Drive download operation on UI thread.
void CancelDriveDownloadOnUIThread(const FilePath& drive_file_path) {
  DriveSystemService* system_service = GetSystemService();
  if (system_service)
    system_service->drive_service()->operation_registry()->CancelForFilePath(
        drive_file_path);
}

// DriveURLRequesetJob is the gateway between network-level drive://...
// requests for drive resources and DriveFileSytem.  It exposes content URLs
// formatted as drive://<resource-id>.
class DriveURLRequestJob : public net::URLRequestJob {
 public:
  DriveURLRequestJob(net::URLRequest* request,
                     net::NetworkDelegate* network_delegate);

  // net::URLRequestJob overrides:
  virtual void Start() OVERRIDE;
  virtual void Kill() OVERRIDE;
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int* bytes_read) OVERRIDE;

 protected:
  virtual ~DriveURLRequestJob();

 private:
  // Helper for Start() to let us start asynchronously.
  void StartAsync(DriveFileSystemInterface** file_system);

  // Helper methods for Delegate::OnUrlFetchDownloadData and ReadRawData to
  // receive download data and copy to response buffer.
  // For detailed description of logic, refer to comments in definitions of
  // Start() and ReadRawData().

  void OnUrlFetchDownloadData(GDataErrorCode error,
                              scoped_ptr<std::string> download_data);
  // Called from ReadRawData, returns true if data is ready, false otherwise.
  bool ContinueReadFromDownloadData(int* bytes_read);
  // Copies from download buffer into response buffer.
  bool ReadFromDownloadData();

  // Helper callback for handling async responses from
  // DriveFileSystem::GetFileByResourceId().
  void OnGetFileByResourceId(DriveFileError error,
                             const FilePath& local_file_path,
                             const std::string& mime_type,
                             DriveFileType file_type);

  // Helper callback for GetFileSizeOnBlockingPool that sets |remaining_bytes_|
  // to |file_size|, and notifies result for Start().
  void OnGetFileSize(int64 *file_size);

  // Helper callback for GetEntryInfoByResourceId invoked by StartAsync.
  void OnGetEntryInfoByResourceId(const std::string& resource_id,
                                  DriveFileError error,
                                  const FilePath& drive_file_path,
                                  scoped_ptr<DriveEntryProto> entry_proto);

  // Helper methods for ReadRawData to open file and read from its corresponding
  // stream in a streaming fashion.
  bool ContinueReadFromFile(int* bytes_read);
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

  DriveFileSystemInterface* file_system_;

  bool error_;  // True if we've encountered an error.
  bool headers_set_;  // True if headers have been set.

  FilePath local_file_path_;
  FilePath drive_file_path_;
  std::string mime_type_;
  int64 initial_file_size_;
  int64 remaining_bytes_;
  scoped_ptr<net::FileStream> stream_;
  scoped_refptr<net::DrainableIOBuffer> read_buf_;
  scoped_ptr<net::HttpResponseInfo> response_info_;
  bool streaming_download_;
  scoped_refptr<net::GrowableIOBuffer> download_growable_buf_;
  scoped_refptr<net::DrainableIOBuffer> download_drainable_buf_;

  // This should remain the last member so it'll be destroyed first and
  // invalidate its weak pointers before other members are destroyed.
  base::WeakPtrFactory<DriveURLRequestJob> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DriveURLRequestJob);
};

DriveURLRequestJob::DriveURLRequestJob(net::URLRequest* request,
                                       net::NetworkDelegate* network_delegate)
    : net::URLRequestJob(request, network_delegate),
      file_system_(NULL),
      error_(false),
      headers_set_(false),
      initial_file_size_(0),
      remaining_bytes_(0),
      streaming_download_(false),
      download_growable_buf_(new net::GrowableIOBuffer),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  download_growable_buf_->SetCapacity(kInitialDownloadBufferSizeInBytes);
  download_drainable_buf_ = new net::DrainableIOBuffer(
      download_growable_buf_, download_growable_buf_->capacity());
}

void DriveURLRequestJob::Start() {
  DVLOG(1) << "Starting request";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // As per pattern shared by most net::URLRequestJob implementations, we start
  // asynchronously.

  // Here is how Start and its helper methods work:
  // 1) Post task to UI thread to get file system.
  //    Note: should we not post task to get file system later, start request
  //    should still be asynchronous, so that all error reporting and data
  //    callbacks happen as they would for network requests, so we should still
  //    post task to same thread to actually start request.
  // 2) Reply task StartAsync is invoked.
  // 3) If unable to get file system or request method is not GET, report start
  //    error and bail out.
  // 4) Otherwise, parse request url to get resource id and file name.
  // 5) Find file from file system to get its mime type, drive file path and
  //    size of physical file.
  // 6) Get file from file system asynchronously with both GetFileCallback and
  //    GetContentCallback - this would either get it from cache or
  //    download it from Drive.
  // 7) If file is downloaded from Drive:
  //    7.1) Whenever net::URLFetcherCore::OnReadCompleted() receives a part
  //         of the response, it invokes
  //         constent::URLFetcherDelegate::OnURLFetchDownloadData() if
  //         net::URLFetcherDelegate::ShouldSendDownloadData() is true.
  //    7.2) gdata::DownloadFileOperation overrides the default implementations
  //         of the following methods of net::URLFetcherDelegate:
  //         - ShouldSendDownloadData(): returns true for non-null
  //                                     GetContentCallback.
  //         - OnURLFetchDownloadData(): invokes non-null
  //                                     GetContentCallback
  //    7.3) DriveProtolHandler::OnURLFetchDownloadData (i.e. this class)
  //         is at the end of the invocation chain and actually implements the
  //         method.
  //    7.4) Copies the formal download data into a growable-drainable dowload
  //         IOBuffer
  //         - IOBuffer has initial size 4096, same as buffer used in
  //           net::URLFetcherCore::OnReadCompleted.
  //         - We may end up with multiple chunks, so we use GrowableIOBuffer.
  //         - We then wrap the growable buffer within a DrainableIOBuffer for
  //           ease of progressively writing into the buffer.
  //    7.5) When we receive the very first chunk of data, notify start success.
  //    7.6) Proceed to streaming of download data in ReadRawData.
  // 8) If file exits in cache:
  //    8.1) in get-file callback, post task to get file size of local file.
  //    8.2) In get-file-size callback, record file size and notify success.
  //    8.3) Proceed to reading from file in ReadRawData.
  // Any error arising from steps 4-8, immediately report start error and bail
  // out.
  // NotifySuccess internally calls ReadRawData, hence we only notify success
  // after we have:
  // - received the first chunk of download data if file is downloaded
  // - gotten size of physical file if file exists in cache.

  // Request job is created and runs on IO thread but getting file system via
  // profile needs to happen on UI thread, so post GetFileSystemOnUIThread to
  // UI thread; StartAsync reply task will proceed with actually starting the
  // request.

  DriveFileSystemInterface** file_system = new DriveFileSystemInterface*(NULL);
  BrowserThread::PostTaskAndReply(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GetFileSystemOnUIThread, file_system),
      base::Bind(&DriveURLRequestJob::StartAsync,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(file_system)));
}

void DriveURLRequestJob::Kill() {
  DVLOG(1) << "Killing request";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  CloseFileStream();

  // If download operation for drive file (via
  // DriveFileSystem::GetFileByResourceId) is still in progress, cancel it by
  // posting a task on the UI thread.
  // Download operation is still in progress if:
  // 1) |local_file_path_| is still empty; it gets filled when callback for
  //    GetFileByResourceId is called, AND
  // 2) we're still streaming download data i.e. |remaining_bytes_| > 0; if
  //    we've finished streaming data, we want to avoid possibly killing last
  //    part of the download process where the last chunk is written to file;
  //    if we're reading directly from cache file, |remaining_bytes_| doesn't
  //    matter 'cos |local_file_path_| will not be empty.
  if (file_system_ && !drive_file_path_.empty() && local_file_path_.empty() &&
      remaining_bytes_ > 0) {
    DVLOG(1) << "Canceling download operation for " << drive_file_path_.value();
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&CancelDriveDownloadOnUIThread,
                   drive_file_path_));
  }

  net::URLRequestJob::Kill();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool DriveURLRequestJob::GetMimeType(std::string* mime_type) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  mime_type->assign(FixupMimeType(mime_type_));
  return !mime_type->empty();
}

void DriveURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (response_info_.get())
    *info = *response_info_;
}

int DriveURLRequestJob::GetResponseCode() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!response_info_.get())
    return -1;

  return response_info_->headers->response_code();
}

bool DriveURLRequestJob::ReadRawData(net::IOBuffer* dest,
                                     int dest_size,
                                     int* bytes_read) {
  // ReadRawData splits into 2 logic paths: streaming downloaded file or reading
  // from physical file, the latter is similar to the reading from file in
  // webkit_blob::BlobURLRequsetJob.
  // For reading from existing file, here is how it and its helper methods work:
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
  //
  // For streaming from downloaded file, there's no difference in the
  // implementation of ReadRawData for first or subsequent times, except where
  // it is called from.  The first ReadRawData is called internally from
  // NotifySuccess whereas subsequent times are called internally from
  // NotifyReadComplete.  Logic flow is as follows:
  // 1) When a chunk of data is received in OnUrlFetchDownloadData, copy it into
  //    download buffer.  If remaining buffer is not enough to hold entire chunk
  //    of downloaded data, grow buffer to size needed, re-wrap it within
  //    drainable buffer, then copy the download chunk.
  // 2) If a response buffer from ReadRawData exists, copy from download buffer
  //    to response buffer.
  //    2.1) If response bufer is filled up:
  //         - if we have to return from ReadRawData to caller, return true to
  //           indicate data's ready.
  //         - otherwise, clear io pending status, and notify read complete.
  //    2.2) Otherwise if response buffer is not filled up:
  //         - if we have to return from ReadRawData to caller, set io pending
  //           status, and return false to indicate data's not ready.
  //         - wait for the chunk of download data and repeat from step 1.
  // 3) Otherwise, the next ReadRawData() call will provide the response buffer,
  //    when we would repeat from step 2.
  // Note that we only notify read complete when the response buffer is all
  // filled up or it's the last chunk of data.  During investgiation, I
  // discovered that if i notify read complete without filling up the response
  // buffer, ReadRawData gets called less and less, resulting in the download
  // buffer growing bigger and bigger, which is definitely undesirable for us.

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

  bool rc = false;
  if (streaming_download_)
    rc = ContinueReadFromDownloadData(bytes_read);
  else
    rc = ContinueReadFromFile(bytes_read);
  DVLOG(1) << "ReadRawData: out with "
           << (rc ? "has" : "no")
           << "_data, bytes_read=" << *bytes_read
           << ", buf_remaining="
           << (read_buf_ ? read_buf_->BytesRemaining() : 0)
           << ", " << (streaming_download_ ? "download" : "file")
           << "_remaining=" << remaining_bytes_;
  return rc;
}

//======================= DriveURLRequestJob protected methods ================

DriveURLRequestJob::~DriveURLRequestJob() {
  CloseFileStream();
}

//======================= DriveURLRequestJob private methods ===================

void DriveURLRequestJob::StartAsync(DriveFileSystemInterface** file_system) {
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
  if (!ParseDriveUrl(request_->url().spec(), &resource_id)) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_INVALID_URL));
    return;
  }

  file_system_->GetEntryInfoByResourceId(
      resource_id,
      base::Bind(&DriveURLRequestJob::OnGetEntryInfoByResourceId,
                 weak_ptr_factory_.GetWeakPtr(),
                 resource_id));
}

void DriveURLRequestJob::OnGetEntryInfoByResourceId(
    const std::string& resource_id,
    DriveFileError error,
    const FilePath& drive_file_path,
    scoped_ptr<DriveEntryProto> entry_proto) {
  if (entry_proto.get() && !entry_proto->has_file_specific_info())
    error = DRIVE_FILE_ERROR_NOT_FOUND;

  if (error == DRIVE_FILE_OK) {
    DCHECK(entry_proto.get());
    mime_type_ = entry_proto->file_specific_info().content_mime_type();
    drive_file_path_ = drive_file_path;
    initial_file_size_ = entry_proto->file_info().size();
  } else {
    mime_type_.clear();
    drive_file_path_.clear();
    initial_file_size_ = 0;
  }
  remaining_bytes_ = initial_file_size_;

  DVLOG(1) << "Getting file for resource id";
  file_system_->GetFileByResourceId(
      resource_id,
      base::Bind(&DriveURLRequestJob::OnGetFileByResourceId,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DriveURLRequestJob::OnUrlFetchDownloadData,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DriveURLRequestJob::OnUrlFetchDownloadData(
    GDataErrorCode error,
    scoped_ptr<std::string> download_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (download_data->empty())
    return;

  // If there's insufficient space remaining in download buffer to copy download
  // data, expand download buffer.
  int bytes_needed = download_data->length() -
                     download_drainable_buf_->BytesRemaining();
  if (bytes_needed > 0) {
    // Grow the download buffer to accommodate entire download data.
    download_growable_buf_->SetCapacity(download_drainable_buf_->size() +
                                        bytes_needed);
    // Remember current offset of download buffer, i.e. where we last finished
    // writing to.
    int offset = download_drainable_buf_->BytesRemaining();
    // Reinitialize drainable buffer with the new size.
    download_drainable_buf_ = new net::DrainableIOBuffer(
        download_growable_buf_, download_growable_buf_->capacity());
    // Set offset in new drainable buffer to what it was before
    // reinitialization, so that its data() points to where we last finished
    // writing to, and the next memcpy will continue from there.
    download_drainable_buf_->SetOffset(offset);
  }

  // Copy from download data into download buffer.
  memcpy(download_drainable_buf_->data(), download_data->data(),
         download_data->length());
  // Advance offset in download buffer.
  download_drainable_buf_->DidConsume(download_data->length());
  DVLOG(1) << "Got OnUrlFetchDownloadData: in_data_size="
           << download_data->length()
           << ", our_data_size=" << download_drainable_buf_->BytesConsumed();

  // If this is the first data we have, report request has started successfully.
  if (!streaming_download_) {
    streaming_download_ = true;
    DVLOG(1) << "Request started successfully: expected_download_size="
             << remaining_bytes_;
    NotifySuccess();
    return;
  }

  // Otherwise, read from download data into read buffer of response.
  if (ReadFromDownloadData()) {
    // Read has completed, report it.
    SetStatus(net::URLRequestStatus());  // Clear the IO_PENDING status.
    int bytes_read = BytesReadCompleted();
    DVLOG(1) << "Completed read: bytes_read=" << bytes_read
             << ", download_remaining=" << remaining_bytes_;
    NotifyReadComplete(bytes_read);
  }
}

bool DriveURLRequestJob::ContinueReadFromDownloadData(int* bytes_read) {
  // Continue to read if there's more to read from download data or read buffer
  // is not filled up.
  if (remaining_bytes_ > 0 && read_buf_->BytesRemaining() > 0) {
    if (!ReadFromDownloadData()) {
      DVLOG(1) << "IO is pending for reading from download data";
      SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
      // Either async IO is pending or there's an error, return false.
      return false;
    }
  }

  // Otherwise, we have data in read buffer, return true with number of bytes
  // read.
  *bytes_read = BytesReadCompleted();
  DVLOG(1) << "Has data: bytes_read=" << *bytes_read
           << ", buf_remaining=0, download_remaining=" << remaining_bytes_;
  return true;
}

bool DriveURLRequestJob::ReadFromDownloadData() {
  DCHECK(streaming_download_);

  // If download buffer is empty or there's no read buffer, return false.
  if (download_drainable_buf_->BytesConsumed() <= 0 ||
      !read_buf_ || read_buf_->BytesRemaining() <= 0) {
    return false;
  }

  // Number of bytes to read is the lesser of remaining bytes in read buffer or
  // written bytes in download buffer.
  int bytes_to_read = std::min(read_buf_->BytesRemaining(),
                               download_drainable_buf_->BytesConsumed());
  // If read buffer doesn't have enough space, there will be bytes in download
  // buffer that will not be copied to read buffer.
  int bytes_not_copied = download_drainable_buf_->BytesConsumed() -
                         bytes_to_read;
  // Set offset in download buffer to 0, so that its data() points to the
  // beginning of the buffer where we'll copy bytes from.
  download_drainable_buf_->SetOffset(0);
  // Copy from download buffer to read buffer.
  memcpy(read_buf_->data(), download_drainable_buf_->data(), bytes_to_read);
  // Advance read buffer.
  RecordBytesRead(bytes_to_read);
  // If download buffer has bytes that are not copied over, move them to
  // beginning of download buffer.
  if (bytes_not_copied > 0) {
    // data() is pointing to the beginning, so the bytes to move start from
    // where we finished copying to read buffer.
    memmove(download_drainable_buf_->data(),
            download_drainable_buf_->data() + bytes_to_read,
            bytes_not_copied);
    // Set offset in download buffer to where we finished moving.
    download_drainable_buf_->SetOffset(bytes_not_copied);
  }
  DVLOG(1) << "Copied from download data: bytes_read=" << bytes_to_read;

  // Return true if read buffer is filled up or there's no more bytes to read.
  return read_buf_->BytesRemaining() == 0 || remaining_bytes_ == 0;
}

void DriveURLRequestJob::OnGetFileByResourceId(
    DriveFileError error,
    const FilePath& local_file_path,
    const std::string& mime_type,
    DriveFileType file_type) {
  DVLOG(1) << "Got OnGetFileByResourceId";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (error != DRIVE_FILE_OK || file_type != REGULAR_FILE) {
    LOG(WARNING) << "Failed to start request: can't get file for resource id";
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_FILE_NOT_FOUND));
    return;
  }

  local_file_path_ = local_file_path;

  // If we've already streamed download data to response, we're done.
  if (streaming_download_)
    return;

  // Even though we're already on BrowserThread::IO thread,
  // file_util::GetFileSize can only be called on a thread with file
  // operations allowed, so post a task to blocking pool instead.
  int64* file_size = new int64(kInvalidFileSize);
  BrowserThread::GetBlockingPool()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GetFileSizeOnBlockingPool,
                 local_file_path_,
                 base::Unretained(file_size)),
      base::Bind(&DriveURLRequestJob::OnGetFileSize,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(file_size)));
}

void DriveURLRequestJob::OnGetFileSize(int64 *file_size) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (*file_size == kInvalidFileSize) {
    LOG(WARNING) << "Failed to open " << local_file_path_.value();
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_FILE_NOT_FOUND));
    return;
  }

  remaining_bytes_ = *file_size;
  DVLOG(1) << "Request started successfully: file_size=" << remaining_bytes_;

  NotifySuccess();
}

bool DriveURLRequestJob::ContinueReadFromFile(int* bytes_read) {
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

void DriveURLRequestJob::ReadFromFile() {
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
      base::Bind(&DriveURLRequestJob::OnFileOpen,
                 weak_ptr_factory_.GetWeakPtr(),
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

void DriveURLRequestJob::OnFileOpen(int bytes_to_read, int open_result) {
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

void DriveURLRequestJob::ReadFileStream(int bytes_to_read) {
  DCHECK(stream_.get());
  DCHECK(stream_->IsOpen());
  DCHECK_GE(read_buf_->BytesRemaining(), bytes_to_read);

  int result = stream_->Read(read_buf_, bytes_to_read,
                             base::Bind(&DriveURLRequestJob::OnReadFileStream,
                                        weak_ptr_factory_.GetWeakPtr()));

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

void DriveURLRequestJob::OnReadFileStream(int bytes_read) {
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
  if (ContinueReadFromFile(&new_bytes_read)) {
    DVLOG(1) << "Completed read: bytes_read=" << bytes_read
             << ", file_remaining=" << remaining_bytes_;
    NotifyReadComplete(new_bytes_read);
  }
}

int DriveURLRequestJob::BytesReadCompleted() {
  int bytes_read = read_buf_->BytesConsumed();
  read_buf_ = NULL;
  return bytes_read;
}

void DriveURLRequestJob::RecordBytesRead(int bytes_read) {
  DCHECK_GT(bytes_read, 0);

  // Subtract the remaining bytes.
  remaining_bytes_ -= bytes_read;
  DCHECK_GE(remaining_bytes_, 0);

  // Adjust the read buffer.
  read_buf_->DidConsume(bytes_read);
  DCHECK_GE(read_buf_->BytesRemaining(), 0);
}

void DriveURLRequestJob::CloseFileStream() {
  if (!stream_.get())
    return;
  stream_->Close(base::Bind(&EmptyCompletionCallback));
  // net::FileStream::Close blocks until stream is closed, so it's safe to
  // release the pointer here.
  stream_.reset(NULL);
}

void DriveURLRequestJob::NotifySuccess() {
  HeadersCompleted(kHTTPOk, kHTTPOkText);
}

void DriveURLRequestJob::NotifyFailure(int error_code) {
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

void DriveURLRequestJob::HeadersCompleted(int status_code,
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

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// DriveProtocolHandler class

DriveProtocolHandler::DriveProtocolHandler() {
}

DriveProtocolHandler::~DriveProtocolHandler() {
}

net::URLRequestJob* DriveProtocolHandler::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  DVLOG(1) << "Handling url: " << request->url().spec();
  return new DriveURLRequestJob(request, network_delegate);
}

}  // namespace gdata
