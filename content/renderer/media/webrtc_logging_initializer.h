// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBRTC_LOGGING_INITIALIZER_H_
#define CONTENT_RENDERER_WEBRTC_LOGGING_INITIALIZER_H_

#include <string>

namespace content {

class WebRtcLogMessageDelegate;

// Must be called on IO thread.
void InitWebRtcLogging(const std::string& app_session_id,
                       const std::string& app_url);

// May be called on any thread.
void WebRtcLogMessage(const std::string& message);

}  // namespace content

#endif  // CONTENT_RENDERER_WEBRTC_LOGGING_INITIALIZER_H_
