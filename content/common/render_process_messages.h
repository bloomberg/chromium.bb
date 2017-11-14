// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_RENDER_PROCESS_MESSAGES_H_
#define CONTENT_COMMON_RENDER_PROCESS_MESSAGES_H_

// Common IPC messages used for render processes.

#include <stdint.h>

#include <string>

#include "base/memory/shared_memory.h"
#include "build/build_config.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_MACOSX)
#include "content/common/mac/font_descriptor.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START RenderProcessMsgStart

#if defined(OS_MACOSX)
IPC_STRUCT_TRAITS_BEGIN(FontDescriptor)
  IPC_STRUCT_TRAITS_MEMBER(font_name)
  IPC_STRUCT_TRAITS_MEMBER(font_point_size)
IPC_STRUCT_TRAITS_END()
#endif

////////////////////////////////////////////////////////////////////////////////
// Messages sent from the render process to the browser.

// Notify the browser that this render process can or can't be suddenly
// terminated.
IPC_MESSAGE_CONTROL1(RenderProcessHostMsg_SuddenTerminationChanged,
                     bool /* enabled */)

#if defined(OS_MACOSX)
// Request that the browser load a font into shared memory for us.
IPC_SYNC_MESSAGE_CONTROL1_3(RenderProcessHostMsg_LoadFont,
                            FontDescriptor /* font to load */,
                            uint32_t /* buffer size */,
                            base::SharedMemoryHandle /* font data */,
                            uint32_t /* font id */)
#endif

#endif  // CONTENT_COMMON_RENDER_PROCESS_MESSAGES_H_
