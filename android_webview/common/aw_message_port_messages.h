// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include <vector>

#include "base/basictypes.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START AwMessagePortMsgStart

//-----------------------------------------------------------------------------
// MessagePort messages
// These are messages sent from the browser to the renderer process.

// Tells the renderer to convert the sent message from a WebSerializeScript
// format to a base::ListValue. Due to the complexities of renderer/browser
// relation, this can only be done in renderer for now.
IPC_MESSAGE_ROUTED3(AwMessagePortMsg_Message,
                    int /* recipient message port id */,
                    base::string16 /* message */,
                    std::vector<int> /* sent message port_ids */)

//-----------------------------------------------------------------------------
// These are messages sent from the renderer to the browser process.

// Response to AwMessagePortMessage_ConvertMessage
IPC_MESSAGE_ROUTED3(AwMessagePortHostMsg_ConvertedMessage,
                    int /* recipient message port id */,
                    base::ListValue /* converted message */,
                    std::vector<int> /* sent message port_ids */)
