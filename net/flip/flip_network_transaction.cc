// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/flip/flip_network_transaction.h"

#include "base/scoped_ptr.h"
#include "base/compiler_specific.h"
#include "base/field_trial.h"
#include "base/string_util.h"
#include "base/trace_event.h"
#include "build/build_config.h"
#include "net/base/connection_type_histograms.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_basic_stream.h"
#include "net/http/http_chunked_decoder.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/ssl_client_socket.h"

using base::Time;

namespace net {

//-----------------------------------------------------------------------------

FlipNetworkTransaction::FlipNetworkTransaction(HttpNetworkSession* session)
    : flip_request_id_(0),
      user_callback_(0),
      user_buffer_bytes_remaining_(0),
      session_(session),
      request_(NULL),
      response_complete_(false),
      response_status_(net::OK),
      next_state_(STATE_NONE) {
}

FlipNetworkTransaction::~FlipNetworkTransaction() {
  LOG(INFO) << "FlipNetworkTransaction dead. " << this;
  if (flip_ && flip_request_id_)
    flip_->CancelStream(flip_request_id_);
}

const HttpRequestInfo* FlipNetworkTransaction::request() {
  return request_;
}

const UploadDataStream* FlipNetworkTransaction::data() {
  return request_body_stream_.get();
}

void FlipNetworkTransaction::OnRequestSent(int status) {
  if (status == OK)
    next_state_ = STATE_SEND_REQUEST_COMPLETE;
  else
    next_state_ = STATE_NONE;

  DoLoop(status);
}

void FlipNetworkTransaction::OnUploadDataSent(int result) {
  NOTIMPLEMENTED();
}

void FlipNetworkTransaction::OnResponseReceived(HttpResponseInfo* response) {
  next_state_ = STATE_READ_HEADERS_COMPLETE;

  response_ = *response;  // TODO(mbelshe): avoid copy.

  DoLoop(net::OK);
}

void FlipNetworkTransaction::OnDataReceived(const char* buffer, int bytes) {
  // TODO(mbelshe): if data is received before a syn reply, this will crash.

  DCHECK(bytes >= 0);
  next_state_ = STATE_READ_BODY;

  if (bytes > 0) {
    DCHECK(buffer);

    // TODO(mbelshe): If read is pending, we should copy the data straight into
    // the read buffer here.  For now, we'll queue it always.

    IOBufferWithSize* io_buffer = new IOBufferWithSize(bytes);
    memcpy(io_buffer->data(), buffer, bytes);

    response_body_.push_back(io_buffer);
  }
  DoLoop(net::OK);
}

void FlipNetworkTransaction::OnClose(int status) {
  next_state_ = STATE_READ_BODY_COMPLETE;
  response_complete_ = true;
  response_status_ = status;
  flip_request_id_ = 0;  // TODO(mbelshe) - do we need this?
  DoLoop(status);
}

int FlipNetworkTransaction::Start(const HttpRequestInfo* request_info,
                                  CompletionCallback* callback,
                                  LoadLog* load_log) {
  request_ = request_info;
  start_time_ = base::Time::Now();

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
  DCHECK(buf_len > 0);
  DCHECK(callback);
  DCHECK(flip_.get());

  // If we have data buffered, complete the IO immediately.
  if (response_body_.size()) {
    int bytes_read = 0;
    while (response_body_.size() && buf_len > 0) {
      scoped_refptr<IOBufferWithSize> data = response_body_.front();
      int bytes_to_copy = std::min(buf_len, data->size());
      memcpy(&(buf->data()[bytes_read]), data->data(), bytes_to_copy);
      buf_len -= bytes_to_copy;
      if (bytes_to_copy == data->size()) {
        response_body_.pop_front();
      } else {
        int bytes_remaining = data->size() - bytes_to_copy;
        IOBufferWithSize* new_buffer = new IOBufferWithSize(bytes_remaining);
        memcpy(new_buffer->data(), &(data->data()[bytes_to_copy]),
               bytes_remaining);
        response_body_.pop_front();
        response_body_.push_front(new_buffer);
      }
      bytes_read += bytes_to_copy;
    }
    return bytes_read;
  }

  if (response_complete_)
    return 0;  // We already finished reading this stream.

  user_callback_ = callback;
  user_buffer_ = buf;
  user_buffer_bytes_remaining_ = buf_len;
  return net::ERR_IO_PENDING;
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
  if (!request_body_stream_.get())
    return 0;

  return request_body_stream_->position();
}


void FlipNetworkTransaction::DoHttpTransactionCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);

  // Because IO is asynchronous from the caller, we could be completing an
  // IO even though the user hasn't issued a Read() yet.
  if (!user_callback_)
    return;

  // Since Run may result in Read being called, clear user_callback_ up front.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
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
        rv = DoInitConnection();
        break;
      case STATE_INIT_CONNECTION_COMPLETE:
        rv = DoInitConnectionComplete(rv);
        break;
      case STATE_SEND_REQUEST:
        DCHECK_EQ(OK, rv);
        rv = DoSendRequest();
        break;
      case STATE_SEND_REQUEST_COMPLETE:
        rv = DoSendRequestComplete(rv);
        break;
      case STATE_READ_HEADERS:
        DCHECK_EQ(OK, rv);
        rv = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        // DoReadHeadersComplete only returns OK  becaue the transaction
        // could be destroyed as part of the call.
        rv = DoReadHeadersComplete(rv);
        DCHECK(rv == net::OK);
        return rv;
        break;
      case STATE_READ_BODY:
        DCHECK_EQ(OK, rv);
        rv = DoReadBody();
        // DoReadBody only returns OK  becaue the transaction could be
        // destroyed as part of the call.
        DCHECK(rv == net::OK);
        return rv;
        break;
      case STATE_READ_BODY_COMPLETE:
        // We return here because the Transaction could be destroyed after this
        // call.
        rv = DoReadBodyComplete(rv);
        DCHECK(rv == net::OK);
        return rv;
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

  std::string connection_group = "flip.";
  connection_group.append(host);

  HostResolver::RequestInfo resolve_info(host, port);

