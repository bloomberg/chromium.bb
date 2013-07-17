// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_LOGGING_HANDLER_HOST_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_LOGGING_HANDLER_HOST_H_

#include "base/basictypes.h"
#include "base/memory/shared_memory.h"
#include "content/public/browser/browser_message_filter.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

class RenderProcessHost;

// WebRtcLoggingHandlerHost handles operations regarding the WebRTC logging:
// - Opens a shared memory buffer that the handler in the render process
//   writes to.
// - Detects when channel, i.e. renderer, is going away and triggers uploading
//   the log.
class WebRtcLoggingHandlerHost : public content::BrowserMessageFilter {
 public:
  WebRtcLoggingHandlerHost();

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<WebRtcLoggingHandlerHost>;

  virtual ~WebRtcLoggingHandlerHost();

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  void OnOpenLog(const std::string& app_session_id, const std::string& app_url);

  void OpenLogIfAllowed();
  void DoOpenLog();
  void LogMachineInfo();
  void NotifyLogOpened();

  void UploadLog();

  scoped_refptr<net::URLRequestContextGetter> system_request_context_;
  scoped_ptr<base::SharedMemory> shared_memory_;
  std::string app_session_id_;
  std::string app_url_;

  // This is the handle to be passed to the render process. It's stored so that
  // it doesn't have to be passed on when posting messages between threads.
  // It's only accessed on the IO thread.
  base::SharedMemoryHandle foreign_memory_handle_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLoggingHandlerHost);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_LOGGING_HANDLER_HOST_H_
