// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_http_stream.h"

#include "base/callback_helpers.h"
#include "base/stringprintf.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/quic/quic_client_session.h"
#include "net/quic/quic_reliable_client_stream.h"
#include "net/quic/quic_utils.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"

namespace net {

static const size_t kHeaderBufInitialSize = 4096;

QuicHttpStream::QuicHttpStream(QuicReliableClientStream* stream,
                               bool use_spdy)
    : io_state_(STATE_NONE),
      stream_(stream),
      request_info_(NULL),
      request_body_stream_(NULL),
      response_info_(NULL),
      response_status_(OK),
      response_headers_received_(false),
      use_spdy_(use_spdy),
      read_buf_(new GrowableIOBuffer()),
      user_buffer_len_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  DCHECK(stream_);
  stream_->SetDelegate(this);
}

QuicHttpStream::~QuicHttpStream() {
  Close(false);
}

int QuicHttpStream::InitializeStream(const HttpRequestInfo* request_info,
                                     const BoundNetLog& stream_net_log,
                                     const CompletionCallback& callback) {
  CHECK(stream_);

  stream_net_log_ = stream_net_log;
  request_info_ = request_info;

  return OK;
}

int QuicHttpStream::SendRequest(const HttpRequestHeaders& request_headers,
                                HttpResponseInfo* response,
                                const CompletionCallback& callback) {
  CHECK(stream_);
  CHECK(!request_body_stream_);
  CHECK(!response_info_);
  CHECK(!callback.is_null());
  CHECK(response);

  // Store the serialized request headers.
  if (use_spdy_) {
    SpdyHeaderBlock headers;
    CreateSpdyHeadersFromHttpRequest(*request_info_, request_headers,
                                     &headers, 3, /*direct=*/true);
    size_t len = SpdyFramer::GetSerializedLength(3, &headers);
    SpdyFrameBuilder builder(len);
    SpdyFramer::WriteHeaderBlock(&builder, 3, &headers);
    scoped_ptr<SpdyFrame> frame(builder.take());
    request_ = std::string(frame->data(), len);
    // Log the actual request with the URL Request's net log.
    stream_net_log_.AddEvent(
        NetLog::TYPE_HTTP_TRANSACTION_SPDY_SEND_REQUEST_HEADERS,
        base::Bind(&SpdyHeaderBlockNetLogCallback, &headers));
    // Also log to the QuicSession's net log.
    stream_->net_log().AddEvent(
        NetLog::TYPE_QUIC_HTTP_STREAM_SEND_REQUEST_HEADERS,
        base::Bind(&SpdyHeaderBlockNetLogCallback, &headers));
  } else {
    std::string path = HttpUtil::PathForRequest(request_info_->url);
    std::string first_line = base::StringPrintf("%s %s HTTP/1.1\r\n",
                                                request_info_->method.c_str(),
                                                path.c_str());
    request_ = first_line + request_headers.ToString();
    // Log the actual request with the URL Request's net log.
    stream_net_log_.AddEvent(
        NetLog::TYPE_HTTP_TRANSACTION_SEND_REQUEST_HEADERS,
        base::Bind(&HttpRequestHeaders::NetLogCallback,
                   base::Unretained(&request_headers),
                   &first_line));
    // Also log to the QuicSession's net log.
    stream_->net_log().AddEvent(
        NetLog::TYPE_QUIC_HTTP_STREAM_SEND_REQUEST_HEADERS,
        base::Bind(&HttpRequestHeaders::NetLogCallback,
                   base::Unretained(&request_headers),
                   &first_line));
  }

  // Store the request body.
  request_body_stream_ = request_info_->upload_data_stream;
  if (request_body_stream_ && (request_body_stream_->size() ||
                               request_body_stream_->is_chunked())) {
    // Use kMaxPacketSize as the buffer size, since the request
    // body data is written with this size at a time.
    // TODO(rch): use a smarter value since we can't write an entire
    // packet due to overhead.
    raw_request_body_buf_ = new IOBufferWithSize(kMaxPacketSize);
    // The request body buffer is empty at first.
    request_body_buf_ = new DrainableIOBuffer(raw_request_body_buf_, 0);
  }

  // Store the response info.
  response_info_ = response;

  io_state_ = STATE_SEND_HEADERS;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv > 0 ? OK : rv;
}

