// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

// Multiply-included message file, no standard include guard.
#include <map>
#include <string>

#include "ipc/ipc_message_macros.h"

// Singly-included section.
#ifndef CHROME_COMMON_DEVTOOLS_MESSAGES_H_
#define CHROME_COMMON_DEVTOOLS_MESSAGES_H_

typedef std::map<std::string, std::string> DevToolsRuntimeProperties;

#endif  // CHROME_COMMON_DEVTOOLS_MESSAGES_H_

#define IPC_MESSAGE_START DevToolsMsgStart

// These are messages sent from DevToolsAgent to DevToolsClient through the
// browser.
// WebKit-level transport.
IPC_MESSAGE_CONTROL1(DevToolsClientMsg_DispatchOnInspectorFrontend,
                     std::string /* message */)

// Legacy debugger output message.
IPC_MESSAGE_CONTROL1(DevToolsClientMsg_DebuggerOutput,
                     std::string /* message */)


//-----------------------------------------------------------------------------
// These are messages sent from DevToolsClient to DevToolsAgent through the
// browser.
// Tells agent that there is a client host connected to it.
IPC_MESSAGE_CONTROL1(DevToolsAgentMsg_Attach,
                     DevToolsRuntimeProperties /* properties */)

// Tells agent that there is no longer a client host connected to it.
IPC_MESSAGE_CONTROL0(DevToolsAgentMsg_Detach)

// Tells agent that the front-end has been loaded
IPC_MESSAGE_CONTROL0(DevToolsAgentMsg_FrontendLoaded)

// WebKit-level transport.
IPC_MESSAGE_CONTROL1(DevToolsAgentMsg_DispatchOnInspectorBackend,
                     std::string /* message */)

// Send debugger command to the debugger agent. Debugger commands should
// be handled on IO thread(while all other devtools messages are handled in
// the render thread) to allow executing the commands when v8 is on a
// breakpoint.
IPC_MESSAGE_CONTROL1(DevToolsAgentMsg_DebuggerCommand,
                     std::string  /* command */)

// Inspect element with the given coordinates.
IPC_MESSAGE_CONTROL2(DevToolsAgentMsg_InspectElement,
                     int /* x */,
                     int /* y */)


//-----------------------------------------------------------------------------
// These are messages sent from the browser to the renderer.

// RenderViewHostDelegate::RenderViewCreated method sends this message to a
// new renderer to notify it that it will host developer tools UI and should
// set up all neccessary bindings and create DevToolsClient instance that
// will handle communication with inspected page DevToolsAgent.
IPC_MESSAGE_ROUTED0(DevToolsMsg_SetupDevToolsClient)


//-----------------------------------------------------------------------------
// These are messages sent from the renderer to the browser.

// Wraps an IPC message that's destined to the DevToolsClient on
// DevToolsAgent->browser hop.
IPC_MESSAGE_ROUTED1(DevToolsHostMsg_ForwardToClient,
                    IPC::Message /* one of DevToolsClientMsg_XXX types */)

// Wraps an IPC message that's destined to the DevToolsAgent on
// DevToolsClient->browser hop.
IPC_MESSAGE_ROUTED1(DevToolsHostMsg_ForwardToAgent,
                    IPC::Message /* one of DevToolsAgentMsg_XXX types */)

// Activates (brings to the front) corresponding dev tools window.
IPC_MESSAGE_ROUTED0(DevToolsHostMsg_ActivateWindow)

// Closes dev tools window that is inspecting current render_view_host.
IPC_MESSAGE_ROUTED0(DevToolsHostMsg_CloseWindow)

// Attaches dev tools window that is inspecting current render_view_host.
IPC_MESSAGE_ROUTED0(DevToolsHostMsg_RequestDockWindow)

// Detaches dev tools window that is inspecting current render_view_host.
IPC_MESSAGE_ROUTED0(DevToolsHostMsg_RequestUndockWindow)

// Updates runtime features store in devtools manager in order to support
// cross-navigation instrumentation.
IPC_MESSAGE_ROUTED2(DevToolsHostMsg_RuntimePropertyChanged,
                    std::string /* name */,
                    std::string /* value */)
