// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_DEVTOOLS_MESSAGES_H_
#define CONTENT_COMMON_DEVTOOLS_MESSAGES_H_

// Developer tools consist of the following parts:
//
// DevToolsAgent lives in the renderer of an inspected page and provides access
// to the pages resources, DOM, v8 etc. by means of IPC messages.
//
// DevToolsClient is a thin delegate that lives in the tools front-end
// renderer and converts IPC messages to frontend method calls and allows the
// frontend to send messages to the DevToolsAgent.
//
// All the messages are routed through browser process. There is a
// DevToolsManager living in the browser process that is responsible for
// routing logistics. It is also capable of sending direct messages to the
// agent rather than forwarding messages between agents and clients only.
//
// Chain of communication between the components may be described by the
// following diagram:
//  ----------------------------
// | (tools frontend            |
// | renderer process)          |
// |                            |            --------------------
// |tools    <--> DevToolsClient+<-- IPC -->+ (browser process)  |
// |frontend                    |           |                    |
//  ----------------------------             ---------+----------
//                                                    ^
//                                                    |
//                                                   IPC
//                                                    |
//                                                    v
//                          --------------------------+--------
//                         | inspected page <--> DevToolsAgent |
//                         |                                   |
//                         | (inspected page renderer process) |
//                          -----------------------------------
//
// This file describes developer tools message types.

#include <map>
#include <string>

#include "content/common/content_export.h"
#include "content/public/common/console_message_level.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START DevToolsMsgStart

// These are messages sent from DevToolsAgent to DevToolsClient through the
// browser.

// Agent -> Client message chunk.
//   |is_first| marks the first chunk, comes with the |message_size| for
//   total message size.
//   |is_last| marks the last chunk. |call_id|, |session_id| and |post_state|
//   are optional parameters passed with the last chunk of the protocol
//   response.
IPC_STRUCT_BEGIN(DevToolsMessageChunk)
  IPC_STRUCT_MEMBER(bool, is_first)
  IPC_STRUCT_MEMBER(bool, is_last)
  IPC_STRUCT_MEMBER(int, message_size)
  IPC_STRUCT_MEMBER(int, session_id)
  IPC_STRUCT_MEMBER(int, call_id)
  IPC_STRUCT_MEMBER(std::string, data)
  IPC_STRUCT_MEMBER(std::string, post_state)
IPC_STRUCT_END()

// Sends response from the agent to the client. Supports chunked encoding.
IPC_MESSAGE_ROUTED1(DevToolsClientMsg_DispatchOnInspectorFrontend,
                    DevToolsMessageChunk /* message */)

//-----------------------------------------------------------------------------
// These are messages sent from DevToolsClient to DevToolsAgent through the
// browser.
// Tells agent that there is a client host connected to it.
IPC_MESSAGE_ROUTED1(DevToolsAgentMsg_Attach, int /* session_id */)

// Tells agent that a client host was disconnected from another agent and
// connected to this one.
IPC_MESSAGE_ROUTED2(DevToolsAgentMsg_Reattach,
                    int /* session_id */,
                    std::string /* agent_state */)

// Tells agent that there is no longer a client host connected to it.
IPC_MESSAGE_ROUTED1(DevToolsAgentMsg_Detach, int /* session_id */)

// WebKit-level transport.
IPC_MESSAGE_ROUTED4(DevToolsAgentMsg_DispatchOnInspectorBackend,
                    int /* session_id */,
                    int /* call_id */,
                    std::string /* method */,
                    std::string /* message */)

// Inspect element with the given coordinates.
IPC_MESSAGE_ROUTED3(DevToolsAgentMsg_InspectElement,
                    int /* session_id */,
                    int /* x */,
                    int /* y */)

// ACK for DevToolsAgentHostMsg_RequestNewWindow message.
IPC_MESSAGE_ROUTED2(DevToolsAgentMsg_RequestNewWindow_ACK,
                    int /* session_id */,
                    bool /* success */)

//-----------------------------------------------------------------------------
// These are messages sent from renderer's DevToolsAgent to browser.

// Requests new DevTools window being opened for frame in the same process
// with given routing id.
IPC_MESSAGE_ROUTED2(DevToolsAgentHostMsg_RequestNewWindow,
                    int /* session_id */,
                    int /* frame_route_id */)

#endif  // CONTENT_COMMON_DEVTOOLS_MESSAGES_H_
