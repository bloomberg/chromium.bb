// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEBRTC_DATA_CHANNEL_HANDLER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEBRTC_DATA_CHANNEL_HANDLER_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/web_task.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandler.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandlerClient.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelInit.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

class WebTestDelegate;

class MockWebRTCDataChannelHandler : public blink::WebRTCDataChannelHandler {
 public:
  MockWebRTCDataChannelHandler(blink::WebString label,
                               const blink::WebRTCDataChannelInit& init,
                               WebTestDelegate* delegate);

  // WebRTCDataChannelHandler related methods
  virtual void setClient(
      blink::WebRTCDataChannelHandlerClient* client) override;
  virtual blink::WebString label() override;
  virtual bool isReliable() override;
  virtual bool ordered() const override;
  virtual unsigned short maxRetransmitTime() const override;
  virtual unsigned short maxRetransmits() const override;
  virtual blink::WebString protocol() const override;
  virtual bool negotiated() const override;
  virtual unsigned short id() const override;
  // TODO(bemasc): Mark |state()| as |override| once https://codereview.chromium.org/782843003/
  // lands in Blink and rolls into Chromium.
  virtual blink::WebRTCDataChannelHandlerClient::ReadyState state() const;
  virtual unsigned long bufferedAmount() override;
  virtual bool sendStringData(const blink::WebString& data) override;
  virtual bool sendRawData(const char* data, size_t size) override;
  virtual void close() override;

  // WebTask related methods
  WebTaskList* mutable_task_list() { return &task_list_; }

 private:
  MockWebRTCDataChannelHandler();

  blink::WebRTCDataChannelHandlerClient* client_;
  blink::WebString label_;
  blink::WebRTCDataChannelInit init_;
  bool reliable_;
  WebTaskList task_list_;
  WebTestDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(MockWebRTCDataChannelHandler);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCK_WEBRTC_DATA_CHANNEL_HANDLER_H_
