// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/url_request_content_job.h"

#include "base/android/content_uri_utils.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/task_runner.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_error_job.h"
#include "url/gurl.h"

namespace content {

// TODO(qinmin): Refactor this class to reuse the common code in
// url_request_file_job.cc.
URLRequestContentJob::ContentMetaInfo::ContentMetaInfo()
    : content_exists(false),
      content_size(0) {
}

URLRequestContentJob::URLRequestContentJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::FilePath& content_path,
    const scoped_refptr<base::TaskRunner>& content_task_runner)
    : net::URLRequestJob(request, network_delegate),
      content_path_(content_path),
      stream_(new net::FileStream(content_task_runner)),
      content_task_runner_(content_task_runner),
      remaining_bytes_(0),
      io_pending_(false),
      weak_ptr_factory_(this) {}

void URLRequestContentJob::Start() {
  ContentMetaInfo* meta_info = new ContentMetaInfo();
  content_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&URLRequestContentJob::FetchMetaInfo, content_path_,
                 base::Unretained(meta_info)),
      base::Bind(&URLRequestContentJob::DidFetchMetaInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(meta_info)));
}

void URLRequestContentJob::Kill() {
  stream_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();

  net::URLRequestJob::Kill();
}

bool URLRequestContentJob::ReadRawData(net::IOBuffer* dest,
                                       int dest_size,
                                       int* bytes_read) {
  DCHECK_GT(dest_size, 0);
  DCHECK(bytes_read);
  DCHECK_GE(remaining_bytes_, 0);

  if (remaining_bytes_ < dest_size)
    dest_size = static_cast<int>(remaining_bytes_);

  // If we should copy zero bytes because |remaining_bytes_| is zero, short
  // circuit here.
  if (!dest_size) {
    *bytes_read = 0;
    return true;
  }

  int rv = stream_->Read(dest,
                         dest_size,
                         base::Bind(&URLRequestContentJob::DidRead,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    make_scoped_refptr(dest)));
  if (rv >= 0) {
    // Data is immediately available.
    *bytes_read = rv;
    remaining_bytes_ -= rv;
    DCHECK_GE(remaining_bytes_, 0);
    return true;
  }

  // Otherwise, a read error occured.  We may just need to wait...
  if (rv == net::ERR_IO_PENDING) {
    io_pending_ = true;
    SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
  } else {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED, rv));
  }
  return false;
}

bool URLRequestContentJob::IsRedirectResponse(GURL* location,
                                              int* http_status_code) {
  return false;
}

bool URLRequestContentJob::GetMimeType(std::string* mime_type) const {
  DCHECK(request_);
  if (!meta_info_.mime_type.empty()) {
    *mime_type = meta_info_.mime_type;
    return true;
  }
  return false;
}

void URLRequestContentJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  if (!headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header))
    return;

  // We only care about "Range" header here.
  std::vector<net::HttpByteRange> ranges;
  if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
    if (ranges.size() == 1) {
      byte_range_ = ranges[0];
    } else {
      // We don't support multiple range requests.
      NotifyDone(net::URLRequestStatus(
          net::URLRequestStatus::FAILED,
          net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    }
  }
}

URLRequestContentJob::~URLRequestContentJob() {}

void URLRequestContentJob::FetchMetaInfo(const base::FilePath& content_path,
                                         ContentMetaInfo* meta_info) {
  base::File::Info file_info;
  meta_info->content_exists = base::GetFileInfo(content_path, &file_info);
  if (meta_info->content_exists) {
    meta_info->content_size = file_info.size;
    meta_info->mime_type = base::GetContentUriMimeType(content_path);
  }
}

void URLRequestContentJob::DidFetchMetaInfo(const ContentMetaInfo* meta_info) {
  meta_info_ = *meta_info;

  if (!meta_info_.content_exists) {
    DidOpen(net::ERR_FILE_NOT_FOUND);
    return;
  }

  int flags = base::File::FLAG_OPEN |
              base::File::FLAG_READ |
              base::File::FLAG_ASYNC;
  int rv = stream_->Open(content_path_, flags,
                         base::Bind(&URLRequestContentJob::DidOpen,
                                    weak_ptr_factory_.GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING)
    DidOpen(rv);
}

void URLRequestContentJob::DidOpen(int result) {
  if (result != net::OK) {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED, result));
    return;
  }

  if (!byte_range_.ComputeBounds(meta_info_.content_size)) {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                     net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    return;
  }

  remaining_bytes_ = byte_range_.last_byte_position() -
                     byte_range_.first_byte_position() + 1;
  DCHECK_GE(remaining_bytes_, 0);

  if (remaining_bytes_ > 0 && byte_range_.first_byte_position() != 0) {
    int rv = stream_->Seek(base::File::FROM_BEGIN,
                           byte_range_.first_byte_position(),
                           base::Bind(&URLRequestContentJob::DidSeek,
                                      weak_ptr_factory_.GetWeakPtr()));
    if (rv != net::ERR_IO_PENDING) {
      // stream_->Seek() failed, so pass an intentionally erroneous value
      // into DidSeek().
      DidSeek(-1);
    }
  } else {
    // We didn't need to call stream_->Seek() at all, so we pass to DidSeek()
    // the value that would mean seek success. This way we skip the code
    // handling seek failure.
    DidSeek(byte_range_.first_byte_position());
  }
}

void URLRequestContentJob::DidSeek(int64 result) {
  if (result != byte_range_.first_byte_position()) {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                     net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    return;
  }

  set_expected_content_size(remaining_bytes_);
  NotifyHeadersComplete();
}

void URLRequestContentJob::DidRead(
    scoped_refptr<net::IOBuffer> buf, int result) {
  if (result > 0) {
    SetStatus(net::URLRequestStatus());  // Clear the IO_PENDING status
    remaining_bytes_ -= result;
    DCHECK_GE(remaining_bytes_, 0);
  }

  DCHECK(io_pending_);
  io_pending_ = false;

  if (result == 0) {
    NotifyDone(net::URLRequestStatus());
  } else if (result < 0) {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED, result));
  }

  NotifyReadComplete(result);
}

}  // namespace content
