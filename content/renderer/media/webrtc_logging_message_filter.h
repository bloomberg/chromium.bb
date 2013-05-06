// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_LOGGING_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_LOGGING_MESSAGE_FILTER_H_

#include "base/shared_memory.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel_proxy.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class WebRtcLoggingHandlerImpl;

// Filter for WebRTC logging messages. Sits between WebRtcLoggingHandlerImpl
// (renderer process) and WebRtcLoggingHandlerHost (browser process). Must be
// called on the IO thread.
class CONTENT_EXPORT WebRtcLoggingMessageFilter
    : public IPC::ChannelProxy::MessageFilter {
 public:
  explicit WebRtcLoggingMessageFilter(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);

  virtual void SetDelegate(
      scoped_refptr<WebRtcLoggingHandlerImpl>& logging_handler);
  virtual void RemoveDelegate();

  virtual void OpenLog();

 protected:
  virtual ~WebRtcLoggingMessageFilter();

 private:
  // IPC::ChannelProxy::MessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

  void OnLogOpened(base::SharedMemoryHandle handle, uint32 length);
  void OnOpenLogFailed();

  void Send(IPC::Message* message);

  scoped_refptr<WebRtcLoggingHandlerImpl> logging_handler_;

  scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  IPC::Channel* channel_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLoggingMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_LOGGING_MESSAGE_FILTER_H_
