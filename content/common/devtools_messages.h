// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "content/common/content_export.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/console_message_level.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START DevToolsMsgStart

// These are messages sent from DevToolsAgent to DevToolsClient through the
// browser.
// WebKit-level transport.
IPC_MESSAGE_ROUTED1(DevToolsClientMsg_DispatchOnInspectorFrontend,
                    std::string /* message */)

//-----------------------------------------------------------------------------
// These are messages sent from DevToolsClient to DevToolsAgent through the
// browser.
// Tells agent that there is a client host connected to it.
IPC_MESSAGE_ROUTED0(DevToolsAgentMsg_Attach)

// Tells agent that a client host was disconnected from another agent and
// connected to this one.
IPC_MESSAGE_ROUTED1(DevToolsAgentMsg_Reattach,
                    std::string /* agent_state */)

// Tells agent that there is no longer a client host connected to it.
IPC_MESSAGE_ROUTED0(DevToolsAgentMsg_Detach)

// WebKit-level transport.
IPC_MESSAGE_ROUTED1(DevToolsAgentMsg_DispatchOnInspectorBackend,
                    std::string /* message */)

// Inspect element with the given coordinates.
IPC_MESSAGE_ROUTED2(DevToolsAgentMsg_InspectElement,
                    int /* x */,
                    int /* y */)

// Add message to the devtools console.
IPC_MESSAGE_ROUTED2(DevToolsAgentMsg_AddMessageToConsole,
                    content::ConsoleMessageLevel /* level */,
                    std::string /* message */)

// Notifies worker devtools agent that it should pause worker context
// when it starts and wait until either DevTools client is attached or
// explicit resume notification is received.
IPC_MESSAGE_ROUTED0(DevToolsAgentMsg_PauseWorkerContextOnStart)

// Worker DevTools agent should resume worker execution.
IPC_MESSAGE_ROUTED0(DevToolsAgentMsg_ResumeWorkerContext)

//-----------------------------------------------------------------------------
// These are messages sent from the browser to the renderer.

// RenderViewHostDelegate::RenderViewCreated method sends this message to a
// new renderer to notify it that it will host developer tools UI and should
// set up all neccessary bindings and create DevToolsClient instance that
// will handle communication with inspected page DevToolsAgent.
IPC_MESSAGE_ROUTED0(DevToolsMsg_SetupDevToolsClient)


//-----------------------------------------------------------------------------
// These are messages sent from the renderer to the browser.

// Activates (brings to the front) corresponding dev tools window.
IPC_MESSAGE_ROUTED0(DevToolsHostMsg_ActivateWindow)

// Closes dev tools window that is inspecting current render_view_host.
IPC_MESSAGE_ROUTED0(DevToolsHostMsg_CloseWindow)

// Moves the corresponding dev tools window by the specified offset.
IPC_MESSAGE_ROUTED2(DevToolsHostMsg_MoveWindow,
                    int /* x */,
                    int /* y */)

// Specifies side for devtools to dock to.
IPC_MESSAGE_ROUTED1(DevToolsHostMsg_RequestSetDockSide,
                    std::string /* side */)

// Opens given URL in the new tab.
IPC_MESSAGE_ROUTED1(DevToolsHostMsg_OpenInNewTab,
                    std::string /* url */)

// Shows Save As dialog for content.
IPC_MESSAGE_ROUTED3(DevToolsHostMsg_Save,
                    std::string /* url */,
                    std::string /* content */,
                    bool /* save_as */)

// Appends given |content| to the file that has been associated with the
// given |url| by Save message handler.
IPC_MESSAGE_ROUTED2(DevToolsHostMsg_Append,
                    std::string /* url */,
                    std::string /* content */)

// Requests the list of filesystems previously added for devtools.
IPC_MESSAGE_ROUTED0(DevToolsHostMsg_RequestFileSystems)

// Shows a dialog to select a folder to which an isolated filesystem is added.
IPC_MESSAGE_ROUTED0(DevToolsHostMsg_AddFileSystem)

// Removes a previously added devtools filesystem given by |file_system_path|.
IPC_MESSAGE_ROUTED1(DevToolsHostMsg_RemoveFileSystem,
                    std::string /* file_system_path */)

// Updates agent runtime state stored in devtools manager in order to support
// cross-navigation instrumentation.
IPC_MESSAGE_ROUTED1(DevToolsHostMsg_SaveAgentRuntimeState,
                    std::string /* state */)

// Clears browser cache.
IPC_MESSAGE_ROUTED0(DevToolsHostMsg_ClearBrowserCache)

// Clears browser cookies.
IPC_MESSAGE_ROUTED0(DevToolsHostMsg_ClearBrowserCookies)


//-----------------------------------------------------------------------------
// These are messages sent from the inspected page renderer to the worker
// renderer.
