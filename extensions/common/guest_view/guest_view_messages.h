// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for GuestViews.
// Multiply-included message file, hence no include guard.

#include <string>

#include "base/values.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

#define IPC_MESSAGE_START GuestViewMsgStart

// Messages sent from the browser to the renderer.

// The ACK for GuestViewHostMsg_CreateMimeHandlerViewGuest.
IPC_MESSAGE_CONTROL1(GuestViewMsg_CreateMimeHandlerViewGuestACK,
                     int /* element_instance_id */)

// Once a MimeHandlerView guest's JavaScript onload function has been called,
// this IPC is sent to the container to notify it.
IPC_MESSAGE_CONTROL1(GuestViewMsg_MimeHandlerViewGuestOnLoadCompleted,
                     int /* element_instance_id */)

// Once a RenderView proxy has been created for the guest in the embedder render
// process, this IPC informs the embedder of the proxy's routing ID.
IPC_MESSAGE_CONTROL2(GuestViewMsg_GuestAttached,
                     int /* element_instance_id */,
                     int /* source_routing_id */)

// This IPC tells the browser process to detach the provided
// |element_instance_id| from a GuestViewBase if it is attached to one.
// In other words, routing of input and graphics will no longer flow through
// the container associated with the provided ID.
IPC_MESSAGE_CONTROL1(GuestViewMsg_GuestDetached,
                     int /* element_instance_id*/)

// Messages sent from the renderer to the browser.

// Sent by the renderer to set initialization parameters of a Browser Plugin
// that is identified by |element_instance_id|.
IPC_MESSAGE_CONTROL3(GuestViewHostMsg_AttachGuest,
                     int /* element_instance_id */,
                     int /* guest_instance_id */,
                     base::DictionaryValue /* attach_params */)

// Tells the browser to create a mime handler guest view for a plugin.
IPC_MESSAGE_CONTROL4(GuestViewHostMsg_CreateMimeHandlerViewGuest,
                     int /* render_frame_id */,
                     std::string /* view_id */,
                     int /* element_instance_id */,
                     gfx::Size /* element_size */)

// A renderer sends this message when it wants to resize a guest.
IPC_MESSAGE_CONTROL3(GuestViewHostMsg_ResizeGuest,
                     int /* routing_id */,
                     int /* element_instance_id*/,
                     gfx::Size /* new_size */)
