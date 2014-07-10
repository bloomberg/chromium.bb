// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_spdy_server_stream.h"

#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "net/quic/quic_in_memory_cache.h"
#include "net/quic/quic_session.h"
#include "net/quic/spdy_utils.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_header_block.h"
#include "net/spdy/spdy_http_utils.h"

using base::StringPiece;
using std::string;

namespace net {

static const size_t kHeaderBufInitialSize = 4096;

QuicSpdyServerStream::QuicSpdyServerStream(QuicStreamId id,
                                           QuicSession* session)
    : QuicDataStream(id, session),
      read_buf_(new GrowableIOBuffer()),
      request_headers_received_(false) {
  read_buf_->SetCapacity(kHeaderBufInitialSize);
}

QuicSpdyServerStream::~QuicSpdyServerStream() {
}

uint32 QuicSpdyServerStream::ProcessData(const char* data, uint32 data_len) {
  if (data_len > INT_MAX) {
    LOG(DFATAL) << "Data length too long: " << data_len;
    return 0;
  }
  // Are we still reading the request headers.
  if (!request_headers_received_) {
    // Grow the read buffer if necessary.
    while (read_buf_->RemainingCapacity() < static_cast<int>(data_len)) {
      read_buf_->SetCapacity(read_buf_->capacity() * 2);
    }
    memcpy(read_buf_->data(), data, data_len);
    read_buf_->set_offset(read_buf_->offset() + data_len);
    // Try parsing the request headers. This will set request_headers_received_
    // if successful; if not, it will be tried again with more data.
    ParseRequestHeaders();
  } else {
    body_.append(data, data_len);
  }
  return data_len;
}

void QuicSpdyServerStream::OnFinRead() {
  ReliableQuicStream::OnFinRead();
  if (write_side_closed() || fin_buffered()) {
    return;
  }

  if (!request_headers_received_) {
    SendErrorResponse();  // We're not done reading headers.
    return;
  }

  SpdyHeaderBlock::const_iterator it = headers_.find("content-length");
  size_t content_length;
  if (it != headers_.end() &&
      (!base::StringToSizeT(it->second, &content_length) ||
       body_.size() != content_length)) {
    SendErrorResponse();  // Invalid content length
    return;
  }

  SendResponse();
}

// Try parsing the request headers. If successful, sets
// request_headers_received_. If not successful, it can just be tried again once
// there's more data.
void QuicSpdyServerStream::ParseRequestHeaders() {
  SpdyFramer framer((kDefaultSpdyMajorVersion));
  const char* data = read_buf_->StartOfBuffer();
  size_t read_buf_len = static_cast<size_t>(read_buf_->offset());
  size_t len = framer.ParseHeaderBlockInBuffer(data, read_buf_len, &headers_);
  if (len == 0) {
    // Not enough data yet, presumably. (If we still don't succeed by the end of
    // the stream, then we'll error in OnFinRead().)
    return;
  }

  // Headers received and parsed: extract the request URL.
  request_url_ = GetUrlFromHeaderBlock(headers_,
                                       kDefaultSpdyMajorVersion,
                                       false);
  if (!request_url_.is_valid()) {
    SendErrorResponse();
    return;
  }

  // Add any data past the headers to the request body.
  size_t delta = read_buf_len - len;
  if (delta > 0) {
    body_.append(data + len, delta);
  }

  request_headers_received_ = true;
}

void QuicSpdyServerStream::SendResponse() {
  // Find response in cache. If not found, send error response.
  const QuicInMemoryCache::Response* response =
      QuicInMemoryCache::GetInstance()->GetResponse(request_url_);
  if (response == NULL) {
    SendErrorResponse();
    return;
  }

  if (response->response_type() == QuicInMemoryCache::CLOSE_CONNECTION) {
    DVLOG(1) << "Special response: closing connection.";
    CloseConnection(QUIC_NO_ERROR);
    return;
  }

  if (response->response_type() == QuicInMemoryCache::IGNORE_REQUEST) {
    DVLOG(1) << "Special response: ignoring request.";
    return;
  }

  DVLOG(1) << "Sending response for stream " << id();
  SendHeadersAndBody(response->headers(), response->body());
}

void QuicSpdyServerStream::SendErrorResponse() {
  DVLOG(1) << "Sending error response for stream " << id();
  scoped_refptr<HttpResponseHeaders> headers
      = new HttpResponseHeaders(string("HTTP/1.1 500 Server Error") + '\0' +
                                "content-length: 3" + '\0' + '\0');
  SendHeadersAndBody(*headers, "bad");
}

void QuicSpdyServerStream::SendHeadersAndBody(
    const HttpResponseHeaders& response_headers,
    StringPiece body) {
  // We only support SPDY and HTTP, and neither handles bidirectional streaming.
  if (!read_side_closed()) {
    CloseReadSide();
  }

  SpdyHeaderBlock header_block;
  CreateSpdyHeadersFromHttpResponse(response_headers,
                                    kDefaultSpdyMajorVersion,
                                    &header_block);

  WriteHeaders(header_block, body.empty(), NULL);

  if (!body.empty()) {
    WriteOrBufferData(body, true, NULL);
  }
}

}  // namespace net
