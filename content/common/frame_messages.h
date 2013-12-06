// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for interacting with frames.
// Multiply-included message file, hence no include guard.

#include "content/common/content_export.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START FrameMsgStart

// Sent by the renderer when a child frame is created in the renderer. The
// |parent_frame_id| and |frame_id| are NOT routing ids. They are
// renderer-allocated identifiers used for tracking a frame's creation.
//
// Each of these messages will have a corresponding FrameHostMsg_Detach message
// sent when the frame is detached from the DOM.
//
// TOOD(ajwong): replace parent_render_frame_id and frame_id with just the
// routing ids.
IPC_SYNC_MESSAGE_CONTROL4_1(FrameHostMsg_CreateChildFrame,
                            int32 /* parent_render_frame_id */,
                            int64 /* parent_frame_id */,
                            int64 /* frame_id */,
                            std::string /* frame_name */,
                            int /* new_render_frame_id */)

// Sent by the renderer to the parent RenderFrameHost when a child frame is
// detached from the DOM.
IPC_MESSAGE_ROUTED2(FrameHostMsg_Detach,
                    int64 /* parent_frame_id */,
                    int64 /* frame_id */)

// Sent when the renderer starts a provisional load for a frame.
IPC_MESSAGE_ROUTED4(FrameHostMsg_DidStartProvisionalLoadForFrame,
                    int64 /* frame_id */,
                    int64 /* parent_frame_id */,
                    bool /* true if it is the main frame */,
                    GURL /* url */)

// Sent to the browser when the renderer detects it is blocked on a pepper
// plugin message for too long. This is also sent when it becomes unhung
// (according to the value of is_hung). The browser can give the user the
// option of killing the plugin.
IPC_MESSAGE_ROUTED3(FrameHostMsg_PepperPluginHung,
                    int /* plugin_child_id */,
                    base::FilePath /* path */,
                    bool /* is_hung */)

// Sent by the renderer process to indicate that a plugin instance has crashed.
// Note: |plugin_pid| should not be trusted. The corresponding process has
// probably died. Moreover, the ID may have been reused by a new process. Any
// usage other than displaying it in a prompt to the user is very likely to be
// wrong.
IPC_MESSAGE_ROUTED2(FrameHostMsg_PluginCrashed,
                    base::FilePath /* plugin_path */,
                    base::ProcessId /* plugin_pid */)

// Return information about a plugin for the given URL and MIME
// type. If there is no matching plugin, |found| is false.
// |actual_mime_type| is the actual mime type supported by the
// found plugin.
IPC_SYNC_MESSAGE_CONTROL4_3(FrameHostMsg_GetPluginInfo,
                            int /* render_frame_id */,
                            GURL /* url */,
                            GURL /* page_url */,
                            std::string /* mime_type */,
                            bool /* found */,
                            content::WebPluginInfo /* plugin info */,
                            std::string /* actual_mime_type */)

// A renderer sends this to the browser process when it wants to
// create a plugin.  The browser will create the plugin process if
// necessary, and will return a handle to the channel on success.
// On error an empty string is returned.
IPC_SYNC_MESSAGE_CONTROL4_2(FrameHostMsg_OpenChannelToPlugin,
                            int /* render_frame_id */,
                            GURL /* url */,
                            GURL /* page_url */,
                            std::string /* mime_type */,
                            IPC::ChannelHandle /* channel_handle */,
                            content::WebPluginInfo /* info */)
