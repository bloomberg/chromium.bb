// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_logging_handler_impl.h"

#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "content/renderer/media/webrtc_logging_message_filter.h"

namespace content {

WebRtcLoggingHandlerImpl::WebRtcLoggingHandlerImpl(
    const scoped_refptr<WebRtcLoggingMessageFilter>& message_filter,
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : message_filter_(message_filter),
      io_message_loop_(io_message_loop) {
}

WebRtcLoggingHandlerImpl::~WebRtcLoggingHandlerImpl() {
}

void WebRtcLoggingHandlerImpl::OnFilterRemoved() {
  message_filter_ = NULL;
}

void WebRtcLoggingHandlerImpl::OpenLog() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  // TODO(grunell): Check if already opened. (Could have been opened by another
  // render view.)
  if (message_filter_)
    message_filter_->OpenLog();
}

void WebRtcLoggingHandlerImpl::OnLogOpened(
    base::SharedMemoryHandle handle,
    uint32 length) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
}

void WebRtcLoggingHandlerImpl::OnOpenLogFailed() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  DLOG(ERROR) << "Could not open log.";
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
}

}  // namespace content
