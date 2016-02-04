// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_client_promised_info.h"

#include "base/logging.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/spdy_utils.h"

using net::SpdyHeaderBlock;
using net::kPushPromiseTimeoutSecs;

namespace net {

QuicClientPromisedInfo::QuicClientPromisedInfo(QuicClientSessionBase* session,
                                               QuicStreamId id,
                                               string url)
    : session_(session),
      helper_(session->connection()->helper()),
      id_(id),
      url_(url),
      stream_visitor_(nullptr),
      client_request_delegate_(nullptr) {}

QuicClientPromisedInfo::~QuicClientPromisedInfo() {}

QuicTime QuicClientPromisedInfo::CleanupAlarm::OnAlarm() {
  DVLOG(1) << "self GC alarm for stream " << promised_->id_;
  promised_->Reset(QUIC_STREAM_CANCELLED);
  return QuicTime::Zero();
}

void QuicClientPromisedInfo::Init() {
  cleanup_alarm_.reset(
      helper_->CreateAlarm(new QuicClientPromisedInfo::CleanupAlarm(this)));
  cleanup_alarm_->Set(helper_->GetClock()->ApproximateNow().Add(
      QuicTime::Delta::FromSeconds(kPushPromiseTimeoutSecs)));
}

void QuicClientPromisedInfo::OnPromiseHeaders(
    std::unique_ptr<SpdyHeaderBlock> headers) {
  if (!SpdyUtils::UrlIsValid(*headers)) {
    DVLOG(1) << "Promise for stream " << id_ << " has invalid URL " << url_;
    Reset(QUIC_INVALID_PROMISE_URL);
    return;
  }
  if (!session_->IsAuthorized(
          SpdyUtils::GetHostNameFromHeaderBlock(*headers))) {
    Reset(QUIC_UNAUTHORIZED_PROMISE_URL);
    return;
  }
  request_headers_ = std::move(headers);
}

void QuicClientPromisedInfo::OnResponseHeaders(
    std::unique_ptr<SpdyHeaderBlock> headers) {
  response_headers_ = std::move(headers);
  if (client_request_delegate_) {
    // We already have a client request waiting.
    FinalValidation();
  }
}

void QuicClientPromisedInfo::Reset(QuicRstStreamErrorCode error_code) {
  QuicClientPushPromiseIndex::Delegate* delegate = client_request_delegate_;
  session_->ResetPromised(id_, error_code);
  session_->DeletePromised(this);
  if (delegate) {
    delegate->OnResponse(nullptr);
  }
}

QuicAsyncStatus QuicClientPromisedInfo::AcceptMatchingRequest(
    QuicSpdyStream::Visitor* visitor) {
  QuicSpdyStream* stream = session_->GetPromisedStream(id_);
  QuicClientPushPromiseIndex::Delegate* delegate = client_request_delegate_;
  // Stream can start draining now
  if (delegate) {
    delegate->OnResponse(stream);
  }
  if (stream) {
    stream->set_visitor(stream_visitor_);
    stream->OnDataAvailable();
  }
  session_->DeletePromised(this);
  if (stream) {
    return QUIC_SUCCESS;
  }
  return QUIC_FAILURE;
}

QuicAsyncStatus QuicClientPromisedInfo::FinalValidation() {
  if (!client_request_delegate_->CheckVary(
          *client_request_headers_, *request_headers_, *response_headers_)) {
    Reset(QUIC_PROMISE_VARY_MISMATCH);
    return QUIC_FAILURE;
  }
  return AcceptMatchingRequest(stream_visitor_);
}

QuicAsyncStatus QuicClientPromisedInfo::HandleClientRequest(
    std::unique_ptr<SpdyHeaderBlock> request_headers,
    QuicClientPushPromiseIndex::Delegate* delegate,
    QuicSpdyStream::Visitor* visitor) {
  client_request_delegate_ = delegate;
  stream_visitor_ = visitor;
  client_request_headers_ = std::move(request_headers);
  if (response_headers_.get()) {
    return FinalValidation();
  }
  return QUIC_PENDING;
}

void QuicClientPromisedInfo::Cancel() {
  // Don't fire OnResponse() for client initiated cancel.
  client_request_delegate_ = nullptr;
  Reset(QUIC_STREAM_CANCELLED);
}

}  // namespace net
