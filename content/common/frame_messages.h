// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for interacting with frames.
// Multiply-included message file, hence no include guard.

#include "content/common/content_export.h"
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

