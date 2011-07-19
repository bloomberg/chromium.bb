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

// Sent to all child processes to enable trace event recording.
IPC_MESSAGE_CONTROL0(ChildProcessMsg_BeginTracing)

// Sent to all child processes to disable trace event recording.
IPC_MESSAGE_CONTROL0(ChildProcessMsg_EndTracing)

// Sent to all child processes to get trace buffer fullness.
IPC_MESSAGE_CONTROL0(ChildProcessMsg_GetTraceBufferPercentFull)

////////////////////////////////////////////////////////////////////////////////
// Messages sent from the child process to the browser.

IPC_MESSAGE_CONTROL0(ChildProcessHostMsg_ShutdownRequest)

// Reply from child processes acking ChildProcessMsg_TraceChangeStatus(false).
IPC_MESSAGE_CONTROL0(ChildProcessHostMsg_EndTracingAck)

// Sent if the trace buffer becomes full.
IPC_MESSAGE_CONTROL0(ChildProcessHostMsg_TraceBufferFull)

// Child processes send trace data back in JSON chunks.
IPC_MESSAGE_CONTROL1(ChildProcessHostMsg_TraceDataCollected,
                     std::string /*json trace data*/)

// Reply to ChildProcessMsg_GetTraceBufferPercentFull.
IPC_MESSAGE_CONTROL1(ChildProcessHostMsg_TraceBufferPercentFullReply,
                     float /*trace buffer percent full*/)

// NaCl's 64 bit Windows build only links with a bare-minimum number of
// libraries, and GURL isn't one of them.
#if !defined(NACL_WIN64)
// Get the list of proxies to use for |url|, as a semicolon delimited list
// of "<TYPE> <HOST>:<PORT>" | "DIRECT".
IPC_SYNC_MESSAGE_CONTROL1_2(ChildProcessHostMsg_ResolveProxy,
                            GURL /* url */,
                            int /* network error */,
                            std::string /* proxy list */)
#endif
