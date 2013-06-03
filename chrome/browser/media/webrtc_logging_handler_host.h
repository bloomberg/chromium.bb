// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_LOGGING_HANDLER_HOST_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_LOGGING_HANDLER_HOST_H_

#include "base/basictypes.h"
#include "base/shared_memory.h"
#include "content/public/browser/browser_message_filter.h"

// WebRtcLoggingHandlerHost handles operations regarding the WebRTC logging:
// opening and closing shared memory buffer that the handler in the renderer
// process writes to.
class WebRtcLoggingHandlerHost : public content::BrowserMessageFilter {
 public:
  WebRtcLoggingHandlerHost();

 private:
  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  friend class content::BrowserThread;
  friend class base::DeleteHelper<WebRtcLoggingHandlerHost>;

  virtual ~WebRtcLoggingHandlerHost();

  void OnOpenLog(const std::string& app_session_id, const std::string& app_url);

  base::SharedMemory shared_memory_;
  std::string app_session_id_;
  std::string app_url_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLoggingHandlerHost);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_LOGGING_HANDLER_HOST_H_
