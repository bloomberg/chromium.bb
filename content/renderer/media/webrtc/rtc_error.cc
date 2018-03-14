// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_error.h"

#include "third_party/webrtc/api/rtcerror.h"

namespace content {

blink::WebRTCError ConvertToWebKitRTCError(
    const webrtc::RTCError& webrtc_error) {
  blink::WebString message = blink::WebString::FromUTF8(
      webrtc_error.message(), strlen(webrtc_error.message()));
  switch (webrtc_error.type()) {
    case webrtc::RTCErrorType::NONE:
      return blink::WebRTCError(blink::WebRTCErrorType::kNone, message);
      break;
    case webrtc::RTCErrorType::UNSUPPORTED_PARAMETER:
      return blink::WebRTCError(blink::WebRTCErrorType::kUnsupportedParameter,
                                message);
      break;
    case webrtc::RTCErrorType::INVALID_PARAMETER:
      return blink::WebRTCError(blink::WebRTCErrorType::kInvalidParameter,
                                message);
      break;
    case webrtc::RTCErrorType::INVALID_RANGE:
      return blink::WebRTCError(blink::WebRTCErrorType::kInvalidRange, message);
      break;
    case webrtc::RTCErrorType::SYNTAX_ERROR:
      return blink::WebRTCError(blink::WebRTCErrorType::kSyntaxError, message);
      break;
    case webrtc::RTCErrorType::INVALID_STATE:
      return blink::WebRTCError(blink::WebRTCErrorType::kInvalidState, message);
      break;
    case webrtc::RTCErrorType::INVALID_MODIFICATION:
      return blink::WebRTCError(blink::WebRTCErrorType::kInvalidModification,
                                message);
      break;
    case webrtc::RTCErrorType::NETWORK_ERROR:
      return blink::WebRTCError(blink::WebRTCErrorType::kNetworkError, message);
      break;
    case webrtc::RTCErrorType::INTERNAL_ERROR:
      return blink::WebRTCError(blink::WebRTCErrorType::kInternalError,
                                message);
      break;
    default:
      // If adding a new error type, need 3 CLs: One to add the enum to webrtc,
      // one to update this mapping code, and one to start using the enum in
      // webrtc.
      NOTREACHED() << "webrtc::RTCErrorType " << webrtc_error.type()
                   << " not covered by switch statement.";
      break;
  }
  NOTREACHED();
  return blink::WebRTCError(blink::WebRTCErrorType::kInternalError,
                            "Impossible code path executed");
}

}  // namespace content