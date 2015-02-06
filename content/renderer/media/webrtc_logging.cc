// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_logging.h"

#include "base/time/time.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "content/public/renderer/webrtc_log_message_delegate.h"
#include "third_party/webrtc/overrides/webrtc/base/logging.h"

namespace content {

// Shall only be set once within a RenderThread and never go back to NULL.
base::LazyInstance<base::ThreadLocalPointer<WebRtcLogMessageDelegate> >::Leaky
    g_webrtc_logging_delegate_tls = LAZY_INSTANCE_INITIALIZER;

void InitWebRtcLoggingDelegate(WebRtcLogMessageDelegate* delegate) {
  CHECK(!g_webrtc_logging_delegate_tls.Pointer()->Get());
  CHECK(delegate);

  g_webrtc_logging_delegate_tls.Pointer()->Set(delegate);
}

void InitWebRtcLogging() {
  // Log messages from Libjingle should not have timestamps.
  rtc::InitDiagnosticLoggingDelegateFunction(&WebRtcLogMessage);
}

void WebRtcLogMessage(const std::string& message) {
  if (g_webrtc_logging_delegate_tls.Pointer()->Get())
    g_webrtc_logging_delegate_tls.Pointer()->Get()->LogMessage(message);
}

}  // namespace content
