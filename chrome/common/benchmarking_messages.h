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
//
// This message must be synchronous so that the test harness can not
// issue further network requests before it completes.
IPC_SYNC_MESSAGE_CONTROL0_0(ChromeViewHostMsg_CloseCurrentConnections)

// Message sent from the renderer to the browser to request that the browser
// enable or disable the cache.  Used for debugging/testing.
IPC_MESSAGE_CONTROL1(ChromeViewHostMsg_SetCacheMode,
                     bool /* enabled */)

// Message sent from the renderer to the browser to request that the browser
// clear the cache.  Used for debugging/testing.
// |result| is the returned status from the operation.
IPC_SYNC_MESSAGE_CONTROL0_1(ChromeViewHostMsg_ClearCache,
                            int /* result */)

// Message sent from the renderer to the browser to request that the browser
// clear the host cache.  Used for debugging/testing.
// |result| is the returned status from the operation.
IPC_SYNC_MESSAGE_CONTROL0_1(ChromeViewHostMsg_ClearHostResolverCache,
                            int /* result */)

// Message sent from the renderer to the browser to request that the browser
// clear the predictor cache.  Used for debugging/testing.
// |result| is the returned status from the operation.
IPC_SYNC_MESSAGE_CONTROL0_1(ChromeViewHostMsg_ClearPredictorCache,
                            int /* result */)
