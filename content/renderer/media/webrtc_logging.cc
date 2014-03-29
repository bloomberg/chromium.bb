// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_logging.h"

#include <iomanip>
#include <sstream>

#include "base/time/time.h"
#include "content/public/renderer/webrtc_log_message_delegate.h"
#include "third_party/libjingle/overrides/talk/base/logging.h"

namespace content {

// Shall only be set once and never go back to NULL.
WebRtcLogMessageDelegate* g_webrtc_logging_delegate = NULL;
uint64 g_started_time_ms_ = 0;

void InitWebRtcLoggingDelegate(WebRtcLogMessageDelegate* delegate) {
  CHECK(!g_webrtc_logging_delegate);
  CHECK(delegate);

  g_webrtc_logging_delegate = delegate;

  g_started_time_ms_ = base::Time::Now().ToInternalValue() /
      base::Time::kMicrosecondsPerMillisecond;
}

void WebRtcLogMessageWithTimestamp(const std::string& message) {
  if (g_webrtc_logging_delegate)
    g_webrtc_logging_delegate->LogMessage(message);
}

void InitWebRtcLogging() {
  // Log messages from Libjingle should always have timestamps.
  talk_base::InitDiagnosticLoggingDelegateFunction(
      WebRtcLogMessageWithTimestamp);
}

void WebRtcLogMessage(const std::string& message) {
  // Keep consistent with talk_base::DiagnosticLogMessage::CreateTimestamp.
  // TODO(grunell): remove the duplicated code and unify the timestamp creation.
  uint64 now_us = base::Time::Now().ToInternalValue();
  uint64 interval_ms = now_us / base::Time::kMicrosecondsPerMillisecond -
      g_started_time_ms_;

  std::ostringstream message_oss;
  message_oss << "[" << std::setfill('0') << std::setw(3)
              << (interval_ms / 1000)
              << ":" << std::setw(3)
              << (interval_ms % 1000)
              << std::setfill(' ') << "] " << message;

  WebRtcLogMessageWithTimestamp(message_oss.str());
}

}  // namespace content
