// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.

#include <string>

#include "base/basictypes.h"
#include "build/build_config.h"

#include "ipc/ipc_message_macros.h"
#include "ui/gfx/native_widget_types.h"

#define IPC_MESSAGE_START ChromeBenchmarkingMsgStart

// Message sent from the renderer to the browser to request that the browser
// close all sockets.  Used for debugging/testing.
IPC_MESSAGE_CONTROL0(ChromeViewHostMsg_CloseCurrentConnections)

// Message sent from the renderer to the browser to request that the browser
// enable or disable the cache.  Used for debugging/testing.
IPC_MESSAGE_CONTROL1(ChromeViewHostMsg_SetCacheMode,
                     bool /* enabled */)

// Message sent from the renderer to the browser to request that the browser
// clear the cache.  Used for debugging/testing.
// |preserve_ssl_host_info| controls whether clearing the cache will preserve
// persisted SSL information stored in the cache.
// |result| is the returned status from the operation.
IPC_SYNC_MESSAGE_CONTROL1_1(ChromeViewHostMsg_ClearCache,
                            bool /* preserve_ssl_host_info */,
                            int /* result */)

// Message sent from the renderer to the browser to request that the browser
// clear the host cache.  Used for debugging/testing.
// |result| is the returned status from the operation.
IPC_SYNC_MESSAGE_CONTROL0_1(ChromeViewHostMsg_ClearHostResolverCache,
                            int /* result */)

// Message sent from the renderer to the browser to request that the browser
// enable or disable spdy.  Used for debugging/testing/benchmarking.
IPC_MESSAGE_CONTROL1(ChromeViewHostMsg_EnableSpdy,
                     bool /* enable */)

// Message sent from the renderer to the browser to request that the browser
// clear the predictor cache.  Used for debugging/testing.
// |result| is the returned status from the operation.
IPC_SYNC_MESSAGE_CONTROL0_1(ChromeViewHostMsg_ClearPredictorCache,
                            int /* result */)
