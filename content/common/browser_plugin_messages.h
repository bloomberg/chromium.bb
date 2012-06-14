// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message header, no traditional include guard.

#include <string>

#include "base/basictypes.h"
#include "base/process.h"
#include "content/common/content_export.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ppapi/c/pp_instance.h"
#include "ui/gfx/size.h"

#define IPC_MESSAGE_START BrowserPluginMsgStart

// Browser plugin messages

// -----------------------------------------------------------------------------
// These messages are from the embedder to the browser process

// A renderer sends this to the browser process when it wants to
// create a browser plugin.  The browser will create a guest renderer process
// if necessary.
IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_NavigateFromEmbedder,
                    int /* plugin instance id*/,
                    long long /* frame id */,
                    std::string /* src */)

// Initially before we create a guest renderer, browser plugin containers
// have a placeholder called BrowserPlugin where each BrowserPlugin has a unique
// ID. During pepper plugin initialization, the embedder page and the plugin
// negotiate an ID of type PP_Instance. The browser talks to the guest
// RenderView via yet another identifier called the routing ID. The browser
// has to keep track of how all these identifiers are associated with one
// another.
// For reference:
// 1. The embedder page sees the guest renderer as a plugin and so it talks
//    to the guest via the PP_Instance identifer.
// 2. The guest renderer talks to the browser and vice versa via a routing ID.
// 3. The BrowserPlugin ID uniquely identifies a browser plugin container
//    instance within an embedder.
//    This identifier exists prior to the existence of the routing ID and the
//    PP_Instance identifier.
// The purpose of this message is to tell the browser to map a PP_Instance
// identifier to BrowserPlugin identifier.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_MapInstance,
                    int /* container_id */,
                    PP_Instance /* instance */)

// -----------------------------------------------------------------------------
// These messages are from the embedder render process to the guest render
// process.

IPC_MESSAGE_CONTROL2(BrowserPluginMsg_GuestReady,
                     PP_Instance /* instance */,
                     int /* embedder_container_id */)

// -----------------------------------------------------------------------------
// These messages are from the guest renderer to the browser process

IPC_MESSAGE_ROUTED1(BrowserPluginHostMsg_ConnectToChannel,
                    IPC::ChannelHandle /* handle */)

// A embedder sends this message to the browser when it wants
// to resize a guest plugin container so that the guest is relaid out
// according to the new size.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_ResizeGuest,
                    int32, /* width */
                    int32  /* height */)

IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_NavigateFromGuest,
                    PP_Instance /* instance */,
                    std::string /* src */)

// -----------------------------------------------------------------------------
// These messages are from the browser process to the embedder.

// A guest instance is ready to be placed.
IPC_MESSAGE_CONTROL3(BrowserPluginMsg_LoadGuest,
                     int /* instance_id */,
                     int /* guest_process_id */,
                     IPC::ChannelHandle /* channel_handle */)

IPC_MESSAGE_CONTROL2(BrowserPluginMsg_AdvanceFocus,
                     int /* instance_id */,
                     bool /* reverse */)

