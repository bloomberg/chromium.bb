// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_LAYOUT_TEST_TEST_WEBSOCKET_HANDSHAKE_THROTTLE_H_
#define CONTENT_SHELL_RENDERER_LAYOUT_TEST_TEST_WEBSOCKET_HANDSHAKE_THROTTLE_H_

#include "base/timer/timer.h"
#include "third_party/WebKit/public/platform/WebCallbacks.h"
#include "third_party/WebKit/public/platform/WebSocketHandshakeThrottle.h"

namespace blink {
class WebLocalFrame;
class WebString;
class WebURL;
}  // namespace blink

namespace content {

// A simple WebSocketHandshakeThrottle that calls callbacks->IsSuccess() after n
// milli-seconds if the URL query contains
// content-shell-websocket-delay-ms=n. Otherwise it calls IsSuccess()
// immediately.
class TestWebSocketHandshakeThrottle
    : public blink::WebSocketHandshakeThrottle {
 public:
  ~TestWebSocketHandshakeThrottle() override = default;

  void ThrottleHandshake(
      const blink::WebURL& url,
      blink::WebLocalFrame* frame,
      blink::WebCallbacks<void, const blink::WebString&>* callbacks) override;

 private:
  base::OneShotTimer timer_;
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_LAYOUT_TEST_TEST_WEBSOCKET_HANDSHAKE_THROTTLE_H_