UploadProgress QuicHttpStream::GetUploadProgress() const {
  if (!request_body_stream_)
    return UploadProgress();

  return UploadProgress(request_body_stream_->position(),
                        request_body_stream_->size());
}

int QuicHttpStream::ReadResponseHeaders(const CompletionCallback& callback) {
  CHECK(!callback.is_null());

  if (stream_ == NULL)
    return response_status_;

  // Check if we already have the response headers. If so, return synchronously.
  if (response_headers_received_)
    return OK;

  // Still waiting for the response, return IO_PENDING.
  CHECK(callback_.is_null());
  callback_ = callback;
  return ERR_IO_PENDING;
}

const HttpResponseInfo* QuicHttpStream::GetResponseInfo() const {
  return response_info_;
}

int QuicHttpStream::ReadResponseBody(
    IOBuffer* buf, int buf_len, const CompletionCallback& callback) {
  CHECK(buf);
  CHECK(buf_len);
  CHECK(!callback.is_null());

  // If we have data buffered, complete the IO immediately.
  if (!response_body_.empty()) {
    int bytes_read = 0;
    while (!response_body_.empty() && buf_len > 0) {
      scoped_refptr<IOBufferWithSize> data = response_body_.front();
      const int bytes_to_copy = std::min(buf_len, data->size());
      memcpy(&(buf->data()[bytes_read]), data->data(), bytes_to_copy);
      buf_len -= bytes_to_copy;
      if (bytes_to_copy == data->size()) {
        response_body_.pop_front();
      } else {
        const int bytes_remaining = data->size() - bytes_to_copy;
        IOBufferWithSize* new_buffer = new IOBufferWithSize(bytes_remaining);
        memcpy(new_buffer->data(), &(data->data()[bytes_to_copy]),
               bytes_remaining);
        response_body_.pop_front();
        response_body_.push_front(make_scoped_refptr(new_buffer));
      }
      bytes_read += bytes_to_copy;
    }
    return bytes_read;
  }

  if (!stream_) {
    // If the stream is already closed, there is no body to read.
    return response_status_;
  }

  CHECK(callback_.is_null());
  CHECK(!user_buffer_);
  CHECK_EQ(0, user_buffer_len_);

  callback_ = callback;
  user_buffer_ = buf;
  user_buffer_len_ = buf_len;
  return ERR_IO_PENDING;
}

void QuicHttpStream::Close(bool not_reusable) {
  // Note: the not_reusable flag has no meaning for SPDY streams.
  if (stream_) {
    stream_->SetDelegate(NULL);
    stream_->Close(QUIC_NO_ERROR);
    stream_ = NULL;
  }
}

HttpStream* QuicHttpStream::RenewStreamForAuth() {
  return NULL;
}

bool QuicHttpStream::IsResponseBodyComplete() const {
  return io_state_ == STATE_OPEN && !stream_;
}

bool QuicHttpStream::CanFindEndOfResponse() const {
  return true;
}

bool QuicHttpStream::IsMoreDataBuffered() const {
  return false;
}

bool QuicHttpStream::IsConnectionReused() const {
  // TODO(rch): do something smarter here.
  return stream_ && stream_->id() > 1;
}

void QuicHttpStream::SetConnectionReused() {
  // QUIC doesn't need an indicator here.
}

bool QuicHttpStream::IsConnectionReusable() const {
  // QUIC streams aren't considered reusable.
  return false;
}

bool QuicHttpStream::GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const {
  // TODO(mmenke):  Figure out what to do here.
  return true;
}

void QuicHttpStream::GetSSLInfo(SSLInfo* ssl_info) {
  DCHECK(stream_);
  NOTIMPLEMENTED();
}

void QuicHttpStream::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {
  DCHECK(stream_);
  NOTIMPLEMENTED();
}

bool QuicHttpStream::IsSpdyHttpStream() const {
  return false;
}

void QuicHttpStream::Drain(HttpNetworkSession* session) {
  Close(false);
  delete this;
}

int QuicHttpStream::OnSendData() {
  // TODO(rch): Change QUIC IO to provide notifications to the streams.
  NOTREACHED();
  return OK;
}

int QuicHttpStream::OnSendDataComplete(int status, bool* eof) {
  // TODO(rch): Change QUIC IO to provide notifications to the streams.
  NOTREACHED();
  return OK;
}

