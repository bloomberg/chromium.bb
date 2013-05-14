// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_logging_handler_impl.h"

#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "content/common/partial_circular_buffer.h"
#include "content/renderer/media/webrtc_logging_message_filter.h"
#include "third_party/libjingle/overrides/talk/base/logging.h"

namespace content {

WebRtcLoggingHandlerImpl::WebRtcLoggingHandlerImpl(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : io_message_loop_(io_message_loop) {
}

WebRtcLoggingHandlerImpl::~WebRtcLoggingHandlerImpl() {
}

void WebRtcLoggingHandlerImpl::LogMessage(const std::string& message) {
  if (!io_message_loop_->BelongsToCurrentThread()) {
    io_message_loop_->PostTask(
        FROM_HERE, base::Bind(
            &WebRtcLoggingHandlerImpl::LogMessage,
            base::Unretained(this),
            message));
    return;
  }

  circular_buffer_->Write(message.c_str(), message.length());
  const char eol = '\n';
  circular_buffer_->Write(&eol, 1);
}

void WebRtcLoggingHandlerImpl::OnFilterRemoved() {
}

void WebRtcLoggingHandlerImpl::OnLogOpened(
    base::SharedMemoryHandle handle,
    uint32 length) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());

  shared_memory_.reset(new base::SharedMemory(handle, false));
  CHECK(shared_memory_->Map(length));
  circular_buffer_.reset(
      new content::PartialCircularBuffer(shared_memory_->memory(),
                                         length,
                                         length / 2));
}

void WebRtcLoggingHandlerImpl::OnOpenLogFailed() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  DLOG(ERROR) << "Could not open log.";
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
}

}  // namespace content
