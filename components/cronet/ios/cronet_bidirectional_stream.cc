// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/ios/cronet_bidirectional_stream.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "components/cronet/ios/cronet_environment.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/bidirectional_stream.h"
#include "net/http/bidirectional_stream_request_info.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_util.h"
#include "net/spdy/spdy_header_block.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace cronet {

CronetBidirectionalStream::CronetBidirectionalStream(
    CronetEnvironment* environment,
    Delegate* delegate)
    : read_state_(NOT_STARTED),
      write_state_(NOT_STARTED),
      write_end_of_stream_(false),
      environment_(environment),
      delegate_(delegate) {}

CronetBidirectionalStream::~CronetBidirectionalStream() {
  DCHECK(environment_->IsOnNetworkThread());
}

int CronetBidirectionalStream::Start(const char* url,
                                     int priority,
                                     const char* method,
                                     const net::HttpRequestHeaders& headers,
                                     bool end_of_stream) {
  // Prepare request info here to be able to return the error.
  std::unique_ptr<net::BidirectionalStreamRequestInfo> request_info(
      new net::BidirectionalStreamRequestInfo());
  request_info->url = GURL(url);
  request_info->priority = static_cast<net::RequestPriority>(priority);
  // Http method is a token, just as header name.
  request_info->method = method;
  if (!net::HttpUtil::IsValidHeaderName(request_info->method))
    return -1;
  request_info->extra_headers.CopyFrom(headers);
  request_info->end_stream_on_headers = end_of_stream;
  write_end_of_stream_ = end_of_stream;
  DCHECK(environment_);
  environment_->PostToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetBidirectionalStream::StartOnNetworkThread,
                 base::Unretained(this), base::Passed(&request_info)));
  return 0;
}

bool CronetBidirectionalStream::ReadData(char* buffer, int capacity) {
  if (!buffer)
    return false;
  scoped_refptr<net::WrappedIOBuffer> read_buffer(
      new net::WrappedIOBuffer(buffer));

  environment_->PostToNetworkThread(
      FROM_HERE, base::Bind(&CronetBidirectionalStream::ReadDataOnNetworkThread,
                            base::Unretained(this), read_buffer, capacity));
  return true;
}

bool CronetBidirectionalStream::WriteData(const char* buffer,
                                          int count,
                                          bool end_of_stream) {
  if (!buffer)
    return false;

  scoped_refptr<net::WrappedIOBuffer> write_buffer(
      new net::WrappedIOBuffer(buffer));

  environment_->PostToNetworkThread(
      FROM_HERE,
      base::Bind(&CronetBidirectionalStream::WriteDataOnNetworkThread,
                 base::Unretained(this), write_buffer, count, end_of_stream));
  return true;
}

void CronetBidirectionalStream::Cancel() {
  environment_->PostToNetworkThread(
      FROM_HERE, base::Bind(&CronetBidirectionalStream::CancelOnNetworkThread,
                            base::Unretained(this)));
}

void CronetBidirectionalStream::Destroy() {
  // Destroy could be called from any thread, including network thread (if
  // posting task to executor throws an exception), but is posted, so |this|
  // is valid until calling task is complete.
  environment_->PostToNetworkThread(
      FROM_HERE, base::Bind(&CronetBidirectionalStream::DestroyOnNetworkThread,
                            base::Unretained(this)));
}

void CronetBidirectionalStream::OnHeadersSent() {
  DCHECK(environment_->IsOnNetworkThread());
  DCHECK(write_state_ == STARTED);
  write_state_ = WAITING_FOR_WRITE;
  if (write_end_of_stream_)
    write_state_ = WRITING_DONE;
  delegate_->OnHeadersSent();
}

void CronetBidirectionalStream::OnHeadersReceived(
    const net::SpdyHeaderBlock& response_headers) {
  DCHECK(environment_->IsOnNetworkThread());
  DCHECK(read_state_ == STARTED);
  read_state_ = WAITING_FOR_READ;
  // Get http status code from response headers.
  int http_status_code = 0;
  const auto http_status_header = response_headers.find(":status");
  if (http_status_header != response_headers.end())
    base::StringToInt(http_status_header->second, &http_status_code);
  const char* protocol = "unknown";
  switch (bidi_stream_->GetProtocol()) {
    case net::kProtoHTTP2:
      protocol = "h2";
      break;
    case net::kProtoQUIC1SPDY3:
      protocol = "quic/1+spdy/3";
      break;
    default:
      break;
  }
  delegate_->OnHeadersReceived(response_headers, protocol);
}

