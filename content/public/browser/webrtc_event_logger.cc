// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webrtc_event_logger.h"

#include "base/logging.h"

namespace content {

WebRtcEventLogger* g_webrtc_event_logger = nullptr;

void WebRtcEventLogger::Set(WebRtcEventLogger* webrtc_event_logger) {
  DCHECK(webrtc_event_logger);
  DCHECK(!g_webrtc_event_logger);
  g_webrtc_event_logger = webrtc_event_logger;
}

WebRtcEventLogger* WebRtcEventLogger::Get() {
  return g_webrtc_event_logger;
}

void WebRtcEventLogger::ClearForTesting() {
  DCHECK(g_webrtc_event_logger);
  g_webrtc_event_logger = nullptr;
}

}  // namespace content