int QuicHttpStream::OnDataReceived(const char* data, int length) {
  // Are we still reading the response headers.
  if (!response_headers_received_) {
    // Grow the read buffer if necessary.
    if (read_buf_->RemainingCapacity() < length) {
      read_buf_->SetCapacity(read_buf_->capacity() + kHeaderBufInitialSize);
    }
    memcpy(read_buf_->data(), data, length);
    read_buf_->set_offset(read_buf_->offset() + length);
    int rv = ParseResponseHeaders();
    if (rv != ERR_IO_PENDING && !callback_.is_null()) {
      DoCallback(rv);
    }
    return OK;
  }

  if (callback_.is_null()) {
    BufferResponseBody(data, length);
    return OK;
  }

  if (length <= user_buffer_len_) {
    memcpy(user_buffer_->data(), data, length);
  } else {
    memcpy(user_buffer_->data(), data, user_buffer_len_);
    int delta = length - user_buffer_len_;
    BufferResponseBody(data + user_buffer_len_, delta);
  }
  user_buffer_ = NULL;
  user_buffer_len_ = 0;
  DoCallback(length);
  return OK;
}

void QuicHttpStream::OnClose(QuicErrorCode error) {
  if (error != QUIC_NO_ERROR) {
    response_status_ = ERR_QUIC_PROTOCOL_ERROR;
  } else if (!response_headers_received_) {
    response_status_ = ERR_ABORTED;
  }

  stream_ = NULL;
  if (!callback_.is_null())
    DoCallback(response_status_);
}

void QuicHttpStream::OnError(int error) {
  stream_ = NULL;
  response_status_ = error;
  if (!callback_.is_null())
    DoCallback(response_status_);
}

void QuicHttpStream::OnIOComplete(int rv) {
  rv = DoLoop(rv);

  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    DoCallback(rv);
  }
}

void QuicHttpStream::DoCallback(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  CHECK(!callback_.is_null());

  // The client callback can do anything, including destroying this class,
  // so any pending callback must be issued after everything else is done.
  base::ResetAndReturn(&callback_).Run(rv);
}

int QuicHttpStream::DoLoop(int rv) {
  do {
    switch (io_state_) {
      case STATE_SEND_HEADERS:
        CHECK_EQ(OK, rv);
        rv = DoSendHeaders();
        break;
      case STATE_SEND_HEADERS_COMPLETE:
        rv = DoSendHeadersComplete(rv);
        break;
      case STATE_READ_REQUEST_BODY:
        CHECK_EQ(OK, rv);
        rv = DoReadRequestBody();
        break;
      case STATE_READ_REQUEST_BODY_COMPLETE:
        rv = DoReadRequestBodyComplete(rv);
        break;
      case STATE_SEND_BODY:
        CHECK_EQ(OK, rv);
        rv = DoSendBody();
        break;
      case STATE_SEND_BODY_COMPLETE:
        rv = DoSendBodyComplete(rv);
        break;
      case STATE_OPEN:
        CHECK_EQ(OK, rv);
        break;
      default:
        NOTREACHED() << "io_state_: " << io_state_;
        break;
    }
  } while (io_state_ != STATE_NONE && io_state_ != STATE_OPEN &&
           rv != ERR_IO_PENDING);

  return rv;
}

int QuicHttpStream::DoSendHeaders() {
  if (!stream_)
    return ERR_UNEXPECTED;

  bool has_upload_data = request_body_stream_ != NULL;

  io_state_ = STATE_SEND_HEADERS_COMPLETE;
  QuicConsumedData rv = stream_->WriteData(request_, !has_upload_data);
  return rv.bytes_consumed;
}

int QuicHttpStream::DoSendHeadersComplete(int rv) {
  if (rv < 0) {
    io_state_ = STATE_NONE;
    return rv;
  }

  io_state_ = request_body_stream_ ?
      STATE_READ_REQUEST_BODY : STATE_OPEN;

  return OK;
}

int QuicHttpStream::DoReadRequestBody() {
  io_state_ = STATE_READ_REQUEST_BODY_COMPLETE;
  return request_body_stream_->Read(raw_request_body_buf_,
                                    raw_request_body_buf_->size(),
                                    base::Bind(&QuicHttpStream::OnIOComplete,
                                               weak_factory_.GetWeakPtr()));
}

