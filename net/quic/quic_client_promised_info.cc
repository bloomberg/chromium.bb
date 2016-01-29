// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_client_promised_info.h"

#include "base/logging.h"
#include "net/quic/spdy_utils.h"
#include "net/tools/quic/quic_client_session.h"

using net::SpdyHeaderBlock;
using net::tools::kPushPromiseTimeoutSecs;

namespace net {

QuicClientPromisedInfo::QuicClientPromisedInfo(QuicClientSessionBase* session,
                                               QuicStreamId id,
                                               string url)
    : session_(session),
      helper_(session->connection()->helper()),
      id_(id),
      url_(url),
      listener_(nullptr) {}

QuicClientPromisedInfo::~QuicClientPromisedInfo() {
  DVLOG(1) << "~QuicClientPromisedInfo(): stream " << id_;
  MaybeNotifyListener();
}

QuicTime QuicClientPromisedInfo::CleanupAlarm::OnAlarm() {
  DVLOG(1) << "PromisedStream self GC alarm";
  promised_->session()->DeletePromised(promised_);
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
  request_headers_ = std::move(headers);
}

void QuicClientPromisedInfo::MaybeNotifyListener() {
  if (!listener_)
    return;
  listener_->OnResponse();
  listener_ = nullptr;
}

void QuicClientPromisedInfo::OnResponseHeaders(
    std::unique_ptr<SpdyHeaderBlock> headers) {
  response_headers_ = std::move(headers);
  MaybeNotifyListener();
}

void QuicClientPromisedInfo::Reset(QuicRstStreamErrorCode error_code) {
  session_->ResetPromised(id_, error_code);
  session_->DeletePromised(this);
}

}  // namespace net
