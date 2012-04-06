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
#include "ui/gfx/size.h"

#define IPC_MESSAGE_START BrowserPluginMsgStart

// Browser plugin messages

// -----------------------------------------------------------------------------
// These messages are from the host renderer to the browser process

// A renderer sends this to the browser process when it wants to
// create a browser plugin.  The browser will create a guest renderer process
// if necessary.
IPC_MESSAGE_ROUTED4(BrowserPluginHostMsg_OpenChannel,
                    int /* plugin instance id*/,
                    long long /* frame id */,
                    std::string /* src */,
                    gfx::Size /* size */)

// -----------------------------------------------------------------------------
// These messages are from the browser process to the guest renderer.

// Creates a channel to talk to a renderer. The guest renderer will respond
// with BrowserPluginHostMsg_ChannelCreated.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_CreateChannel,
                     base::ProcessHandle /* host_renderer_process_handle */,
                     int /* host_renderer_id */)

// -----------------------------------------------------------------------------
// These messages are from the guest renderer to the browser process

// Reply to BrowserPluginMsg_CreateChannel. The handle will be NULL if the
// channel could not be established. This could be because the IPC could not be
// created for some weird reason, but more likely that the renderer failed to
// initialize properly.
IPC_MESSAGE_CONTROL1(BrowserPluginHostMsg_ChannelCreated,
                     IPC::ChannelHandle /* handle */)

// Indicates the guest renderer is ready in response to a ViewMsg_New
IPC_MESSAGE_ROUTED0(BrowserPluginHostMsg_GuestReady)

// A host renderer sends this message to the browser when it wants
// to resize a guest plugin container so that the guest is relaid out
// according to the new size.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_ResizeGuest,
                    int32, /* width */
                    int32  /* height */)

// -----------------------------------------------------------------------------
// These messages are from the browser process to the host renderer.

// A guest instance is ready to be placed.
IPC_MESSAGE_ROUTED3(BrowserPluginMsg_GuestReady_ACK,
                    int /* instance id */,
                    base::ProcessHandle /* plugin_process_handle */,
                    IPC::ChannelHandle /* handle to channel */)


