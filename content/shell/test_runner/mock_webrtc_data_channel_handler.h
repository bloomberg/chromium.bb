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
  void SetClient(blink::WebRTCDataChannelHandlerClient* client) override;
  blink::WebString Label() override;
  bool IsReliable() override;
  bool Ordered() const override;
  unsigned short MaxRetransmitTime() const override;
  unsigned short MaxRetransmits() const override;
  blink::WebString Protocol() const override;
  bool Negotiated() const override;
  unsigned short Id() const override;
  blink::WebRTCDataChannelHandlerClient::ReadyState GetState() const override;
  unsigned long BufferedAmount() override;
  bool SendStringData(const blink::WebString& data) override;
  bool SendRawData(const char* data, size_t size) override;
  void Close() override;

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
