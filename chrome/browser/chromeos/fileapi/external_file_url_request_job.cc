// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/external_file_url_request_job.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "chrome/browser/extensions/api/file_handlers/mime_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "storage/browser/fileapi/file_system_backend.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"

using content::BrowserThread;

namespace chromeos {
namespace {

const char kMimeTypeForRFC822[] = "message/rfc822";
const char kMimeTypeForMHTML[] = "multipart/related";

// Helper for obtaining FileSystemContext, FileSystemURL, and mime type on the
// UI thread.
class URLHelper {
 public:
  // The scoped pointer to control lifetime of the instance itself. The pointer
  // is passed to callback functions and binds the lifetime of the instance to
  // the callback's lifetime.
  typedef scoped_ptr<URLHelper> Lifetime;

  URLHelper(void* profile_id,
            const GURL& url,
            const ExternalFileURLRequestJob::HelperCallback& callback)
      : profile_id_(profile_id), url_(url), callback_(callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    Lifetime lifetime(this);
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&URLHelper::RunOnUIThread,
                                       base::Unretained(this),
                                       base::Passed(&lifetime)));
  }

 private:
  void RunOnUIThread(Lifetime lifetime) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    Profile* const profile = reinterpret_cast<Profile*>(profile_id_);
    if (!g_browser_process->profile_manager()->IsValidProfile(profile)) {
      ReplyResult(net::ERR_FAILED);
      return;
    }
    content::StoragePartition* const storage =
        content::BrowserContext::GetStoragePartitionForSite(profile, url_);
    DCHECK(storage);

    scoped_refptr<storage::FileSystemContext> context =
        storage->GetFileSystemContext();
    DCHECK(context.get());

    // Obtain the absolute path in the file system.
    const base::FilePath virtual_path = ExternalFileURLToVirtualPath(url_);

    // Obtain the file system URL.
    // TODO(hirono): After removing MHTML support, stop to use the special
    // drive: scheme and use filesystem: URL directly.  crbug.com/415455
    file_system_url_ = context->CreateCrackedFileSystemURL(
        GURL(std::string(chrome::kExternalFileScheme) + ":"),
        storage::kFileSystemTypeExternal,
        virtual_path);

    // Check if the obtained path providing external file URL or not.
    if (FileSystemURLToExternalFileURL(file_system_url_).is_empty()) {
      ReplyResult(net::ERR_INVALID_URL);
      return;
    }

    file_system_context_ = context;

    extensions::app_file_handler_util::GetMimeTypeForLocalPath(
        profile,
        file_system_url_.path(),
        base::Bind(&URLHelper::OnGotMimeTypeOnUIThread,
                   base::Unretained(this),
                   base::Passed(&lifetime)));
  }

  void OnGotMimeTypeOnUIThread(Lifetime lifetime,
                               const std::string& mime_type) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    mime_type_ = mime_type;

    if (mime_type_ == kMimeTypeForRFC822)
      mime_type_ = kMimeTypeForMHTML;

    ReplyResult(net::OK);
  }

  void ReplyResult(net::Error error) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            base::Bind(callback_,
                                       error,
                                       file_system_context_,
                                       file_system_url_,
                                       mime_type_));
  }

  void* const profile_id_;
  const GURL url_;
  const ExternalFileURLRequestJob::HelperCallback callback_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  storage::FileSystemURL file_system_url_;
  std::string mime_type_;

  DISALLOW_COPY_AND_ASSIGN(URLHelper);
};

}  // namespace

ExternalFileURLRequestJob::ExternalFileURLRequestJob(
    void* profile_id,
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate)
    : net::URLRequestJob(request, network_delegate),
      profile_id_(profile_id),
      remaining_bytes_(0),
      weak_ptr_factory_(this) {
}

void ExternalFileURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    // Note: We only support single range requests.
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges) &&
        ranges.size() == 1) {
      byte_range_ = ranges[0];
    } else {
      // Failed to parse Range: header, so notify the error.
      NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                       net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    }
  }
}

void ExternalFileURLRequestJob::Start() {
  DVLOG(1) << "Starting request";
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!stream_reader_);

  // We only support GET request.
  if (request()->method() != "GET") {
    LOG(WARNING) << "Failed to start request: " << request()->method()
                 << " method is not supported";
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_METHOD_NOT_SUPPORTED));
    return;
  }

  // Check if the scheme is correct.
  if (!request()->url().SchemeIs(chrome::kExternalFileScheme)) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_INVALID_URL));
    return;
  }

  // Owned by itself.
  new URLHelper(profile_id_,
                request()->url(),
                base::Bind(&ExternalFileURLRequestJob::OnHelperResultObtained,
                           weak_ptr_factory_.GetWeakPtr()));
}

