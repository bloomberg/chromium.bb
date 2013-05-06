// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_LOGGING_HANDLER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_LOGGING_HANDLER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/shared_memory.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel_proxy.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class WebRtcLoggingMessageFilter;

// WebRtcLoggingHandlerImpl handles WebRTC logging. There is one object per
// render thread. It communicates with WebRtcLoggingHandlerHost and receives
// logging messages from libjingle and writes them to a shared memory buffer.
class CONTENT_EXPORT WebRtcLoggingHandlerImpl
    : public base::RefCounted<WebRtcLoggingHandlerImpl> {
 public:
  WebRtcLoggingHandlerImpl(
      const scoped_refptr<WebRtcLoggingMessageFilter>& message_filter,
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);

  void OnFilterRemoved();

  void OpenLog();

  void OnLogOpened(base::SharedMemoryHandle handle, uint32 length);
  void OnOpenLogFailed();

 private:
  friend class base::RefCounted<WebRtcLoggingHandlerImpl>;
  virtual ~WebRtcLoggingHandlerImpl();

  scoped_refptr<WebRtcLoggingMessageFilter> message_filter_;

  scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLoggingHandlerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_LOGGING_HANDLER_IMPL_H_
