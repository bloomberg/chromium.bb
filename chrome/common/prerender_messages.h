// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"

#define IPC_MESSAGE_START PrerenderMsgStart

// Tells a renderer if it's currently being prerendered.  Must only be set
// to true before any navigation occurs, and only set to false at most once
// after that.
IPC_MESSAGE_ROUTED1(PrerenderMsg_SetIsPrerendering,
                    bool /* whether the RenderView is prerendering */)

// Specifies that a URL is currently being prerendered.
IPC_MESSAGE_CONTROL1(PrerenderMsg_AddPrerenderURL,
                     GURL /* url */)

// Specifies that a URL is no longer being prerendered.
IPC_MESSAGE_CONTROL1(PrerenderMsg_RemovePrerenderURL,
                     GURL /* url */)