int QuicHttpStream::DoReadRequestBodyComplete(int rv) {
  // |rv| is the result of read from the request body from the last call to
  // DoSendBody().
  if (rv < 0) {
    io_state_ = STATE_NONE;
    return rv;
  }

  request_body_buf_ = new DrainableIOBuffer(raw_request_body_buf_, rv);
  if (rv == 0) {  // Reached the end.
    DCHECK(request_body_stream_->IsEOF());
  }

  io_state_ = STATE_SEND_BODY;
  return OK;
}

int QuicHttpStream::DoSendBody() {
  if (!stream_)
    return ERR_UNEXPECTED;

  CHECK(request_body_stream_);
  CHECK(request_body_buf_);
  const bool eof = request_body_stream_->IsEOF();
  int len = request_body_buf_->BytesRemaining();
  if (len > 0 || eof) {
    base::StringPiece data(request_body_buf_->data(), len);
    QuicConsumedData rv = stream_->WriteData(data, eof);
    request_body_buf_->DidConsume(rv.bytes_consumed);
    if (eof) {
      io_state_ = STATE_OPEN;
      return OK;
    }
    return rv.bytes_consumed;
  }

  io_state_ = STATE_SEND_BODY_COMPLETE;
  return OK;
}

int QuicHttpStream::DoSendBodyComplete(int rv) {
  if (rv < 0) {
    io_state_ = STATE_NONE;
    return rv;
  }

  io_state_ = STATE_READ_REQUEST_BODY;
  return OK;
}

int QuicHttpStream::ParseResponseHeaders() {
  if (use_spdy_) {
    size_t read_buf_len = static_cast<size_t>(read_buf_->offset());
    SpdyFramer framer(3);
    SpdyHeaderBlock headers;
    char* data = read_buf_->StartOfBuffer();
    size_t len = framer.ParseHeaderBlockInBuffer(data, read_buf_->offset(),
                                                 &headers);

    if (len == 0) {
      return ERR_IO_PENDING;
    }

    // Save the remaining received data.
    size_t delta = read_buf_len - len;
    if (delta > 0) {
      BufferResponseBody(data + len, delta);
    }

    // The URLRequest logs these headers, so only log to the QuicSession's
    // net log.
    stream_->net_log().AddEvent(
        NetLog::TYPE_QUIC_HTTP_STREAM_READ_RESPONSE_HEADERS,
        base::Bind(&SpdyHeaderBlockNetLogCallback, &headers));

    if (!SpdyHeadersToHttpResponse(headers, 3, response_info_)) {
      DLOG(WARNING) << "Invalid headers";
      return ERR_QUIC_PROTOCOL_ERROR;
    }
    // Put the peer's IP address and port into the response.
    IPEndPoint address = stream_->GetPeerAddress();
    response_info_->socket_address = HostPortPair::FromIPEndPoint(address);
    response_info_->vary_data.Init(*request_info_, *response_info_->headers);
    response_headers_received_ = true;

    return OK;
  }
  int end_offset = HttpUtil::LocateEndOfHeaders(read_buf_->StartOfBuffer(),
                                                read_buf_->offset(), 0);

  if (end_offset == -1) {
    return ERR_IO_PENDING;
  }

  if (!stream_)
    return ERR_UNEXPECTED;

  scoped_refptr<HttpResponseHeaders> headers = new HttpResponseHeaders(
      HttpUtil::AssembleRawHeaders(read_buf_->StartOfBuffer(), end_offset));

  // Put the peer's IP address and port into the response.
  IPEndPoint address = stream_->GetPeerAddress();
  response_info_->socket_address = HostPortPair::FromIPEndPoint(address);
  response_info_->headers = headers;
  response_info_->vary_data.Init(*request_info_, *response_info_->headers);
  response_headers_received_ = true;

  // The URLRequest logs these headers, so only log to the QuicSession's
  // net log.
  stream_->net_log().AddEvent(
      NetLog::TYPE_QUIC_HTTP_STREAM_READ_RESPONSE_HEADERS,
      base::Bind(&HttpResponseHeaders::NetLogCallback,
                 response_info_->headers));

  // Save the remaining received data.
  int delta = read_buf_->offset() - end_offset;
  if (delta > 0) {
    BufferResponseBody(read_buf_->data(), delta);
  }

  return OK;
}

void QuicHttpStream::BufferResponseBody(const char* data, int length) {
  if (length == 0)
    return;
  IOBufferWithSize* io_buffer = new IOBufferWithSize(length);
  memcpy(io_buffer->data(), data, length);
  response_body_.push_back(make_scoped_refptr(io_buffer));
}

}  // namespace net
