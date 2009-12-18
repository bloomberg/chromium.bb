// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/flip/flip_network_transaction.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/upload_data_stream.h"
#include "net/flip/flip_stream.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"

using base::Time;

namespace net {

//-----------------------------------------------------------------------------

FlipNetworkTransaction::FlipNetworkTransaction(HttpNetworkSession* session)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
        io_callback_(this, &FlipNetworkTransaction::OnIOComplete)),
      user_callback_(NULL),
      user_buffer_len_(0),
      session_(session),
      request_(NULL),
      next_state_(STATE_NONE),
      stream_(NULL) {
}

FlipNetworkTransaction::~FlipNetworkTransaction() {
  LOG(INFO) << "FlipNetworkTransaction dead. " << this;
  if (stream_.get())
    stream_->Cancel();
}

int FlipNetworkTransaction::Start(const HttpRequestInfo* request_info,
                                  CompletionCallback* callback,
                                  LoadLog* load_log) {
  CHECK(request_info);
  CHECK(callback);

  load_log_ = load_log;
  request_ = request_info;
  start_time_ = base::TimeTicks::Now();

  next_state_ = STATE_INIT_CONNECTION;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

int FlipNetworkTransaction::RestartIgnoringLastError(
    CompletionCallback* callback) {
  // TODO(mbelshe): implement me.
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int FlipNetworkTransaction::RestartWithCertificate(
    X509Certificate* client_cert, CompletionCallback* callback) {
  // TODO(mbelshe): implement me.
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int FlipNetworkTransaction::RestartWithAuth(
    const std::wstring& username,
    const std::wstring& password,
    CompletionCallback* callback) {
  // TODO(mbelshe): implement me.
  NOTIMPLEMENTED();
  return 0;
}

int FlipNetworkTransaction::Read(IOBuffer* buf, int buf_len,
                                 CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);
  DCHECK(callback);
  DCHECK(flip_.get());

  user_buffer_ = buf;
  user_buffer_len_ = buf_len;

  next_state_ = STATE_READ_BODY;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = callback;
  return rv;
}

const HttpResponseInfo* FlipNetworkTransaction::GetResponseInfo() const {
  return (response_.headers || response_.ssl_info.cert) ? &response_ : NULL;
}

LoadState FlipNetworkTransaction::GetLoadState() const {
  switch (next_state_) {
    case STATE_INIT_CONNECTION_COMPLETE:
      if (flip_.get())
        return flip_->GetLoadState();
      return LOAD_STATE_CONNECTING;
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

uint64 FlipNetworkTransaction::GetUploadProgress() const {
  if (!stream_.get())
    return 0;

  return stream_->GetUploadProgress();
}

void FlipNetworkTransaction::DoCallback(int rv) {
  CHECK(rv != ERR_IO_PENDING);
  CHECK(user_callback_);

  // Since Run may result in Read being called, clear user_callback_ up front.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
}

void FlipNetworkTransaction::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int FlipNetworkTransaction::DoLoop(int result) {
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
        LoadLog::BeginEvent(load_log_,
                            LoadLog::TYPE_FLIP_TRANSACTION_INIT_CONNECTION);
        rv = DoInitConnection();
        break;
      case STATE_INIT_CONNECTION_COMPLETE:
        LoadLog::EndEvent(load_log_,
                          LoadLog::TYPE_FLIP_TRANSACTION_INIT_CONNECTION);
        rv = DoInitConnectionComplete(rv);
        break;
      case STATE_SEND_REQUEST:
        DCHECK_EQ(OK, rv);
        LoadLog::BeginEvent(load_log_,
                            LoadLog::TYPE_FLIP_TRANSACTION_SEND_REQUEST);
        rv = DoSendRequest();
        break;
      case STATE_SEND_REQUEST_COMPLETE:
        LoadLog::EndEvent(load_log_,
                          LoadLog::TYPE_FLIP_TRANSACTION_SEND_REQUEST);
        rv = DoSendRequestComplete(rv);
        break;
      case STATE_READ_HEADERS:
        DCHECK_EQ(OK, rv);
        LoadLog::BeginEvent(load_log_,
                            LoadLog::TYPE_FLIP_TRANSACTION_READ_HEADERS);
        rv = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        LoadLog::EndEvent(load_log_,
                          LoadLog::TYPE_FLIP_TRANSACTION_READ_HEADERS);
        rv = DoReadHeadersComplete(rv);
        break;
      case STATE_READ_BODY:
        DCHECK_EQ(OK, rv);
        LoadLog::BeginEvent(load_log_,
                            LoadLog::TYPE_FLIP_TRANSACTION_READ_BODY);
        rv = DoReadBody();
        break;
      case STATE_READ_BODY_COMPLETE:
        LoadLog::EndEvent(load_log_,
                          LoadLog::TYPE_FLIP_TRANSACTION_READ_BODY);
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

int FlipNetworkTransaction::DoInitConnection() {
  next_state_ = STATE_INIT_CONNECTION_COMPLETE;

  std::string host = request_->url.HostNoBrackets();
  int port = request_->url.EffectiveIntPort();

  // Use the fixed testing ports if they've been provided.  This is useful for
  // debugging.
  if (FlipSession::SSLMode()) {
    if (session_->fixed_https_port() != 0)
      port = session_->fixed_https_port();
  } else if (session_->fixed_http_port() != 0) {
    port = session_->fixed_http_port();
  }

  std::string connection_group = "flip.";
  connection_group.append(host);

  HostResolver::RequestInfo resolve_info(host, port);

  flip_ = session_->flip_session_pool()->Get(resolve_info, session_);
  DCHECK(flip_);

  return flip_->Connect(
      connection_group, resolve_info, request_->priority, load_log_);
}

int FlipNetworkTransaction::DoInitConnectionComplete(int result) {
  if (result < 0)
    return result;

  next_state_ = STATE_SEND_REQUEST;
  return OK;
}

int FlipNetworkTransaction::DoSendRequest() {
  next_state_ = STATE_SEND_REQUEST_COMPLETE;
  CHECK(!stream_.get());
  UploadDataStream* upload_data = request_->upload_data ?
      new UploadDataStream(request_->upload_data) : NULL;
  stream_ = flip_->GetOrCreateStream(*request_, upload_data, load_log_.get());
  return stream_->SendRequest(upload_data, &response_, &io_callback_);
}

int FlipNetworkTransaction::DoSendRequestComplete(int result) {
  if (result < 0)
    return result;

  next_state_ = STATE_READ_HEADERS;
  return OK;
}

int FlipNetworkTransaction::DoReadHeaders() {
  next_state_ = STATE_READ_HEADERS_COMPLETE;
  return stream_->ReadResponseHeaders(&io_callback_);
}

int FlipNetworkTransaction::DoReadHeadersComplete(int result) {
  // TODO(willchan): Flesh out the support for HTTP authentication here.
  return OK;
}

int FlipNetworkTransaction::DoReadBody() {
  next_state_ = STATE_READ_BODY_COMPLETE;

  return stream_->ReadResponseBody(
      user_buffer_, user_buffer_len_, &io_callback_);
}

int FlipNetworkTransaction::DoReadBodyComplete(int result) {
  user_buffer_ = NULL;
  user_buffer_len_ = 0;

  if (result <= 0)
    stream_ = NULL;

  return result;
}

}  // namespace net
