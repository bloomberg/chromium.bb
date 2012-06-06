// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include <string>

#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START ShellMsgStart

// Tells the render view to capture a text dump of the page. The render view
// responds with a ShellViewHostMsg_TextDump.
IPC_MESSAGE_ROUTED3(ShellViewMsg_CaptureTextDump,
                    bool /* as_text */,
                    bool /* printing */,
                    bool /* recursive */)

// Send a text dump of the WebContents to the render host.
IPC_MESSAGE_ROUTED1(ShellViewHostMsg_TextDump,
                    std::string /* dump */)

// The main frame of the render view finished loading.
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_DidFinishLoad)

// The following messages correspond to methods of the layoutTestController.
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_NotifyDone)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_DumpAsText)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_DumpChildFramesAsText)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_SetPrinting)
IPC_MESSAGE_ROUTED1(
    ShellViewHostMsg_SetShouldStayOnPageAfterHandlingBeforeUnload,
    bool /* should_stay_on_page */)
IPC_MESSAGE_ROUTED0(ShellViewHostMsg_WaitUntilDone)
