// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for WebRTC logging.
// Multiply-included message file, hence no include guard.

#include "base/memory/shared_memory.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START WebRtcLoggingMsgStart

// Messages sent from the renderer to the browser.

// Notification that the log has been closed.
IPC_MESSAGE_CONTROL0(WebRtcLoggingMsg_LoggingStopped)

// Messages sent from the browser to the renderer.

// Tells the renderer to open the log.
IPC_MESSAGE_CONTROL2(WebRtcLoggingMsg_StartLogging,
                     base::SharedMemoryHandle /* handle */,
                     uint32 /* length */)

// Tells the renderer to close the log.
IPC_MESSAGE_CONTROL0(WebRtcLoggingMsg_StopLogging)
