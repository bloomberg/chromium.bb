// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/mock_webrtc_data_channel_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandlerClient.h"

using namespace blink;

namespace test_runner {

MockWebRTCDataChannelHandler::MockWebRTCDataChannelHandler(
    WebString label,
    const WebRTCDataChannelInit& init,
    WebTestDelegate* delegate)
    : client_(0),
      label_(label),
      init_(init),
      delegate_(delegate),
      weak_factory_(this) {
  reliable_ = (init.ordered && init.max_retransmits == -1 &&
               init.max_retransmit_time == -1);
}

MockWebRTCDataChannelHandler::~MockWebRTCDataChannelHandler() {}

void MockWebRTCDataChannelHandler::SetClient(
    WebRTCDataChannelHandlerClient* client) {
  client_ = client;
  if (client_)
    delegate_->PostTask(
        base::Bind(&MockWebRTCDataChannelHandler::ReportOpenedState,
                   weak_factory_.GetWeakPtr()));
}

blink::WebString MockWebRTCDataChannelHandler::Label() {
  return label_;
}

bool MockWebRTCDataChannelHandler::IsReliable() {
  return reliable_;
}

bool MockWebRTCDataChannelHandler::Ordered() const {
  return init_.ordered;
}

unsigned short MockWebRTCDataChannelHandler::MaxRetransmitTime() const {
  return init_.max_retransmit_time;
}

unsigned short MockWebRTCDataChannelHandler::MaxRetransmits() const {
  return init_.max_retransmits;
}

WebString MockWebRTCDataChannelHandler::Protocol() const {
  return init_.protocol;
}

bool MockWebRTCDataChannelHandler::Negotiated() const {
  return init_.negotiated;
}

unsigned short MockWebRTCDataChannelHandler::Id() const {
  return init_.id;
}

blink::WebRTCDataChannelHandlerClient::ReadyState
MockWebRTCDataChannelHandler::GetState() const {
  return blink::WebRTCDataChannelHandlerClient::kReadyStateConnecting;
}

unsigned long MockWebRTCDataChannelHandler::BufferedAmount() {
  return 0;
}

bool MockWebRTCDataChannelHandler::SendStringData(const WebString& data) {
  DCHECK(client_);
  client_->DidReceiveStringData(data);
  return true;
}

bool MockWebRTCDataChannelHandler::SendRawData(const char* data, size_t size) {
  DCHECK(client_);
  client_->DidReceiveRawData(data, size);
  return true;
}

void MockWebRTCDataChannelHandler::Close() {
  DCHECK(client_);
  delegate_->PostTask(
      base::Bind(&MockWebRTCDataChannelHandler::ReportClosedState,
                 weak_factory_.GetWeakPtr()));
}

void MockWebRTCDataChannelHandler::ReportOpenedState() {
  client_->DidChangeReadyState(WebRTCDataChannelHandlerClient::kReadyStateOpen);
}

void MockWebRTCDataChannelHandler::ReportClosedState() {
  client_->DidChangeReadyState(
      WebRTCDataChannelHandlerClient::kReadyStateClosed);
}

}  // namespace test_runner
