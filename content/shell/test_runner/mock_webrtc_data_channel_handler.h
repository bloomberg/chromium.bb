// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_MOCK_WEBRTC_DATA_CHANNEL_HANDLER_H_
#define CONTENT_SHELL_TEST_RUNNER_MOCK_WEBRTC_DATA_CHANNEL_HANDLER_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandler.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandlerClient.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelInit.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace test_runner {

class WebTestDelegate;

class MockWebRTCDataChannelHandler : public blink::WebRTCDataChannelHandler {
 public:
  MockWebRTCDataChannelHandler(blink::WebString label,
                               const blink::WebRTCDataChannelInit& init,
                               WebTestDelegate* delegate);
  ~MockWebRTCDataChannelHandler() override;

  // WebRTCDataChannelHandler related methods
  void setClient(blink::WebRTCDataChannelHandlerClient* client) override;
  blink::WebString label() override;
  bool isReliable() override;
  bool ordered() const override;
  unsigned short maxRetransmitTime() const override;
  unsigned short maxRetransmits() const override;
  blink::WebString protocol() const override;
  bool negotiated() const override;
  unsigned short id() const override;
  blink::WebRTCDataChannelHandlerClient::ReadyState state() const override;
  unsigned long bufferedAmount() override;
  bool sendStringData(const blink::WebString& data) override;
  bool sendRawData(const char* data, size_t size) override;
  void close() override;

 private:
  MockWebRTCDataChannelHandler();
  void ReportOpenedState();
  void ReportClosedState();

  blink::WebRTCDataChannelHandlerClient* client_;
  blink::WebString label_;
  blink::WebRTCDataChannelInit init_;
  bool reliable_;
  WebTestDelegate* delegate_;

  base::WeakPtrFactory<MockWebRTCDataChannelHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockWebRTCDataChannelHandler);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_MOCK_WEBRTC_DATA_CHANNEL_HANDLER_H_
