// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/webrtc_logging_message_filter.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/common/media/webrtc_logging_messages.h"
#include "chrome/renderer/media/chrome_webrtc_log_message_delegate.h"
#include "ipc/ipc_logging.h"

WebRtcLoggingMessageFilter::WebRtcLoggingMessageFilter(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : log_message_delegate_(NULL),
      io_message_loop_(io_message_loop),
      channel_(NULL) {
  io_message_loop_->PostTask(
      FROM_HERE, base::Bind(
          &WebRtcLoggingMessageFilter::CreateLoggingHandler,
          base::Unretained(this)));
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
  log_message_delegate_->OnFilterRemoved();
}

void WebRtcLoggingMessageFilter::OnChannelClosing() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  channel_ = NULL;
  log_message_delegate_->OnFilterRemoved();
}

void WebRtcLoggingMessageFilter::InitLogging(
    const std::string& app_session_id,
    const std::string& app_url) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  Send(new WebRtcLoggingMsg_OpenLog(app_session_id, app_url));
}

void WebRtcLoggingMessageFilter::CreateLoggingHandler() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  log_message_delegate_ =
      new ChromeWebRtcLogMessageDelegate(io_message_loop_, this);
}

void WebRtcLoggingMessageFilter::OnLogOpened(
    base::SharedMemoryHandle handle,
    uint32 length) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  log_message_delegate_->OnLogOpened(handle, length);
}

void WebRtcLoggingMessageFilter::OnOpenLogFailed() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  log_message_delegate_->OnOpenLogFailed();
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
