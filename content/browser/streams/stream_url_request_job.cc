// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/streams/stream_url_request_job.h"

#include "base/string_number_conversions.h"
#include "content/browser/streams/stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

const int kHTTPOk = 200;
const int kHTTPNotAllowed = 403;
const int kHTTPNotFound = 404;
const int kHTTPMethodNotAllow = 405;
const int kHTTPInternalError = 500;

const char kHTTPOKText[] = "OK";
const char kHTTPNotAllowedText[] = "Not Allowed";
const char kHTTPNotFoundText[] = "Not Found";
const char kHTTPMethodNotAllowText[] = "Method Not Allowed";
const char kHTTPInternalErrorText[] = "Internal Server Error";

}  // namespace

StreamURLRequestJob::StreamURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    scoped_refptr<Stream> stream)
    : net::URLRequestJob(request, network_delegate),
      weak_factory_(this),
      stream_(stream),
      headers_set_(false),
      pending_buffer_size_(0),
      total_bytes_read_(0),
      max_range_(0),
      request_failed_(false) {
  DCHECK(stream_);
  stream_->SetReadObserver(this);
}

StreamURLRequestJob::~StreamURLRequestJob() {
  ClearStream();
}

void StreamURLRequestJob::OnDataAvailable(Stream* stream) {
  // Clear the IO_PENDING status.
  SetStatus(net::URLRequestStatus());
  if (pending_buffer_) {
    int bytes_read;
    stream_->ReadRawData(pending_buffer_, pending_buffer_size_, &bytes_read);

    // Clear the buffers before notifying the read is complete, so that it is
    // safe for the observer to read.
    pending_buffer_ = NULL;
    pending_buffer_size_ = 0;

    total_bytes_read_ += bytes_read;
    NotifyReadComplete(bytes_read);
  }
}

// net::URLRequestJob methods.
void StreamURLRequestJob::Start() {
  // Continue asynchronously.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&StreamURLRequestJob::DidStart, weak_factory_.GetWeakPtr()));
}

void StreamURLRequestJob::Kill() {
  net::URLRequestJob::Kill();
  weak_factory_.InvalidateWeakPtrs();
  ClearStream();
}

bool StreamURLRequestJob::ReadRawData(net::IOBuffer* buf,
                                      int buf_size,
                                      int* bytes_read) {
  if (request_failed_)
    return true;

  DCHECK(bytes_read);
  int to_read = buf_size;
  if (max_range_ && to_read) {
    if (to_read + total_bytes_read_ > max_range_)
      to_read = max_range_ - total_bytes_read_;

    if (to_read <= 0) {
      *bytes_read = 0;
      return true;
    }
  }

  switch (stream_->ReadRawData(buf, to_read, bytes_read)) {
    case Stream::STREAM_HAS_DATA:
    case Stream::STREAM_COMPLETE:
      total_bytes_read_ += *bytes_read;
      return true;
    case Stream::STREAM_EMPTY:
      pending_buffer_ = buf;
      pending_buffer_size_ = to_read;
      SetStatus(net::URLRequestStatus(net::URLRequestStatus::IO_PENDING, 0));
      return false;
  }
  NOTREACHED();
  return false;
}

bool StreamURLRequestJob::GetMimeType(std::string* mime_type) const {
  if (!response_info_)
    return false;

  // TODO(zork): Support registered MIME types if needed.
  return response_info_->headers->GetMimeType(mime_type);
}

void StreamURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (response_info_)
    *info = *response_info_;
}

int StreamURLRequestJob::GetResponseCode() const {
  if (!response_info_)
    return -1;

  return response_info_->headers->response_code();
}

void StreamURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
      if (ranges.size() == 1) {
        // Streams don't support seeking, so a non-zero starting position
        // doesn't make sense.
        if (ranges[0].first_byte_position() == 0) {
          max_range_ = ranges[0].last_byte_position() + 1;
        } else {
          NotifyFailure(net::ERR_METHOD_NOT_SUPPORTED);
          return;
        }
      } else {
        NotifyFailure(net::ERR_METHOD_NOT_SUPPORTED);
        return;
      }
    }
  }
}

void StreamURLRequestJob::DidStart() {
  // We only support GET request.
  if (request()->method() != "GET") {
    NotifyFailure(net::ERR_METHOD_NOT_SUPPORTED);
    return;
  }

  HeadersCompleted(kHTTPOk, kHTTPOKText);
}

void StreamURLRequestJob::NotifyFailure(int error_code) {
  request_failed_ = true;

  // If we already return the headers on success, we can't change the headers
  // now. Instead, we just error out.
  if (headers_set_) {
    NotifyDone(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                     error_code));
    return;
  }

  // TODO(zork): Share these with BlobURLRequestJob.
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
    case net::ERR_METHOD_NOT_SUPPORTED:
      status_code = kHTTPMethodNotAllow;
      status_txt = kHTTPMethodNotAllowText;
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

void StreamURLRequestJob::HeadersCompleted(int status_code,
                                           const std::string& status_text) {
  std::string status("HTTP/1.1 ");
  status.append(base::IntToString(status_code));
  status.append(" ");
  status.append(status_text);
  status.append("\0\0", 2);
  net::HttpResponseHeaders* headers = new net::HttpResponseHeaders(status);

  if (status_code == kHTTPOk) {
    std::string content_type_header(net::HttpRequestHeaders::kContentType);
    content_type_header.append(": ");
    content_type_header.append("plain/text");
    headers->AddHeader(content_type_header);
  }

  response_info_.reset(new net::HttpResponseInfo());
  response_info_->headers = headers;

  headers_set_ = true;

  NotifyHeadersComplete();
}

void StreamURLRequestJob::ClearStream() {
  if (stream_) {
    stream_->RemoveReadObserver(this);
    stream_ = NULL;
  }
}

}  // namespace content
