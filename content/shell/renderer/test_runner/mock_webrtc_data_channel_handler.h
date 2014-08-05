// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCDATACHANNELHANDLER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCDATACHANNELHANDLER_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandler.h"
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
      blink::WebRTCDataChannelHandlerClient* client) OVERRIDE;
  virtual blink::WebString label() OVERRIDE;
  virtual bool isReliable() OVERRIDE;
  virtual bool ordered() const OVERRIDE;
  virtual unsigned short maxRetransmitTime() const OVERRIDE;
  virtual unsigned short maxRetransmits() const OVERRIDE;
  virtual blink::WebString protocol() const OVERRIDE;
  virtual bool negotiated() const OVERRIDE;
  virtual unsigned short id() const OVERRIDE;
  virtual unsigned long bufferedAmount() OVERRIDE;
  virtual bool sendStringData(const blink::WebString& data) OVERRIDE;
  virtual bool sendRawData(const char* data, size_t size) OVERRIDE;
  virtual void close() OVERRIDE;

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

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_MOCKWEBRTCDATACHANNELHANDLER_H_
