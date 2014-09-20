// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message header, no traditional include guard.

#include <string>

#include "base/basictypes.h"
#include "base/process/process.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/edit_command.h"
#include "content/common/frame_param_macros.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/drop_data.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/WebKit/public/web/WebCompositionUnderline.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"
#include "third_party/WebKit/public/web/WebDragStatus.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START BrowserPluginMsgStart


IPC_ENUM_TRAITS_MAX_VALUE(blink::WebDragStatus, blink::WebDragStatusLast)

IPC_STRUCT_BEGIN(BrowserPluginHostMsg_ResizeGuest_Params)
  // The new size of guest view.
  IPC_STRUCT_MEMBER(gfx::Size, view_size)
  // Indicates the scale factor of the embedder WebView.
  IPC_STRUCT_MEMBER(float, scale_factor)
  // Indicates a request for a full repaint of the page.
  // This is required for switching from compositing to the software
  // rendering path.
  IPC_STRUCT_MEMBER(bool, repaint)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(BrowserPluginHostMsg_Attach_Params)
  IPC_STRUCT_MEMBER(bool, focused)
  IPC_STRUCT_MEMBER(bool, visible)
  IPC_STRUCT_MEMBER(BrowserPluginHostMsg_ResizeGuest_Params,
                    resize_guest_params)
  IPC_STRUCT_MEMBER(gfx::Point, origin)
IPC_STRUCT_END()

// Browser plugin messages

// -----------------------------------------------------------------------------
// These messages are from the embedder to the browser process.

// This message is sent from BrowserPlugin to BrowserPluginGuest to issue an
// edit command.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_ExecuteEditCommand,
                    int /* browser_plugin_instance_id */,
                    std::string /* command */)

// This message must be sent just before sending a key event.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_SetEditCommandsForNextKeyEvent,
                    int /* browser_plugin_instance_id */,
                    std::vector<content::EditCommand> /* edit_commands */)

// This message is sent from BrowserPlugin to BrowserPluginGuest whenever IME
// composition state is updated.
IPC_MESSAGE_ROUTED5(
    BrowserPluginHostMsg_ImeSetComposition,
    int /* browser_plugin_instance_id */,
    std::string /* text */,
    std::vector<blink::WebCompositionUnderline> /* underlines */,
    int /* selectiont_start */,
    int /* selection_end */)

// This message is sent from BrowserPlugin to BrowserPluginGuest to notify that
// confirming the current composition is requested.
IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_ImeConfirmComposition,
                    int /* browser_plugin_instance_id */,
                    std::string /* text */,
                    bool /* keep selection */)

// Deletes the current selection plus the specified number of characters before
// and after the selection or caret.
IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_ExtendSelectionAndDelete,
                    int /* browser_plugin_instance_id */,
                    int /* before */,
                    int /* after */)

// This message is sent to the browser process to indicate that a BrowserPlugin
// has taken ownership of the lifetime of the guest of the given
// |browser_plugin_instance_id|. |params| is the state of the BrowserPlugin
// taking ownership of the guest.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_Attach,
                    int /* browser_plugin_instance_id */,
                    BrowserPluginHostMsg_Attach_Params /* params */)

// Tells the guest to focus or defocus itself.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_SetFocus,
                    int /* browser_plugin_instance_id */,
                    bool /* enable */)

// Sends an input event to the guest.
IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_HandleInputEvent,
                    int /* browser_plugin_instance_id */,
                    gfx::Rect /* guest_window_rect */,
                    IPC::WebInputEventPointer /* event */)

IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_CopyFromCompositingSurfaceAck,
                    int /* browser_plugin_instance_id */,
                    int /* request_id */,
                    SkBitmap)

// Notify the guest renderer that some resources given to the embededer
// are not used any more.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_ReclaimCompositorResources,
                    int /* browser_plugin_instance_id */,
                    FrameHostMsg_ReclaimCompositorResources_Params /* params */)

