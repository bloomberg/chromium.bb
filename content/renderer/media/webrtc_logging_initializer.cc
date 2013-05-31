// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_logging_initializer.h"

#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/webrtc_log_message_delegate.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/libjingle/overrides/talk/base/logging.h"

namespace content {

// Shall only be set once and never go back to NULL.
WebRtcLogMessageDelegate* g_webrtc_logging_delegate = NULL;

void InitWebRtcLoggingDelegate(WebRtcLogMessageDelegate* delegate) {
  CHECK(!g_webrtc_logging_delegate);
  CHECK(delegate);

  g_webrtc_logging_delegate = delegate;
}

void InitWebRtcLogging(const std::string& app_session_id,
                       const std::string& app_url) {
  if (g_webrtc_logging_delegate) {
    g_webrtc_logging_delegate->InitLogging(app_session_id, app_url);
    talk_base::InitDiagnosticLoggingDelegateFunction(WebRtcLogMessage);
  }
}

void WebRtcLogMessage(const std::string& message) {
  if (g_webrtc_logging_delegate)
    g_webrtc_logging_delegate->LogMessage(message);
}

}  // namespace content
