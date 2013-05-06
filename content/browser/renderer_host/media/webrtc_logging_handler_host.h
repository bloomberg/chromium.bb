// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_WEBRTC_LOGGING_HANDLER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_WEBRTC_LOGGING_HANDLER_HOST_H_

#include "base/basictypes.h"
#include "base/shared_memory.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

// WebRtcLoggingHandlerHost handles operations regarding the WebRTC logging:
// opening and closing shared memory buffer that the handler in the renderer
// process writes to.
class WebRtcLoggingHandlerHost : public BrowserMessageFilter {
 public:
  WebRtcLoggingHandlerHost();

 private:
  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  friend class BrowserThread;
  friend class base::DeleteHelper<WebRtcLoggingHandlerHost>;

  virtual ~WebRtcLoggingHandlerHost();

  void OnOpenLog();

  base::SharedMemory shared_memory_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLoggingHandlerHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_WEBRTC_LOGGING_HANDLER_HOST_H_
