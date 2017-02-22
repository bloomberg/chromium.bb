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
  reliable_ = (init.ordered && init.maxRetransmits == -1 &&
               init.maxRetransmitTime == -1);
}

MockWebRTCDataChannelHandler::~MockWebRTCDataChannelHandler() {}

void MockWebRTCDataChannelHandler::setClient(
    WebRTCDataChannelHandlerClient* client) {
  client_ = client;
  if (client_)
    delegate_->PostTask(
        base::Bind(&MockWebRTCDataChannelHandler::ReportOpenedState,
                   weak_factory_.GetWeakPtr()));
}

blink::WebString MockWebRTCDataChannelHandler::label() {
  return label_;
}

bool MockWebRTCDataChannelHandler::isReliable() {
  return reliable_;
}

bool MockWebRTCDataChannelHandler::ordered() const {
  return init_.ordered;
}

unsigned short MockWebRTCDataChannelHandler::maxRetransmitTime() const {
  return init_.maxRetransmitTime;
}

unsigned short MockWebRTCDataChannelHandler::maxRetransmits() const {
  return init_.maxRetransmits;
}

WebString MockWebRTCDataChannelHandler::protocol() const {
  return init_.protocol;
}

bool MockWebRTCDataChannelHandler::negotiated() const {
  return init_.negotiated;
}

unsigned short MockWebRTCDataChannelHandler::id() const {
  return init_.id;
}

blink::WebRTCDataChannelHandlerClient::ReadyState
MockWebRTCDataChannelHandler::state() const {
  return blink::WebRTCDataChannelHandlerClient::ReadyStateConnecting;
}

unsigned long MockWebRTCDataChannelHandler::bufferedAmount() {
  return 0;
}

bool MockWebRTCDataChannelHandler::sendStringData(const WebString& data) {
  DCHECK(client_);
  client_->didReceiveStringData(data);
  return true;
}

bool MockWebRTCDataChannelHandler::sendRawData(const char* data, size_t size) {
  DCHECK(client_);
  client_->didReceiveRawData(data, size);
  return true;
}

void MockWebRTCDataChannelHandler::close() {
  DCHECK(client_);
  delegate_->PostTask(
      base::Bind(&MockWebRTCDataChannelHandler::ReportClosedState,
                 weak_factory_.GetWeakPtr()));
}

void MockWebRTCDataChannelHandler::ReportOpenedState() {
  client_->didChangeReadyState(WebRTCDataChannelHandlerClient::ReadyStateOpen);
}

void MockWebRTCDataChannelHandler::ReportClosedState() {
  client_->didChangeReadyState(
      WebRTCDataChannelHandlerClient::ReadyStateClosed);
}

}  // namespace test_runner
