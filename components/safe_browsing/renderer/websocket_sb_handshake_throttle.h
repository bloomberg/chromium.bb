// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of SafeBrowsing for WebSockets. This code runs inside the
// render process, calling the interface defined in safe_browsing.mojom to
// communicate with the SafeBrowsing service.

#ifndef COMPONENTS_SAFE_BROWSING_RENDERER_WEBSOCKET_SB_HANDSHAKE_THROTTLE_H_
#define COMPONENTS_SAFE_BROWSING_RENDERER_WEBSOCKET_SB_HANDSHAKE_THROTTLE_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "third_party/WebKit/public/platform/WebCallbacks.h"
#include "third_party/WebKit/public/platform/WebSocketHandshakeThrottle.h"
#include "url/gurl.h"

namespace safe_browsing {

class WebSocketSBHandshakeThrottle : public blink::WebSocketHandshakeThrottle {
 public:
  explicit WebSocketSBHandshakeThrottle(mojom::SafeBrowsing* safe_browsing);
  ~WebSocketSBHandshakeThrottle() override;

  void ThrottleHandshake(
      const blink::WebURL& url,
      blink::WebLocalFrame* web_local_frame,
      blink::WebCallbacks<void, const blink::WebString&>* callbacks) override;

 private:
  // These values are logged to UMA so do not renumber or reuse.
  enum class Result {
    UNKNOWN = 0,
    SAFE = 1,
    BLOCKED = 2,
    ABANDONED = 3,
    RESULT_COUNT
  };
  void OnCheckResult(bool safe);

  GURL url_;
  blink::WebCallbacks<void, const blink::WebString&>* callbacks_;
  mojom::SafeBrowsingUrlCheckerPtr url_checker_;
  mojom::SafeBrowsing* safe_browsing_;
  base::TimeTicks start_time_;
  Result result_;

  base::WeakPtrFactory<WebSocketSBHandshakeThrottle> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketSBHandshakeThrottle);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_RENDERER_WEBSOCKET_SB_HANDSHAKE_THROTTLE_H_
