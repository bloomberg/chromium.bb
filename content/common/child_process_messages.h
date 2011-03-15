// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common IPC messages used for child processes.
// Multiply-included message file, hence no include guard.

#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START ChildProcessMsgStart

// Messages sent from the browser to the child process.

// Tells the child process it should stop.
IPC_MESSAGE_CONTROL0(ChildProcessMsg_AskBeforeShutdown)

// Sent in response to ChildProcessHostMsg_ShutdownRequest to tell the child
// process that it's safe to shutdown.
IPC_MESSAGE_CONTROL0(ChildProcessMsg_Shutdown)

#if defined(IPC_MESSAGE_LOG_ENABLED)
// Tell the child process to begin or end IPC message logging.
IPC_MESSAGE_CONTROL1(ChildProcessMsg_SetIPCLoggingEnabled,
                     bool /* on or off */)
#endif

// Messages sent from the child process to the browser.

IPC_MESSAGE_CONTROL0(ChildProcessHostMsg_ShutdownRequest)

// Get the list of proxies to use for |url|, as a semicolon delimited list
// of "<TYPE> <HOST>:<PORT>" | "DIRECT".
IPC_SYNC_MESSAGE_CONTROL1_2(ChildProcessHostMsg_ResolveProxy,
                            GURL /* url */,
                            int /* network error */,
                            std::string /* proxy list */)