// When a BrowserPlugin has been removed from the embedder's DOM, it informs
// the browser process to cleanup the guest.
IPC_MESSAGE_ROUTED1(BrowserPluginHostMsg_PluginDestroyed,
                    int /* browser_plugin_instance_id */)

// Tells the guest it has been shown or hidden.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_SetVisibility,
                    int /* browser_plugin_instance_id */,
                    bool /* visible */)

// Tells the guest that a drag event happened on the plugin.
IPC_MESSAGE_ROUTED5(BrowserPluginHostMsg_DragStatusUpdate,
                    int /* browser_plugin_instance_id */,
                    blink::WebDragStatus /* drag_status */,
                    content::DropData /* drop_data */,
                    blink::WebDragOperationsMask /* operation_mask */,
                    gfx::Point /* plugin_location */)

// Sends a PointerLock Lock ACK to the BrowserPluginGuest.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_LockMouse_ACK,
                    int /* browser_plugin_instance_id */,
                    bool /* succeeded */)

// Sends a PointerLock Unlock ACK to the BrowserPluginGuest.
IPC_MESSAGE_ROUTED1(BrowserPluginHostMsg_UnlockMouse_ACK,
                    int /* browser_plugin_instance_id */)

// Sent when plugin's position has changed.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_UpdateGeometry,
                    int /* browser_plugin_instance_id */,
                    gfx::Rect /* view_rect */)

// -----------------------------------------------------------------------------
// These messages are from the guest renderer to the browser process

// A embedder sends this message to the browser when it wants
// to resize a guest plugin container so that the guest is relaid out
// according to the new size.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_ResizeGuest,
                    int /* browser_plugin_instance_id*/,
                    BrowserPluginHostMsg_ResizeGuest_Params)

// -----------------------------------------------------------------------------
// These messages are from the browser process to the embedder.

// This message is sent in response to a completed attachment of a guest
// to a BrowserPlugin.
IPC_MESSAGE_CONTROL1(BrowserPluginMsg_Attach_ACK,
                     int /* browser_plugin_instance_id */)

// When the guest crashes, the browser process informs the embedder through this
// message.
IPC_MESSAGE_CONTROL1(BrowserPluginMsg_GuestGone,
                     int /* browser_plugin_instance_id */)

// When the user tabs to the end of the tab stops of a guest, the browser
// process informs the embedder to tab out of the browser plugin.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_AdvanceFocus,
                     int /* browser_plugin_instance_id */,
                     bool /* reverse */)

// When the guest starts/stops listening to touch events, it needs to notify the
// plugin in the embedder about it.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_ShouldAcceptTouchEvents,
                     int /* browser_plugin_instance_id */,
                     bool /* accept */)

// Tells the guest to change its background opacity.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_SetContentsOpaque,
                     int /* browser_plugin_instance_id */,
                     bool /* opaque */)

// Inform the embedder of the cursor the guest wishes to display.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_SetCursor,
                     int /* browser_plugin_instance_id */,
                     content::WebCursor /* cursor */)

IPC_MESSAGE_CONTROL4(BrowserPluginMsg_CopyFromCompositingSurface,
                     int /* browser_plugin_instance_id */,
                     int /* request_id */,
                     gfx::Rect  /* source_rect */,
                     gfx::Size  /* dest_size */)

IPC_MESSAGE_CONTROL2(BrowserPluginMsg_CompositorFrameSwapped,
                     int /* browser_plugin_instance_id */,
                     FrameMsg_CompositorFrameSwapped_Params /* params */)

// Forwards a PointerLock Unlock request to the BrowserPlugin.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_SetMouseLock,
                     int /* browser_plugin_instance_id */,
                     bool /* enable */)

// Acknowledge that we presented an ubercomp frame.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_CompositorFrameSwappedACK,
                    int /* browser_plugin_instance_id */,
                    FrameHostMsg_CompositorFrameSwappedACK_Params /* params */)