void ExternalFileURLRequestJob::OnHelperResultObtained(
    net::Error error,
    const scoped_refptr<storage::FileSystemContext>& file_system_context,
    const storage::FileSystemURL& file_system_url,
    const std::string& mime_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (error != net::OK) {
    NotifyStartError(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, error));
    return;
  }

  DCHECK(file_system_context.get());
  file_system_context_ = file_system_context;
  file_system_url_ = file_system_url;
  mime_type_ = mime_type;

  // Check if the entry has a redirect URL.
  file_system_context_->external_backend()->GetRedirectURLForContents(
      file_system_url_,
      base::Bind(&ExternalFileURLRequestJob::OnRedirectURLObtained,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ExternalFileURLRequestJob::OnRedirectURLObtained(
    const GURL& redirect_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  redirect_url_ = redirect_url;
  if (!redirect_url_.is_empty()) {
    NotifyHeadersComplete();
    return;
  }

  // Obtain file system context.
  file_system_context_->operation_runner()->GetMetadata(
      file_system_url_,
      base::Bind(&ExternalFileURLRequestJob::OnFileInfoObtained,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ExternalFileURLRequestJob::OnFileInfoObtained(
    base::File::Error result,
    const base::File::Info& file_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (result == base::File::FILE_ERROR_NOT_FOUND) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_FILE_NOT_FOUND));
    return;
  }

  if (result != base::File::FILE_OK || file_info.is_directory ||
      file_info.size < 0) {
    NotifyStartError(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED));
    return;
  }

  // Compute content size.
  if (!byte_range_.ComputeBounds(file_info.size)) {
    NotifyStartError(net::URLRequestStatus(
        net::URLRequestStatus::FAILED, net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    return;
  }
  const int64 offset = byte_range_.first_byte_position();
  const int64 size =
      byte_range_.last_byte_position() + 1 - byte_range_.first_byte_position();
  set_expected_content_size(size);
  remaining_bytes_ = size;

  // Create file stream reader.
  stream_reader_ = file_system_context_->CreateFileStreamReader(
      file_system_url_, offset, size, base::Time());
  if (!stream_reader_) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_FILE_NOT_FOUND));
    return;
  }

  NotifyHeadersComplete();
}

void ExternalFileURLRequestJob::Kill() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  stream_reader_.reset();
  file_system_context_ = NULL;
  net::URLRequestJob::Kill();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool ExternalFileURLRequestJob::GetMimeType(std::string* mime_type) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  mime_type->assign(mime_type_);
  return !mime_type->empty();
}

bool ExternalFileURLRequestJob::IsRedirectResponse(GURL* location,
                                                   int* http_status_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (redirect_url_.is_empty())
    return false;

  // Redirect a hosted document.
  *location = redirect_url_;
  const int kHttpFound = 302;
  *http_status_code = kHttpFound;
  return true;
}

bool ExternalFileURLRequestJob::ReadRawData(net::IOBuffer* buf,
                                            int buf_size,
                                            int* bytes_read) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(stream_reader_);

  if (remaining_bytes_ == 0) {
    *bytes_read = 0;
    return true;
  }

  const int result = stream_reader_->Read(
      buf,
      std::min<int64>(buf_size, remaining_bytes_),
      base::Bind(&ExternalFileURLRequestJob::OnReadCompleted,
                 weak_ptr_factory_.GetWeakPtr()));

  if (result == net::ERR_IO_PENDING) {
    // The data is not yet available.
    SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
    return false;
  }
  if (result < 0) {
    // An error occurs.
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED, result));
    return false;
  }

  // Reading has been finished immediately.
  *bytes_read = result;
  remaining_bytes_ -= result;
  return true;
}

ExternalFileURLRequestJob::~ExternalFileURLRequestJob() {
}

void ExternalFileURLRequestJob::OnReadCompleted(int read_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (read_result < 0) {
    DCHECK_NE(read_result, net::ERR_IO_PENDING);
    NotifyDone(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, read_result));
  }

  remaining_bytes_ -= read_result;
  SetStatus(net::URLRequestStatus());  // Clear the IO_PENDING status.
  NotifyReadComplete(read_result);
}

}  // namespace chromeos
