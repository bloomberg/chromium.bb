// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PRESENTATION_PRESENTATION_CONNECTION_TEST_HELPER_H_
#define CONTENT_RENDERER_PRESENTATION_PRESENTATION_CONNECTION_TEST_HELPER_H_

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/modules/presentation/WebPresentationConnection.h"

namespace content {

class TestPresentationConnection : public blink::WebPresentationConnection {
 public:
  TestPresentationConnection();
  ~TestPresentationConnection();

  void bindProxy(
      std::unique_ptr<blink::WebPresentationConnectionProxy> proxy) override;

  MOCK_METHOD1(didReceiveTextMessage, void(const blink::WebString& message));
  MOCK_METHOD2(didReceiveBinaryMessage,
               void(const uint8_t* data, size_t length));
  MOCK_METHOD1(didChangeState, void(blink::WebPresentationConnectionState));

  blink::WebPresentationConnectionProxy* proxy();

 private:
  std::unique_ptr<blink::WebPresentationConnectionProxy> proxy_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PRESENTATION_PRESENTATION_CONNECTION_TEST_HELPER_H_
