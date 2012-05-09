// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for drag and drop.
// Multiply-included message file, hence no include guard.

#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "ui/gfx/point.h"
#include "webkit/glue/webdropdata.h"

#define IPC_MESSAGE_START DragMsgStart

IPC_ENUM_TRAITS(WebKit::WebDragOperation)

IPC_STRUCT_TRAITS_BEGIN(WebDropData::FileInfo)
  IPC_STRUCT_TRAITS_MEMBER(path)
  IPC_STRUCT_TRAITS_MEMBER(display_name)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebDropData)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(url_title)
  IPC_STRUCT_TRAITS_MEMBER(download_metadata)
  IPC_STRUCT_TRAITS_MEMBER(filenames)
  IPC_STRUCT_TRAITS_MEMBER(filesystem_id)
  IPC_STRUCT_TRAITS_MEMBER(plain_text)
  IPC_STRUCT_TRAITS_MEMBER(text_html)
  IPC_STRUCT_TRAITS_MEMBER(html_base_url)
  IPC_STRUCT_TRAITS_MEMBER(file_description_filename)
  IPC_STRUCT_TRAITS_MEMBER(file_contents)
  IPC_STRUCT_TRAITS_MEMBER(custom_data)
IPC_STRUCT_TRAITS_END()

// Messages sent from the browser to the renderer.

IPC_MESSAGE_ROUTED4(DragMsg_TargetDragEnter,
                    WebDropData /* drop_data */,
                    gfx::Point /* client_pt */,
                    gfx::Point /* screen_pt */,
                    WebKit::WebDragOperationsMask /* ops_allowed */)

IPC_MESSAGE_ROUTED3(DragMsg_TargetDragOver,
                    gfx::Point /* client_pt */,
                    gfx::Point /* screen_pt */,
                    WebKit::WebDragOperationsMask /* ops_allowed */)

IPC_MESSAGE_ROUTED0(DragMsg_TargetDragLeave)

IPC_MESSAGE_ROUTED2(DragMsg_TargetDrop,
                    gfx::Point /* client_pt */,
                    gfx::Point /* screen_pt */)

// Notifies the renderer of updates in mouse position of an in-progress
// drag.  if |ended| is true, then the user has ended the drag operation.
IPC_MESSAGE_ROUTED4(DragMsg_SourceEndedOrMoved,
                    gfx::Point /* client_pt */,
                    gfx::Point /* screen_pt */,
                    bool /* ended */,
                    WebKit::WebDragOperation /* drag_operation */)

// Notifies the renderer that the system DoDragDrop call has ended.
IPC_MESSAGE_ROUTED0(DragMsg_SourceSystemDragEnded)

// Messages sent from the renderer to the browser.

// Used to tell the parent the user started dragging in the content area. The
// WebDropData struct contains contextual information about the pieces of the
// page the user dragged. The parent uses this notification to initiate a
// drag session at the OS level.
IPC_MESSAGE_ROUTED4(DragHostMsg_StartDragging,
                    WebDropData /* drop_data */,
                    WebKit::WebDragOperationsMask /* ops_allowed */,
                    SkBitmap /* image */,
                    gfx::Point /* image_offset */)

// The page wants to update the mouse cursor during a drag & drop operation.
// |is_drop_target| is true if the mouse is over a valid drop target.
IPC_MESSAGE_ROUTED1(DragHostMsg_UpdateDragCursor,
                    WebKit::WebDragOperation /* drag_operation */)

// Notifies the host that the renderer finished a drop operation.
IPC_MESSAGE_ROUTED0(DragHostMsg_TargetDrop_ACK)
