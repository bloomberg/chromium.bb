// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/mock_webrtc_data_channel_handler.h"

#include "base/logging.h"
#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandlerClient.h"

using namespace blink;

namespace content {

class DataChannelReadyStateTask
    : public WebMethodTask<MockWebRTCDataChannelHandler> {
 public:
  DataChannelReadyStateTask(MockWebRTCDataChannelHandler* object,
                            WebRTCDataChannelHandlerClient* data_channel_client,
                            WebRTCDataChannelHandlerClient::ReadyState state)
      : WebMethodTask<MockWebRTCDataChannelHandler>(object),
        data_channel_client_(data_channel_client),
        state_(state) {}

  virtual void runIfValid() OVERRIDE {
    data_channel_client_->didChangeReadyState(state_);
  }

 private:
  WebRTCDataChannelHandlerClient* data_channel_client_;
  WebRTCDataChannelHandlerClient::ReadyState state_;
};

/////////////////////

MockWebRTCDataChannelHandler::MockWebRTCDataChannelHandler(
    WebString label,
    const WebRTCDataChannelInit& init,
    WebTestDelegate* delegate)
    : client_(0), label_(label), init_(init), delegate_(delegate) {
  reliable_ = (init.ordered && init.maxRetransmits == -1 &&
               init.maxRetransmitTime == -1);
}

void MockWebRTCDataChannelHandler::setClient(
    WebRTCDataChannelHandlerClient* client) {
  client_ = client;
  if (client_)
    delegate_->postTask(new DataChannelReadyStateTask(
        this, client_, WebRTCDataChannelHandlerClient::ReadyStateOpen));
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
  delegate_->postTask(new DataChannelReadyStateTask(
      this, client_, WebRTCDataChannelHandlerClient::ReadyStateClosed));
}

}  // namespace content
