// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

#define IPC_MESSAGE_START WebCacheMsgStart

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the browser to the renderer process.

// Tells the renderer to set its maximum cache size to the supplied value.
IPC_MESSAGE_CONTROL3(WebCacheMsg_SetCacheCapacities,
                     size_t /* min_dead_capacity */,
                     size_t /* max_dead_capacity */,
                     size_t /* capacity */)

// Tells the renderer to clear the cache.
IPC_MESSAGE_CONTROL1(WebCacheMsg_ClearCache,
                     bool /* on_navigation */)
