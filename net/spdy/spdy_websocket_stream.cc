// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_websocket_stream.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream.h"

namespace net {

SpdyWebSocketStream::SpdyWebSocketStream(
    SpdySession* spdy_session, Delegate* delegate)
    : weak_ptr_factory_(this),
      spdy_session_(spdy_session),
      delegate_(delegate) {
  DCHECK(spdy_session_);
  DCHECK(delegate_);
}

SpdyWebSocketStream::~SpdyWebSocketStream() {
  delegate_ = NULL;
  Close();
}

int SpdyWebSocketStream::InitializeStream(const GURL& url,
                                          RequestPriority request_priority,
                                          const BoundNetLog& net_log) {
  if (spdy_session_->IsClosed())
    return ERR_SOCKET_NOT_CONNECTED;

  int rv = stream_request_.StartRequest(
      spdy_session_, url, request_priority, net_log,
      base::Bind(&SpdyWebSocketStream::OnSpdyStreamCreated,
                 weak_ptr_factory_.GetWeakPtr()));

  if (rv == OK) {
    stream_ = stream_request_.ReleaseStream();
    DCHECK(stream_);
    stream_->SetDelegate(this);
  }
  return rv;
}

int SpdyWebSocketStream::SendRequest(scoped_ptr<SpdyHeaderBlock> headers) {
  if (!stream_) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }
  stream_->set_spdy_headers(headers.Pass());
  int result = stream_->SendRequest(true);
  if (result < OK && result != ERR_IO_PENDING)
    Close();
  return result;
}

int SpdyWebSocketStream::SendData(const char* data, int length) {
  if (!stream_) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }
  scoped_refptr<IOBuffer> buf(new IOBuffer(length));
  memcpy(buf->data(), data, length);
  stream_->QueueStreamData(buf.get(), length, DATA_FLAG_NONE);
  return ERR_IO_PENDING;
}

void SpdyWebSocketStream::Close() {
  if (stream_) {
    stream_->Close();
    DCHECK(!stream_);
  }
}

SpdySendStatus SpdyWebSocketStream::OnSendHeadersComplete() {
  DCHECK(delegate_);
  delegate_->OnSentSpdyHeaders();
  return NO_MORE_DATA_TO_SEND;
}

int SpdyWebSocketStream::OnSendBody() {
  NOTREACHED();
  return ERR_UNEXPECTED;
}

SpdySendStatus SpdyWebSocketStream::OnSendBodyComplete(size_t bytes_sent) {
  NOTREACHED();
  return NO_MORE_DATA_TO_SEND;
}

int SpdyWebSocketStream::OnResponseReceived(
    const SpdyHeaderBlock& response,
    base::Time response_time, int status) {
  DCHECK(delegate_);
  return delegate_->OnReceivedSpdyResponseHeader(response, status);
}

void SpdyWebSocketStream::OnHeadersSent() {
  // This will be called when WebSocket over SPDY supports new framing.
  NOTREACHED();
}

int SpdyWebSocketStream::OnDataReceived(scoped_ptr<SpdyBuffer> buffer) {
  DCHECK(delegate_);
  delegate_->OnReceivedSpdyData(buffer.Pass());
  return OK;
}

void SpdyWebSocketStream::OnDataSent(size_t bytes_sent) {
  DCHECK(delegate_);
  delegate_->OnSentSpdyData(bytes_sent);
}

void SpdyWebSocketStream::OnClose(int status) {
  stream_.reset();

  // Destruction without Close() call OnClose() with delegate_ being NULL.
  if (!delegate_)
    return;
  Delegate* delegate = delegate_;
  delegate_ = NULL;
  delegate->OnCloseSpdyStream();
}

void SpdyWebSocketStream::OnSpdyStreamCreated(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  if (result == OK) {
    stream_ = stream_request_.ReleaseStream();
    DCHECK(stream_);
    stream_->SetDelegate(this);
  }
  DCHECK(delegate_);
  delegate_->OnCreatedSpdyStream(result);
}

}  // namespace net
