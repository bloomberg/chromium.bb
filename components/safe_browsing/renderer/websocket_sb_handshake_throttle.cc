// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/renderer/websocket_sb_handshake_throttle.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "content/public/common/resource_type.h"
#include "content/public/renderer/render_frame.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace safe_browsing {

WebSocketSBHandshakeThrottle::WebSocketSBHandshakeThrottle(
    mojom::SafeBrowsing* safe_browsing)
    : callbacks_(nullptr),
      safe_browsing_(safe_browsing),
      result_(Result::UNKNOWN),
      weak_factory_(this) {}

WebSocketSBHandshakeThrottle::~WebSocketSBHandshakeThrottle() {
  // ThrottleHandshake() should always be called, but since that is done all the
  // way over in Blink, just avoid logging if it is not called rather than
  // DCHECK()ing.
  if (start_time_.is_null())
    return;
  if (result_ == Result::UNKNOWN) {
    result_ = Result::ABANDONED;
    UMA_HISTOGRAM_TIMES("SafeBrowsing.WebSocket.Elapsed.Abandoned",
                        base::TimeTicks::Now() - start_time_);
  }
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.WebSocket.Result", result_,
                            Result::RESULT_COUNT);
}

void WebSocketSBHandshakeThrottle::ThrottleHandshake(
    const blink::WebURL& url,
    blink::WebLocalFrame* web_local_frame,
    blink::WebCallbacks<void, const blink::WebString&>* callbacks) {
  DCHECK(!callbacks_);
  DCHECK(!url_checker_);
  callbacks_ = callbacks;
  url_ = url;
  int render_frame_id = MSG_ROUTING_NONE;
  if (web_local_frame) {
    render_frame_id =
        content::RenderFrame::FromWebFrame(web_local_frame)->GetRoutingID();
  }
  int load_flags = 0;
  start_time_ = base::TimeTicks::Now();
  safe_browsing_->CreateCheckerAndCheck(
      render_frame_id, mojo::MakeRequest(&url_checker_), url, load_flags,
      content::RESOURCE_TYPE_SUB_RESOURCE,
      base::BindOnce(&WebSocketSBHandshakeThrottle::OnCheckResult,
                     weak_factory_.GetWeakPtr()));
}

void WebSocketSBHandshakeThrottle::OnCheckResult(bool safe) {
  DCHECK(!start_time_.is_null());
  base::TimeDelta elapsed = base::TimeTicks::Now() - start_time_;
  if (safe) {
    result_ = Result::SAFE;
    UMA_HISTOGRAM_TIMES("SafeBrowsing.WebSocket.Elapsed.Safe", elapsed);
    callbacks_->OnSuccess();
  } else {
    // When the insterstitial is dismissed the page is navigated and this object
    // is destroyed before reaching here.
    result_ = Result::BLOCKED;
    UMA_HISTOGRAM_TIMES("SafeBrowsing.WebSocket.Elapsed.Blocked", elapsed);
    callbacks_->OnError(blink::WebString::FromUTF8(base::StringPrintf(
        "WebSocket connection to %s failed safe browsing check",
        url_.spec().c_str())));
  }
  // |this| is destroyed here.
}

}  // namespace safe_browsing