// TODO(mbelshe): Cleanup these testing tricks.
// If we want to use multiple connections, grab the flip session
// up front using the original domain name.
#undef USE_MULTIPLE_CONNECTIONS
#undef DIVERT_URLS_TO_TEST_SERVER
#if defined(USE_MULTIPLE_CONNECTIONS) || !defined(DIVERT_URLS_TO_TEST_SERVER)
  flip_ = FlipSession::GetFlipSession(resolve_info, session_);
#endif

// Use this to divert URLs to a test server.
#ifdef DIVERT_URLS_TO_TEST_SERVER
  // Fake out this session to go to our test server.
  host = "servername";
  port = 443;
  resolve_info = HostResolver::RequestInfo(host, port);
#ifndef USE_MULTIPLE_CONNECTIONS
  flip_ = FlipSession::GetFlipSession(resolve_info, session_);
#endif  // USE_MULTIPLE_CONNECTIONS

#endif  // DIVERT_URLS_TO_TEST_SERVER

  int rv = flip_->Connect(connection_group, resolve_info, request_->priority);
  DCHECK(rv == net::OK);  // The API says it will always return OK.
  return net::OK;
}

int FlipNetworkTransaction::DoInitConnectionComplete(int result) {
  if (result < 0)
    return result;

  next_state_ = STATE_SEND_REQUEST;
  return net::OK;
}

int FlipNetworkTransaction::DoSendRequest() {
  // TODO(mbelshe): rethink this UploadDataStream wrapper.
  if (request_->upload_data)
    request_body_stream_.reset(new UploadDataStream(request_->upload_data));

  flip_request_id_ = flip_->CreateStream(this);

  if (response_complete_)
    return net::OK;

  // The FlipSession will always call us back when the send is complete.
  return net::ERR_IO_PENDING;
}

int FlipNetworkTransaction::DoSendRequestComplete(int result) {
  if (result < 0)
    return result;

  next_state_ = STATE_READ_HEADERS;
  return net::OK;
}

int FlipNetworkTransaction::DoReadHeaders() {
  // The FlipSession will always call us back when the headers have been
  // received.
  return net::ERR_IO_PENDING;
}

int FlipNetworkTransaction::DoReadHeadersComplete(int result) {
  // Notify the user that the headers are ready.
  DoHttpTransactionCallback(result);
  return net::OK;
}

int FlipNetworkTransaction::DoReadBody() {
  // The caller has not issued a read request yet.
  if (!user_callback_)
    return net::OK;

  int bytes_read = 0;
  while (response_body_.size() && user_buffer_bytes_remaining_ > 0) {
    scoped_refptr<IOBufferWithSize> data = response_body_.front();
    int bytes_to_copy = std::min(user_buffer_bytes_remaining_, data->size());
    memcpy(&(user_buffer_->data()[bytes_read]), data->data(), bytes_to_copy);
    user_buffer_bytes_remaining_ -= bytes_to_copy;
    if (bytes_to_copy == data->size()) {
      response_body_.pop_front();
    } else {
      int bytes_remaining = data->size() - bytes_to_copy;
      IOBufferWithSize* new_buffer = new IOBufferWithSize(bytes_remaining);
      memcpy(new_buffer->data(), &(data->data()[bytes_to_copy]),
             bytes_remaining);
      response_body_.pop_front();
      response_body_.push_front(new_buffer);
    }
    bytes_read += bytes_to_copy;
  }

  DoHttpTransactionCallback(bytes_read);
  return net::OK;
}

int FlipNetworkTransaction::DoReadBodyComplete(int result) {
  // TODO(mbelshe): record success or failure of the transaction?
  if (user_callback_)
    DoHttpTransactionCallback(result);
  return OK;
}

}  // namespace net
