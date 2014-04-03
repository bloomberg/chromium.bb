// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CHROME_WEBRTC_LOG_MESSAGE_DELEGATE_H_
#define CHROME_RENDERER_MEDIA_CHROME_WEBRTC_LOG_MESSAGE_DELEGATE_H_

#include <string>

#include "base/memory/shared_memory.h"
#include "chrome/common/media/webrtc_logging_message_data.h"
#include "content/public/renderer/webrtc_log_message_delegate.h"
#include "ipc/ipc_channel_proxy.h"

namespace base {
class MessageLoopProxy;
}

class PartialCircularBuffer;
class WebRtcLoggingMessageFilter;

// ChromeWebRtcLogMessageDelegate handles WebRTC logging. There is one object
// per render process, owned by WebRtcLoggingMessageFilter. It communicates with
// WebRtcLoggingHandlerHost and receives logging messages from libjingle and
// writes them to a shared memory buffer.
class ChromeWebRtcLogMessageDelegate
    : public content::WebRtcLogMessageDelegate,
      public base::NonThreadSafe {
 public:
  ChromeWebRtcLogMessageDelegate(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop,
      WebRtcLoggingMessageFilter* message_filter);

  virtual ~ChromeWebRtcLogMessageDelegate();

  // content::WebRtcLogMessageDelegate implementation.
  virtual void LogMessage(const std::string& message) OVERRIDE;

  void OnFilterRemoved();

  void OnStartLogging();
  void OnStopLogging();

 private:
  void LogMessageOnIOThread(const WebRtcLoggingMessageData& message);
  void SendLogBuffer();

  scoped_refptr<base::MessageLoopProxy> io_message_loop_;
  bool logging_started_;
  std::vector<WebRtcLoggingMessageData> log_buffer_;

  base::TimeTicks last_log_buffer_send_;

  WebRtcLoggingMessageFilter* message_filter_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebRtcLogMessageDelegate);
};

#endif  // CHROME_RENDERER_MEDIA_CHROME_WEBRTC_LOG_MESSAGE_DELEGATE_H_
