// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for WebRTC logging.
// Multiply-included message file, hence no include guard.

#include "base/shared_memory.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START WebRtcLoggingMsgStart

// Messages sent from the renderer to the browser.

// Request to open a log.
IPC_MESSAGE_CONTROL0(WebRtcLoggingMsg_OpenLog)

// Messages sent from the browser to the renderer.

// Notification that a log could not be opened.
IPC_MESSAGE_CONTROL0(WebRtcLoggingMsg_OpenLogFailed)

// Notification that a log has been opened.
IPC_MESSAGE_CONTROL2(WebRtcLoggingMsg_LogOpened,
                     base::SharedMemoryHandle /* handle */,
                     uint32 /* length */)
