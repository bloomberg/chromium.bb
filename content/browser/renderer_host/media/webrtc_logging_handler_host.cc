// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/webrtc_logging_handler_host.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/common/media/webrtc_logging_messages.h"

namespace content {

const size_t kWebRtcLogSize = 6 * 1024 * 1024;  // 6 MB

WebRtcLoggingHandlerHost::WebRtcLoggingHandlerHost() {
}

WebRtcLoggingHandlerHost::~WebRtcLoggingHandlerHost() {
}

void WebRtcLoggingHandlerHost::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();
}

void WebRtcLoggingHandlerHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool WebRtcLoggingHandlerHost::OnMessageReceived(const IPC::Message& message,
                                                 bool* message_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(WebRtcLoggingHandlerHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(WebRtcLoggingMsg_OpenLog, OnOpenLog)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()

  return handled;
}

void WebRtcLoggingHandlerHost::OnOpenLog() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!base::SharedMemory::IsHandleValid(shared_memory_.handle()));

  if (!shared_memory_.CreateAndMapAnonymous(kWebRtcLogSize)) {
    DLOG(ERROR) << "Failed to create shared memory.";
    Send(new WebRtcLoggingMsg_OpenLogFailed());
    return;
  }

  base::SharedMemoryHandle foreign_memory_handle;
  if (!shared_memory_.ShareToProcess(peer_handle(),
                                     &foreign_memory_handle)) {
    Send(new WebRtcLoggingMsg_OpenLogFailed());
    return;
  }

  Send(new WebRtcLoggingMsg_LogOpened(foreign_memory_handle, kWebRtcLogSize));
}

}  // namespace content
