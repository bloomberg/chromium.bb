// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START PrefetchMsgStart

// Prefetch Messages
// These are messages sent from the browser to the renderer related to
// prefetching.

// Instructs the renderer to launch a prefetch within its context.
IPC_MESSAGE_ROUTED1(PrefetchMsg_Prefetch,
                    GURL /* url */)
