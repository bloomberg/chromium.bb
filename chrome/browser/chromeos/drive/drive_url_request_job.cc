// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_url_request_job.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_file_stream_reader.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;

namespace drive {
namespace {

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

}  // namespace

DriveURLRequestJob::DriveURLRequestJob(
    const FileSystemGetter& file_system_getter,
    base::SequencedTaskRunner* file_task_runner,
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate)
    : net::URLRequestJob(request, network_delegate),
      file_system_getter_(file_system_getter),
      file_task_runner_(file_task_runner),
      weak_ptr_factory_(this) {
}

void DriveURLRequestJob::SetExtraRequestHeaders(
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
      NotifyDone(
          net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    }
  }
}

void DriveURLRequestJob::Start() {
  DVLOG(1) << "Starting request";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!stream_reader_);

  // We only support GET request.
  if (request()->method() != "GET") {
    LOG(WARNING) << "Failed to start request: "
                 << request()->method() << " method is not supported";
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_METHOD_NOT_SUPPORTED));
    return;
  }

  base::FilePath drive_file_path(util::DriveURLToFilePath(request_->url()));
  if (drive_file_path.empty()) {
    // Not a valid url.
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_INVALID_URL));
    return;
  }

  // Initialize the stream reader.
  stream_reader_.reset(
      new DriveFileStreamReader(file_system_getter_, file_task_runner_.get()));
  stream_reader_->Initialize(
      drive_file_path,
      byte_range_,
      base::Bind(&DriveURLRequestJob::OnDriveFileStreamReaderInitialized,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DriveURLRequestJob::Kill() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  stream_reader_.reset();
  net::URLRequestJob::Kill();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool DriveURLRequestJob::GetMimeType(std::string* mime_type) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!entry_) {
    return false;
  }

  mime_type->assign(
      FixupMimeType(entry_->file_specific_info().content_mime_type()));
  return !mime_type->empty();
}

bool DriveURLRequestJob::IsRedirectResponse(
    GURL* location, int* http_status_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!entry_ || !entry_->file_specific_info().is_hosted_document()) {
    return false;
  }

  // Redirect a hosted document.
  *location = GURL(entry_->file_specific_info().alternate_url());
  const int kHttpFound = 302;
  *http_status_code = kHttpFound;
  return true;
}

bool DriveURLRequestJob::ReadRawData(
    net::IOBuffer* buf, int buf_size, int* bytes_read) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(stream_reader_ && stream_reader_->IsInitialized());

  int result = stream_reader_->Read(
      buf, buf_size,
      base::Bind(&DriveURLRequestJob::OnReadCompleted,
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
  return true;
}

DriveURLRequestJob::~DriveURLRequestJob() {
}

void DriveURLRequestJob::OnDriveFileStreamReaderInitialized(
    int error, scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(stream_reader_);

  if (error != FILE_ERROR_OK) {
    NotifyStartError(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, error));
    return;
  }

  DCHECK(entry && entry->has_file_specific_info());
  entry_ = entry.Pass();

  if (!entry_->file_specific_info().is_hosted_document()) {
    // We don't need to set content size for hosted documents,
    // because it will be redirected.
    set_expected_content_size(entry_->file_info().size());
  }

  NotifyHeadersComplete();
}

void DriveURLRequestJob::OnReadCompleted(int read_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (read_result < 0) {
    DCHECK_NE(read_result, net::ERR_IO_PENDING);
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                     read_result));
  }

  SetStatus(net::URLRequestStatus());  // Clear the IO_PENDING status.
  NotifyReadComplete(read_result);
}

}  // namespace drive
