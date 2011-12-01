// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common IPC messages used for child processes.
// Multiply-included message file, hence no include guard.

#include "base/shared_memory.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

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
IPC_MESSAGE_CONTROL2(ChildProcessMsg_BeginTracing,
                     std::vector<std::string> /* included_categories */,
                     std::vector<std::string> /* excluded_categories */)

// Sent to all child processes to disable trace event recording.
IPC_MESSAGE_CONTROL0(ChildProcessMsg_EndTracing)

// Sent to all child processes to get trace buffer fullness.
IPC_MESSAGE_CONTROL0(ChildProcessMsg_GetTraceBufferPercentFull)

// Tell the child process to enable or disable the profiler status.
IPC_MESSAGE_CONTROL1(ChildProcessMsg_SetProfilerStatus,
                     bool /* profiler status */)

// Send to all the child processes to send back profiler data (ThreadData in
// tracked_objects).
IPC_MESSAGE_CONTROL2(ChildProcessMsg_GetChildProfilerData,
                     int,         /* sequence number. */
                     std::string  /* pickled Value of process type. */)

// Sent to child processes to dump their handle table.
IPC_MESSAGE_CONTROL0(ChildProcessMsg_DumpHandles)

////////////////////////////////////////////////////////////////////////////////
// Messages sent from the child process to the browser.

IPC_MESSAGE_CONTROL0(ChildProcessHostMsg_ShutdownRequest)

// Notify the browser that this child process supports tracing.
IPC_MESSAGE_CONTROL0(ChildProcessHostMsg_ChildSupportsTracing)

// Reply from child processes acking ChildProcessMsg_TraceChangeStatus(false).
IPC_MESSAGE_CONTROL1(ChildProcessHostMsg_EndTracingAck,
                     std::vector<std::string> /* known_categories */)

// Sent if the trace buffer becomes full.
IPC_MESSAGE_CONTROL0(ChildProcessHostMsg_TraceBufferFull)

// Child processes send trace data back in JSON chunks.
IPC_MESSAGE_CONTROL1(ChildProcessHostMsg_TraceDataCollected,
                     std::string /*json trace data*/)

// Reply to ChildProcessMsg_GetTraceBufferPercentFull.
IPC_MESSAGE_CONTROL1(ChildProcessHostMsg_TraceBufferPercentFullReply,
                     float /*trace buffer percent full*/)

// Send back profiler data (ThreadData in tracked_objects).
IPC_MESSAGE_CONTROL2(ChildProcessHostMsg_ChildProfilerData,
                     int, /* sequence number. */
                     DictionaryValue /* profiler data. */)

// Reply to ChildProcessMsg_DumpHandles when handle table dump is complete.
IPC_MESSAGE_CONTROL0(ChildProcessHostMsg_DumpHandlesDone)

#if defined(OS_WIN)
// Request that the given font be loaded by the host so it's cached by the
// OS. Please see ChildProcessHost::PreCacheFont for details.
IPC_SYNC_MESSAGE_CONTROL1_0(ChildProcessHostMsg_PreCacheFont,
                            LOGFONT /* font data */)

// Release the cached font
IPC_MESSAGE_CONTROL0(ChildProcessHostMsg_ReleaseCachedFonts)
#endif  // defined(OS_WIN)

// Asks the browser to create a block of shared memory for the child process to
// fill in and pass back to the browser.
IPC_SYNC_MESSAGE_CONTROL1_1(ChildProcessHostMsg_SyncAllocateSharedMemory,
                            uint32 /* buffer size */,
                            base::SharedMemoryHandle)
