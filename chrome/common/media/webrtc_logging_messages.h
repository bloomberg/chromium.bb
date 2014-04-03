// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for WebRTC logging.
// Multiply-included message file, hence no include guard.

#include "base/memory/shared_memory.h"
#include "chrome/common/media/webrtc_logging_message_data.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START WebRtcLoggingMsgStart

IPC_STRUCT_TRAITS_BEGIN(WebRtcLoggingMessageData)
  IPC_STRUCT_TRAITS_MEMBER(timestamp)
  IPC_STRUCT_TRAITS_MEMBER(message)
IPC_STRUCT_TRAITS_END()

// Messages sent from the renderer to the browser.

// Send log message to add to log.
IPC_MESSAGE_CONTROL1(WebRtcLoggingMsg_AddLogMessages,
                     std::vector<WebRtcLoggingMessageData> /* messages */)

// Notification that the renderer has stopped sending log messages to the
// browser.
IPC_MESSAGE_CONTROL0(WebRtcLoggingMsg_LoggingStopped)

// Messages sent from the browser to the renderer.

// Tells the renderer to start sending log messages to the browser.
IPC_MESSAGE_CONTROL0(WebRtcLoggingMsg_StartLogging)

// Tells the renderer to stop sending log messages to the browser.
IPC_MESSAGE_CONTROL0(WebRtcLoggingMsg_StopLogging)
