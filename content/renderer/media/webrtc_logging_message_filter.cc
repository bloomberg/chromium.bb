// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_logging_message_filter.h"

#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "content/common/media/webrtc_logging_messages.h"
#include "content/renderer/media/webrtc_logging_handler_impl.h"
#include "ipc/ipc_logging.h"

namespace content {

WebRtcLoggingMessageFilter::WebRtcLoggingMessageFilter(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : io_message_loop_(io_message_loop),
      channel_(NULL) {
}

WebRtcLoggingMessageFilter::~WebRtcLoggingMessageFilter() {
}

bool WebRtcLoggingMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebRtcLoggingMessageFilter, message)
    IPC_MESSAGE_HANDLER(WebRtcLoggingMsg_LogOpened, OnLogOpened)
    IPC_MESSAGE_HANDLER(WebRtcLoggingMsg_OpenLogFailed, OnOpenLogFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebRtcLoggingMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  channel_ = channel;
}

void WebRtcLoggingMessageFilter::OnFilterRemoved() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  channel_ = NULL;
  RemoveDelegate();
}

void WebRtcLoggingMessageFilter::OnChannelClosing() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  channel_ = NULL;
  RemoveDelegate();
}

void WebRtcLoggingMessageFilter::SetDelegate(
    scoped_refptr<WebRtcLoggingHandlerImpl>& logging_handler) {
  logging_handler_ = logging_handler;
}

void WebRtcLoggingMessageFilter::RemoveDelegate() {
  logging_handler_ = NULL;
}

void WebRtcLoggingMessageFilter::OpenLog() {
  Send(new WebRtcLoggingMsg_OpenLog());
}

void WebRtcLoggingMessageFilter::OnLogOpened(
    base::SharedMemoryHandle handle,
    uint32 length) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  if (logging_handler_)
    logging_handler_->OnLogOpened(handle, length);
}

void WebRtcLoggingMessageFilter::OnOpenLogFailed() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  if (logging_handler_)
    logging_handler_->OnOpenLogFailed();
}

void WebRtcLoggingMessageFilter::Send(IPC::Message* message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  if (!channel_) {
    DLOG(ERROR) << "IPC channel not available.";
    delete message;
  } else {
    channel_->Send(message);
  }
}

}  // namespace content
