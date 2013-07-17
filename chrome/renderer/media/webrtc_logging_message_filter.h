// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_WEBRTC_LOGGING_MESSAGE_FILTER_H_
#define CHROME_RENDERER_MEDIA_WEBRTC_LOGGING_MESSAGE_FILTER_H_

#include "base/memory/shared_memory.h"
#include "ipc/ipc_channel_proxy.h"

namespace base {
class MessageLoopProxy;
}

class ChromeWebRtcLogMessageDelegate;

// Filter for WebRTC logging messages. Sits between
// ChromeWebRtcLogMessageDelegate (renderer process) and
// WebRtcLoggingHandlerHost (browser process). Must be called on the IO thread.
class WebRtcLoggingMessageFilter
    : public IPC::ChannelProxy::MessageFilter {
 public:
  explicit WebRtcLoggingMessageFilter(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);

  virtual void InitLogging(const std::string& app_session_id,
                           const std::string& app_url);

  const scoped_refptr<base::MessageLoopProxy>& io_message_loop() {
    return io_message_loop_;
  }

 protected:
  virtual ~WebRtcLoggingMessageFilter();

 private:
  // IPC::ChannelProxy::MessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

  void CreateLoggingHandler();

  void OnLogOpened(base::SharedMemoryHandle handle, uint32 length);
  void OnOpenLogFailed();

  void Send(IPC::Message* message);

  // Owned by this class. The only other pointer to it is in libjingle's logging
  // file. That's a global pointer used on different threads, so we will leak
  // this object when we go away to ensure that it outlives any log messages
  // coming from libjingle.
  ChromeWebRtcLogMessageDelegate* log_message_delegate_;

  scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  IPC::Channel* channel_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLoggingMessageFilter);
};

#endif  // CHROME_RENDERER_MEDIA_WEBRTC_LOGGING_MESSAGE_FILTER_H_
