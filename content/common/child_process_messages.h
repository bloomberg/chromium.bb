// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CHILD_PROCESS_MESSAGES_H_
#define CONTENT_COMMON_CHILD_PROCESS_MESSAGES_H_

// Common IPC messages used for child processes.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/shared_memory.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_bitmap_manager.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits_macros.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/gpu_param_traits_macros.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_features.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

#if defined(OS_LINUX)
#include "base/threading/platform_thread.h"
#endif

IPC_STRUCT_TRAITS_BEGIN(base::LocationSnapshot)
  IPC_STRUCT_TRAITS_MEMBER(file_name)
  IPC_STRUCT_TRAITS_MEMBER(function_name)
  IPC_STRUCT_TRAITS_MEMBER(line_number)
IPC_STRUCT_TRAITS_END()

#if defined(OS_LINUX)
IPC_ENUM_TRAITS_MAX_VALUE(base::ThreadPriority,
                          base::ThreadPriority::REALTIME_AUDIO)
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ChildProcessMsgStart

// Messages sent from the browser to the child process.

// Sent in response to ChildProcessHostMsg_ShutdownRequest to tell the child
// process that it's safe to shutdown.
IPC_MESSAGE_CONTROL0(ChildProcessMsg_Shutdown)

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
// Tell the child process to begin or end IPC message logging.
IPC_MESSAGE_CONTROL1(ChildProcessMsg_SetIPCLoggingEnabled,
                     bool /* on or off */)
#endif

// Sent to child processes to tell them to enter or leave background mode.
IPC_MESSAGE_CONTROL1(ChildProcessMsg_SetProcessBackgrounded,
                     bool /* background */)

// Sent to child processes to tell them to purge and suspend.
IPC_MESSAGE_CONTROL0(ChildProcessMsg_PurgeAndSuspend)

// Sent to child processes to tell them to resume from suspended mode.
IPC_MESSAGE_CONTROL0(ChildProcessMsg_Resume)

////////////////////////////////////////////////////////////////////////////////
// Messages sent from the child process to the browser.

// A renderer sends this when it wants to know whether a gpu process exists.
IPC_SYNC_MESSAGE_CONTROL0_1(ChildProcessHostMsg_HasGpuProcess,
                            bool /* result */)

IPC_MESSAGE_CONTROL0(ChildProcessHostMsg_ShutdownRequest)

// Request a histogram from the browser. The browser will send the histogram
// data only if it has been passed the command line flag
// switches::kDomAutomationController.
IPC_SYNC_MESSAGE_CONTROL1_1(ChildProcessHostMsg_GetBrowserHistogram,
                            std::string, /* histogram_name */
                            std::string /* histogram_json */)

#if defined(OS_WIN)
// Request that the given font be loaded by the host so it's cached by the
// OS. Please see ChildProcessHost::PreCacheFont for details.
IPC_SYNC_MESSAGE_CONTROL1_0(ChildProcessHostMsg_PreCacheFont,
                            LOGFONT /* font data */)

// Release the cached font
IPC_MESSAGE_CONTROL0(ChildProcessHostMsg_ReleaseCachedFonts)
#endif  // defined(OS_WIN)

#if defined(OS_LINUX)
// Asks the browser to change the priority of thread.
IPC_MESSAGE_CONTROL2(ChildProcessHostMsg_SetThreadPriority,
                     base::PlatformThreadId,
                     base::ThreadPriority)
#endif

#endif  // CONTENT_COMMON_CHILD_PROCESS_MESSAGES_H_
