// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_LOGGING_HANDLER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_LOGGING_HANDLER_IMPL_H_

#include <string>

#include "base/shared_memory.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel_proxy.h"
#include "third_party/libjingle/overrides/logging/log_message_delegate.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class PartialCircularBuffer;
class WebRtcLoggingMessageFilter;

// WebRtcLoggingHandlerImpl handles WebRTC logging. There is one object per
// render process, owned by WebRtcLoggingMessageFilter. It communicates with
// WebRtcLoggingHandlerHost and receives logging messages from libjingle and
// writes them to a shared memory buffer.
class CONTENT_EXPORT WebRtcLoggingHandlerImpl
    : public NON_EXPORTED_BASE(talk_base::LogMessageDelegate) {
 public:
  WebRtcLoggingHandlerImpl(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);

  virtual ~WebRtcLoggingHandlerImpl();

  // talk_base::LogMessageDelegate implementation.
  virtual void LogMessage(const std::string& message) OVERRIDE;

  void OnFilterRemoved();

  void OnLogOpened(base::SharedMemoryHandle handle, uint32 length);
  void OnOpenLogFailed();

 private:
  scoped_refptr<base::MessageLoopProxy> io_message_loop_;
  scoped_ptr<base::SharedMemory> shared_memory_;
  scoped_ptr<content::PartialCircularBuffer> circular_buffer_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLoggingHandlerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_LOGGING_HANDLER_IMPL_H_
