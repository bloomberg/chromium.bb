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
// Messages sent from the browser to the render process.

////////////////////////////////////////////////////////////////////////////////
// Messages sent from the render process to the browser.

// Message sent from the renderer to the browser to request that the browser
// cache |data| associated with |url| and |expected_response_time|.
IPC_MESSAGE_CONTROL3(RenderProcessHostMsg_DidGenerateCacheableMetadata,
                     GURL /* url */,
                     base::Time /* expected_response_time */,
                     std::vector<char> /* data */)

// Message sent from the renderer to the browser to request that the browser
// cache |data| for the specified CacheStorage entry.
IPC_MESSAGE_CONTROL5(
    RenderProcessHostMsg_DidGenerateCacheableMetadataInCacheStorage,
    GURL /* url */,
    base::Time /* expected_response_time */,
    std::vector<char> /* data */,
    url::Origin /* cache_storage_origin*/,
    std::string /* cache_storage_cache_name */)

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
