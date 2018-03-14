// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_ERROR_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_ERROR_H_

#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/platform/WebRTCError.h"

namespace webrtc {
class RTCError;
}

namespace content {
blink::WebRTCError ConvertToWebKitRTCError(
    const webrtc::RTCError& webrtc_error);
}

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_ERROR_H_