// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_network_transaction.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stats_counters.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/socket/tcp_client_socket_pool.h"
#include "net/spdy/spdy_http_stream.h"

using base::Time;

namespace net {

//-----------------------------------------------------------------------------

SpdyNetworkTransaction::SpdyNetworkTransaction(HttpNetworkSession* session)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
        io_callback_(this, &SpdyNetworkTransaction::OnIOComplete)),
      user_callback_(NULL),
      user_buffer_len_(0),
      session_(session),
      request_(NULL),
      next_state_(STATE_NONE),
      stream_(NULL) {
}

SpdyNetworkTransaction::~SpdyNetworkTransaction() {
  LOG(INFO) << "SpdyNetworkTransaction dead. " << this;
  if (stream_.get())
    stream_->Cancel();
}

int SpdyNetworkTransaction::Start(const HttpRequestInfo* request_info,
                                  CompletionCallback* callback,
                                  const BoundNetLog& net_log) {
  CHECK(request_info);
  CHECK(callback);

  SIMPLE_STATS_COUNTER("SpdyNetworkTransaction.Count");

  net_log_ = net_log;
  request_ = request_info;
  start_time_ = base::TimeTicks::Now();

  next_state_ = STATE_INIT_CONNECTION;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int SpdyNetworkTransaction::RestartIgnoringLastError(
    CompletionCallback* callback) {
  // TODO(mbelshe): implement me.
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int SpdyNetworkTransaction::RestartWithCertificate(
    X509Certificate* client_cert, CompletionCallback* callback) {
  // TODO(mbelshe): implement me.
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int SpdyNetworkTransaction::RestartWithAuth(
    const std::wstring& username,
    const std::wstring& password,
    CompletionCallback* callback) {
  // TODO(mbelshe): implement me.
  NOTIMPLEMENTED();
  return 0;
}

int SpdyNetworkTransaction::Read(IOBuffer* buf, int buf_len,
                                 CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);
  DCHECK(callback);

  user_buffer_ = buf;
  user_buffer_len_ = buf_len;

  next_state_ = STATE_READ_BODY;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

const HttpResponseInfo* SpdyNetworkTransaction::GetResponseInfo() const {
  return (response_.headers || response_.ssl_info.cert) ? &response_ : NULL;
}

LoadState SpdyNetworkTransaction::GetLoadState() const {
  switch (next_state_) {
    case STATE_INIT_CONNECTION_COMPLETE:
      if (spdy_.get())
        return spdy_->GetLoadState();
      return LOAD_STATE_CONNECTING;
    case STATE_GET_STREAM_COMPLETE:
    case STATE_SEND_REQUEST_COMPLETE:
      return LOAD_STATE_SENDING_REQUEST;
    case STATE_READ_HEADERS_COMPLETE:
      return LOAD_STATE_WAITING_FOR_RESPONSE;
    case STATE_READ_BODY_COMPLETE:
      return LOAD_STATE_READING_RESPONSE;
    default:
      return LOAD_STATE_IDLE;
  }
}

uint64 SpdyNetworkTransaction::GetUploadProgress() const {
  if (!stream_.get())
    return 0;

  return stream_->GetUploadProgress();
}

void SpdyNetworkTransaction::DoCallback(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  CHECK(user_callback_);

  // Since Run may result in Read being called, clear user_callback_ up front.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
}

void SpdyNetworkTransaction::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int SpdyNetworkTransaction::DoLoop(int result) {
  DCHECK(next_state_ != STATE_NONE);
  DCHECK(request_);

  if (!request_)
    return 0;

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_INIT_CONNECTION:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_SPDY_TRANSACTION_INIT_CONNECTION,
                            NULL);
        rv = DoInitConnection();
        break;
      case STATE_INIT_CONNECTION_COMPLETE:
        net_log_.EndEvent(NetLog::TYPE_SPDY_TRANSACTION_INIT_CONNECTION, NULL);
        rv = DoInitConnectionComplete(rv);
        break;
      case STATE_GET_STREAM:
        DCHECK_EQ(OK, rv);
        rv = DoGetStream();
        break;
      case STATE_GET_STREAM_COMPLETE:
        rv = DoGetStreamComplete(rv);
      case STATE_SEND_REQUEST:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_SPDY_TRANSACTION_SEND_REQUEST, NULL);
        rv = DoSendRequest();
        break;
      case STATE_SEND_REQUEST_COMPLETE:
        net_log_.EndEvent(NetLog::TYPE_SPDY_TRANSACTION_SEND_REQUEST, NULL);
        rv = DoSendRequestComplete(rv);
        break;
      case STATE_READ_HEADERS:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_SPDY_TRANSACTION_READ_HEADERS, NULL);
        rv = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        net_log_.EndEvent(NetLog::TYPE_SPDY_TRANSACTION_READ_HEADERS, NULL);
        rv = DoReadHeadersComplete(rv);
        break;
      case STATE_READ_BODY:
        DCHECK_EQ(OK, rv);
        net_log_.BeginEvent(NetLog::TYPE_SPDY_TRANSACTION_READ_BODY, NULL);
        rv = DoReadBody();
        break;
      case STATE_READ_BODY_COMPLETE:
        net_log_.EndEvent(NetLog::TYPE_SPDY_TRANSACTION_READ_BODY, NULL);
        rv = DoReadBodyComplete(rv);
        break;
      case STATE_NONE:
        rv = ERR_FAILED;
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int SpdyNetworkTransaction::DoInitConnection() {
  next_state_ = STATE_INIT_CONNECTION_COMPLETE;

  std::string host = request_->url.HostNoBrackets();
  int port = request_->url.EffectiveIntPort();

  // Use the fixed testing ports if they've been provided.  This is useful for
  // debugging.
  if (SpdySession::SSLMode()) {
    if (session_->fixed_https_port() != 0)
      port = session_->fixed_https_port();
  } else if (session_->fixed_http_port() != 0) {
    port = session_->fixed_http_port();
  }

  std::string connection_group = "spdy.";
  connection_group.append(host);

  HostPortPair host_port_pair(host, port);
  scoped_refptr<TCPSocketParams> tcp_params =
      new TCPSocketParams(host_port_pair, request_->priority,
                          request_->referrer, false);

  spdy_ = session_->spdy_session_pool()->Get(
      host_port_pair, session_, net_log_);
  DCHECK(spdy_);

  return spdy_->Connect(
      connection_group, tcp_params, request_->priority);
}

int SpdyNetworkTransaction::DoInitConnectionComplete(int result) {
  if (result < 0)
    return result;

  next_state_ = STATE_GET_STREAM;
  return OK;
}

int SpdyNetworkTransaction::DoGetStream() {
  next_state_ = STATE_GET_STREAM_COMPLETE;

  // It is possible that the spdy session was shut down while it was
  // asynchronously waiting to connect.
  if (spdy_->IsClosed())
    return ERR_CONNECTION_CLOSED;

  CHECK(!stream_.get());

  stream_.reset(new SpdyHttpStream());
  return stream_->InitializeStream(spdy_, *request_,
                                   net_log_, &io_callback_);
}

int SpdyNetworkTransaction::DoGetStreamComplete(int result) {
  if (result < 0) {
    return result;
  }

  next_state_ = STATE_SEND_REQUEST;
  return OK;
}

int SpdyNetworkTransaction::DoSendRequest() {
  next_state_ = STATE_SEND_REQUEST_COMPLETE;

  UploadDataStream* upload_data_stream = NULL;
  if (request_->upload_data) {
    int error_code;
    upload_data_stream = UploadDataStream::Create(request_->upload_data,
                                                  &error_code);
    if (!upload_data_stream)
      return error_code;
  }
  stream_->InitializeRequest(base::Time::Now(), upload_data_stream);
  spdy_ = NULL;

  return stream_->SendRequest(&response_, &io_callback_);
}

int SpdyNetworkTransaction::DoSendRequestComplete(int result) {
  if (result < 0) {
    stream_.reset();
    return result;
  }

  next_state_ = STATE_READ_HEADERS;
  return OK;
}

int SpdyNetworkTransaction::DoReadHeaders() {
  next_state_ = STATE_READ_HEADERS_COMPLETE;
  return stream_->ReadResponseHeaders(&io_callback_);
}

int SpdyNetworkTransaction::DoReadHeadersComplete(int result) {
  // TODO(willchan): Flesh out the support for HTTP authentication here.
  return result;
}

int SpdyNetworkTransaction::DoReadBody() {
  next_state_ = STATE_READ_BODY_COMPLETE;

  return stream_->ReadResponseBody(
      user_buffer_, user_buffer_len_, &io_callback_);
}

int SpdyNetworkTransaction::DoReadBodyComplete(int result) {
  user_buffer_ = NULL;
  user_buffer_len_ = 0;

  if (result <= 0)
    stream_.reset();

  return result;
}

}  // namespace net