void CronetBidirectionalStream::OnDataRead(int bytes_read) {
  DCHECK(environment_->IsOnNetworkThread());
  DCHECK(read_state_ == READING);
  read_state_ = WAITING_FOR_READ;
  delegate_->OnDataRead(read_buffer_->data(), bytes_read);

  // Free the read buffer.
  read_buffer_ = nullptr;
  if (bytes_read == 0)
    read_state_ = READING_DONE;
  MaybeOnSucceded();
}

void CronetBidirectionalStream::OnDataSent() {
  DCHECK(environment_->IsOnNetworkThread());
  DCHECK(write_state_ == WRITING);
  write_state_ = WAITING_FOR_WRITE;
  delegate_->OnDataSent(write_buffer_->data());
  // Free the write buffer.
  write_buffer_ = nullptr;
  if (write_end_of_stream_)
    write_state_ = WRITING_DONE;
  MaybeOnSucceded();
}

void CronetBidirectionalStream::OnTrailersReceived(
    const net::SpdyHeaderBlock& response_trailers) {
  DCHECK(environment_->IsOnNetworkThread());
  delegate_->OnTrailersReceived(response_trailers);
}

void CronetBidirectionalStream::OnFailed(int error) {
  DCHECK(environment_->IsOnNetworkThread());
  bidi_stream_.reset();
  read_state_ = write_state_ = ERROR;
  delegate_->OnFailed(error);
}

void CronetBidirectionalStream::StartOnNetworkThread(
    std::unique_ptr<net::BidirectionalStreamRequestInfo> request_info) {
  DCHECK(environment_->IsOnNetworkThread());
  DCHECK(!bidi_stream_);
  DCHECK(environment_->GetURLRequestContext());
  request_info->extra_headers.SetHeaderIfMissing(
      net::HttpRequestHeaders::kUserAgent, environment_->user_agent());
  bidi_stream_.reset(new net::BidirectionalStream(
      std::move(request_info), environment_->GetURLRequestContext()
                                   ->http_transaction_factory()
                                   ->GetSession(),
      this));
  DCHECK(read_state_ == NOT_STARTED && write_state_ == NOT_STARTED);
  read_state_ = write_state_ = STARTED;
}

void CronetBidirectionalStream::ReadDataOnNetworkThread(
    scoped_refptr<net::WrappedIOBuffer> read_buffer,
    int buffer_size) {
  DCHECK(environment_->IsOnNetworkThread());
  DCHECK(read_buffer);
  DCHECK(!read_buffer_);
  if (read_state_ != WAITING_FOR_READ) {
    DLOG(ERROR) << "Unexpected Read Data in read_state " << WAITING_FOR_READ;
    OnFailed(net::ERR_UNEXPECTED);
    return;
  }
  read_state_ = READING;
  read_buffer_ = read_buffer;

  int bytes_read = bidi_stream_->ReadData(read_buffer_.get(), buffer_size);
  // If IO is pending, wait for the BidirectionalStream to call OnDataRead.
  if (bytes_read == net::ERR_IO_PENDING)
    return;

  if (bytes_read < 0) {
    OnFailed(bytes_read);
    return;
  }
  OnDataRead(bytes_read);
}

void CronetBidirectionalStream::WriteDataOnNetworkThread(
    scoped_refptr<net::WrappedIOBuffer> write_buffer,
    int buffer_size,
    bool end_of_stream) {
  DCHECK(environment_->IsOnNetworkThread());
  DCHECK(write_buffer);
  DCHECK(!write_buffer_);
  if (write_state_ != WAITING_FOR_WRITE) {
    DLOG(ERROR) << "Unexpected Write Data in write_state " << write_state_;
    OnFailed(net::ERR_UNEXPECTED);
    return;
  }
  write_state_ = WRITING;
  write_end_of_stream_ = end_of_stream;

  write_buffer_ = write_buffer;
  bidi_stream_->SendData(write_buffer_.get(), buffer_size, end_of_stream);
}

void CronetBidirectionalStream::CancelOnNetworkThread() {
  DCHECK(environment_->IsOnNetworkThread());
  if (!bidi_stream_)
    return;
  read_state_ = write_state_ = CANCELED;
  bidi_stream_.reset();
  delegate_->OnCanceled();
}

void CronetBidirectionalStream::DestroyOnNetworkThread() {
  DCHECK(environment_->IsOnNetworkThread());
  delete this;
}

void CronetBidirectionalStream::MaybeOnSucceded() {
  DCHECK(environment_->IsOnNetworkThread());
  if (read_state_ == READING_DONE && write_state_ == WRITING_DONE) {
    read_state_ = write_state_ = SUCCESS;
    bidi_stream_.reset();
    delegate_->OnSucceeded();
  }
}

}  // namespace cronet
